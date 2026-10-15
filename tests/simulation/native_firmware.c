/* PC hardware/clock/scheduler boundary ONLY. All control, safety, RX and TX code
 * is linked from production sources. No serial device or physical register. */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#else
#define _WIN32_WINNT 0x0601
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdatomic.h>
#include "main.h"
#include "comm.h"
#include "motor_control.h"
#include "motor_safety.h"
#include "motor_guard.h"
#include "motor_lock.h"
#include "sim_session.h"
#include "uart4_transport.h"
#include "board_uart_compat.h"
#include "queue.h"
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#else
#include <time.h>
#include <poll.h>
#include <unistd.h>
#include <pthread.h>
#endif
static bool realtime;
/* The PC stdout pipe can block if its reader freezes. Isolate that wait from
 * the C scheduler, with a bounded SPSC frame queue (no unbounded buffering). */
typedef struct { unsigned length; uint8_t bytes[256]; } ipc_frame_t;
static ipc_frame_t ipc_frames[8];
static atomic_uint ipc_head,ipc_tail;
static atomic_bool ipc_error;
static bool ipc_offer(const uint8_t *bytes,unsigned length) {
    unsigned head=atomic_load(&ipc_head),next=(head+1U)%8U;
    if(atomic_load(&ipc_error) || next==atomic_load(&ipc_tail)) return false;
    ipc_frames[head].length=length; memcpy(ipc_frames[head].bytes,bytes,length);
    atomic_store(&ipc_head,next); return true;
}
#ifdef _WIN32
static DWORD WINAPI ipc_writer(LPVOID unused)
#else
static void *ipc_writer(void *unused)
#endif
{
    (void)unused;
    for(;;) {
        unsigned tail=atomic_load(&ipc_tail);
        if(tail!=atomic_load(&ipc_head)) {
            ipc_frame_t *frame=&ipc_frames[tail];
            if(fwrite(frame->bytes,1,frame->length,stdout)!=frame->length || fflush(stdout)!=0) {
                atomic_store(&ipc_error,true); break;
            }
            atomic_store(&ipc_tail,(tail+1U)%8U);
        } else {
#ifdef _WIN32
            Sleep(1);
#else
            struct timespec delay={.tv_sec=0,.tv_nsec=1000000}; nanosleep(&delay,NULL);
#endif
        }
    }
    return 0;
}
UART_HandleTypeDef huart4;
static uint32_t clock_ms,rtos_ms,primask,ipsr,current_task=2U,tx_blocked_until;
static uint8_t *rx_destination,wire[256];
static uint16_t wire_size;
static unsigned tx_mode,physical_calls,task_count,queue_count,queue_item;
static bool pending_stop,tx_started,tx_blocked;
static uint8_t queue_store[8][256];
uint32_t __get_PRIMASK(void) { return primask; }
uint32_t __get_IPSR(void) { return ipsr; }
void __disable_irq(void) { primask=1U; }
void __set_PRIMASK(uint32_t key) {
    primask=key;
    if(!key && pending_stop) { pending_stop=false; Motor_StopAll(); }
}
uint32_t HAL_GetTick(void) { return clock_ms; }
void __DSB(void) {}
void Error_Handler(void) { fprintf(stderr,"UNEXPECTED Error_Handler\n"); abort(); }
/* If a future SIM change accidentally links a physical call, it fails here. */
void HAL_Delay(uint32_t n) { (void)n; ++physical_calls; abort(); }
void HAL_GPIO_WritePin(GPIO_TypeDef *p,uint32_t b,GPIO_PinState s) { (void)p;(void)b;(void)s; ++physical_calls; abort(); }
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *p,uint32_t b) { (void)p;(void)b; ++physical_calls; abort(); }
#define TRAP_START(name,type) HAL_StatusTypeDef name(type *h,uint32_t c) { (void)h;(void)c;++physical_calls;abort();return HAL_ERROR; }
TRAP_START(HAL_HRTIM_WaveformCountStart,HRTIM_HandleTypeDef)
TRAP_START(HAL_HRTIM_WaveformOutputStart,HRTIM_HandleTypeDef)
TRAP_START(HAL_TIM_PWM_Start,TIM_HandleTypeDef)
TRAP_START(HAL_TIM_Encoder_Start,TIM_HandleTypeDef)
TRAP_START(HAL_LPTIM_Encoder_Start,LPTIM_HandleTypeDef)
void HAL_NVIC_EnableIRQ(int irq) { assert(!primask && irq==UART4_IRQn); }
void HAL_NVIC_DisableIRQ(int irq) { assert(!primask && irq==UART4_IRQn); }
void HAL_NVIC_ClearPendingIRQ(int irq) { assert(!primask && irq==UART4_IRQn); }
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *h,uint8_t *p,uint16_t n) {
    assert(!primask && h==&huart4 && n==1U); rx_destination=p; return tx_mode==5U ? HAL_ERROR : HAL_OK;
}
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *h) { assert(!primask && h==&huart4); return HAL_OK; }
HAL_StatusTypeDef HAL_UART_AbortTransmit(UART_HandleTypeDef *h) {
    assert(!primask && h==&huart4); tx_started=false; h->gState=HAL_UART_STATE_READY;
    return tx_mode==4U ? HAL_ERROR : HAL_OK;
}
HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *h,uint8_t *p,uint16_t n) {
    assert(primask && !ipsr && h==&huart4 && n<=sizeof(wire));
    memcpy(wire,p,n); wire_size=n; h->gState=HAL_UART_STATE_BUSY_TX; h->TxXferCount=n; tx_started=true;
    if(tx_mode==3U) pending_stop=true; /* ISR becomes pending at real commitment. */
    return tx_mode==1U ? HAL_ERROR : HAL_OK;
}
BaseType_t xTaskCreate(void (*fn)(void *),const char *name,unsigned stack,void *arg,unsigned priority,TaskHandle_t *handle) {
    (void)fn;(void)name;(void)stack;(void)arg; assert(priority<3U); ++task_count;
    if(handle) *handle=(void *)(uintptr_t)task_count;
    return pdPASS;
}
TickType_t xTaskGetTickCount(void) { return rtos_ms; }
TaskHandle_t xTaskGetCurrentTaskHandle(void) { return (void *)(uintptr_t)current_task; }
uint32_t ulTaskNotifyTake(BaseType_t clear,TickType_t wait) {
    (void)clear; assert(!primask && !ipsr);
    if(current_task==1U && wait && tx_started) {
        if(tx_mode==2U || tx_mode==4U) { rtos_ms+=wait; return 0U; }
        if(realtime) {
            if(!ipc_offer(wire,wire_size)) { rtos_ms+=wait; return 0U; }
        }
        else { printf("F "); for(unsigned i=0;i<wire_size;++i) printf("%02x",wire[i]); putchar('\n'); }
        tx_started=false; huart4.gState=HAL_UART_STATE_READY; huart4.TxXferCount=0U;
        ipsr=1U; HAL_UART_TxCpltCallback(&huart4); ipsr=0U;
        return 1U;
    }
    return 0U;
}
void vTaskNotifyGiveFromISR(TaskHandle_t task,BaseType_t *woken) { assert(task && ipsr && !primask); *woken=pdFALSE; }
void xTaskNotifyGive(TaskHandle_t task) { assert(task && !primask && !ipsr); }
void vTaskDelay(TickType_t ms) { assert(!primask && ms); tx_blocked_until=clock_ms+ms+((tx_mode==2U || tx_mode==4U)?25U:0U); tx_blocked=true; }
QueueHandle_t xQueueCreate(unsigned n,unsigned size) { assert(n==8U && size<=256U); queue_item=size; queue_count=0U; return (void *)1; }
BaseType_t xQueueSend(QueueHandle_t q,const void *item,TickType_t wait) {
    assert(q && !primask && !wait); if(queue_count==8U) return pdFALSE;
    memcpy(queue_store[queue_count++],item,queue_item); return pdPASS;
}
BaseType_t xQueueReceive(QueueHandle_t q,void *item,TickType_t wait) { (void)q;(void)item;(void)wait; abort(); }
static void rx(const char *hex) {
    assert(rx_destination);
    for(size_t i=0;i+1U<strlen(hex);i+=2U) {
        unsigned value; assert(sscanf(hex+i,"%2x",&value)==1); *rx_destination=(uint8_t)value;
        ipsr=1U; HAL_UART_RxCpltCallback(&huart4); ipsr=0U;
    }
}
static void snapshot(void) {
    sim_diagnostics_t d; SimSession_GetDiagnostics(&d);
    printf("S %"PRIu32" %"PRIu32" %"PRIu64" %"PRIu32" %u %u %u\n",clock_ms,MotorSafety_GetStopCauses(),d.session,d.run,d.live,d.configured,d.armed);
    for(unsigned i=0;i<MOTOR_COUNT;++i) {
        motor_state_t m; assert(Motor_GetSnapshot((motor_id_t)i,&m));
        printf("M %u %.9g %.9g %.9g %.9g %u %u %"PRIu32"\n",i,m.target_position,m.reference_position,m.measured_position,m.output_pwm,m.enabled,m.feedback_valid,clock_ms-m.feedback_ms);
    }
    uart4_diagnostics_t u; UART4Transport_GetDiagnostics(&u);
    printf("D %"PRIu32" %"PRIu32" %"PRIu32" %"PRIu32" %"PRIu32" %"PRIu32" %"PRIu32" %"PRIu32" %u %u\n",d.accepted,d.rejected,d.old,d.malformed,u.tx_replaced,u.tx_expired,u.tx_timeouts,u.rx_overflow,physical_calls,queue_count);
    puts("E"); fflush(stdout);
}
static uint64_t monotonic_ms(void) {
#ifdef _WIN32
    LARGE_INTEGER counter,frequency;
    assert(QueryPerformanceCounter(&counter) && QueryPerformanceFrequency(&frequency));
    return (uint64_t)(counter.QuadPart/frequency.QuadPart)*1000U +
        (uint64_t)(counter.QuadPart%frequency.QuadPart)*1000U/(uint64_t)frequency.QuadPart;
#else
    struct timespec now; clock_gettime(CLOCK_MONOTONIC,&now);
    return (uint64_t)now.tv_sec*1000U+(uint64_t)now.tv_nsec/1000000U;
#endif
}
static int read_pipe(uint8_t *bytes,unsigned capacity) {
#ifdef _WIN32
    DWORD available=0;
    if(!PeekNamedPipe(GetStdHandle(STD_INPUT_HANDLE),NULL,0,NULL,&available,NULL)) return -1;
    if(!available) return 0;
    return _read(0,bytes,available<capacity?available:capacity);
#else
    struct pollfd fd={.fd=STDIN_FILENO,.events=POLLIN};
    if(poll(&fd,1,0)<=0) return 0;
    int n=(int)read(STDIN_FILENO,bytes,capacity); return n==0?-1:n;
#endif
}
static void realtime_loop(void) {
    uint64_t origin=monotonic_ms(); uint32_t last_control=0,last_safety=0;
#ifdef _WIN32
    _setmode(0,_O_BINARY); _setmode(1,_O_BINARY);
    HANDLE writer=CreateThread(NULL,0,ipc_writer,NULL,0,NULL); assert(writer); CloseHandle(writer);
#else
    pthread_t writer; assert(pthread_create(&writer,NULL,ipc_writer,NULL)==0); pthread_detach(writer);
#endif
    for(;;) {
        clock_ms=(uint32_t)(monotonic_ms()-origin); rtos_ms=clock_ms;
        uint8_t bytes[COMMS_RX_CAPACITY-1U]; bool eof=false;
        current_task=2U;
        /* IPC writes whole frames without UART pacing. Drain bounded chunks
         * between parser calls so a burst cannot overflow the ISR ring or
         * split a buffered frame across the scheduler's interbyte deadline. */
        for(unsigned batch=0;batch<4;++batch) {
            int n=read_pipe(bytes,sizeof(bytes));
            if(n<0) { eof=true; break; }
            for(int i=0;i<n;++i) {
                *rx_destination=bytes[i]; ipsr=1U; HAL_UART_RxCpltCallback(&huart4); ipsr=0U;
            }
            Comms_ProcessRxOnce(0U);
            if(!n) break;
        }
        if(eof) break;
        if((uint32_t)(clock_ms-last_control)>=10U) { last_control=clock_ms; Motor_Update(); }
        if((uint32_t)(clock_ms-last_safety)>=20U) {
            last_safety=clock_ms; MotorGuard_Poll(); MotorSafety_Poll(); Motor_ServiceStop();
        }
        SimSession_Service(); current_task=1U;
        if(!tx_blocked || (int32_t)(clock_ms-tx_blocked_until)>=0) {
            tx_blocked=false;
            for(unsigned i=0;i<3;++i) if(!UART4Transport_TxOnce(0U)) break;
        }
        current_task=2U;
#ifdef _WIN32
        Sleep(1);
#else
        struct timespec pause={.tv_sec=0,.tv_nsec=1000000}; nanosleep(&pause,NULL);
#endif
    }
    Motor_StopAll(); Motor_ServiceStop();
}
int main(int argc,char **argv) {
    realtime=argc==2 && !strcmp(argv[1],"--realtime");
    if(argc>1 && !realtime) return 2;
    setvbuf(stdout,NULL,_IOLBF,0); Motor_InitAll(); Comms_Init(); Comms_TaskCreate();
    assert(UART4Transport_StartRx((void *)2));
    if(realtime) { realtime_loop(); _Exit(0); }
    char line[8192],op[32],arg[8000];
    while(fgets(line,sizeof(line),stdin)) {
        arg[0]='\0'; assert(sscanf(line,"%31s %7999[^\n]",op,arg)>=1);
        if(!strcmp(op,"RX")) rx(arg);
        else if(!strcmp(op,"TIME")) { clock_ms=(uint32_t)strtoull(arg,NULL,10); rtos_ms=clock_ms; }
        else if(!strcmp(op,"STEP")) {
            unsigned control=1U,safety=1U,drain=1U; sscanf(arg,"%u %u %u %u",&control,&safety,&drain,&tx_mode);
            current_task=2U; Comms_ProcessRxOnce(0U);
            if(control) Motor_Update();
            if(safety) { MotorGuard_Poll(); MotorSafety_Poll(); Motor_ServiceStop(); }
            SimSession_Service();
            if(drain && (!tx_blocked || (int32_t)(clock_ms-tx_blocked_until)>=0)) {
                tx_blocked=false;
                current_task=1U; for(unsigned i=0;i<3U;++i) if(!UART4Transport_TxOnce(0U)) break;
                current_task=2U;
            }
        } else if(!strcmp(op,"TX")) {
            current_task=1U; for(unsigned i=0;i<3U;++i) if(!UART4Transport_TxOnce(0U)) break;
            current_task=2U;
        } else if(!strcmp(op,"STOP")) Motor_StopAll();
        else if(!strcmp(op,"ERROR")) { ipsr=1U; HAL_UART_ErrorCallback(&huart4); ipsr=0U; }
        else if(!strcmp(op,"FILL")) { uint8_t b=1U; for(unsigned i=0;i<9U;++i) UART4Transport_QueueFrame(&b,1U); }
        else if(!strcmp(op,"QUIT")) break;
        else abort();
        assert(!primask && !physical_calls); snapshot();
    }
    return 0;
}
