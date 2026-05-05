#include "motor_backend.h"

#ifdef MOTOR_BACKEND_SIM

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "timers.h"

#include "comm.h"
#include "main.h"
#include "motor_safety.h"
#include "stm32g4xx_nucleo.h"

#define MAX_SIM_MOTORS         8U
#define SIM_FRAME_SYNC         0xAAU
#define SIM_FRAME_VERSION      0x01U
#define SIM_MSG_SET_TARGETS    0x01U
#define SIM_MSG_ALL_STOP       0x02U
#define SIM_MSG_JOINT_FEEDBACK 0x10U
#define SIM_MSG_FSR_FEEDBACK   0x11U
#define SIM_MSG_HEARTBEAT_ACK  0x12U
#define SIM_MSG_SELECT_MODE    0x20U
#define SIM_MSG_INTENT_ACK     0x21U
#define SIM_MSG_MOTOR_PREVIEW  0x22U
#define SIM_MAX_PAYLOAD        96U
#define SIM_POSITION_RANGE_RAD 3.1415926f

typedef enum
{
    RX_WAIT_SYNC,
    RX_HEADER,
    RX_PAYLOAD,
    RX_CRC
} rx_state_t;

typedef struct
{
    motor_id_t id;
    float      position;
    uint8_t    speed_percent;
} motor_cmd_t;

typedef struct
{
    rx_state_t state;
    uint8_t    version;
    uint8_t    msg_id;
    uint8_t    length;
    uint8_t    index;
    uint8_t    buffer[SIM_MAX_PAYLOAD];
} rx_parser_t;

static motor_cmd_t          s_pending_cmds[MAX_SIM_MOTORS];
static uint8_t              s_cmd_count;
static uint16_t             s_heartbeat_counter;
static volatile bool        s_ack_pending;
static volatile uint8_t     s_ack_intent_id;
static motor_cmd_t          s_preview_cmds[MAX_SIM_MOTORS];
static uint8_t              s_preview_count;
static UART_HandleTypeDef  *s_uart_handle;
static volatile uint8_t     s_rx_byte;
static volatile bool        s_rx_armed;
static rx_parser_t          s_rx_parser;
static uint16_t             s_last_ack;
static TaskHandle_t         s_tx_task;
static SemaphoreHandle_t    s_cmd_lock;
static TimerHandle_t        s_ros_watchdog;
static bool                 s_ros_timer_started;

extern UART_HandleTypeDef hcom_uart[COMn];

static void   enqueue_cmd(motor_id_t id, float position, uint8_t speed_percent);
static void   ensure_uart_ready(void);
static void   start_rx_interrupt(void);
static void   handle_rx_byte(uint8_t byte);
static void   reset_rx_parser(void);
static void   process_frame(uint8_t msg_id, const uint8_t *payload, uint8_t length);
static void   process_joint_feedback(const uint8_t *payload, uint8_t length);
static void   process_fsr_feedback(const uint8_t *payload, uint8_t length);
static void   process_heartbeat_ack(const uint8_t *payload, uint8_t length);
static void   send_pending_commands(void);
static void   send_all_stop(void);
static bool   build_set_targets_payload(uint8_t *payload, uint8_t *payload_len);
static int16_t float_to_q15(float value);
static float  q15_to_float(int16_t raw);
static uint8_t crc8_update(uint8_t crc, uint8_t data);
static uint8_t compute_crc(uint8_t version, uint8_t msg_id, uint8_t length, const uint8_t *payload);
static void   transmit_frame(uint8_t msg_id, const uint8_t *payload, uint8_t length);
static void   SimTxTask(void *argument);
static void   RosWatchdogTimeout(TimerHandle_t timer);

/* Temporary step beacons for MotorBackend_Init diagnosis — remove once boot confirmed */
#define MINIT_BEACON(id, crc) \
    do { \
        uint8_t _mb[] = {0xAAU, 0x01U, (id), 0x00U, (crc)}; \
        HAL_UART_Transmit(&hcom_uart[COM1], _mb, sizeof(_mb), 50U); \
    } while(0)

