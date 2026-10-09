"""Inspect real ARM handlers, fatal dependency closure and paired output objects.

These checks do not execute firmware or measure pins. Existing lot 1/2/3
verifiers remain separate and must pass without weakening startup invariants.
"""
from pathlib import Path
import re
import struct
import subprocess

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'build/validation/lot4'

def output(*args):
    return subprocess.check_output(args,cwd=ROOT,text=True)

def calls(text):
    # Only actual branch/call instructions, never data/literal annotations.
    return set(re.findall(r'\b(?:blx?|b\.w)\s+[^\n]*?<([^>+]+)(?:\+[^>]*)?>',text))

def verify(mode):
    directory=ROOT/'build'/mode; elf=directory/'n-pulse.elf'
    symbols={}
    for line in output('arm-none-eabi-nm','-n',str(elf)).splitlines():
        m=re.match(r'^([0-9a-fA-F]+)\s+(\S)\s+(.+)$',line)
        if m: symbols[m[3]]=(int(m[1],16),m[2])
    disassembly=output('arm-none-eabi-objdump','-d',str(elf))
    functions={}
    by_address={}
    matches=list(re.finditer(r'^([0-9a-f]+) <([^>]+)>:',disassembly,re.M))
    for i,m in enumerate(matches):
        functions[m[2]]=disassembly[m.start():matches[i+1].start() if i+1<len(matches) else None]
        by_address[int(m[1],16)]=functions[m[2]]
    # objdump may label the default address with one of its weak IRQ aliases.
    functions['Default_Handler']=by_address[symbols['Default_Handler'][0]]
    handlers=('NMI_Handler','HardFault_Handler','MemManage_Handler','BusFault_Handler','UsageFault_Handler')
    for f in (*handlers,'Error_Handler','vApplicationMallocFailedHook','vApplicationStackOverflowHook'):
        assert symbols[f][1]=='T', (mode,f)
        assert 'N_PULSE_FatalHalt' in calls(functions[f]), (mode,f)
    assert calls(functions['N_PULSE_FatalHalt'])=={'N_PULSE_FatalCutoff'}
    assert 'N_PULSE_FatalUnhandledIRQ' in functions['Default_Handler']
    assert calls(functions['N_PULSE_FatalUnhandledIRQ'])=={'N_PULSE_FatalHalt'}
    cutoff=functions['N_PULSE_FatalCutoff']
    assert calls(cutoff)==set(),(mode,calls(cutoff))
    assert 'cpsid' in cutoff and 'cpsie' not in cutoff and 'dsb' in cutoff
    # Audit fixed peripheral literals. No HAL handle / indirect call / FP used.
    for address in ('40021000','48001800','48000800','40016800','40014000','40014400','40014800'):
        assert address in cutoff,(mode,address)
    assert not re.search(r'\bblx\s+r\d+',cutoff)
    assert not re.search(r'\bv(?:ldr|str|mov|mul|add|sub|div)',cutoff)
    for name in ('HAL_', 'Task', 'Queue', 'Semaphore', 'UART', 'Delay', 'Motor_Lock', 'memset', 'malloc'):
        assert not any(name in f for f in calls(cutoff)),(mode,name)
    vec=OUT/(mode+'-vectors.bin'); OUT.mkdir(parents=True,exist_ok=True)
    subprocess.run(['arm-none-eabi-objcopy','-O','binary','--only-section=.isr_vector',str(elf),str(vec)],check=True)
    values=struct.unpack(f'<{vec.stat().st_size//4}I',vec.read_bytes())
    for index,f in enumerate(handlers,2): assert values[index]==symbols[f][0]|1
    assert values[0]==symbols['_estack'][0]==0x20020000
    assert symbols['g_n_pulse_fatal_cause'][1]=='B'
    # configASSERT callers retained in both kernels (assert cause goes to Halt).
    assert any('N_PULSE_FatalHalt' in calls(body) for f,body in functions.items() if f.startswith(('xTask','vTask','xQueue')))
    assert 'vApplicationMallocFailedHook' in calls(functions['pvPortMalloc'])
    assert 'vApplicationStackOverflowHook' in calls(functions['vTaskSwitchContext'])
    obj=directory/'obj/fw/motors/motor_control/motor_outputs.o'
    assembly=output('arm-none-eabi-objdump','-dr',str(obj))
    for forbidden in ('HAL_UART','HAL_Delay','xQueue','Semaphore','WaveformOutputStart','HAL_TIM_PWM_Start'):
        assert forbidden not in assembly,(mode,forbidden)
    for f in ('MotorOutputs_HRTIMPair','MotorOutputs_TIMPair'):
        assert f'<{f}>' in assembly
    # HW reaches pair writers through production driver. SIM default uses staging
    # but still compiles the same driver/pair object; unused writes are gc'd.
    if mode=='HW':
        assert {'MotorOutputs_HRTIMPair','MotorOutputs_TIMPair'} <= calls(functions['set_compare'])
    else:
        for f in ('set_compare','MotorOutputs_HRTIMPair','MotorOutputs_TIMPair','Motor_DRV8848_Arm'):
            assert f not in functions,(mode,'physical output path',f)
    (OUT/(mode+'-fatal-disassembly.txt')).write_text(functions['N_PULSE_FatalHalt']+cutoff)
    print(f'{mode}: strong fatal vectors/hooks -> bounded fixed-register cutoff, no external call/RTOS/UART; pair writers compiled; MSP=0x20020000 (CCM alias)')

if __name__=='__main__':
    verify('HW'); verify('SIM')
