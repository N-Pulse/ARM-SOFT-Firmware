"""Compile the actual HW lifecycle with the existing fake board/RTOS fixture."""
from pathlib import Path
import os
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'build/lifecycle-tests'
def main():
    OUT.mkdir(parents=True,exist_ok=True)
    cc=os.environ.get('HOST_CC','clang'); cxx=os.environ.get('HOST_CXX','clang++')
    inc=['tests/comms/mocks','tests/safety/mocks','fw/motors/motor_control','fw/comm',
         'fw/comm-stack','fw/app','fw/motors/estimation','fw/bsp/encoder','fw/motors/motor_map','Middlewares/Third_Party/EmbeddedProto/src']
    commissioning='--commissioning' in sys.argv
    if commissioning: OUT_MODE=OUT/'commissioning'
    else: OUT_MODE=OUT
    OUT_MODE.mkdir(parents=True,exist_ok=True)
    flags=['-Wall','-Wextra','-Werror','-g','-O0']
    for p in inc: flags+=['-I',str(ROOT/p)]
    if commissioning: flags+=['-DN_PULSE_HW_SINGLE_MOTOR_TEST=1','-DTEST_COMMISSION_APP']
    if '--sanitize' in sys.argv: flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
    control=ROOT/'fw/motors/motor_control'; comm=ROOT/'fw/comm'
    sources=[control/(s+'.c') for s in ('motor_control','motor_safety','motor_guard','motor_feedback','drv8848','motor_outputs','motor_pid','motor_pid_config')]
    sources += [comm/(s+'.c') for s in ('comm','comms_parser','comms_session','uart4_transport','board_uart_compat','lifecycle_protocol')]
    sources += [ROOT/'fw/app/app.c',ROOT/'fw/app/intent_router.c',ROOT/'fw/motors/motor_map/motor_map.c',ROOT/'tests/lifecycle/test_lifecycle.c']
    sources += [ROOT/'fw/app/hw_commission.c', ROOT/'fw/app/hw_telemetry.c',
                ROOT/'fw/motors/estimation/motor_kalman.c', ROOT/'fw/motors/estimation/motor_estimation.c']
    objects=[]
    def run(args): subprocess.run([str(x) for x in args],cwd=ROOT,check=True)
    for i,s in enumerate(sources):
        o=OUT_MODE/f'c{i}.o';run([cc,'-std=c11','-finstrument-functions',*flags,'-c',s,'-o',o]);objects.append(o)
    proto=ROOT/'Middlewares/Third_Party/EmbeddedProto/src'
    for i,s in enumerate([ROOT/'fw/comm-stack/rx.cpp',ROOT/'fw/comm-stack/tx.cpp',*proto.glob('*.cpp')]):
        o=OUT_MODE/f'cpp{i}.o';run([cxx,'-std=c++14',*flags,'-Wno-unused-parameter','-c',s,'-o',o]);objects.append(o)
    binary=OUT_MODE/'lifecycle';run([cxx,*flags,*objects,'-o',binary,'-lm']);subprocess.run([str(binary)],cwd=ROOT,check=True,timeout=30)
if __name__=='__main__': main()