void MotorBackend_Init(void)
{
    MINIT_BEACON(0xE0U, 0xD7U); /* E0: MotorBackend_Init entered */
    memset(s_pending_cmds, 0, sizeof(s_pending_cmds));
    s_cmd_count          = 0U;
    s_heartbeat_counter  = 0U;
    s_uart_handle        = NULL;
    s_rx_armed           = false;
    s_last_ack           = 0U;
    s_tx_task            = NULL;
    s_cmd_lock           = xSemaphoreCreateMutex();
    configASSERT(s_cmd_lock != NULL);
    MINIT_BEACON(0xE1U, 0x9BU); /* E1: mutex OK */
    s_ros_watchdog      = xTimerCreate("rosHB",
                                       pdMS_TO_TICKS(150),
                                       pdFALSE,
                                       NULL,
                                       RosWatchdogTimeout);
    configASSERT(s_ros_watchdog != NULL);
    MINIT_BEACON(0xE2U, 0x4FU); /* E2: timer OK */
    s_ros_timer_started = false;
    reset_rx_parser();

    BaseType_t status = xTaskCreate(SimTxTask,
                                    "simtx",
                                    512,
                                    NULL,
                                    tskIDLE_PRIORITY + 3,
                                    &s_tx_task);
    configASSERT(status == pdPASS);
    MINIT_BEACON(0xE3U, 0x03U); /* E3: SimTxTask created OK */
}

void MotorBackend_SetTarget(motor_id_t id, float position, uint8_t speed_percent)
{
    enqueue_cmd(id, position, speed_percent);
}

void MotorBackend_SendPreview(motor_id_t id, float position, uint8_t speed_percent)
{
    if (s_preview_count >= MAX_SIM_MOTORS)
    {
        return;
    }
    s_preview_cmds[s_preview_count].id            = id;
    s_preview_cmds[s_preview_count].position      = position;
    s_preview_cmds[s_preview_count].speed_percent = speed_percent;
    s_preview_count++;
}

void MotorBackend_Flush(void)
{
    if (s_tx_task != NULL)
    {
        xTaskNotifyGive(s_tx_task);
    }
}

void MotorBackend_StopAll(void)
{
    if ((s_cmd_lock != NULL) && (xSemaphoreTake(s_cmd_lock, pdMS_TO_TICKS(2)) == pdPASS))
    {
        s_cmd_count = 0U;
        xSemaphoreGive(s_cmd_lock);
    }
    send_all_stop();
}

void MotorBackend_OnFeedback(const motor_feedback_t *feedback)
{
    MotorSafety_OnFeedback(feedback);
}

static void enqueue_cmd(motor_id_t id, float position, uint8_t speed_percent)
{
    if ((s_cmd_lock == NULL) || (xSemaphoreTake(s_cmd_lock, portMAX_DELAY) != pdPASS))
    {
        return;
    }

    for (uint8_t i = 0U; i < s_cmd_count; ++i)
    {
        if (s_pending_cmds[i].id == id)
        {
            s_pending_cmds[i].position      = position;
            s_pending_cmds[i].speed_percent = speed_percent;
            xSemaphoreGive(s_cmd_lock);
            return;
        }
    }

    if (s_cmd_count < MAX_SIM_MOTORS)
    {
        s_pending_cmds[s_cmd_count].id            = id;
        s_pending_cmds[s_cmd_count].position      = position;
        s_pending_cmds[s_cmd_count].speed_percent = speed_percent;
        s_cmd_count++;
    }

    xSemaphoreGive(s_cmd_lock);
}

static void ensure_uart_ready(void)
{
    if ((s_uart_handle == NULL) && (hcom_uart[COM1].Instance != NULL))
    {
        s_uart_handle = &hcom_uart[COM1];
        HAL_NVIC_SetPriority(LPUART1_IRQn, 6, 0);
        HAL_NVIC_EnableIRQ(LPUART1_IRQn);
        /* Re-enable RX (was disabled in main() during boot to keep TX clean). */
        s_uart_handle->Instance->CR1 |= USART_CR1_RE;
        __HAL_UART_CLEAR_FLAG(s_uart_handle, UART_CLEAR_OREF | UART_CLEAR_NEF | UART_CLEAR_FEF);
        s_uart_handle->ErrorCode = HAL_UART_ERROR_NONE;
        start_rx_interrupt();
        transmit_frame(0xFE, NULL, 0U);
    }
}

static void start_rx_interrupt(void)
{
    if ((s_uart_handle != NULL) && !s_rx_armed)
    {
        if (HAL_UART_Receive_IT(s_uart_handle, (uint8_t *)&s_rx_byte, 1U) == HAL_OK)
        {
            s_rx_armed = true;
        }
    }
}

