#!/usr/bin/env python3
"""Framed UART4 HW monitor and explicit commissioning console (PowerShell).
Requires protobuf and pyserial. Importing this module opens no port.
"""
import argparse
import math
from pathlib import Path
import queue
import shlex
import sys
import threading
import time
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'fw/comm-stack/PyUART'))
import protocol_messages_pb2 as pb
from uart4_hw_client import frame, hello_frame
from finger_motion import FINGER_IDS, FingerMotion, DiagnosticFingerMotion, QuickFingerTest, degree_limits, raw_finger

HELP='status [ID] | raw [ID] | raw XX [ANGLE_DEG [PWM]] (select/stage finger) | link | limits MIN MAX STEP PWM RATE | limits-deg MIN MAX STEP PWM RATE | calibrate CPR QUAD RATIO DIR OFFSET | zero | recover | preflight verified | link-recover | link-authorize | arm | enable | target RAD PWM | angle DEG PWM | move | disable | estimator-reset verified | estimator-stop | stop | quit'
FAULTS=('MANUAL','GUARD','FEEDBACK','CONTROL_DELAY','FORCE','DRIVER','PID','LINK')
ADC=('INVALID','RAW_VALID','CURRENT_CALIBRATED','STALE','ERROR','SATURATED')
def number(x): return f'{x:.6g}' if math.isfinite(x) else 'unavailable'
def faults(bits): return '|'.join(n for i,n in enumerate(FAULTS) if bits&(1<<i)) or 'none'
def stop_frame():
    msg=pb.DeviceMessage(); msg.stop.SetInParent(); return frame(msg)
def diagnostic_frame(mid):
    msg=pb.DeviceMessage(); msg.diagnostic.motor_id=mid; return frame(msg)
def command_frame(text,mid,epoch,request):
    words=shlex.split(text)
    if not words: raise ValueError(HELP)
    command,*args=words
    if command in ('status','raw') and len(args)<=1:
        selected=int(args[0]) if args else mid
        if selected not in range(8): raise ValueError('motor ID must be 0..7')
        return diagnostic_frame(selected)
    if command=='link' and not args: return diagnostic_frame(8)
    if command=='stop' and not args: return stop_frame()
    if epoch is None: raise ValueError('wait for a recent diagnostic epoch')
    msg=pb.DeviceMessage(); c=msg.commission
    c.motor_id=mid; c.epoch=epoch; c.request_id=request
    counts={'select':0,'estimate':1,'limits':5,'calibrate':5,'zero':0,'recover':0,'arm':0,'enable':0,'target':2,'disable':0,'estimator-reset':1,'estimator-stop':0,'preflight':1,'link-recover':0,'link-authorize':0}
    if command not in counts: raise ValueError(f'unknown command {command!r}; type help for commands')
    if len(args)!=counts[command]: raise ValueError(f'{command} expects {counts[command]} arguments; type help')
    c.operation={'select':pb.CommissionCommand.SELECT_FINGER,'estimate':pb.CommissionCommand.DIAGNOSTIC_POSITION,'limits':pb.CommissionCommand.LIMITS,'calibrate':pb.CommissionCommand.CALIBRATE,
        'zero':pb.CommissionCommand.ZERO,'recover':pb.CommissionCommand.RECOVER,'arm':pb.CommissionCommand.ARM,
        'enable':pb.CommissionCommand.ENABLE,'target':pb.CommissionCommand.TARGET,'disable':pb.CommissionCommand.DISABLE,
        'estimator-reset':pb.CommissionCommand.ESTIMATOR_RESET,'estimator-stop':pb.CommissionCommand.ESTIMATOR_STOP,
        'preflight':pb.CommissionCommand.OUTPUT_PREFLIGHT,'link-recover':pb.CommissionCommand.LINK_RECOVER,
        'link-authorize':pb.CommissionCommand.LINK_AUTHORIZE}[command]
    if command=='limits':
        c.minimum_rad,c.maximum_rad,c.maximum_step_rad=map(float,args[:3])
        c.pwm_percent=int(args[3]); c.reference_rate_rad_s=float(args[4])
    elif command=='calibrate':
        c.cpr=int(args[0]); c.quadrature=int(args[1]); c.gear_ratio=float(args[2]); c.direction=int(args[3]); c.zero_offset=int(args[4])
    elif command=='target': c.target_rad=float(args[0]); c.pwm_percent=int(args[1])
    elif command=='estimate': c.target_rad=float(args[0])
    elif command in ('estimator-reset','preflight'):
        if args[0]!='verified': raise ValueError('explicit verified reference/electrical preflight declaration required')
        c.stationary_reference_verified=True
    for field in ('gear_ratio','minimum_rad','maximum_rad','maximum_step_rad','reference_rate_rad_s','target_rad'):
        if not math.isfinite(getattr(c,field)): raise ValueError('finite parameters required')
    return frame(msg)

