"""Single-finger CLI/planner tests. No serial port or programmer opened."""
from pathlib import Path
import math
import sys
import io
import contextlib
import queue
import serial
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
import hw_monitor as h
import send_action
from finger_motion import DEFAULT_ENCODER_QUANTUM, FingerMotion, QuickFingerTest, degree_limits, raw_finger

def rejects(fn):
    try:
        fn()
    except ValueError:
        return
    raise AssertionError('invalid movement was accepted')

def diagnostic(mid=1, remote=True):
    return h.pb.MotorDiagnostic(motor_id=mid, epoch=5,
        flags=63 | (1 << 20) | ((1 << 21) if remote else 0))

def console_session(reject_target=False, universal=False, verbose=False, run=False, reject_op=None, missing_encoder=False, thumb=False, lost_ack=False, motor=1, diagnostic_mode=False, firmware_diagnostic=None, diagnostic_timeout=False, timed=False, opening=False, assume=False):
    """Exercise the actual serial loop with explicit operator input and framed ACKs."""
    clock=[100.]
    firmware_diagnostic=diagnostic_mode if firmware_diagnostic is None else firmware_diagnostic
    pwm=25 if diagnostic_mode else 3
    goal=30 if diagnostic_mode or not run else 5
    if opening: goal=0
    commands=queue.Queue(maxsize=8)
    setup=['link-recover','limits-deg 0 40 5 3 45']
    if not universal: setup.append('calibrate 1000 4 1 1 0')
    if not run:
        for text in (*setup,'preflight verified','arm','enable','move'):
            commands.put_nowait(text)
    class Port:
        in_waiting=1
        def __init__(self):
            self.d=h.pb.MotorDiagnostic(motor_id=motor,epoch=1,flags=16|(1<<16)|(1<<21)|((1<<23) if universal else 0),faults=128)
            if universal: self.d.flags |= 12  # Nominal scale/real feedback available at boot.
            if missing_encoder: self.d.flags &= ~12
            if thumb: self.d.flags |= 1<<22
            if firmware_diagnostic: self.d.flags |= 1<<9
            if timed:
                self.d.flags |= 1<<14
                self.d.flags &= ~12
                self.d.encoder_rad=math.nan
                self.d.target_rad=self.d.reference_rad=0 if assume or not opening else math.radians(30)
            self.replies=[]; self.sent=[]; self.stopped=False; self.closed=False
            self.next_diagnostic=0.; self.arm_time=None; self.target_time=None
            self.cutoff=math.inf
        def write(self, data):
            msg=h.pb.DeviceMessage(); msg.ParseFromString(data[2:]); self.sent.append(msg)
            kind=msg.WhichOneof('message')
            if kind=='stop':
                self.d.epoch+=1; self.d.faults=128
                self.stopped=any(m.WhichOneof('message')=='commission' for m in self.sent)
            elif kind=='commission':
                c=msg.commission; result=h.pb.CommandReply.OK
                if c.operation==reject_op: result=h.pb.CommandReply.REJECTED
                elif c.operation==14:
                    self.d.motor_id=c.motor_id; self.d.epoch+=1; self.d.faults=128
                    self.d.flags &= ~(1|2|(1<<20))
                elif c.operation==13: self.d.faults=0; self.d.epoch+=1
                elif c.operation==1: self.d.flags|=32
                elif c.operation==15:
                    assert assume and not self.d.flags&3
                    self.d.target_rad=self.d.reference_rad=c.target_rad
                elif c.operation==2: self.d.flags|=12
                elif c.operation==11: self.d.flags|=1<<20
                elif c.operation==5:
                    self.d.flags|=2; self.arm_time=clock[0]
                elif c.operation==6: self.d.flags|=1
                elif c.operation==7:
                    self.target_time=clock[0]
                    assert clock[0]-self.arm_time<5
                    assert c.motor_id==motor and c.pwm_percent==pwm
                    if not diagnostic_mode: assert abs(c.target_rad-self.d.encoder_rad)<math.radians(5)
                    else: assert abs(c.target_rad-math.radians(goal))<1e-6
                    if reject_target: result=h.pb.CommandReply.PARAMETERS
                    else:
                        self.d.target_rad=self.d.reference_rad=c.target_rad
                        self.d.pwm_percent=-pwm if opening else pwm
                        measured=round(c.target_rad/DEFAULT_ENCODER_QUANTUM)*DEFAULT_ENCODER_QUANTUM if universal else c.target_rad
                        if diagnostic_timeout: self.cutoff=clock[0]+2
                        elif not timed: self.d.encoder_rad=round(measured*1000)/1000
                        if diagnostic_mode and not diagnostic_timeout:
                            self.d.epoch+=1; self.d.faults=1|128; self.d.flags &= ~3
                if not (lost_ack and c.operation==1):
                    self.replies.append(h.frame(h.pb.ProsthesisMessage(reply=h.pb.CommandReply(
                        request_id=c.request_id,result=result,epoch=self.d.epoch,faults=self.d.faults))))
            return len(data)
        def read(self, n):
            if self.stopped: raise KeyboardInterrupt
            clock[0]+=.02
            if clock[0]>=self.cutoff:
                self.cutoff=math.inf; self.d.epoch+=1; self.d.faults=1|128; self.d.flags &= ~3; self.d.pwm_percent=0
            data=b''.join(self.replies)
            if not run or clock[0]>=self.next_diagnostic:
                data+=h.frame(h.pb.ProsthesisMessage(diagnostic=self.d))
                self.next_diagnostic=clock[0]+.2  # Real console feedback cadence.
            self.replies.clear(); return data
        def close(self): self.closed=True
    port=Port(); output=io.StringIO()
    with patch.object(serial,'Serial',return_value=port), patch.object(h.threading,'Thread'), \
         patch.object(h.queue,'Queue',return_value=commands), patch.object(h.time,'monotonic',side_effect=lambda:clock[0]), \
         patch.object(h.time,'time',return_value=100.), contextlib.redirect_stdout(output):
        failed=reject_target or reject_op is not None or (missing_encoder and not timed) or thumb or lost_ack or (run and not universal) or diagnostic_timeout or (run and diagnostic_mode!=firmware_diagnostic)
        result=h.main(['--port','FAKE','--finger',('thumb' if motor==0 else 'index'),'--angle',str(goal),'--pwm',str(pwm)]+
                      (['--verbose'] if verbose else [])+(['--run'] if run else [])+
                      (['--diagnostic','--max-angle','30'] if diagnostic_mode else [])+
                      (['--from-angle','30'] if assume else []))
        assert result==(int(failed) if run else 0),output.getvalue()
    operations=[m.commission.operation for m in port.sent if m.WhichOneof('message')=='commission']
    prefix=([14,13,1] if universal else [13,1,2])+[11,5,6]
    if assume: prefix=[14,13,1,15,11,5,6]
    if universal: assert 2 not in operations  # No calibration command is hidden in the host.
    assert port.sent[0].WhichOneof('message')=='stop'
    assert port.sent[-1].WhichOneof('message')=='stop' and port.closed
    if diagnostic_timeout:
        assert operations.count(7)==1 and operations.count(5)==1 and operations.count(13)==1
        assert 'Movement cancelled' in output.getvalue(); return
    if run and failed and not reject_target:
        assert 7 not in operations
        assert operations.count(5)<=1 and operations.count(13)<=1
        if missing_encoder or thumb: assert 5 not in operations
        if reject_op is not None: assert operations[-1]==reject_op
        if lost_ack: assert operations[-1]==1 and 'acknowledgement timeout' in output.getvalue()
        return
    assert operations[:len(prefix)]==prefix and operations.count(5)==1 and operations.count(6)==1
    assert port.stopped and port.closed
    lines=output.getvalue().splitlines()
    samples=[line for line in lines if line.startswith('t=')]
    if not verbose: assert len(samples)<10
    else: assert len(samples)>10
    assert ('Common -Fingers firmware detected' if universal else 'Fixed-finger firmware detected') in output.getvalue()
    if reject_target:
        assert operations.count(7)==1 and 'Movement rejected' in output.getvalue()
    else:
        assert operations.count(7)==1 if diagnostic_mode else operations.count(7)>1
        assert ('Timed movement finished' if timed else 'Requested angle reached') in output.getvalue()
        assert abs((port.d.reference_rad if timed else port.d.encoder_rad)-math.radians(goal))<.005
        if run: assert port.target_time-port.arm_time<1
        if run: assert 'Movement: encoder_deg=' in output.getvalue() and f'PWM_applied={-pwm if opening else pwm}%' in output.getvalue()

