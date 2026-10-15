"""Run production startup/BSP routines on a host with a minimal fake HAL.

Requires host GCC, not ARM GCC. These are not tests on an STM32 board.
"""
from pathlib import Path
import os
import subprocess

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
OUT = ROOT / "build" / "startup-tests"


def run(args):
    subprocess.run([str(a) for a in args], cwd=ROOT, check=True)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    common = [os.environ.get("HOST_CC", "gcc"), "-std=c11", "-Wall", "-Wextra",
              "-Werror", "-I", HERE / "mocks", "-I", ROOT / "fw/bsp/encoder",
              "-I", ROOT / "fw/bsp/adc_dma", "-I", ROOT / "fw/motors/motor_control"]
    cases = [
        ("timebase", "Core/Src/stm32g4xx_hal_timebase_tim.c", "test_timebase.c", [], [[]]),
        ("encoder", "fw/bsp/encoder/encoder_bsp.c", "test_encoder.c", [], [[], ["1"], ["2"]]),
        ("encoder-numeric", "fw/bsp/encoder/encoder_bsp.c", "test_encoder_numeric.c", [], [["product"], ["offset"], ["reset"], ["all"]]),
        ("adc-off", "fw/bsp/adc_dma/adc_dma_bsp.c", "test_adc.c", [], [[]]),
        ("adc-on", "fw/bsp/adc_dma/adc_dma_bsp.c", "test_adc.c",
         ["-DN_PULSE_ENABLE_CURRENT_ACQUISITION=1"], [[], *[[str(i)] for i in range(1,6)],
             *[[f'a{i}'] for i in range(1,7)], *[[f'c{i}'] for i in range(1,6)], *[[f'i{i}'] for i in range(1,6)]]),
    ]
    for name, source, test, flags, arguments in cases:
        binary = OUT / (name + (".exe" if os.name == "nt" else ""))
        run([*common, *flags, *([ROOT / source] if name != "encoder-numeric" else []), HERE / test, "-o", binary, "-lm"])
        for args in arguments:
            run([binary, *args])
    from sanitizers import run_sanitized
    run_sanitized(ROOT, OUT, [*common[1:], HERE / "test_encoder_numeric.c", "-lm"], [["product"], ["offset"], ["reset"], ["all"]])
    print("31 targeted host scenarios passed; HAL and RTOS are mocked.")


if __name__ == "__main__":
    main()