class Decoder:
    def __init__(self): self.buffer=bytearray(); self.since=None
    def feed(self,data,now):
        if self.since is not None and now-self.since>.15: self.buffer.clear(); self.since=None
        self.buffer.extend(data)
        messages=[]
        while self.buffer:
            if self.buffer[0]!=0xaa: del self.buffer[0]; self.since=None; continue
            if self.since is None: self.since=now
            if len(self.buffer)<2: break
            n=self.buffer[1]
            if not 1<=n<=128: del self.buffer[0]; self.since=None; continue
            if len(self.buffer)<n+2: break
            payload=bytes(self.buffer[2:n+2]); del self.buffer[:n+2]; self.since=None
            # Responses 3/4 are unambiguous; legacy firmware Hello is field1.
            message=pb.ProsthesisMessage()
            try:
                message.ParseFromString(payload)
                if message.WhichOneof('message') in ('diagnostic','reply','link'): messages.append(message)
            except Exception as exc: print(f'malformed protobuf: {exc}')
        if len(self.buffer)>130: self.buffer.clear(); self.since=None
        return messages

def describe(d):
    checks='|'.join(name for bit,name in ((10,'GUARD'),(11,'TRAVEL'),(12,'STEP'),(13,'PREFLIGHT')) if d.flags&(1<<bit)) or 'none'
    if d.flags&(1<<9) and not d.flags&(1<<14): checks=('ENCODER' if checks=='none' else checks+'|ENCODER')
    encoder=number(d.encoder_rad) if d.flags&4 else 'unavailable'
    velocity=number(d.velocity_rad_s) if d.flags&4 else 'unavailable'
    state=ADC[d.adc_state] if d.adc_state<len(ADC) else 'UNKNOWN'
    raw=str(d.adc_raw) if d.adc_state in (1,2,5) else 'unavailable'
    voltage=number(d.adc_voltage) if d.adc_state in (1,2,5) else 'unavailable'
    current=number(d.current_a) if d.adc_state==2 else 'unavailable'
    kalman_valid=d.kalman_status&4 and not d.kalman_status&(16|32|64|128|256)
    kalman=number(d.kalman_rad) if kalman_valid else 'unavailable'
    kalman_velocity=number(d.kalman_velocity_rad_s) if kalman_valid else 'unavailable'
    source='remote' if d.flags&(1<<21) else 'local'
    remote_age=str(d.flags>>24) if source=='remote' and d.flags>>24<255 else 'unavailable'
    role='MASTER' if d.flags&(1<<16) else 'SLAVE' if d.flags&(1<<17) else 'unknown'
    counts=str(d.raw_count) if d.flags&128 else 'unavailable'
    return (f't={d.timestamp_ms} role={role} id={d.motor_id} epoch={d.epoch} enabled={bool(d.flags&1)} armed={bool(d.flags&2)} '
        f'test={bool(d.flags&16)} diagnostic={bool(d.flags&(1<<9))} timed_estimate={bool(d.flags&(1<<14))} diagnostic_checks={checks if d.flags&(1<<9) else "normal"} selectable={bool(d.flags&(1<<23))} encoder_configured={bool(d.flags&8)} limits={bool(d.flags&32)} thumb_blocked={bool(d.flags&(1<<22))} thumb_test={bool(d.flags&(1<<8))} target={number(d.target_rad)} ref={number(d.reference_rad)} '
        f'source={source} remote_age_ms={remote_age} counts={counts} encoder={encoder} velocity={velocity} PWM={number(d.pwm_percent)} '
        f'ADC={raw}/{state} active={bool(d.flags&64)} age={d.adc_age_ms} error=0x{d.adc_error:x} '
        f'V={voltage} A={current} '
        f'Kalman={kalman}/{kalman_velocity} status={d.kalman_status} faults={faults(d.faults)}')

