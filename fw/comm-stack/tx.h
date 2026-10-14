/*
 * tx.h
 *
 *  Created on: 5 Apr 2026
 *      Author: romch
 */

#pragma once
#include <stdint.h>
typedef struct {
    uint32_t timestamp_ms, motor_id, epoch, flags, faults;
    float target_rad, reference_rad;
    int64_t raw_count;
    float encoder_rad, velocity_rad_s, pwm_percent;
    uint32_t adc_raw;
    float adc_voltage, current_a;
    uint32_t adc_state, adc_error, adc_age_ms;
    float kalman_rad, kalman_velocity_rad_s;
    uint32_t kalman_status;
} hw_diagnostic_t;
typedef struct {
    uint32_t timestamp_ms, role;
    uint64_t local_session, remote_session;
    uint32_t local_epoch, remote_epoch, local_stops, remote_stops;
    uint32_t heartbeat_age_ms, echo_age_ms, flags, rx_sequence, tx_sequence;
    uint32_t malformed, overflows, restarts, rejected, transport_errors;
} link_diagnostic_t;
#include <stdbool.h>

#ifndef INC_TX_H_
#define INC_TX_H_

#define TX_BUFFER_SIZE  128
#define DATA_REP_LENGTH  64

#ifdef __cplusplus
extern "C" {
#endif
bool SendMotorDiagnostic(const hw_diagnostic_t *diagnostic);
bool SendLinkDiagnostic(const link_diagnostic_t *diagnostic);
bool SendCommandReply(uint32_t request, uint32_t result, uint32_t epoch, uint32_t faults);
bool SendHelloMessage(void);

// void SendDataMessage(void);

bool SendClassificationData(uint8_t input);
#ifdef __cplusplus
}
#endif

#endif /* INC_TX_H_ */
