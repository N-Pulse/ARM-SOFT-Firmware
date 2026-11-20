#include "battery.h"
#include "battery_config.h"
#include "adc_dma_bsp.h"
#include "motor_lock.h"
#include "motor_safety.h"
#include <math.h>

static battery_snapshot_t sample;
static bool have_sample,low_pending,clear_pending,read_started;
static uint32_t low_since,clear_since,last_read_ms;
static float soc_percent;
static uint32_t soc_ms,soc_sequence,effective_warning_revision;
static bool soc_seen,soc_valid,soc_low,effective_low;
static battery_shutdown_state_t shutdown_state;
static uint32_t shutdown_started_ms;
static bool power_off_attempted,power_off_accepted;
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
    soc_percent=NAN; soc_ms=soc_sequence=effective_warning_revision=0;
    soc_seen=soc_valid=soc_low=effective_low=false;
    shutdown_state=BATTERY_SHUTDOWN_IDLE; shutdown_started_ms=0;
    power_off_attempted=power_off_accepted=false;
    Motor_Unlock(key);
}
bool Battery_PublishStateOfCharge(float percent,uint32_t ms) {
    uint32_t key=Motor_Lock(),now=HAL_GetTick();
    if((int32_t)(now-ms)<0 || (soc_seen && (int32_t)(ms-soc_ms)<=0)) {
        Motor_Unlock(key); return false;
    }
    soc_seen=true; soc_ms=ms;
    if(++soc_sequence==0) ++soc_sequence;
    soc_valid=isfinite(percent) && percent>=0 && percent<=100 &&
        (uint32_t)(now-ms)<=BATTERY_SOC_SAMPLE_TIMEOUT_MS;
    soc_percent=soc_valid ? percent : NAN;
    if(soc_valid) soc_low=percent<BATTERY_SOC_LOW_PERCENT;
    if(soc_valid && BATTERY_SOC_CRITICAL_PERCENT>0 &&
       percent<BATTERY_SOC_CRITICAL_PERCENT && shutdown_state==BATTERY_SHUTDOWN_IDLE) {
        /* Latch one countdown. Recovery, stale input and repeated packets never
         * extend/cancel the deadline or re-enable the motors after shutdown. */
        shutdown_state=BATTERY_SHUTDOWN_COUNTDOWN; shutdown_started_ms=now;
    }
    bool valid=soc_valid;
    Motor_Unlock(key); return valid;
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
                sample.low_warning=true; low_pending=false;
            }
        } else low_pending=false;
        sample.state=sample.low_warning ? BATTERY_LOW : BATTERY_NORMAL;
    } else {
        low_pending=false;
        if(voltage>=BATTERY_WARNING_CLEAR_V) {
            if(!clear_pending) { clear_pending=true; clear_since=ms; }
            if((uint32_t)(ms-clear_since)>=BATTERY_CLEAR_CONFIRM_MS) {
                sample.low_warning=false; clear_pending=false;
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
__attribute__((weak)) bool Battery_ReadStateOfCharge(float *percent,uint32_t *ms) {
    (void)percent; (void)ms; return false;
}
__attribute__((weak)) bool Battery_BoardRequestPowerOff(void) { return false; }
void Battery_Service(void) {
    uint32_t now=HAL_GetTick(),key=Motor_Lock();
    poll_locked(now);
    bool due=!read_started || (uint32_t)(now-last_read_ms)>=BATTERY_READ_PERIOD_MS;
    if(due) { read_started=true; last_read_ms=now; }
    Motor_Unlock(key);
    if(due) {
        float voltage; uint32_t ms;
        if(Battery_ReadPackVoltage(&voltage,&ms)) Battery_PublishVoltage(voltage,ms);
        if(Battery_ReadStateOfCharge(&voltage,&ms)) Battery_PublishStateOfCharge(voltage,ms);
    }
    key=Motor_Lock();
    now=HAL_GetTick(); /* A reader/preemption may have advanced the clock. */
    bool stop=shutdown_state==BATTERY_SHUTDOWN_COUNTDOWN &&
        (uint32_t)(now-shutdown_started_ms)>=BATTERY_SHUTDOWN_DELAY_MS;
    if(stop) {
        shutdown_state=BATTERY_SHUTDOWN_STOPPED;
        MotorSafety_RequestStopCauseLocked(MOTOR_STOP_BATTERY,MOTOR_COUNT);
    }
    bool stopped=shutdown_state==BATTERY_SHUTDOWN_STOPPED;
    Motor_Unlock(key);
    if(stopped) {
        Motor_ServiceStop();
        if(MotorSafety_StopComplete()) {
            key=Motor_Lock(); bool request=!power_off_attempted;
            power_off_attempted=true; Motor_Unlock(key);
            if(request) {
                bool accepted=Battery_BoardRequestPowerOff();
                key=Motor_Lock(); power_off_accepted=accepted; Motor_Unlock(key);
            }
        }
    }
}
void Battery_GetSnapshot(battery_snapshot_t *out) {
    if(!out) return;
    uint32_t key=Motor_Lock(),now=HAL_GetTick(); poll_locked(now); *out=sample;
    out->state_of_charge_percent=soc_percent;
    out->soc_timestamp_ms=soc_ms; out->soc_sequence=soc_sequence;
    out->soc_valid=soc_seen && soc_valid && (uint32_t)(now-soc_ms)<=BATTERY_SOC_SAMPLE_TIMEOUT_MS;
    if(soc_seen) {
        out->low_warning=soc_low || shutdown_state!=BATTERY_SHUTDOWN_IDLE;
        out->state=out->soc_valid ? (out->low_warning ? BATTERY_LOW : BATTERY_NORMAL) :
            ((uint32_t)(now-soc_ms)>BATTERY_SOC_SAMPLE_TIMEOUT_MS ? BATTERY_STALE : BATTERY_INVALID);
    }
    out->shutdown_state=shutdown_state; out->shutdown_started_ms=shutdown_started_ms;
    out->fast_warning=shutdown_state==BATTERY_SHUTDOWN_COUNTDOWN;
    out->power_off_accepted=power_off_accepted;
    if(effective_low!=out->low_warning) {
        effective_low=out->low_warning; ++effective_warning_revision;
    }
    out->warning_revision=effective_warning_revision;
    Motor_Unlock(key);
}
