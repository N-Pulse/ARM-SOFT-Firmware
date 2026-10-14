"""Inspect real ARM artifacts. Static checks do not replace board measurement."""
from pathlib import Path
import hashlib
import subprocess
ROOT=Path(__file__).resolve().parents[2]
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def symbols(path):
    return subprocess.check_output(['arm-none-eabi-nm','--defined-only',str(path)],text=True)
def main():
    hw=ROOT/'build/HW/n-pulse.elf'; canonical_hash=sha(hw)
    for mode,test,current in [('HW',False,False),('SIM',False,False),
                              ('HW-single-1',True,False),('HW-single-1-current',True,True)]:
        directory=ROOT/'build'/mode; flags=(directory/'compile.flags').read_text()
        assert f'-DN_PULSE_HW_SINGLE_MOTOR_TEST={int(test)}' in flags
        assert f'-DN_PULSE_ENABLE_CURRENT_ACQUISITION={int(current)}' in flags
        names=symbols(directory/'n-pulse.elf')
        for function in ('Motor_DRV8848_Arm','Encoder_BSP_SetCalibration','Encoder_BSP_Reset'):
            assert (f' {function}\n' in names)==test,(mode,function)
        assert (' HAL_ADC_Start_DMA\n' in names)==current
        assert (' HAL_ADCEx_Calibration_Start\n' in names)==current
        if test: assert not (directory/'build-manifest.json').exists()
    assert sha(ROOT/'Debug/N-PULSE FIRMWARE.elf')==canonical_hash
    ioc=(ROOT/'N-PULSE FIRMWARE.ioc').read_text()
    assert 'PE13.Signal=ADC3_IN3' in ioc and 'PE15.Signal=ADC4_IN2' in ioc
    assert 'PC10.Signal=UART4_TX' in ioc and 'PC11.Signal=UART4_RX' in ioc
    print('PASS Patch7 ARM modes: production has no commissioning arm/calibration paths; isolated test/ADC symbols; canonical HW alias preserved; UART4 and Curr5 mappings retained')
if __name__=='__main__': main()