/* External entry point for LPUART1_IRQHandler (declared extern in stm32g4xx_it.c) */
void handle_rx_byte_external(uint8_t b);

static void handle_rx_byte(uint8_t byte);

void handle_rx_byte_external(uint8_t b) { handle_rx_byte(b); }

static void handle_rx_byte(uint8_t byte)
{
    switch (s_rx_parser.state)
    {
    case RX_WAIT_SYNC:
        if (byte == SIM_FRAME_SYNC)
        {
            s_rx_parser.state = RX_HEADER;
            s_rx_parser.index = 0U;
        }
        break;

    case RX_HEADER:
        if (s_rx_parser.index == 0U)
        {
            s_rx_parser.version = byte;
        }
        else if (s_rx_parser.index == 1U)
        {
            s_rx_parser.msg_id = byte;
        }
        else if (s_rx_parser.index == 2U)
        {
            s_rx_parser.length = byte;
            if (s_rx_parser.length > SIM_MAX_PAYLOAD)
            {
                reset_rx_parser();
                break;
            }
            s_rx_parser.state = (s_rx_parser.length == 0U) ? RX_CRC : RX_PAYLOAD;
            s_rx_parser.index = 0U;
            break;
        }

        s_rx_parser.index++;
        break;

    case RX_PAYLOAD:
        s_rx_parser.buffer[s_rx_parser.index++] = byte;
        if (s_rx_parser.index >= s_rx_parser.length)
        {
            s_rx_parser.state = RX_CRC;
            s_rx_parser.index = 0U;
        }
        break;

    case RX_CRC:
    {
        uint8_t computed_crc = compute_crc(s_rx_parser.version,
                                           s_rx_parser.msg_id,
                                           s_rx_parser.length,
                                           s_rx_parser.buffer);
        if (byte == computed_crc)
        {
            process_frame(s_rx_parser.msg_id, s_rx_parser.buffer, s_rx_parser.length);
        }
        reset_rx_parser();
        break;
    }

    default:
        reset_rx_parser();
        break;
    }
}

static void reset_rx_parser(void)
{
    s_rx_parser.state  = RX_WAIT_SYNC;
    s_rx_parser.index  = 0U;
    s_rx_parser.length = 0U;
}

/* Send INTENT_ACK frame directly from ISR context — no FreeRTOS, no HAL.
 * Frame: [0xAA][0x01][0x21][0x01][intent_id][crc8] */
static void isr_send_intent_ack(uint8_t intent_id)
{
    USART_TypeDef *u = LPUART1;
    uint8_t crc = compute_crc(0x01U, 0x21U, 0x01U, &intent_id);
    uint8_t bytes[6] = {0xAAU, 0x01U, 0x21U, 0x01U, intent_id, crc};
    for (int i = 0; i < 6; ++i) {
        while ((u->ISR & (1U << 7)) == 0U) { __asm__ volatile ("nop"); }
        u->TDR = bytes[i];
    }
    while ((u->ISR & (1U << 6)) == 0U) { __asm__ volatile ("nop"); }
}

static void process_select_mode(const uint8_t *payload, uint8_t length)
{
    if (length < 2U || payload[0U] != 0x08U)
    {
        return;
    }
    s_ack_intent_id = payload[1U];
    s_ack_pending   = true;
    if (s_tx_task != NULL)
    {
        BaseType_t higher_woken = pdFALSE;
        vTaskNotifyGiveFromISR(s_tx_task, &higher_woken);
        portYIELD_FROM_ISR(higher_woken);
    }
}

static void process_frame(uint8_t msg_id, const uint8_t *payload, uint8_t length)
{
    switch (msg_id)
    {
    case SIM_MSG_JOINT_FEEDBACK:
        process_joint_feedback(payload, length);
        break;
    case SIM_MSG_FSR_FEEDBACK:
        process_fsr_feedback(payload, length);
        break;
    case SIM_MSG_HEARTBEAT_ACK:
        process_heartbeat_ack(payload, length);
        break;
    case SIM_MSG_SELECT_MODE:
        process_select_mode(payload, length);
        break;
    default:
        break;
    }
}

