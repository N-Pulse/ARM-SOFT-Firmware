"""Final OLD_HW realistic adapter/controller tests. No hardware or programmer."""
from pathlib import Path
import hashlib,json,os,shutil,subprocess
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'build/old-hw-lot4-tests'
def main():
    OUT.mkdir(parents=True,exist_ok=True);records=[]
    def run(command):
        r=subprocess.run(list(map(str,command)),cwd=ROOT,capture_output=True,text=True)
        records.append(dict(command=list(map(str,command)),stdout=r.stdout,stderr=r.stderr,
                            exit_code=r.returncode,exit_hex=f'0x{r.returncode&0xffffffff:08X}'))
        (OUT/'results.json').write_text(json.dumps(records,indent=2))
        print(r.stdout,end='');print(r.stderr,end='');assert r.returncode==0,records[-1]
    cc=os.environ.get('HOST_CC','gcc')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DN_PULSE_OLD_HW=1']
    for directory in ('tests/old_hw/dual_mocks','tests/old_hw/mocks','Core/Inc','fw/motors/motor_control',
                      'fw/motors/motor_l298n','fw/app','fw/bsp/encoder','fw/bsp/adc_dma','fw/comm',
                      'fw/comm-stack','fw/motors/estimation'):
        flags+=['-I',str(ROOT/directory)]
    sources=['Core/Src/old_hw.c','Core/Src/fatal_stop.c','fw/comm/board_link.c',
             'fw/comm/board_protocol.c','fw/comm/board_commission.c','fw/comm/comms_session.c','fw/app/hw_commission.c',
             'fw/bsp/encoder/encoder_bsp.c','fw/bsp/encoder/thumb_encoder.c',
             'fw/bsp/encoder/quadrature_decoder.c','fw/motors/estimation/motor_kalman.c',
             'fw/motors/estimation/motor_estimation.c']
    sources+=['fw/motors/motor_control/'+name+'.c' for name in
              ('motor_control','motor_safety','motor_guard','motor_feedback','motor_backend_hw','motor_pid','motor_pid_config')]
    for motor in (1,4):
        binary=OUT/f'local-{motor}.exe'
        run([cc,*flags,'-finstrument-functions','-DN_PULSE_HW_SINGLE_MOTOR_TEST=1',
             '-DN_PULSE_OLD_HW_MOTION_OPT_IN=1','-DN_PULSE_THUMB_EXTI_MOTION_QUALIFIED=0',
             f'-DN_PULSE_HW_TEST_MOTOR={motor}',*sources,'tests/old_hw/test_lot4_local.c','-o',binary,'-lm'])
        for case in ('recovery-repeat','recovery-race','reset-all'):run([binary,case])
    for name,motor,enabled in (('default',1,0),('thumb',0,1),('index',1,1),('middle',2,1),('ring',3,1),('little',4,1),('selectable',1,1),('diagnostic',0,1),('diagnostic-checks',0,1),('diagnostic-timed',0,1)):
        dll=OUT/f'bringup-{name}.dll'
        run([cc,*flags,'-shared','-DN_PULSE_DUAL_BOARD_LINK=1',
             f'-DN_PULSE_HW_SINGLE_MOTOR_TEST={enabled}',f'-DN_PULSE_OLD_HW_MOTION_OPT_IN={enabled}',
             f'-DN_PULSE_HW_SELECTABLE_FINGER={int(name=="selectable" or name.startswith("diagnostic"))}',
             f'-DN_PULSE_HW_FINGER_DIAGNOSTIC={int(name.startswith("diagnostic"))}',
             f'-DN_PULSE_HW_TEST_PWM_CAP={40 if name.startswith("diagnostic") else 5}',
             *[f'-DN_PULSE_DIAG_CHECK_{check}={int(name=="diagnostic-checks")}' for check in ('GUARD','TRAVEL','STEP','PREFLIGHT')],
             f'-DN_PULSE_DIAG_CHECK_ENCODER={int(name!="diagnostic-timed")}',
             '-DN_PULSE_THUMB_EXTI_MOTION_QUALIFIED=0',f'-DN_PULSE_HW_TEST_MOTOR={motor}',
             *sources,'fw/motors/motor_l298n/motor_l298n.c','tests/old_hw/test_bringup_board.c','-o',dll,'-lm'])
        for role in ('master','slave'):
            target=OUT/f'{name}-{role}.dll';shutil.copy2(dll,target)
            assert hashlib.sha256(dll.read_bytes()).digest()==hashlib.sha256(target.read_bytes()).digest()
    harness=OUT/'two-bringup.exe'
    run([cc,'-std=c11','-Wall','-Wextra','-Werror','tests/old_hw/test_two_bringup.c','-o',harness,
         *([] if os.name=='nt' else ['-ldl'])])
    cases=[('default','default',c) for c in ('default','late-start','independent-stop')]
    cases += [('index','index',c) for c in ('index','reset','encoder','oneway','overflow','host-loss')]
    cases += [('little','little','little'),('default','index','index'),('little','default','little')]
    cases += [(name,name,'routed-'+name) for name in ('thumb','index','middle','ring','little')]
    cases += [('index','index','routed-'+case) for case in
              ('host-loss','oneway','reset','wrong-motor','stale','ack-timeout','inflight-stop')]
    cases += [('selectable','selectable',case) for case in ('selectable','selectable-loss','selectable-reset','selectable-nominal','selectable-thumb-irq','selectable-thumb-invalid','selectable-stall','selectable-settle')]
    cases += [('diagnostic','diagnostic',case) for case in ('diagnostic-move','diagnostic-timeout','diagnostic-stop','diagnostic-loss')]
    cases += [('diagnostic-checks','diagnostic-checks',case) for case in ('diagnostic-check-guard','diagnostic-check-travel')]
    cases += [('diagnostic-timed','diagnostic-timed',case) for case in ('timed-move','timed-stop','timed-loss','timed-cap','timed-assume')]
    for master,slave,case in cases:run([harness,OUT/f'{master}-master.dll',OUT/f'{slave}-slave.dll',case])
    timing=OUT/'timing.exe'
    run([cc,*flags,'-DN_PULSE_TIMING_DIAGNOSTICS=1','tests/old_hw/test_timing.c',
         'fw/app/old_hw_timing.c','-o',timing]);run([timing])
    codec=OUT/'link-codec.exe';proto=ROOT/'Middlewares/Third_Party/EmbeddedProto/src'
    run([os.environ.get('HOST_CXX','g++'),'-std=c++14','-Wall','-Wextra','-Werror',
         '-Wno-attributes','-Wno-unused-parameter',
         *[part for d in ('tests/comms/mocks','fw/comm','fw/comm-stack','fw/app',
                         'fw/motors/motor_control','fw/motors/motor_l298n',str(proto))
           for part in ('-I',str(ROOT/d))],
         'tests/old_hw/test_link_codec.cpp','fw/comm-stack/tx.cpp',*proto.glob('*.cpp'),'-o',codec]);run([codec])
    import sys
    run([sys.executable,ROOT/'tests/old_hw/test_link_codec.py'])
    run([sys.executable,'-B',ROOT/'tests/old_hw/test_finger_motion.py'])
    commission=OUT/'commission-codec.exe'
    run([cc,'-std=c11','-Wall','-Wextra','-Werror','-Ifw/comm','-Ifw/app','-Ifw/motors/motor_control',
         'fw/comm/board_protocol.c','fw/comm/board_commission.c','tests/old_hw/test_commission_codec.c','-o',commission])
    run([commission])
    print(f'6 local + {len(cases)} actual dual-adapter/controller scenarios passed; identical-image copies verified; every exit recorded.')
if __name__=='__main__':main()
