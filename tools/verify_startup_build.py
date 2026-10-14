"""Check linked handlers and disarmed/default-ADC invariants in HW/SIM ELFs.

These checks inspect ARM binaries; they do not execute firmware on a board.
"""
from pathlib import Path
import json
import hashlib
import re
import shlex
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def output(*command):
    return subprocess.check_output(command, cwd=ROOT, text=True)


def verify(mode):
    directory = ROOT / "build" / mode
    elf = directory / "n-pulse.elf"
    symbols = {}
    for line in output("arm-none-eabi-nm", "-n", str(elf)).splitlines():
        match = re.match(r"^([0-9a-fA-F]+)\s+(\S)\s+(.+)$", line)
        if match:
            symbols[match[3]] = (int(match[1], 16), match[2])
    vectors_path = ROOT / "build" / "validation" / f"{mode}-vectors.bin"
    vectors_path.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(["arm-none-eabi-objcopy", "-O", "binary", "--only-section=.isr_vector",
                    str(elf), str(vectors_path)], cwd=ROOT, check=True)
    vector_bytes = vectors_path.read_bytes()
    vectors = struct.unpack(f"<{len(vector_bytes) // 4}I", vector_bytes)
    device = (ROOT / "Drivers/CMSIS/Device/ST/STM32G4xx/Include/stm32g474xx.h").read_text()
    timer_irq = int(re.search(r"TIM6_DAC_IRQn\s*=\s*(\d+)", device)[1])
    for index, name in [(11, "SVC_Handler"), (14, "PendSV_Handler"),
                        (15, "SysTick_Handler"), (16 + timer_irq, "TIM6_DAC_IRQHandler")]:
        address, kind = symbols[name]
        assert kind == "T", (mode, name, "handler is not strong")
        assert vectors[index] == address | 1, (mode, name, "wrong vector")
    for name in ("HAL_InitTick", "HAL_TIM_PeriodElapsedCallback"):
        assert symbols[name][1] == "T", (mode, name, "weak fallback retained")
    host_irq = int(re.search(r"LPUART1_IRQn\s*=\s*(\d+)", device)[1])
    assert symbols['LPUART1_IRQHandler'][1] == 'T'
    assert vectors[16 + host_irq] == symbols['LPUART1_IRQHandler'][0] | 1
    for irq in ('UART4', 'USART3'):
        index = int(re.search(irq + r'_IRQn\s*=\s*(\d+)', device)[1])
        if irq=='USART3' and mode=='HW':
            assert symbols['USART3_IRQHandler'][1]=='T'
            assert vectors[16+index]==symbols['USART3_IRQHandler'][0]|1
            for required in ('BL_Feed','BL_Take','BoardLink_SafetyPollLocked','BoardLink_RuntimeInit'):
                assert symbols[required][1]=='T'
        else:
            assert vectors[16 + index] == symbols['Default_Handler'][0] | 1, (mode, irq, 'unexpected active handler')
    assert symbols['_estack'][0] == 0x20018000, 'Stack must stay in 96K system SRAM'
    assert 0x20000000 <= symbols['_ebss'][0] < symbols['_estack'][0]
    for forbidden in ('huart4', 'hhrtim1', 'hlptim1', 'htim5', 'htim16', 'htim17',
                      'Motor_L298N_Arm', 'program_locked', 'MotorOutputs_HRTIMPair', 'MotorOutputs_TIMPair',
                      'HAL_LPTIM_Encoder_Start', 'HAL_TIMEx_PWMN_Start'):
        assert forbidden not in symbols, (mode, 'new-board/actuation dependency', forbidden)
    if mode == 'HW':
        exti_irq = int(re.search(r'EXTI15_10_IRQn\s*=\s*(\d+)', device)[1])
        assert symbols['EXTI15_10_IRQHandler'][1] == 'T'
        assert vectors[16+exti_irq] == symbols['EXTI15_10_IRQHandler'][0] | 1
        assert 'ThumbEncoder_IRQHandler' in symbols
    else:
        assert 'ThumbEncoder_Init' not in symbols and 'ThumbEncoder_IRQHandler' not in symbols
    assert symbols['g_old_hw_role'][1] in ('B', 'D')
    assert symbols['OldHW_HoldOutputsLow'][1] == 'T'
    port_object = directory / "obj/Middlewares/Third_Party/FreeRTOS-Kernel/portable/GCC/ARM_CM4F/port.o"
    port_symbols = output("arm-none-eabi-nm", str(port_object))
    for handler in ("SVC_Handler", "PendSV_Handler", "SysTick_Handler"):
        assert re.search(r"\bT " + handler + r"$", port_symbols, re.M), (mode, handler)
    for function, called in [("SysTick_Handler", "xTaskIncrementTick"),
                             ("TIM6_DAC_IRQHandler", "HAL_TIM_IRQHandler"),
                             ("HAL_TIM_PeriodElapsedCallback", "HAL_IncTick")]:
        assembly = output("arm-none-eabi-objdump", "-d", f"--disassemble={function}", str(elf))
        assert f"<{called}>" in assembly, (mode, function, "missing expected call")
        if function == "SysTick_Handler":
            assert "<HAL_IncTick>" not in assembly, "double HAL tick in SysTick"
    # --gc-sections removes these functions only if the firmware cannot reach them.
    for forbidden in ("Encoder_BSP_SetCalibration", "Encoder_BSP_Reset",
                      "Encoder_BSP_ResetAll", "Motor_DRV8848_Arm", "HAL_HRTIM_WaveformOutputStart",
                      "HAL_HRTIM_WaveformCountStart", "HAL_TIM_PWM_Start",
                      "HAL_ADC_Start_DMA", "HAL_OPAMP_Start", "wt_init_one", "s_wt"):
        assert forbidden not in symbols, (mode, "unexpected active path", forbidden)
    if mode == "HW":
        for forbidden in ('Motor_Enable','SimSession_OnFrame','SimParser_Feed'):
            assert forbidden not in symbols, (mode, forbidden)
    else:
        for required in ('Motor_Enable','SimSession_OnFrame','SimParser_Feed','SimSession_StageOutputLocked'):
            assert required in symbols, (mode, required)
        for forbidden in ('Motor_L298N_Init','Motor_L298N_SetRaw','Motor_L298N_InhibitLocked',
                          'Encoder_BSP_Init','Encoder_BSP_GetRawCount','Encoder_BSP_GetSnapshot',
                          'MotorOutputs_HRTIMPair','MotorOutputs_TIMPair','set_sleep','set_compare'):
            assert forbidden not in symbols, (mode, 'physical dependency', forbidden)
        init = output('arm-none-eabi-objdump','-d','--disassemble=SimSession_Init',str(elf))
        for forbidden in ('Motor_Enable','Motor_SetTarget','HAL_','SimSession_OnFrame'):
            assert '<'+forbidden+'>' not in init
    backend = output("arm-none-eabi-objdump", "-d", "--disassemble=MotorBackend_Init", str(elf))
    assert ("<ADC_DMA_BSP_Init>" in backend) == (mode == "HW"), "wrong linked backend"
    receipt = (directory / "compile.flags").read_text()
    assert ("-DMOTOR_BACKEND_SIM=1" in receipt) == (mode == "SIM")
    assert "-DN_PULSE_ENABLE_CURRENT_ACQUISITION=0" in receipt
    assert "-DN_PULSE_OLD_HW=1" in receipt and "-DN_PULSE_TARGET_RET6=1" in receipt
    assert '-DN_PULSE_THUMB_EXTI_MOTION_QUALIFIED=0' in receipt
    assert '-DN_PULSE_TIMING_DIAGNOSTICS=0' in receipt
    assert 'OldHW_TimingInit' not in symbols and 'g_old_hw_timing_enabled' not in symbols
    manifest = json.loads((directory / 'build-manifest.json').read_text())
    assert manifest['hardware'] == 'STM32G474RET6' and manifest['milestone'] == 'OLD_HW_LOT4'
    assert manifest['physical_outputs'] == 'permanently_disabled'
    assert '-DN_PULSE_OLD_HW_MOTION_OPT_IN=0' in receipt
    main = output('arm-none-eabi-objdump', '-d', '--disassemble=main', str(elf))
    ordered = [main.index('<'+name+'>') for name in ('HAL_Init','BoardLink_Init',
               'OldHW_HoldOutputsLow','OldHW_ClockConfig','OldHW_Init','App_Init','CreateTasks','StartScheduler')]
    assert ordered == sorted(ordered), (mode, 'unsafe startup order')
    assert '<OldHW_CutoffRegisters>' in output('arm-none-eabi-objdump','-d',
        '--disassemble=N_PULSE_FatalCutoff',str(elf))
    assert '-DN_PULSE_DUAL_BOARD_LINK=1' in receipt
    print(f"OLD_HW {mode}: RET6/96K SRAM, role before outputs, LPUART1, USART3 active in HW only, strong RTOS/HAL vectors, tick ownership and no physical activation path OK")


