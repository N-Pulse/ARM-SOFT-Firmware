/* Actual USART3 owner/service/callbacks on modeled HAL and motor boundaries.
 * Controller/PID/decoder integration is separately exercised by two DLLs. */
#include "../../fw/comm/board_link_runtime.c"
#include <assert.h>
#include <stdio.h>
#include <string.h>
UART_HandleTypeDef huart3;
RCC_TypeDef mock_rcc; RNG_TypeDef mock_rng;
USART_TypeDef mock_uarts[2];
volatile board_role_t g_old_hw_role=BOARD_ROLE_MASTER;
volatile uint32_t g_old_hw_rejected_commands;
static uint32_t now=400,epoch=19,causes,masked,aborts,rx_starts,tx_starts;
static bool abort_fail,rx_fail,tx_fail,irq;
static uint8_t *rx_pointer;
uint32_t HAL_GetTick(void) { return now; }
uint32_t __get_PRIMASK(void) { return masked; }
void __disable_irq(void) { masked=1; }
void __set_PRIMASK(uint32_t x) { masked=x; }
board_role_t BoardLink_Role(void) { return g_old_hw_role; }
bool BoardLink_IsLocalMotor(motor_id_t id) { return BL_Local((uint8_t)g_old_hw_role,(uint8_t)id); }
uint32_t MotorSafety_EpochLocked(void) { return epoch; }
uint32_t MotorSafety_GetStopCauses(void) { return causes; }
void MotorSafety_RequestStopCauseLocked(motor_stop_cause_t cause,motor_id_t id)
{ (void)id; if(!(causes&cause)) { ++epoch; causes|=cause; } }
void MotorSafety_RequestStopCause(motor_stop_cause_t cause,motor_id_t id) { MotorSafety_RequestStopCauseLocked(cause,id); }
bool MotorSafety_FilterCommand(motor_id_t id,float *p,uint8_t *s) { (void)id;(void)p;(void)s; return !causes; }
bool Motor_IsEnabled(motor_id_t id) { (void)id; return false; }
bool Motor_OutputAllowedLocked(motor_id_t id) { (void)id; return false; }
bool Motor_HasFreshPositionFeedback(motor_id_t id) { (void)id; return false; }
bool Motor_SetTargetAtEpoch(motor_id_t id,float target,uint8_t speed,uint32_t e) { (void)id;(void)target;(void)speed;(void)e; return false; }
bool Motor_RecoverFromFault(void) { return false; }
motor_recovery_ticket_t Motor_RecoveryTicketLocked(void) { return (motor_recovery_ticket_t){epoch,0}; }
bool Motor_RecoverFromFaultWithTicket(motor_recovery_ticket_t ticket) { (void)ticket; return false; }
bool Motor_GetSnapshot(motor_id_t id,motor_state_t *out) { (void)id; memset(out,0,sizeof(*out)); return true; }
bool Encoder_BSP_IsCalibrated(motor_id_t id) { (void)id; return false; }
bool Encoder_BSP_RawAvailable(motor_id_t id) { (void)id; return false; }
bool Encoder_BSP_GetSnapshot(motor_id_t id,encoder_snapshot_t *out) { (void)id; memset(out,0,sizeof(*out)); return false; }
bool Motor_L298N_IsArmed(void) { return false; }
bool Motor_L298N_IsTransitioning(void) { return false; }
float Motor_L298N_AppliedPWM(motor_id_t id) { (void)id; return 0; }
bool CommsSession_IsFreshLocked(uint32_t ms) { (void)ms; return true; }
void HAL_NVIC_DisableIRQ(IRQn_Type id) { assert(id==USART3_IRQn); irq=false; }
void HAL_NVIC_EnableIRQ(IRQn_Type id) { assert(id==USART3_IRQn); irq=true; }
void HAL_NVIC_ClearPendingIRQ(IRQn_Type id) { assert(id==USART3_IRQn); }
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *u,uint8_t *p,uint16_t n)
{ assert(u==&huart3 && n==1); rx_pointer=p; ++rx_starts; return rx_fail?HAL_ERROR:HAL_OK; }
HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *u,uint8_t *p,uint16_t n)
{ assert(u==&huart3 && p==tx && n<=BL_WIRE_MAX && masked); ++tx_starts; return tx_fail?HAL_ERROR:HAL_OK; }
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *u)
{ assert(u==&huart3 && !masked); ++aborts; return abort_fail?HAL_ERROR:HAL_OK; }
HAL_StatusTypeDef HAL_UART_AbortTransmit(UART_HandleTypeDef *u)
{ assert(u==&huart3 && !masked); ++aborts; return abort_fail?HAL_ERROR:HAL_OK; }
void vTaskDelay(TickType_t ms) { assert(!masked); now+=ms; }
void vTaskDelete(TaskHandle_t t) { (void)t; assert(!masked); }
TaskHandle_t xTaskCreateStatic(void (*fn)(void *),const char *name,uint32_t depth,void *arg,
                               uint32_t priority,StackType_t *stack,StaticTask_t *control)
{ assert(fn && name && depth==768 && !arg && priority==2 && stack && control); return control; }
static void initialize(void)
{
    BoardLink_RuntimeInit();
    assert(!BoardLink_ActuationAllowedLocked(now));
    assert(BL_Init(&link,BL_MASTER,111,now,(bl_hooks_t){stop_hook,target_hook,NULL}));
    link.peer=222; link.established=link.have_heartbeat=true; link.rx_ms=link.ack_ms=now;
    link.have_peer_epoch=true; link.peer_epoch=77;
    started=true; huart3.Instance=USART3;
}
int main(int argc,char **argv)
{
    assert(argc==2); const char *test=argv[1]; initialize();
    if(!strcmp(test,"ownership")) {
        UART_HandleTypeDef other={0}; assert(!BoardLink_RxCallback(&other) && !BoardLink_TxCallback(&other) && !BoardLink_ErrorCallback(&other));
        BoardLink_StartTask(); service(); assert(tx_active && tx_starts==1 && !causes);
        huart3.gState=1; assert(BoardLink_TxCallback(&huart3) && !tx_complete);
        huart3.gState=HAL_UART_STATE_READY; huart3.TxXferCount=0; BoardLink_TxCallback(&huart3);
        assert(tx_complete); service(); assert(tx_active && tx_starts==2 && !tx_complete); /* Due heartbeat now owns TX. */
    } else if(!strcmp(test,"rx-overflow")) {
        for(unsigned i=0;i<256;++i) { rx_byte=1; BoardLink_RxCallback(&huart3); }
        assert(causes && recover_rx && !aborts); service(); assert(aborts==1 && !recover_rx && rx_pointer==&rx_byte);
    } else if(!strcmp(test,"rx-error")) {
        BoardLink_ErrorCallback(&huart3); assert(causes && recover_rx && !aborts);
        service(); assert(aborts==1 && !recover_rx);
    } else if(!strcmp(test,"rx-rearm")) {
        rx_fail=true; BoardLink_RxCallback(&huart3); assert(causes && recover_rx);
    } else if(!strcmp(test,"tx-start")) {
        tx_fail=true; service(); assert(causes && quarantined && !tx_active);
    } else if(!strcmp(test,"tx-timeout")) {
        service(); assert(tx_active); now+=16; service(); assert(causes && aborts==1);
    } else if(!strcmp(test,"abort-failed")) {
        service(); abort_fail=true; now+=16; service(); assert(causes && quarantined && !tx_active);
        unsigned before=tx_starts; now+=200; BoardLink_TxCallback(&huart3); service(); assert(tx_starts==before);
    } else if(!strcmp(test,"rng-failed")) {
        uint64_t value; mock_rcc.CRRCR=RCC_CRRCR_HSI48RDY; mock_rng.SR=RNG_SR_SECS;
        assert(!nonce(&value)); mock_rng.SR=0; assert(!nonce(&value));
        mock_rng.SR=RNG_SR_DRDY; mock_rng.DR=11; assert(!nonce(&value));
    } else { return 2; }
    printf("PASS actual USART3 transport %s; exit=0\n",test); return 0;
}
