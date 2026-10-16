"""Actual dedicated board protocol, two independently clocked C instances. No hardware."""
from pathlib import Path
import json, os, subprocess
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'build/old-hw-lot3-tests'
CASES=('boot','hello-not-healthy','targets','partial','corrupt','duplicate','order','reset','disconnect','oneway',
       'stop-master','stop-slave','inflight','replay','wrap-time','wrap-sequence','wrap-epoch',
       'wrap-session','saturation','ack-timeout','feedback','invalid-feedback','thumb-fault',
       'forbidden','version-role','version','old-version','deadline','critical-full','invalid-id','unexpected-hello',
       'malformed-stop','truncated','high-load','baud-budget','application','application-duplicate',
       'application-overflow','application-stop','application-stale','application-deadline')
def main():
    OUT.mkdir(parents=True,exist_ok=True); records=[]
    def run(command):
        r=subprocess.run(list(map(str,command)),cwd=ROOT,capture_output=True,text=True)
        records.append(dict(command=list(map(str,command)),stdout=r.stdout,stderr=r.stderr,
                            exit_code=r.returncode,exit_hex=f'0x{r.returncode&0xffffffff:08X}'))
        (OUT/'results.json').write_text(json.dumps(records,indent=2))
        print(r.stdout,end=''); print(r.stderr,end='')
        assert r.returncode==0, records[-1]
    binary=OUT/('dual-board.exe' if os.name=='nt' else 'dual-board')
    run([os.environ.get('HOST_CC','gcc'),'-std=c11','-Wall','-Wextra','-Werror','-Ifw/comm',
         'fw/comm/board_protocol.c','tests/old_hw/test_board_protocol.c','-o',binary])
    for case in CASES: run([binary,case])
    routing=OUT/('routing.exe' if os.name=='nt' else 'routing')
    run([os.environ.get('HOST_CC','gcc'),'-std=c11','-Wall','-Wextra','-Werror',
         '-DN_PULSE_OLD_HW=1','-DN_PULSE_DUAL_BOARD_LINK=1',
         *[part for d in ('tests/old_hw/mocks','Core/Inc','fw/comm','fw/app',
                         'fw/motors/motor_control','fw/motors/motor_l298n','fw/motors/motor_map','fw/comm-stack')
           for part in ('-I',str(ROOT/d))],
         'tests/old_hw/test_board_routing.c','fw/comm/comms_session.c','fw/app/intent_router.c',
         'fw/motors/motor_map/motor_map.c','-o',routing,'-lm'])
    run([routing])
    transport=OUT/('transport.exe' if os.name=='nt' else 'transport')
    run([os.environ.get('HOST_CC','gcc'),'-std=c11','-Wall','-Wextra','-Werror',
         '-DN_PULSE_OLD_HW=1','-DN_PULSE_DUAL_BOARD_LINK=1',
         *[part for d in ('tests/old_hw/dual_mocks','tests/old_hw/mocks','Core/Inc','fw/comm','fw/app',
                         'fw/bsp/encoder','fw/motors/motor_control','fw/motors/motor_l298n','fw/comm-stack')
           for part in ('-I',str(ROOT/d))],
         'tests/old_hw/test_board_transport.c','fw/comm/board_protocol.c','fw/comm/board_commission.c','-o',transport,'-lm'])
    for case in ('ownership','rx-overflow','rx-error','error-pending-rx','rx-rearm','tx-start','tx-timeout','abort-failed','rng-failed'):
        run([transport,case])
    flags=['-std=c11','-Wall','-Wextra','-Werror','-shared','-DN_PULSE_OLD_HW=1',
           '-DN_PULSE_HW_SINGLE_MOTOR_TEST=1','-DN_PULSE_OLD_HW_MOTION_OPT_IN=1',
           '-DN_PULSE_THUMB_EXTI_MOTION_QUALIFIED=1','-finstrument-functions']
    for directory in ('tests/old_hw/mocks','Core/Inc','fw/motors/motor_control',
                      'fw/motors/motor_l298n','fw/app','fw/bsp/encoder','fw/bsp/adc_dma',
                      'fw/comm','fw/comm-stack','fw/motors/estimation'):
        flags+=['-I',str(ROOT/directory)]
    sources=['Core/Src/old_hw.c','Core/Src/fatal_stop.c','fw/comm/board_link.c',
             'fw/comm/board_protocol.c','fw/comm/comms_session.c','fw/app/hw_commission.c',
             'fw/bsp/encoder/encoder_bsp.c','fw/bsp/encoder/thumb_encoder.c',
             'fw/bsp/encoder/quadrature_decoder.c','fw/motors/estimation/motor_kalman.c',
             'fw/motors/estimation/motor_estimation.c','tests/old_hw/test_board_controller.c']
    sources+=['fw/motors/motor_control/'+name+'.c' for name in
              ('motor_control','motor_safety','motor_guard','motor_feedback',
               'motor_backend_hw','motor_pid','motor_pid_config')]
    for motor in (0,1,4):
        run([os.environ.get('HOST_CC','gcc'),*flags,f'-DN_PULSE_HW_TEST_MOTOR={motor}',
             *sources,'-o',OUT/f'controller-{motor}.dll','-lm'])
    harness=OUT/('two-controllers.exe' if os.name=='nt' else 'two-controllers')
    run([os.environ.get('HOST_CC','gcc'),'-std=c11','-Wall','-Wextra','-Werror',
         ROOT/'tests/old_hw/test_two_controllers.c','-o',harness,*([] if os.name=='nt' else ['-ldl'])])
    run([harness,OUT/'controller-4.dll',OUT/'controller-1.dll','0'])
    run([harness,OUT/'controller-4.dll',OUT/'controller-0.dll','1'])
    print(f'{len(CASES)} protocol + 1 host routing + 9 transport + 2 real-controller scenarios passed; all Windows exits recorded, no hardware.')
if __name__=='__main__': main()
