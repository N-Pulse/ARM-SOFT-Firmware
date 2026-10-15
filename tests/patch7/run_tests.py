"""Focused Patch 7 native tests; no serial devices or physical hardware."""
from pathlib import Path
import os
import subprocess
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'build/patch7-tests'
def run(args): subprocess.run([str(a) for a in args],cwd=ROOT,check=True)
def main():
    OUT.mkdir(parents=True,exist_ok=True)
    ctrl=ROOT/'fw/motors/motor_control'
    flags=['-std=c11','-Wall','-Wextra','-Werror','-g','-O0','-finstrument-functions']
    for p in ('tests/safety/mocks','fw/motors/motor_control','fw/bsp/encoder','fw/comm','fw/comm-stack','fw/app','fw/motors/estimation'):
        flags+=['-I',ROOT/p]
    sources=[ctrl/(n+'.c') for n in ('motor_control','motor_safety','motor_guard','motor_feedback',
        'drv8848','motor_outputs','motor_pid','motor_pid_config','motor_backend_hw')]
    sources += [ROOT/'fw/comm/comms_session.c',ROOT/'fw/app/hw_commission.c',ROOT/'tests/safety/test_commission.c']
    sources += [ROOT/'fw/motors/estimation/motor_kalman.c',ROOT/'fw/motors/estimation/motor_estimation.c']
    binary=OUT/('commission.exe' if os.name=='nt' else 'commission')
    run([os.environ.get('HOST_CC','gcc'),*flags,'-DTEST_HW_BACKEND','-DN_PULSE_HW_SINGLE_MOTOR_TEST=1',
         *sources,'-o',binary,'-lm']); run([binary])
    binary=OUT/('kalman.exe' if os.name=='nt' else 'kalman')
    run([os.environ.get('HOST_CC','gcc'),'-std=c11','-Wall','-Wextra','-Werror',
         '-I',ROOT/'fw/motors/estimation',ROOT/'fw/motors/estimation/motor_kalman.c',
         ROOT/'tests/patch7/test_kalman.c','-o',binary,'-lm']); run([binary])
if __name__=='__main__': main()