def describe_link(d):
    role={1:'MASTER',2:'SLAVE'}.get(d.role,'unknown')
    age=lambda x: 'unavailable' if x==2**32-1 else str(x)
    uart_error=(d.flags>>8)&255
    uart_names='|'.join(name for bit,name in enumerate(('PARITY','NOISE','FRAME','OVERRUN','DMA','TIMEOUT')) if uart_error&(1<<bit)) or 'none'
    reasons=('none','RX_ERROR','RX_REARM','RX_OVERFLOW','TX_START','TX_TIMEOUT','RX_ABORT','ENTROPY')
    reason=(d.flags>>16)&255
    return (f't={d.timestamp_ms} role={role} link_fresh={bool(d.flags&1)} established={bool(d.flags&2)} '
        f'authorized={bool(d.flags&4)}/{bool(d.flags&8)} motion_build={bool(d.flags&16)} quarantine={bool(d.flags&32)} '
        f'worker_started={bool(d.flags&128)} peer_epoch_known={bool(d.flags&64)} '
        f'sessions={d.local_session:016x}/{d.remote_session:016x} epochs={d.local_epoch}/{d.remote_epoch} '
        f'local_STOP={faults(d.local_stops)} remote_STOP={faults(d.remote_stops)} '
        f'heartbeat_age_ms={age(d.heartbeat_age_ms)} echo_age_ms={age(d.echo_age_ms)} '
        f'sequences={d.rx_sequence}/{d.tx_sequence} malformed={d.malformed} overflows={d.overflows} restarts={d.restarts} '
        f'rejected={d.rejected} transport_errors={d.transport_errors} '
        f'last_transport={reasons[reason] if reason<len(reasons) else reason} last_uart_error={uart_names}')