def verify_commission(motor):
    directory = ROOT / f'build/HW-single-{motor}-optin'
    elf = directory / 'n-pulse.elf'
    symbols = output('arm-none-eabi-nm', str(elf))
    for name in ('Motor_L298N_Arm', 'Motor_L298N_ApplySigned', 'program_locked',
                 'HWCommission_OutputPreflightReady', 'Encoder_BSP_SetCalibration', 'Encoder_BSP_Reset'):
        assert re.search(r'\b[Tt] '+name+r'$', symbols, re.M), ('commission', motor, name)
    for name in ('HAL_TIM_PWM_Start','HAL_HRTIM_WaveformOutputStart','Motor_DRV8848_Arm'):
        assert not re.search(r'\b'+name+r'$', symbols, re.M), ('commission', motor, name)
    arm = output('arm-none-eabi-objdump','-d','--disassemble=Motor_L298N_Arm',str(elf))
    for name in ('HWCommission_Ready','HWCommission_OutputPreflightReady','MotorSafety_EpochLocked'):
        assert '<'+name+'>' in arm
    assert '-DN_PULSE_OLD_HW_MOTION_OPT_IN=1' in (directory/'compile.flags').read_text()
    assert '-DN_PULSE_THUMB_EXTI_MOTION_QUALIFIED=0' in (directory/'compile.flags').read_text()
    print(f'ARM commissioning motor={motor}: real L298N register path linked; explicit preflight/feedback/epoch gates, no historical start/DRV8848, THUMB remains unqualified')


