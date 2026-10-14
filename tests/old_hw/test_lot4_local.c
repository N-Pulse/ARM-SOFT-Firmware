#define main lot2_reference_main
#include "test_l298n.c"
#undef main
int main(int argc,char **argv)
{
    assert(argc==2); slave=selected!=MOTOR_LITTLE;
    BoardLink_Init(); OldHW_HoldOutputsLow(); OldHW_ClockConfig(); OldHW_Init();
    Motor_InitAll(); CommsSession_Init(true); configure();
    if(!strcmp(argv[1],"recovery-repeat")) {
        Motor_StopAll(); Motor_ServiceStop();
        motor_recovery_ticket_t ticket=Motor_RecoveryTicketLocked();
        Motor_StopAll(); assert(MotorSafety_EpochLocked()==ticket.epoch);
        assert(!Motor_RecoverFromFaultWithTicket(ticket)); stopped();
        assert(command((hw_command_t){.operation=4})==HW_OK);
    } else if(!strcmp(argv[1],"recovery-race")) {
        Motor_StopAll(); Motor_ServiceStop(); stop_entry=(void *)MotorGuard_Poll;
        assert(command((hw_command_t){.operation=4})==HW_FAULT);
        assert(!stop_entry && (MotorSafety_GetStopCauses()&MOTOR_STOP_MANUAL)); stopped();
        assert(command((hw_command_t){.operation=4})==HW_OK);
    } else if(!strcmp(argv[1],"reset-all")) {
        TIM_TypeDef *t=selected==MOTOR_INDEX?TIM2:TIM3; t->CNT+=17;
        int64_t before=Encoder_BSP_GetRawCount(selected); assert(before==17);
        TIM8->CR1&=~TIM_CR1_CEN;
        assert(!Encoder_BSP_ResetAll()); assert(Encoder_BSP_GetRawCount(selected)==before);
        TIM8->CR1|=TIM_CR1_CEN; assert(Encoder_BSP_ResetAll());
        assert(Encoder_BSP_GetRawCount(selected)==0);
        if(slave) { HAL_NVIC_DisableIRQ(EXTI15_10_IRQn); assert(!Encoder_BSP_ResetAll()); }
    } else return 2;
    puts("PASS lot4 local recovery ticket / transactional encoder reset; exit=0"); return 0;
}
