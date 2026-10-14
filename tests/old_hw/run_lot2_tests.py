"""Lot 2 production driver/PID/BSP and software quadrature tests, without hardware."""
from pathlib import Path
import json
import os
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'build/old-hw-lot2-tests'
results = []

def run(command):
    command = [str(x) for x in command]
    result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True)
    results.append({'command': command, 'exit_code': result.returncode,
                    'exit_hex': f'0x{result.returncode & 0xffffffff:08X}',
                    'stdout': result.stdout, 'stderr': result.stderr})
    (OUT / 'results.json').write_text(json.dumps(results, indent=2), encoding='utf-8')
    if result.returncode:
        print(result.stdout, result.stderr)
        raise RuntimeError(f'Failed executable/compiler exit={result.returncode} ({results[-1]["exit_hex"]})')
    if len(command) <= 3:
        print(result.stdout, end='')

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    cc = os.environ.get('HOST_CC', 'gcc')
    suffix = '.exe' if os.name == 'nt' else ''
    base = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-g', '-O0']
    binary = OUT / ('quadrature' + suffix)
    run([cc, *base, '-I', ROOT/'fw/bsp/encoder', ROOT/'fw/bsp/encoder/quadrature_decoder.c',
         ROOT/'tests/old_hw/test_quadrature.c', '-o', binary])
    run([binary])
    protocol = ROOT/'Middlewares/Third_Party/EmbeddedProto/src'
    binary = OUT / ('preflight-protocol' + suffix)
    run([os.environ.get('HOST_CXX','g++'), '-std=c++14', '-Wall', '-Wextra', '-Werror',
         '-Wno-attributes', '-Wno-unused-parameter',
         *[part for directory in ('fw/comm-stack','fw/app','fw/motors/motor_control',str(protocol))
           for part in ('-I',ROOT/directory)],
         ROOT/'fw/comm-stack/rx.cpp', *protocol.glob('*.cpp'), ROOT/'tests/old_hw/test_preflight_protocol.cpp',
         '-o',binary])
    run([binary])
    for directory in ('tests/old_hw/mocks', 'Core/Inc', 'fw/motors/motor_control',
                      'fw/motors/motor_l298n', 'fw/app', 'fw/bsp/encoder', 'fw/bsp/adc_dma',
                      'fw/comm', 'fw/comm-stack', 'fw/motors/estimation'):
        base += ['-I', ROOT / directory]
    base += ['-DN_PULSE_OLD_HW=1', '-DN_PULSE_HW_SINGLE_MOTOR_TEST=1',
             '-DN_PULSE_OLD_HW_MOTION_OPT_IN=1', '-finstrument-functions']
    sources = ['Core/Src/old_hw.c', 'Core/Src/fatal_stop.c', 'fw/comm/board_link.c',
               'fw/comm/comms_session.c', 'fw/app/hw_commission.c', 'fw/bsp/encoder/encoder_bsp.c',
               'fw/bsp/encoder/thumb_encoder.c', 'fw/bsp/encoder/quadrature_decoder.c',
               'fw/motors/estimation/motor_kalman.c', 'fw/motors/estimation/motor_estimation.c',
               'tests/old_hw/test_l298n.c']
    sources += ['fw/motors/motor_control/'+name+'.c' for name in
                ('motor_control','motor_safety','motor_guard','motor_feedback',
                 'motor_backend_hw','motor_pid','motor_pid_config')]
    cases = ('pid','reverse','arm-stop','pid-stop','concurrent','stale','encoder-fault',
             'register-fault','stall','nan','deadline','wrap','reinit')
    scenarios = 2
    for motor in (0, 1, 2, 3, 4):
        binary = OUT / (f'l298n-{motor}' + suffix)
        run([cc, *base, '-DN_PULSE_THUMB_EXTI_MOTION_QUALIFIED=1',
             f'-DN_PULSE_HW_TEST_MOTOR={motor}', *sources, '-o', binary, '-lm'])
        chosen = ('registers', 'thumb', 'thumb-init-fault', 'thumb-irq-fault') if motor == 0 else ('registers',) if motor in (2,3) else ('registers', *cases)
        for case in chosen:
            run([binary, case])
            scenarios += 1
    binary = OUT / ('thumb-unqualified' + suffix)
    run([cc, *base, '-DN_PULSE_THUMB_EXTI_MOTION_QUALIFIED=0', '-DN_PULSE_HW_TEST_MOTOR=0',
         *sources, '-o', binary, '-lm'])
    run([binary, 'thumb-unqualified']); scenarios += 1
    print(f'{scenarios} lot-2 scenarios passed, every exit=0; results.json records command/output/Windows exit code.')

if __name__ == '__main__':
    main()
