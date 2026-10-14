#include "adc_dma_bsp.h"
#include "adc_current_config.h"
#include "main.h"
#include "motor_lock.h"
#include <math.h>
#include <string.h>
extern ADC_HandleTypeDef hadc1,hadc2,hadc3,hadc4,hadc5;
#if N_PULSE_ENABLE_CURRENT_ACQUISITION
extern OPAMP_HandleTypeDef hopamp1,hopamp2,hopamp3,hopamp4,hopamp5,hopamp6;
#endif
static ADC_HandleTypeDef *const adcs[5]={&hadc1,&hadc2,&hadc3,&hadc4,&hadc5};
#if N_PULSE_ENABLE_CURRENT_ACQUISITION
static OPAMP_HandleTypeDef *const amps[6]={&hopamp1,&hopamp2,&hopamp3,&hopamp4,&hopamp5,&hopamp6};
static const uint32_t ranks[5]={1,2,1,3,2};
static const IRQn_Type irqs[5]={DMA1_Channel8_IRQn,DMA1_Channel2_IRQn,DMA1_Channel3_IRQn,DMA1_Channel4_IRQn,DMA1_Channel5_IRQn};
#endif
/* One finite scan per request. ADC4 rank2 remains Batt_lvl, never current. */
static volatile uint16_t buffers[5][3];
static const uint8_t adc_map[MOTOR_COUNT]={2,0,1,1,4,4,3,3};
static const uint8_t rank_map[MOTOR_COUNT]={0,0,0,1,0,1,2,0};
static uint16_t completed[5][3];
static uint32_t stamp[5], errors[5], sequence[5], last_request;
static bool seen[5], pending[5], started;
static adc_current_calibration_t calibration[MOTOR_COUNT];
static bool calibrated[MOTOR_COUNT];
static float vref;
bool ADC_DMA_BSP_SetVref(float volts) {
    if (!isfinite(volts) || volts<=0 || volts>5.0f) return false;
    uint32_t key=Motor_Lock();
    for(unsigned i=0;i<MOTOR_COUNT;++i) if(Motor_IsEnabled((motor_id_t)i)) { Motor_Unlock(key); return false; }
    vref=volts; Motor_Unlock(key); return true;
}
bool ADC_DMA_BSP_SetCalibration(motor_id_t id,const adc_current_calibration_t *c) {
    if ((unsigned)id>=MOTOR_COUNT || !c || !isfinite(c->offset_v) || c->offset_v<0 ||
        !isfinite(c->shunt_ohm) || c->shunt_ohm<=0 || !isfinite(c->analog_gain) || c->analog_gain<=0 ||
        !isfinite(c->scale) || c->scale<=0 || (c->polarity!=1 && c->polarity!=-1)) return false;
    double derived=(double)c->scale/((double)c->shunt_ohm*c->analog_gain);
    if (!isfinite(derived) || derived<1e-30 || derived>1e30) return false;
    uint32_t key=Motor_Lock();
    if (Motor_IsEnabled(id) || (vref>0 && c->offset_v>vref)) { Motor_Unlock(key); return false; }
    calibration[id]=*c; calibrated[id]=true; Motor_Unlock(key); return true;
}
void ADC_DMA_BSP_Stop(void) {
    uint32_t key=Motor_Lock(); started=false;
    memset(pending,0,sizeof(pending)); Motor_Unlock(key);
#if N_PULSE_ENABLE_CURRENT_ACQUISITION
    for(unsigned i=0;i<5;++i) if(HAL_ADC_Stop_DMA(adcs[i])!=HAL_OK) errors[i]|=ADC_SAMPLE_HAL_ERROR;
    for(unsigned i=0;i<6;++i) (void)HAL_OPAMP_Stop(amps[i]);
#endif
}
bool ADC_DMA_BSP_Init(void) {
    started=false; vref=0; last_request=HAL_GetTick()-ADC_DMA_PERIOD_MS;
    memset(seen,0,sizeof(seen)); memset(pending,0,sizeof(pending));
    memset(errors,0,sizeof(errors)); memset(sequence,0,sizeof(sequence));
    memset(calibrated,0,sizeof(calibrated));
    if (N_PULSE_ADC_VREF_V>0) (void)ADC_DMA_BSP_SetVref(N_PULSE_ADC_VREF_V);
    for(unsigned i=0;i<MOTOR_COUNT;++i)
        (void)ADC_DMA_BSP_SetCalibration((motor_id_t)i,&g_adc_current_calibration[i]);
#if N_PULSE_ENABLE_CURRENT_ACQUISITION
    for(unsigned i=0;i<6;++i) if(HAL_OPAMP_Start(amps[i])!=HAL_OK) {
        for(unsigned a=0;a<5;++a) errors[a]|=ADC_SAMPLE_HAL_ERROR;
        goto failed;
    }
    for(unsigned i=0;i<5;++i) {
        /* Acquisition override only; .ioc pins, ranks and PGA gains retained.
         * Continuous tiny rings cannot prove freshness by polling; completion
         * IRQs at ADC clock rate would overload the controller. */
        adcs[i]->Init.ContinuousConvMode=DISABLE;
        adcs[i]->Init.DMAContinuousRequests=DISABLE;
        adcs[i]->DMA_Handle->Init.Mode=DMA_NORMAL;
        if(HAL_DMA_Init(adcs[i]->DMA_Handle)!=HAL_OK || HAL_ADC_Init(adcs[i])!=HAL_OK ||
           HAL_ADCEx_Calibration_Start(adcs[i],ADC_SINGLE_ENDED)!=HAL_OK) {
            errors[i]|=ADC_SAMPLE_HAL_ERROR; goto failed;
        }
    }
    started=true; ADC_DMA_BSP_Service(); return started;
failed:
    ADC_DMA_BSP_Stop(); return false;
#else
    return true; /* Skipped acquisition is not a valid zero sample. */
#endif
}
void ADC_DMA_BSP_Service(void) {
#if N_PULSE_ENABLE_CURRENT_ACQUISITION
    if(!started) return;
    for(unsigned i=0;i<5;++i) {
        if(__HAL_ADC_GET_FLAG(adcs[i],ADC_FLAG_OVR)) errors[i]|=ADC_SAMPLE_HAL_ERROR;
        if(errors[i]) { ADC_DMA_BSP_Stop(); return; }
    }
    uint32_t now=HAL_GetTick();
    if((uint32_t)(now-last_request)<ADC_DMA_PERIOD_MS) return;
    last_request=now;
    for(unsigned i=0;i<5;++i) {
        /* HAL operations in low priority task, outside Motor_Lock. */
        if(HAL_ADC_Stop_DMA(adcs[i])!=HAL_OK) { errors[i]|=ADC_SAMPLE_HAL_ERROR; continue; }
        uint32_t key=Motor_Lock();
        if(pending[i]) errors[i]|=ADC_SAMPLE_TIMEOUT;
        if(errors[i]) { Motor_Unlock(key); continue; }
        pending[i]=true; Motor_Unlock(key);
        uint32_t enabled=NVIC_GetEnableIRQ(irqs[i]); HAL_NVIC_DisableIRQ(irqs[i]);
        HAL_StatusTypeDef status=HAL_ADC_Start_DMA(adcs[i],(uint32_t *)buffers[i],ranks[i]);
        __HAL_DMA_DISABLE_IT(adcs[i]->DMA_Handle,DMA_IT_HT);
        if(status!=HAL_OK) { errors[i]|=ADC_SAMPLE_HAL_ERROR; pending[i]=false; }
        if(enabled) HAL_NVIC_EnableIRQ(irqs[i]);
    }
    for(unsigned i=0;i<5;++i) if(errors[i]) { ADC_DMA_BSP_Stop(); break; }
#endif
}
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *adc) {
    for(unsigned i=0;i<5;++i) if(adcs[i]==adc) {
        uint32_t key=Motor_Lock();
        if(started && pending[i] && !errors[i]) {
            for(unsigned j=0;j<3;++j) completed[i][j]=buffers[i][j];
            stamp[i]=HAL_GetTick(); ++sequence[i]; seen[i]=true; pending[i]=false;
        }
        Motor_Unlock(key); return;
    }
}
void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *adc) {
    for(unsigned i=0;i<5;++i) if(adcs[i]==adc) {
        uint32_t key=Motor_Lock(); errors[i]|=adc->ErrorCode | ADC_SAMPLE_HAL_ERROR;
        seen[i]=false; pending[i]=false; Motor_Unlock(key); return;
    }
}
bool ADC_DMA_BSP_GetSample(motor_id_t id,adc_current_sample_t *out) {
    if(!out || (unsigned)id>=MOTOR_COUNT) return false;
    *out=(adc_current_sample_t){.voltage_v=NAN,.current_a=NAN,.age_ms=UINT32_MAX};
    uint32_t key=Motor_Lock(); unsigned a=adc_map[id];
    out->active=started; out->error=errors[a]; out->raw=completed[a][rank_map[id]];
    out->timestamp_ms=stamp[a]; out->sequence=sequence[a];
    bool valid=seen[a]; float reference=vref; bool cal=calibrated[id]; adc_current_calibration_t c=calibration[id];
    Motor_Unlock(key);
    if(valid) out->age_ms=HAL_GetTick()-out->timestamp_ms;
    if(out->error) out->state=ADC_CURRENT_ERROR;
    else if(!out->active || !valid) out->state=ADC_CURRENT_INVALID;
    else if(out->age_ms>ADC_DMA_STALE_MS) out->state=ADC_CURRENT_STALE;
    else if(out->raw==0 || out->raw>=4095U) {
        out->state=ADC_CURRENT_SATURATED;
        if(reference>0 && out->raw<=4095U) out->voltage_v=(float)out->raw*reference/4095.0f;
    }
    else {
        out->raw_valid=true; out->state=ADC_CURRENT_RAW_VALID;
        if(reference>0) {
            out->voltage_v=(float)out->raw*reference/4095.0f;
            if(cal && c.offset_v>reference) { out->state=ADC_CURRENT_ERROR; out->error|=ADC_SAMPLE_COEFFICIENT; }
            else if(cal) {
                double current=(out->voltage_v-c.offset_v)*(double)c.polarity*c.scale/(c.shunt_ohm*(double)c.analog_gain);
                if(isfinite(current) && fabs(current)<=3.402823466e38) {
                    out->current_a=(float)current; out->state=ADC_CURRENT_CALIBRATED;
                } else { out->state=ADC_CURRENT_ERROR; out->error|=ADC_SAMPLE_COEFFICIENT; }
            }
        }
    }
    return true;
}
uint16_t ADC_DMA_BSP_GetRaw(motor_id_t id) {
    adc_current_sample_t sample;
    return ADC_DMA_BSP_GetSample(id,&sample) && sample.raw_valid ? sample.raw : 0U;
}
float ADC_DMA_BSP_GetCurrentA(motor_id_t id) {
    adc_current_sample_t sample;
    return ADC_DMA_BSP_GetSample(id,&sample) && sample.state==ADC_CURRENT_CALIBRATED ? sample.current_a : NAN;
}