def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port',required=True)
    selection=parser.add_mutually_exclusive_group()
    selection.add_argument('--motor',type=int,choices=range(8))
    selection.add_argument('--finger',choices=FINGER_IDS)
    parser.add_argument('--angle',type=float,help='stage an angle in degrees; type move after explicit setup/arm/enable')
    parser.add_argument('--pwm',type=int,default=3,help='PWM ceiling for the staged angle (default 3 percent)')
    parser.add_argument('--link',action='store_true',help='start with independent board/link diagnostics')
    parser.add_argument('--device-id',type=int,default=1)
    parser.add_argument('--verbose',action='store_true',help='print every diagnostic sample; default prints status changes only')
    parser.add_argument('--run',action='store_true',help='authorize ONE test: recover, limits, preflight, arm, enable, move, STOP and exit')
    parser.add_argument('--diagnostic',action='store_true',help='explicit diagnostic test; timed estimates by default, local cutoff <=2s')
    parser.add_argument('--max-angle',type=float,default=10,help='travel ceiling in degrees for --run (default 10)')
    parser.add_argument('--from-angle',type=float,help='explicit estimated current angle; timed diagnostic only, useful after a reboot')
    parser.add_argument('--duration',type=float,default=0,help='diagnostic duration; STOP on timeout/exit; 0 is interactive')
    args=parser.parse_args(argv)
    args.motor=FINGER_IDS[args.finger] if args.finger else (1 if args.motor is None else args.motor)
    if not math.isfinite(args.duration) or args.duration<0: parser.error('nonnegative finite duration required')
    if args.angle is not None and (args.motor>4 or not math.isfinite(args.angle) or not 0<=args.angle<=90):
        parser.error('--angle requires a finger and a finite angle within 0..90 degrees')
    if not 1<=args.pwm<=(40 if args.diagnostic else 10): parser.error('--pwm must be 1..10 percent, or 1..40 with --diagnostic')
    if args.diagnostic and not args.run: parser.error('--diagnostic requires --run')
    if args.from_angle is not None and not (args.diagnostic and args.run): parser.error('--from-angle requires --diagnostic --run')
    if args.angle is not None and args.duration: parser.error('--angle requires an interactive session')
    runner=None
    if args.run:
        if args.link or args.duration: parser.error('--run cannot be combined with --link or --duration')
        try: runner=QuickFingerTest(args.angle,args.pwm,args.max_angle,args.diagnostic,args.from_angle)
        except ValueError as exc: parser.error(str(exc))
    import serial
    commands=queue.Queue(maxsize=8); urgent=threading.Event(); done=threading.Event()
    def console():
        while not done.is_set():
            try: text=input().strip()
            except EOFError: done.set(); break
            if text=='stop': urgent.set()
            elif text in ('quit','exit'): done.set(); break
            elif text=='help': print(HELP)
            else:
                try: commands.put_nowait(text)
                except queue.Full: print('command queue full; dropped')
    port=None; error=False
    try:
        options={'exclusive':True} if sys.platform!='win32' else {}
        port=serial.Serial(args.port,115200,timeout=.02,write_timeout=.1,**options)
        if runner:
            print(f'One finger test authorized: target {args.angle:g} degrees, limits 0..{args.max_angle:g}, PWM {"direct" if args.diagnostic else "<="} {args.pwm}%. Ctrl+C stops.')
        else:
            print(HELP); print('Physical cutoff/current limiting remain necessary. No automatic arming.')
            if args.angle is not None: print(f'Angle staged: {args.angle:g} degrees, PWM <= {args.pwm}%. After setup type move.')
        if not args.duration and not runner: threading.Thread(target=console,daemon=True).start()
        decoder=Decoder(); epoch=None; last_diag=0.; last_rx=time.monotonic(); next_hello=0.; request=int(time.time()*1000)&0xffffffff
        pending=None; began=time.monotonic(); diagnostic=None; selected_diag_time=0.
        motion_type=DiagnosticFingerMotion if args.diagnostic else FingerMotion
        motion=motion_type(args.motor) if args.motor<5 else None
        motions={args.motor:motion} if motion else {}
        selection_pending=None; need_selection=args.angle is not None
        last_motor_status=None; profile_reported=False
        last_motion_progress=None
        run_selection_deadline=began+3; run_movement_deadline=math.inf
        def send(data):
            if port.write(data)!=len(data): raise OSError('short serial write')
        send(stop_frame())
        print('Startup STOP sent.' if runner else 'Startup STOP sent; explicit link-recover/setup/arm/enable required.')
        send(diagnostic_frame(8 if args.link else args.motor))
        while not done.is_set() and (not args.duration or time.monotonic()-began<args.duration):
            now=time.monotonic()
            if urgent.is_set():
                urgent.clear(); send(stop_frame()); pending=None
                selection_pending=None; need_selection=False
                if motion: motion.cancel()
                while not commands.empty(): commands.get_nowait()
            if now>=next_hello: send(hello_frame(args.device_id)); next_hello=now+.5
            for msg in decoder.feed(port.read(min(port.in_waiting or 1,256)),now):
                last_rx=now
                if msg.WhichOneof('message')=='diagnostic':
                    epoch=msg.diagnostic.epoch; last_diag=now
                    # Remote age and positions change continuously. Keep the
                    # interactive console readable; faults/arming/validity
                    # changes are still printed immediately.
                    status=(msg.diagnostic.motor_id,epoch,msg.diagnostic.flags&0x00ffffff,msg.diagnostic.faults)
                    if args.verbose or status!=last_motor_status:
                        print(describe(msg.diagnostic)); last_motor_status=status
                    if msg.diagnostic.motor_id==args.motor:
                        diagnostic=msg.diagnostic; selected_diag_time=now
                        if runner and motion and motion.active:
                            progress=(diagnostic.raw_count,round(diagnostic.pwm_percent,2),round(diagnostic.target_rad,3),round(diagnostic.reference_rad,3))
                            if progress!=last_motion_progress:
                                print(f'Movement: encoder_deg={number(math.degrees(diagnostic.encoder_rad))} '
                                      f'target_deg={number(math.degrees(diagnostic.target_rad))} '
                                      f'estimated_deg={number(math.degrees(diagnostic.reference_rad)) if diagnostic.flags&(1<<14) else "n/a"} '
                                      f'PWM_applied={number(diagnostic.pwm_percent)}% counts={diagnostic.raw_count}')
                                last_motion_progress=progress
                        if not profile_reported and args.angle is not None:
                            profile_reported=True
                            if diagnostic.flags&(1<<23):
                                print('Common -Fingers firmware detected; selecting the finger.'+('' if runner else ' Type commands in this console after selection is confirmed.'))
                                if diagnostic.flags&(1<<14): print('Timed diagnostic: encoder not required; angles are estimates; local cutoff <=2 seconds.')
                            else:
                                print('Fixed-finger firmware detected: runtime selection and automatic encoder setup require the current -Fingers image on BOTH boards. Type commands here; Ctrl+C exits.')
                                if runner: error=True; done.set()
                elif msg.WhichOneof('message')=='link':
                    epoch=msg.link.local_epoch; last_diag=now; print(describe_link(msg.link))
                else:
                    try: result_name=pb.CommandReply.Result.Name(msg.reply.result)
                    except ValueError: result_name=f'UNKNOWN({msg.reply.result})'
                    print(f'request={msg.reply.request_id} result={result_name} '
                          f'epoch={msg.reply.epoch} faults={faults(msg.reply.faults)}')
                    epoch=msg.reply.epoch
                    if pending and msg.reply.request_id==pending[0]:
                        if msg.reply.result==pb.CommandReply.OK:
                            if selection_pending is not None and pending[2]=='select':
                                args.motor,args.angle,args.pwm=selection_pending
                                motion=motions.setdefault(args.motor,motion_type(args.motor)); motion.cancel()
                                diagnostic=None; selected_diag_time=0.
                                send(diagnostic_frame(args.motor))
                                print(f'Finger ID {args.motor} selected on both boards; angle {args.angle:g} degrees staged.'+(' Preparing test.' if runner else ' Explicit link-recover/preflight/arm/enable required.'))
                                if runner: runner.begin(now)
                            elif motion:
                                motion.accept_limits(pending[2])
                                motion.accept_calibration(pending[2])
                            if runner: runner.acknowledged(now)
                        elif pending[2]=='select':
                            if motion: motion.cancel()
                            send(stop_frame()); print('Finger selection rejected; STOP sent. No automatic retry.')
                            while not commands.empty(): commands.get_nowait()
                        elif motion and motion.active:
                            motion.cancel(); send(stop_frame()); print('Movement rejected; STOP sent. No automatic retry.')
                        if runner and msg.reply.result!=pb.CommandReply.OK:
                            print(f'Test aborted at {pending[2]}: {result_name}; no retry.')
                            error=True; done.set()
                        if pending[2]=='select': selection_pending=None
                        pending=None
            if pending and now-pending[1]>1:
                print('command acknowledgement timeout; outcome unknown; sending STOP')
                send(stop_frame()); pending=None; epoch=None
                if motion: motion.cancel()
                selection_pending=None
                while not commands.empty(): commands.get_nowait()
                if runner: error=True; done.set()
            if now-last_rx>1.5:
                print('diagnostic timeout; sending STOP'); send(stop_frame()); epoch=None; last_rx=now
                if motion: motion.cancel()
                if runner: error=True; done.set()
            if runner and ((runner.index==0 and now>run_selection_deadline and runner.ready_after==math.inf) or now>run_movement_deadline):
                print('Test timed out; sending STOP. No automatic rearm.')
                error=True; done.set()
            if done.is_set(): break
            if need_selection and pending is None and diagnostic is not None and diagnostic.epoch==epoch:
                need_selection=False
                if diagnostic.flags&(1<<23):
                    request+=1; selection_pending=(args.motor,args.angle,args.pwm)
                    send(command_frame('select',args.motor,epoch,request)); pending=(request,now,'select')
            if pending is None:
                try:
                    if runner:
                        text=runner.next_command(diagnostic,epoch,selected_diag_time,now)
                        if text is not None: print(f'Test: {text}')
                    else:
                        try: text=commands.get_nowait()
                        except queue.Empty: text=None
                    choice=raw_finger(text,args.pwm) if text is not None else None
                    if choice is not None:
                        if diagnostic is None or now-selected_diag_time>.3 or diagnostic.epoch!=epoch:
                            raise ValueError('wait for fresh selected-finger diagnostics')
                        if not diagnostic.flags&(1<<23): raise ValueError('runtime selection requires the common -Fingers firmware')
                        if motion: motion.cancel()
                        request+=1; selection_pending=choice
                        send(command_frame('select',choice[0],epoch,request)); pending=(request,now,'select')
                        continue
                    if text is None: pass
                    elif motion and motion.active: raise ValueError('movement in progress; use stop or raw XX to change finger')
                    elif text=='move' or text.startswith('angle '):
                        if not motion: raise ValueError('select a finger (ID 0..4)')
                        if now-selected_diag_time>.3: raise ValueError('wait for fresh selected-finger diagnostics')
                        if diagnostic is None or diagnostic.epoch!=epoch: raise ValueError('wait for diagnostics at the current STOP epoch')
                        if text=='move':
                            if args.angle is None: raise ValueError('supply --angle or use angle DEG PWM')
                            degrees,pwm=args.angle,args.pwm
                        else:
                            words=shlex.split(text)
                            if len(words)!=3: raise ValueError('angle DEG PWM')
                            degrees,pwm=float(words[1]),int(words[2])
                        motion.start(degrees,pwm,diagnostic)
                        last_motion_progress=None
                        if runner: run_movement_deadline=now+(2.5 if args.diagnostic else 5)
                        continue
                    elif text is not None:
                        if text.startswith('limit-deg '): text='limits-deg '+text[len('limit-deg '):]
                        if text.startswith('limits-deg '): text=degree_limits(text)
                        request+=1; data=command_frame(text,args.motor,epoch if now-last_diag<.6 else None,request)
                        send(data)
                        if data[2]==0x1a: pending=(request,now,text)
                except (ValueError,OverflowError) as exc:
                    print(f'rejected locally: {exc}')
                    if runner: error=True; done.set()
            if done.is_set(): break
            if motion and motion.active and pending is None:
                try:
                    if now-selected_diag_time>.3: raise ValueError('selected-finger diagnostics are stale')
                    target=motion.next_target(diagnostic)
                    if motion.completed:
                        send(stop_frame()); print('Timed movement finished; angle is an estimate; STOP sent.' if getattr(motion,'timed',False) else 'Requested angle reached; STOP sent.')
                        while not commands.empty(): commands.get_nowait()
                        if runner: done.set()
                    elif target is not None:
                        request+=1; text=f'target {target:.9g} {motion.pwm}'
                        send(command_frame(text,args.motor,epoch,request)); pending=(request,now,text)
                except ValueError as exc:
                    motion.cancel(); send(stop_frame()); print(f'Movement cancelled: {exc}; STOP sent.')
                    while not commands.empty(): commands.get_nowait()
                    if runner: error=True; done.set()
    except (serial.SerialException,OSError) as exc:
        print(f'serial error/disconnected: {exc}',file=sys.stderr); error=True
    except KeyboardInterrupt: pass
    finally:
        done.set()
        if port is not None:
            try: port.write(stop_frame()); print('Exit STOP attempted; delivery is not guaranteed.')
            except (serial.SerialException,OSError): pass
            port.close()
    return 1 if error else 0
if __name__=='__main__': raise SystemExit(main())
