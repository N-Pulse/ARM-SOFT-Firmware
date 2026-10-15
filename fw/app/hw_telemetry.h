#ifndef HW_TELEMETRY_H
#define HW_TELEMETRY_H
#include <stdint.h>
#ifndef N_PULSE_TELEMETRY_PERIOD_MS
#define N_PULSE_TELEMETRY_PERIOD_MS 200U
#endif
#if N_PULSE_TELEMETRY_PERIOD_MS < 100U
#error Hardware telemetry must not exceed 10 Hz
#endif
void HWTelemetry_Select(uint32_t motor);
void HWTelemetry_Service(void);
void HWTelemetry_Reply(uint32_t request, uint32_t result);
#endif