def main():
    link=h.pb.LinkDiagnostic(flags=(8<<8)|(1<<16))
    assert 'last_transport=RX_ERROR last_uart_error=OVERRUN' in h.describe_link(link)
    for name, mid in h.FINGER_IDS.items():
        assert raw_finger(f'raw {10+mid} 12.5 2')==(mid,12.5,2)
        assert raw_finger(f'raw {20+mid}')==(mid,0.,3)
        for raw, default in ((10 + mid, 30), (20 + mid, 0)):
            with patch.object(h, 'main', return_value=0) as monitor:
                assert send_action.main(['--port', 'COM99', '--raw', str(raw)]) == 0
                args = monitor.call_args.args[0]
                assert args[args.index('--finger') + 1] == name
                assert float(args[args.index('--angle') + 1]) == default
        with patch.object(h, 'main', return_value=0) as monitor:
            send_action.main(['--finger', name, '--angle', '12.5', '--pwm', '2'])
            args = monitor.call_args.args[0]
            assert args[args.index('--angle') + 1] == '12.5'
            assert args[args.index('--pwm') + 1] == '2'
    text = degree_limits('limits-deg 0 40 5 3 45')
    values = text.split()
    assert math.isclose(float(values[2]), math.radians(40))
    request = h.pb.DeviceMessage()
    request.ParseFromString(h.command_frame(text, 1, 5, 10)[2:])
    assert request.commission.operation == h.pb.CommissionCommand.LIMITS
    assert abs(request.commission.maximum_step_rad - math.radians(5)) < 1e-7
    rejects(lambda: degree_limits('limits-deg 0 40 nan 3 45'))
    rejects(lambda: degree_limits('limits-deg 0 40 5 1.5 45'))
    for angle,pwm,maximum in ((None,3,10),(11,3,10),(5,6,10),(5,3,0),(5,3,math.nan)):
        rejects(lambda: QuickFingerTest(angle,pwm,maximum))
    with patch.object(h,'main',return_value=0) as monitor:
        send_action.main(['--port','COM8','--raw','11','--angle','5','--run'])
        assert '--run' in monitor.call_args.args[0]
    with patch.object(h,'main',return_value=0) as monitor:
        send_action.main(['--port','COM8','--raw','20','--angle','0','--max-angle','30','--pwm','25','--run','--diagnostic','--from-angle','30'])
        args=monitor.call_args.args[0]
        assert args[args.index('--finger')+1]=='thumb' and args[args.index('--from-angle')+1]=='30.0'
    for remote in (False, True):
        for start, goal in ((0, 30), (30, 0)):
            d = diagnostic(remote=remote)
            d.encoder_rad = d.reference_rad = d.target_rad = math.radians(start)
            plan = FingerMotion(1)
            rejects(lambda: plan.start(goal, 3, d))  # No invented limits.
            plan.accept_limits(text)
            plan.start(goal, 3, d)
            targets = []
            for _ in range(100):
                target = plan.next_target(d)
                if plan.completed:
                    break
                assert target is not None
                assert abs(target - d.encoder_rad) < math.radians(5)
                assert abs(target - d.reference_rad) < math.radians(5)
                frame = h.command_frame(f'target {target:.9g} 3', 1, 5, len(targets)+20)
                msg = h.pb.DeviceMessage(); msg.ParseFromString(frame[2:])
                assert msg.commission.motor_id == 1 and msg.commission.operation == h.pb.CommissionCommand.TARGET
                targets.append(target)
                assert plan.next_target(d) is None  # No next target before feedback converges.
                position = round(target*1000)/1000 if remote else target
                d.target_rad = d.reference_rad = d.encoder_rad = position
            assert plan.completed and len(targets) > 1
            assert abs(d.encoder_rad - math.radians(goal)) <= plan.tolerance
            assert plan.next_target(d) is None
    # Real nominal encoder counts cannot exactly match arbitrary angle targets.
    for remote in (False,True):
        for start,goal in ((0,30),(30,0)):
            d=diagnostic(remote=remote); d.flags |= 1<<23
            measured=round(math.radians(start)/DEFAULT_ENCODER_QUANTUM)*DEFAULT_ENCODER_QUANTUM
            d.encoder_rad=round(measured*1000)/1000 if remote else measured
            d.target_rad=d.reference_rad=d.encoder_rad
            plan=FingerMotion(1); plan.accept_limits(text); plan.start(goal,3,d)
            for _ in range(150):
                target=plan.next_target(d)
                if plan.completed: break
                assert target is not None
                d.target_rad=d.reference_rad=round(target*1000)/1000 if remote else target
                measured=round(target/DEFAULT_ENCODER_QUANTUM)*DEFAULT_ENCODER_QUANTUM
                d.encoder_rad=round(measured*1000)/1000 if remote else measured
            assert plan.completed
            assert abs(d.encoder_rad-math.radians(goal))<=plan.tolerance
    plan = FingerMotion(1); plan.accept_limits(text)
    for angle, pwm in ((math.nan,3), (math.inf,3), (-1,3), (91,3), (41,3), (1,4), (1,0)):
        rejects(lambda: plan.start(angle, pwm, diagnostic()))
    for flag in (1,2,4,8,16,32,1<<20):
        d=diagnostic(); d.flags &= ~flag
        rejects(lambda: plan.start(1,3,d))
    for flag in (1<<18,1<<22):
        d=diagnostic(); d.flags |= flag
        rejects(lambda: plan.start(1,3,d))
    rejects(lambda: plan.start(1,3,diagnostic(mid=2)))
    d=diagnostic(); plan.start(1,3,d); d.epoch+=1
    rejects(lambda: plan.next_target(d))
    plan.cancel(); assert not plan.active and plan.next_target(d) is None
    d=diagnostic(); plan.start(1,3,d); d.faults=128
    rejects(lambda: plan.next_target(d))
    console_session(); console_session(reject_target=True); console_session(universal=True); console_session(universal=True,verbose=True)
    console_session(universal=True,run=True)
    console_session(universal=True,run=True,motor=0)  # New common image reports THUMB bench permission.
    console_session(universal=True,run=True,reject_target=True)
    for op in (14,13,1,11,5,6): console_session(universal=True,run=True,reject_op=op)
    console_session(universal=True,run=True,missing_encoder=True)
    console_session(universal=True,run=True,thumb=True)
    console_session(universal=True,run=True,lost_ack=True)
    console_session(run=True)  # Old fixed profile must never auto-arm.
    console_session(universal=True,run=True,motor=0,diagnostic_mode=True)
    console_session(universal=True,run=True,motor=0,diagnostic_mode=True,diagnostic_timeout=True)
    console_session(universal=True,run=True,diagnostic_mode=True,firmware_diagnostic=False)
    console_session(universal=True,run=True,firmware_diagnostic=True)
    console_session(universal=True,run=True,motor=0,diagnostic_mode=True,timed=True)
    console_session(universal=True,run=True,motor=0,diagnostic_mode=True,timed=True,opening=True)
    console_session(universal=True,run=True,motor=0,diagnostic_mode=True,timed=True,opening=True,assume=True)
    for value in (math.nan,-1,91): rejects(lambda: QuickFingerTest(0,25,30,True,value))
    rejects(lambda: QuickFingerTest(0,3,30,False,30))
    d=diagnostic(mid=0); d.flags=(d.flags|1<<9|1<<14)&~12; d.encoder_rad=math.nan
    timed=h.DiagnosticFingerMotion(0); timed.accept_limits(degree_limits('limits-deg 0 30 5 25 45'))
    timed.start(30,25,d); assert abs(timed.next_target(d)-math.radians(30))<1e-6
    d.reference_rad=math.radians(15); d.faults=1|128; d.flags &= ~3; d.epoch+=1
    rejects(lambda: timed.next_target(d))  # STOP halfway never reports arrival.
    print('PASS finger CLI aliases/degrees, measured/reference-paced targets in both directions, limits, wrong motor, STOP epoch, fault and readiness gates; no hardware')

if __name__ == '__main__':
    main()