static void process_joint_feedback(const uint8_t *payload, uint8_t length)
{
    if (length < 1U)
    {
        return;
    }

    uint8_t joint_count = payload[0];
    uint8_t expected    = 1U + (joint_count * 5U);
    if (length < expected)
    {
        return;
    }

    uint8_t offset = 1U;
    for (uint8_t i = 0U; i < joint_count; ++i)
    {
        motor_feedback_t feedback = {0};
        feedback.id       = (motor_id_t)payload[offset++];
        int16_t pos_raw   = (int16_t)((payload[offset + 1U] << 8U) | payload[offset]);
        offset           += 2U;
        int16_t vel_raw   = (int16_t)((payload[offset + 1U] << 8U) | payload[offset]);
        offset           += 2U;
        feedback.position = q15_to_float(pos_raw);
        feedback.velocity = q15_to_float(vel_raw);
        feedback.force_mN = 0U;
        MotorBackend_OnFeedback(&feedback);
    }
}

static void process_fsr_feedback(const uint8_t *payload, uint8_t length)
{
    if (length < 1U)
    {
        return;
    }

    uint8_t count    = payload[0];
    uint8_t expected = 1U + (count * 3U);
    if (length < expected)
    {
        return;
    }

    uint8_t offset = 1U;
    for (uint8_t i = 0U; i < count; ++i)
    {
        motor_feedback_t feedback = {0};
        feedback.id       = (motor_id_t)payload[offset++];
        feedback.position = 0.0f;
        feedback.velocity = 0.0f;
        feedback.force_mN = (uint16_t)((payload[offset + 1U] << 8U) | payload[offset]);
        offset           += 2U;
        MotorBackend_OnFeedback(&feedback);
    }
}

static void process_heartbeat_ack(const uint8_t *payload, uint8_t length)
{
    if (length < 2U)
    {
        return;
    }
    s_last_ack = (uint16_t)((payload[1U] << 8U) | payload[0U]);
    /* Timer ops disabled — they fault when IRQ fires pre-scheduler. */
}

static void send_pending_commands(void)
{
    if ((s_cmd_lock == NULL) || (xSemaphoreTake(s_cmd_lock, pdMS_TO_TICKS(2)) != pdPASS))
    {
        return;
    }

    if (s_cmd_count == 0U)
    {
        xSemaphoreGive(s_cmd_lock);
        return;
    }

    ensure_uart_ready();
    if (s_uart_handle == NULL)
    {
        xSemaphoreGive(s_cmd_lock);
        return;
    }

    uint8_t payload[SIM_MAX_PAYLOAD] = {0};
    uint8_t payload_len              = 0U;
    if (build_set_targets_payload(payload, &payload_len))
    {
        transmit_frame(SIM_MSG_SET_TARGETS, payload, payload_len);
    }

    s_cmd_count = 0U;
    xSemaphoreGive(s_cmd_lock);
}

static void send_all_stop(void)
{
    ensure_uart_ready();
    if (s_uart_handle == NULL)
    {
        return;
    }

    transmit_frame(SIM_MSG_ALL_STOP, NULL, 0U);
}

static bool build_set_targets_payload(uint8_t *payload, uint8_t *payload_len)
{
    if ((payload == NULL) || (payload_len == NULL))
    {
        return false;
    }

    uint8_t offset = 0U;
    payload[offset++] = (uint8_t)(s_heartbeat_counter & 0xFFU);
    payload[offset++] = (uint8_t)(s_heartbeat_counter >> 8U);
    payload[offset++] = s_cmd_count;

    for (uint8_t i = 0U; i < s_cmd_count; ++i)
    {
        payload[offset++] = (uint8_t)s_pending_cmds[i].id;

        int16_t position_q15 = float_to_q15(s_pending_cmds[i].position);
        payload[offset++]    = (uint8_t)(position_q15 & 0xFFU);
        payload[offset++]    = (uint8_t)((position_q15 >> 8U) & 0xFFU);

        uint8_t speed = (s_pending_cmds[i].speed_percent > 100U) ? 100U : s_pending_cmds[i].speed_percent;
        payload[offset++] = speed;
    }

    *payload_len       = offset;
    s_heartbeat_counter++;
    return true;
}

static int16_t float_to_q15(float value)
{
    float normalized = value / SIM_POSITION_RANGE_RAD;
    if (normalized > 1.0f)
    {
        normalized = 1.0f;
    }
    else if (normalized < -1.0f)
    {
        normalized = -1.0f;
    }
    return (int16_t)lrintf(normalized * 32767.0f);
}

