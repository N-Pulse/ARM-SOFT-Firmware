"""Production register and driver tests. No board, UART or programmer involved."""
from pathlib import Path
import os
import subprocess

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
OUT = ROOT / 'build/output-tests'
CTRL = ROOT / 'fw/motors/motor_control'

def run(args):
    subprocess.run([str(a) for a in args], cwd=ROOT, check=True)

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    cc = os.environ.get('HOST_CC', 'gcc')
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-g', '-O0']
    includes = [HERE/'mocks', ROOT/'tests/safety/mocks', ROOT/'Core/Inc', CTRL,
                ROOT/'fw/bsp/encoder', ROOT/'fw/comm', ROOT/'fw/comm-stack', ROOT/'fw/app']
    for p in includes: flags += ['-I', p]
    suffix = '.exe' if os.name=='nt' else ''
    binary = OUT / ('fatal'+suffix)
    run([cc,*flags,ROOT/'Core/Src/fatal_stop.c',HERE/'test_fatal.c','-o',binary])
    run([binary])
    binary = OUT / ('assert'+suffix)
    run([cc,*flags,HERE/'test_assert.c','-o',binary])
    run([binary])
    sources = [CTRL/(s+'.c') for s in ('motor_control','motor_safety','motor_guard',
        'motor_feedback','drv8848','motor_outputs','motor_pid','motor_pid_config')]
    sources += [ROOT/'fw/comm/comms_session.c', HERE/'test_pwm.c']
    binary = OUT / ('pwm'+suffix)
    run([cc,*flags,'-finstrument-functions',*sources,'-o',binary,'-lm'])
    run([binary])
    run(['python',HERE/'test_flash.py'])

if __name__ == '__main__': main()
