"""Run production OLD_HW code on a register/HAL model; no hardware access."""
from pathlib import Path
import os
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'build/old-hw-tests'

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    binary = OUT / ('old-hw.exe' if os.name == 'nt' else 'old-hw')
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-DN_PULSE_OLD_HW=1']
    for directory in ('tests/old_hw/mocks', 'Core/Inc', 'fw/motors/motor_control', 'fw/motors/motor_l298n', 'fw/app', 'fw/bsp/encoder', 'fw/comm'):
        flags += ['-I', str(ROOT / directory)]
    sources = ['Core/Src/old_hw.c', 'Core/Src/fatal_stop.c', 'fw/comm/board_link.c',
               'fw/bsp/encoder/encoder_bsp.c', 'fw/bsp/encoder/thumb_encoder.c', 'fw/bsp/encoder/quadrature_decoder.c', 'fw/motors/motor_l298n/motor_l298n.c',
               'tests/old_hw/test_old_hw.c']
    subprocess.run([os.environ.get('HOST_CC', 'gcc'), *flags, *sources, '-o', str(binary), '-lm'], cwd=ROOT, check=True)
    # All fallible initialization/start operations in each role, one per process.
    scenarios = 0
    for role in (0, 1):
        result = subprocess.run([str(binary), str(role)], check=True, capture_output=True, text=True)
        print(result.stdout, end='')
        calls = int(re.search(r'\((\d+) HAL calls\)', result.stdout)[1])
        scenarios += 1 + calls
        for failure in range(1, calls + 1):
            subprocess.run([str(binary), str(role), str(failure)], check=True)
    print(f'{scenarios} OLD_HW host scenarios passed; register/HAL model only.')

if __name__ == '__main__':
    main()
