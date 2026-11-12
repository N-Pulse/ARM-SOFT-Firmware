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
         'fw/comm-stack','fw/app','fw/motors/estimation','fw/bsp/encoder','fw/bsp/force_sensor','fw/bsp/battery','fw/bsp/status_leds','fw/motors/motor_map','Middlewares/Third_Party/EmbeddedProto/src']
    modes={
        '--commissioning':('commissioning','test_commission_app.c'),
        '--handshake':('handshake','test_handshake.c'),
        '--haptics':('haptics','test_haptics.c'),
        '--grip':('grip','test_grip.c'),
        '--leds':('leds','test_status_leds.c'),
        '--battery':('battery','test_battery.c'),
        '--battery-defaults':('battery-defaults','test_battery.c'),
        '--boot-safety':('boot-safety','test_boot_safety.c'),
        '--bad-force-config':('bad-force-config','test_bad_force_config.c'),
    }
    selected=[flag for flag in modes if flag in sys.argv]
    if len(selected)>1: raise SystemExit('Choose one test mode per invocation')
    mode=selected[0] if selected else None
    folder,test=modes[mode] if mode else ('','test_streaming.c')
    commissioning=mode=='--commissioning'
    OUT_MODE=OUT/folder
    OUT_MODE.mkdir(parents=True,exist_ok=True)
    flags=['-Wall','-Wextra','-Werror','-g','-O0']
    for p in inc: flags+=['-I',str(ROOT/p)]
    if commissioning: flags+=['-DN_PULSE_HW_SINGLE_MOTOR_TEST=1','-DTEST_COMMISSION_APP']
    if mode in ('--battery','--leds'):
        flags+=['-DTEST_BATTERY_CONFIGURED','-DBATTERY_LOW_WARNING_V=13.2f','-DBATTERY_WARNING_CLEAR_V=13.6f','-DBATTERY_ADC_VREF_V=3.3f','-DBATTERY_DIVIDER_RATIO=6.0f']
    if mode=='--bad-force-config': flags+=['-DMOTOR_FORCE_HARD_LIMIT_V=NAN']
    if '--sanitize' in sys.argv: flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
    control=ROOT/'fw/motors/motor_control'; comm=ROOT/'fw/comm'
    sources=[control/(s+'.c') for s in ('motor_control','motor_safety','motor_guard','motor_feedback','drv8848','motor_outputs','motor_pid','motor_pid_config')]
    sources += [comm/(s+'.c') for s in ('comm','comms_parser','data_protocol','handshake_protocol','haptics','comms_session','uart4_transport','board_uart_compat','lifecycle_protocol')]
    if not commissioning:
        flags+=['-DHANDSHAKE_STATUS_LEN=2','-DHANDSHAKE_DATA_LEN=6']
    sources += [ROOT/'fw/app/app.c',ROOT/'fw/app/intent_router.c',ROOT/'fw/motors/motor_map/motor_map.c',ROOT/'tests/lifecycle'/test]
    if mode=='--boot-safety': sources += [ROOT/'fw/app/boot_homing.c']
    sources += [ROOT/'fw/bsp/force_sensor/force_sensor.c',ROOT/'fw/bsp/battery/battery.c',ROOT/'fw/bsp/status_leds/status_leds.c']
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
