#include "comm.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "timers.h"

#include "motor_safety.h"

#define COMMS_TASK_STACK_WORDS 256
#define COMMS_TASK_PRIORITY    (tskIDLE_PRIORITY + 4)
#define COMMS_QUEUE_LENGTH     8
#define COMMS_HEARTBEAT_MS     100U

#if defined(__GNUC__)
#define COMM_UNUSED __attribute__((unused))
#else
#define COMM_UNUSED
#endif

static QueueHandle_t s_intent_queue;
static TaskHandle_t  s_comm_task;
static TimerHandle_t s_heartbeat_timer;
static bool          s_heartbeat_started;

static void Comms_Task(void *argument);
static void Comms_HeartbeatTimeout(TimerHandle_t timer);

void Comms_Init(void)
{
    s_intent_queue = xQueueCreate(COMMS_QUEUE_LENGTH, sizeof(intent_t));
    configASSERT(s_intent_queue != NULL);

    BaseType_t status = xTaskCreate(Comms_Task,
                                    "comm",
                                    COMMS_TASK_STACK_WORDS,
                                    NULL,
                                    COMMS_TASK_PRIORITY,
                                    &s_comm_task);
    configASSERT(status == pdPASS);

    s_heartbeat_timer = xTimerCreate("bmiHB",
                                     pdMS_TO_TICKS(COMMS_HEARTBEAT_MS),
                                     pdFALSE,
                                     NULL,
                                     Comms_HeartbeatTimeout);
    configASSERT(s_heartbeat_timer != NULL);
    s_heartbeat_started = false;
}

bool Comms_GetNextIntent(intent_t *out_intent)
{
    if ((out_intent == NULL) || (s_intent_queue == NULL))
    {
        return false;
    }

    return (xQueueReceive(s_intent_queue, out_intent, 0) == pdPASS);
}

static void enqueue_intent(const intent_t *intent) COMM_UNUSED
{
    if ((intent == NULL) || (s_intent_queue == NULL))
    {
        return;
    }

    if (xQueueSendToBack(s_intent_queue, intent, 0) == pdPASS)
    {
        if (!s_heartbeat_started)
        {
            xTimerStart(s_heartbeat_timer, 0);
            s_heartbeat_started = true;
        }
        else
        {
            xTimerReset(s_heartbeat_timer, 0);
        }
    }
}

static void Comms_Task(void *argument)
{
    (void)argument;

    while (1)
    {
        /*
         * TODO: Replace this placeholder with BMI UART/CAN frame parsing.
         * For now the task idles and waits for real hardware to deliver intents.
         */
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static void Comms_HeartbeatTimeout(TimerHandle_t timer)
{
    (void)timer;
    s_heartbeat_started = false;
    MotorSafety_RequestStop();
}
