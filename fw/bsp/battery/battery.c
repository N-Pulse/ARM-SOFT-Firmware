#include "battery.h"
#include "battery_config.h"
#include "adc_dma_bsp.h"
#include "motor_lock.h"
#include <math.h>

static battery_snapshot_t sample;
static bool have_sample,low_pending,clear_pending,read_started;
static uint32_t low_since,clear_since,last_read_ms;
static bool thresholds_ready(void) {
    return isfinite(BATTERY_LOW_WARNING_V) && BATTERY_LOW_WARNING_V>0 &&
        isfinite(BATTERY_WARNING_CLEAR_V) && BATTERY_WARNING_CLEAR_V>BATTERY_LOW_WARNING_V;
}
static bool divider_ready(void) {
    return isfinite(BATTERY_ADC_VREF_V) && BATTERY_ADC_VREF_V>0 &&
        isfinite(BATTERY_DIVIDER_RATIO) && BATTERY_DIVIDER_RATIO>=1.0f;
}
static void poll_locked(uint32_t now) {
    if(have_sample && (uint32_t)(now-sample.timestamp_ms)>BATTERY_SAMPLE_TIMEOUT_MS) {
        sample.voltage_valid=false; sample.state=BATTERY_STALE;
        low_pending=clear_pending=false;
        /* A missing sensor must not silently clear a previous low warning. */
    }
}
void Battery_Init(void) {
    uint32_t key=Motor_Lock();
    sample=(battery_snapshot_t){.pack_voltage_v=NAN,.state=BATTERY_UNKNOWN};
    have_sample=low_pending=clear_pending=read_started=false;
    low_since=clear_since=last_read_ms=0;
    Motor_Unlock(key);
}
bool Battery_PublishVoltage(float voltage,uint32_t ms) {
    uint32_t key=Motor_Lock(),now=HAL_GetTick();
    if((int32_t)(now-ms)<0 || (have_sample && (int32_t)(ms-sample.timestamp_ms)<=0)) {
        Motor_Unlock(key); return false;
    }
    poll_locked(now);
    if(have_sample && (uint32_t)(ms-sample.timestamp_ms)>BATTERY_SAMPLE_TIMEOUT_MS)
        low_pending=clear_pending=false;
    have_sample=true; sample.timestamp_ms=ms;
    if(++sample.sequence==0) ++sample.sequence;
    sample.pack_voltage_v=voltage;
    sample.voltage_valid=isfinite(voltage) && voltage>0 && (uint32_t)(now-ms)<=BATTERY_SAMPLE_TIMEOUT_MS;
    if(!sample.voltage_valid) {
        sample.state=(uint32_t)(now-ms)>BATTERY_SAMPLE_TIMEOUT_MS ? BATTERY_STALE : BATTERY_INVALID;
        low_pending=clear_pending=false;
    } else if(!thresholds_ready()) {
        sample.state=BATTERY_UNCONFIGURED; low_pending=clear_pending=false;
    } else if(!sample.low_warning) {
        clear_pending=false;
        if(voltage<=BATTERY_LOW_WARNING_V) {
            if(!low_pending) { low_pending=true; low_since=ms; }
            if((uint32_t)(ms-low_since)>=BATTERY_LOW_CONFIRM_MS) {
                sample.low_warning=true; ++sample.warning_revision; low_pending=false;
            }
        } else low_pending=false;
        sample.state=sample.low_warning ? BATTERY_LOW : BATTERY_NORMAL;
    } else {
        low_pending=false;
        if(voltage>=BATTERY_WARNING_CLEAR_V) {
            if(!clear_pending) { clear_pending=true; clear_since=ms; }
            if((uint32_t)(ms-clear_since)>=BATTERY_CLEAR_CONFIRM_MS) {
                sample.low_warning=false; ++sample.warning_revision; clear_pending=false;
            }
        } else clear_pending=false;
        sample.state=sample.low_warning ? BATTERY_LOW : BATTERY_NORMAL;
    }
    bool valid=sample.voltage_valid;
    Motor_Unlock(key); return valid;
}
bool Battery_PublishRaw(uint16_t raw,uint32_t ms) {
    if(!divider_ready()) return false;
    float voltage=raw>0 && raw<BATTERY_ADC_FULL_SCALE ?
        (float)raw*BATTERY_ADC_VREF_V/BATTERY_ADC_FULL_SCALE*BATTERY_DIVIDER_RATIO : NAN;
    return Battery_PublishVoltage(voltage,ms);
}
__attribute__((weak)) bool Battery_ReadPackVoltage(float *voltage,uint32_t *ms) {
    if(!voltage || !ms || !divider_ready()) return false;
    adc_battery_sample_t adc;
    if(!ADC_DMA_BSP_GetBatterySample(&adc) || !adc.valid) return false;
    *voltage=(float)adc.raw*BATTERY_ADC_VREF_V/BATTERY_ADC_FULL_SCALE*BATTERY_DIVIDER_RATIO;
    *ms=adc.timestamp_ms; return true;
}
void Battery_Service(void) {
    uint32_t now=HAL_GetTick(),key=Motor_Lock();
    poll_locked(now);
    bool due=!read_started || (uint32_t)(now-last_read_ms)>=BATTERY_READ_PERIOD_MS;
    if(due) { read_started=true; last_read_ms=now; }
    Motor_Unlock(key);
    if(due) {
        float voltage; uint32_t ms;
        if(Battery_ReadPackVoltage(&voltage,&ms)) Battery_PublishVoltage(voltage,ms);
    }
}
void Battery_GetSnapshot(battery_snapshot_t *out) {
    if(!out) return;
    uint32_t key=Motor_Lock(); poll_locked(HAL_GetTick()); *out=sample; Motor_Unlock(key);
}
