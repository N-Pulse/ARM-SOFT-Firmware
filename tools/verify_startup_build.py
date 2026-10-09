"""Check linked handlers and disarmed/default-ADC invariants in HW/SIM ELFs.

These checks inspect ARM binaries; they do not execute firmware on a board.
"""
from pathlib import Path
import re
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
        for forbidden in ('Motor_DRV8848_Init','Motor_DRV8848_SetRaw','Motor_DRV8848_InhibitLocked',
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
    print(f"{mode}: strong port/HAL handlers, vector addresses, tick calls, backend, disarmed/default ADC OK")


if __name__ == "__main__":
    verify("HW")
    verify("SIM")
