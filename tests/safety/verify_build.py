"""ARM call paths for shared safety and explicitly selected HW/SIM inputs.
ELF checks prove code separation; no electrical or timing measurement is made.
"""
from pathlib import Path
import re
import subprocess
ROOT=Path(__file__).resolve().parents[2]
def output(*args): return subprocess.check_output(args,cwd=ROOT,text=True)
def verify(mode):
    elf=ROOT/'build'/mode/'n-pulse.elf'
    symbols=output('arm-none-eabi-nm',str(elf))
    paths={
      'MotorSafety_RequestStopCauseLocked':('Motor_InhibitAllLocked','MotorBackend_InhibitActuationLocked','MotorBackend_InhibitOutputsLocked'),
      'MotorSafety_Poll':('Motor_CheckHealth','MotorSafety_RequestStopCauseLocked'),
      'Motor_Update':('HAL_GetTick','MotorPID_Update','MotorBackend_ApplyOutput'),
      'MotorBackend_InhibitActuationLocked':(('Motor_DRV8848_InhibitLocked',) if mode=='HW' else ('SimSession_InhibitLocked',)),
      'MotorGuard_Poll':(('Encoder_BSP_GetRawCount','MotorSafety_RequestStopCauseLocked') if mode=='HW' else ('SimSession_GuardSampleLocked','MotorSafety_RequestStopCauseLocked')),
      'MotorBackend_ApplyOutput':(('Motor_DRV8848_SetRaw',) if mode=='HW' else ('Motor_OutputAllowedLocked','SimSession_StageOutputLocked')),
      'MotorSafety_OnFeedbackAtTime':('MotorSafety_RequestStopCauseLocked',),
    }
    if mode=='HW':
        paths['Motor_DRV8848_InhibitLocked']=('set_sleep','Motor_DRV8848_StopAll')
        paths['MotorBackend_OnFeedback']=('MotorSafety_OnFeedback','Motor_InvalidatePositionFeedback','Motor_OnPositionFeedback')
        paths['MotorSafety_OnFeedback']=('HAL_GetTick','MotorSafety_OnFeedbackAtTime')
    else:
        paths['MotorBackend_OnFeedbackAtTime']=('MotorSafety_OnFeedbackAtTime','Motor_InvalidatePositionFeedback','Motor_OnPositionFeedbackAtTime')
        paths['SimSession_OnFrame']=('Motor_RecoverFromFault','Motor_Enable','Motor_SetTargetAtEpoch','MotorBackend_OnFeedbackAtTime')
        paths['SimSession_FeedbackReadyLocked']=('HAL_GetTick','MotorSafety_HasFreshForceLocked')
        assert 'Motor_DRV8848' not in symbols and 'Encoder_BSP_' not in symbols
    for function,expected in paths.items():
        assembly=output('arm-none-eabi-objdump','-d','--disassemble='+function,str(elf))
        assert '<'+function+'>' in assembly,(mode,function)
        for name in expected: assert '<'+name+'>' in assembly,(mode,function,name)
        if function.endswith('Locked'):
            for forbidden in ('HAL_UART_', 'Semaphore','HAL_Delay','WaveformOutputStart','xQueue'):
                assert forbidden not in assembly,(mode,function,forbidden)
    print(mode+': linked shared inhibit/PID/force/delay/freshness and backend-specific guard; physical separation OK')
if __name__=='__main__': verify('HW'); verify('SIM')