def verify_final_artifacts():
    """Lot 4: read ELF/receipts only; no debugger connection or programmer."""
    records = {}
    device = (ROOT / 'Drivers/CMSIS/Device/ST/STM32G4xx/Include/stm32g474xx.h').read_text()
    for mode in ('HW', 'SIM', 'HW-single-1-optin', 'HW-single-4-optin', 'HW-timing'):
        directory = ROOT / 'build' / mode
        elf = directory / 'n-pulse.elf'
        flags = (directory / 'compile.flags').read_bytes()
        symbols = {}
        for line in output('arm-none-eabi-nm', '-n', str(elf)).splitlines():
            match = re.match(r'^([0-9a-fA-F]+)\s+(\S)\s+(.+)$', line)
            if match:
                symbols[match[3]] = (int(match[1], 16), match[2])
        for forbidden in ('HAL_IWDG_Init', 'HAL_WWDG_Init', 'hhrtim1', 'huart4',
                          'MotorOutputs_HRTIMPair', 'Motor_DRV8848_Arm', 'HAL_TIM_PWM_Start'):
            assert forbidden not in symbols, (mode, forbidden)
        assert b'-DN_PULSE_THUMB_EXTI_MOTION_QUALIFIED=0' in flags
        assert b'-DN_PULSE_ENABLE_CURRENT_ACQUISITION=0' in flags
        timing = mode == 'HW-timing'
        assert (b'-DN_PULSE_TIMING_DIAGNOSTICS=1' in flags) == timing
        assert ('g_old_hw_timing' in symbols) == timing
        if timing:
            assert 'OldHW_TimingInit' in symbols and 'g_old_hw_primask_max_cycles' in symbols
            assert 'Motor_L298N_Arm' not in symbols and 'program_locked' not in symbols
        vectors_path = ROOT / 'build/validation' / (mode + '-final-vectors.bin')
        subprocess.run(['arm-none-eabi-objcopy', '-O', 'binary', '--only-section=.isr_vector',
                        str(elf), str(vectors_path)], cwd=ROOT, check=True)
        data = vectors_path.read_bytes()
        vectors = struct.unpack(f'<{len(data)//4}I', data)
        handlers = [(11, 'SVC_Handler'), (14, 'PendSV_Handler'), (15, 'SysTick_Handler')]
        for irq in ('TIM6_DAC', 'LPUART1', 'USART3', 'EXTI15_10'):
            index = int(re.search(irq+r'_IRQn\s*=\s*(\d+)', device)[1])
            name = irq + '_IRQHandler'
            if mode == 'SIM' and irq in ('USART3', 'EXTI15_10'):
                assert vectors[16+index] == symbols['Default_Handler'][0] | 1
            else:
                handlers.append((16+index, name))
        for index, name in handlers:
            assert symbols[name][1] == 'T' and vectors[index] == symbols[name][0] | 1, (mode, name)
        assert symbols['_estack'][0] == 0x20018000
        assert symbols['_end'][0] + 0x600 <= symbols['_estack'][0], (mode, 'linker reserves')
        fields = output('arm-none-eabi-size', str(elf)).splitlines()[-1].split()
        text_size, data_size, bss_size = map(int, fields[:3])
        assert text_size + data_size < 512*1024 and data_size + bss_size + 0x600 < 96*1024
        digest = hashlib.sha256(elf.read_bytes()).hexdigest()
        if mode in ('HW', 'SIM'):
            manifest = json.loads((directory/'build-manifest.json').read_text())
            assert manifest['elf_sha256'] == digest
            assert manifest['flags_sha256'] == hashlib.sha256(flags).hexdigest()
            alias = ROOT/'Debug'/('SIM' if mode == 'SIM' else '')/'N-PULSE FIRMWARE.elf'
            assert hashlib.sha256(alias.read_bytes()).hexdigest() == digest
        else:
            assert not (directory/'build-manifest.json').exists(), (mode, 'bench image published')
        frames = []
        selected = r'ControlTask|SafetyTask|TelemetryTask|HWTelemetry_Service|HandleDeviceMessage|HandleProsthesisMessage|SendMotorDiagnostic|SendLinkDiagnostic|SendCommandReply|Motor_Update|Motor_RecoverFromFault|ThumbEncoder_IRQHandler|LPUART1_IRQHandler|USART3_IRQHandler|TIM6_DAC_IRQHandler|\bservice\b|\bworker\b|\bfeedback\b'
        for path in directory.glob('obj/**/*.su'):
            frames.extend(line for line in path.read_text().splitlines() if re.search(selected, line))
        records[mode] = dict(text=text_size, data=data_size, bss=bss_size,
                             elf_sha256=digest, selected_static_frames=frames)
    target = ROOT/'build/validation/old-hw-lot4-elf.json'
    target.write_text(json.dumps(records, indent=2)+'\n')
    print('Five ARM artifacts: hashes/aliases, vectors, memory bounds, timing isolation and THUMB/current gates passed; static frames recorded, physical stack high-water/WCET remain unmeasured.')


if __name__ == "__main__":
    verify("HW")
    verify("SIM")
    for motor in (1,4):
        verify_commission(motor)
    flags = shlex.split((ROOT / 'build/HW/compile.flags').read_text().split('|')[2])
    flags.remove('-DN_PULSE_OLD_HW=1')
    rejected = subprocess.run(['arm-none-eabi-gcc', *flags, '-E', 'Core/Src/main.c'],
                              cwd=ROOT, capture_output=True, text=True)
    assert rejected.returncode and 'requires the maintained OLD_HW RET6 build' in rejected.stderr
    print('ARM build without OLD_HW interlock marker rejected, including generated legacy configuration')
    verify_final_artifacts()