static float q15_to_float(int16_t raw)
{
    return ((float)raw / 32767.0f) * SIM_POSITION_RANGE_RAD;
}

static uint8_t crc8_update(uint8_t crc, uint8_t data)
{
    crc ^= data;
    for (uint8_t i = 0U; i < 8U; ++i)
    {
        if ((crc & 0x80U) != 0U)
        {
            crc = (uint8_t)((crc << 1U) ^ 0x1DU);
        }
        else
        {
            crc <<= 1U;
        }
    }
    return crc;
}

static uint8_t compute_crc(uint8_t version, uint8_t msg_id, uint8_t length, const uint8_t *payload)
{
    uint8_t crc = 0xFFU;
    crc         = crc8_update(crc, version);
    crc         = crc8_update(crc, msg_id);
    crc         = crc8_update(crc, length);
    for (uint8_t i = 0U; i < length; ++i)
    {
        crc = crc8_update(crc, payload[i]);
    }
    return crc;
}

static void transmit_frame(uint8_t msg_id, const uint8_t *payload, uint8_t length)
{
    uint8_t frame[SIM_MAX_PAYLOAD + 5U];
    uint8_t index = 0U;

    frame[index++] = SIM_FRAME_SYNC;
    frame[index++] = SIM_FRAME_VERSION;
    frame[index++] = msg_id;
    frame[index++] = length;

    for (uint8_t i = 0U; i < length; ++i)
    {
        frame[index++] = payload[i];
    }

    uint8_t crc = compute_crc(SIM_FRAME_VERSION, msg_id, length, payload);
    frame[index++] = crc;

    if (s_uart_handle != NULL)
    {
        HAL_UART_Transmit(s_uart_handle, frame, index, 10U);
    }
}

static void SimTxTask(void *argument)
{
    (void)argument;
    const TickType_t period = pdMS_TO_TICKS(10);

    /* Task-start beacon: FreeRTOS scheduler running, SimTxTask scheduled.
     * Frame: [0xAA][0x01][0xFA][0x00][CRC=0xA1] */
    {
        uint8_t task_beacon[] = {0xAAU, 0x01U, 0xFAU, 0x00U, 0xA1U};
        HAL_UART_Transmit(&hcom_uart[COM1], task_beacon, sizeof(task_beacon), 50U);
    }

    while (1)
    {
        ulTaskNotifyTake(pdTRUE, period);

        ensure_uart_ready();

        if (s_ack_pending)
        {
            s_ack_pending = false;
            // Safe to call non-ISR FreeRTOS APIs here (task context).
            enqueue_intent_from_proto((intent_id_t)s_ack_intent_id);
            ensure_uart_ready();
            if (s_uart_handle != NULL)
            {
                uint8_t ack_payload[1] = { s_ack_intent_id };
                transmit_frame(SIM_MSG_INTENT_ACK, ack_payload, 1U);
            }
        }

        if (s_preview_count > 0U)
        {
            ensure_uart_ready();
            if (s_uart_handle != NULL)
            {
                uint8_t payload[SIM_MAX_PAYLOAD] = {0};
                uint8_t offset = 0U;
                payload[offset++] = s_preview_count;
                for (uint8_t i = 0U; i < s_preview_count; ++i)
                {
                    payload[offset++] = (uint8_t)s_preview_cmds[i].id;
                    int16_t q15 = float_to_q15(s_preview_cmds[i].position);
                    payload[offset++] = (uint8_t)(q15 & 0xFFU);
                    payload[offset++] = (uint8_t)((q15 >> 8U) & 0xFFU);
                    payload[offset++] = s_preview_cmds[i].speed_percent;
                }
                transmit_frame(SIM_MSG_MOTOR_PREVIEW, payload, offset);
            }
            s_preview_count = 0U;
        }

        send_pending_commands();
    }
}

static void RosWatchdogTimeout(TimerHandle_t timer)
{
    (void)timer;
    s_ros_timer_started = false;
    MotorSafety_RequestStop();
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if ((s_uart_handle != NULL) && (huart == s_uart_handle))
    {
        s_rx_armed = false;
        handle_rx_byte(s_rx_byte);
        start_rx_interrupt();
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if ((s_uart_handle != NULL) && (huart == s_uart_handle))
    {
        s_rx_armed = false;
        start_rx_interrupt();
    }
}

#endif /* MOTOR_BACKEND_SIM */

