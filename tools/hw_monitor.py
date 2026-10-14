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

HELP='status | raw | limits MIN MAX STEP PWM RATE | calibrate CPR QUAD RATIO DIR OFFSET | zero | recover | arm | enable | target RAD PWM | disable | estimator-reset verified | estimator-stop | stop | quit'
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
    if command in ('status','raw') and not args: return diagnostic_frame(mid)
    if command=='stop' and not args: return stop_frame()
    if epoch is None: raise ValueError('wait for a recent diagnostic epoch')
    msg=pb.DeviceMessage(); c=msg.commission
    c.motor_id=mid; c.epoch=epoch; c.request_id=request
    counts={'limits':5,'calibrate':5,'zero':0,'recover':0,'arm':0,'enable':0,'target':2,'disable':0,'estimator-reset':1,'estimator-stop':0}
    if command not in counts or len(args)!=counts[command]: raise ValueError(HELP)
    c.operation={'limits':pb.CommissionCommand.LIMITS,'calibrate':pb.CommissionCommand.CALIBRATE,
        'zero':pb.CommissionCommand.ZERO,'recover':pb.CommissionCommand.RECOVER,'arm':pb.CommissionCommand.ARM,
        'enable':pb.CommissionCommand.ENABLE,'target':pb.CommissionCommand.TARGET,'disable':pb.CommissionCommand.DISABLE,
        'estimator-reset':pb.CommissionCommand.ESTIMATOR_RESET,'estimator-stop':pb.CommissionCommand.ESTIMATOR_STOP}[command]
    if command=='limits':
        c.minimum_rad,c.maximum_rad,c.maximum_step_rad=map(float,args[:3])
        c.pwm_percent=int(args[3]); c.reference_rate_rad_s=float(args[4])
    elif command=='calibrate':
        c.cpr=int(args[0]); c.quadrature=int(args[1]); c.gear_ratio=float(args[2]); c.direction=int(args[3]); c.zero_offset=int(args[4])
    elif command=='target': c.target_rad=float(args[0]); c.pwm_percent=int(args[1])
    elif command=='estimator-reset':
        if args[0]!='verified': raise ValueError('explicit verified stationary, trustworthy reference declaration required')
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
                if message.WhichOneof('message') in ('diagnostic','reply'): messages.append(message)
            except Exception as exc: print(f'malformed protobuf: {exc}')
        if len(self.buffer)>130: self.buffer.clear(); self.since=None
        return messages

def describe(d):
    encoder=number(d.encoder_rad) if d.flags&4 else 'unavailable'
    velocity=number(d.velocity_rad_s) if d.flags&4 else 'unavailable'
    state=ADC[d.adc_state] if d.adc_state<len(ADC) else 'UNKNOWN'
    raw=str(d.adc_raw) if d.adc_state in (1,2,5) else 'unavailable'
    voltage=number(d.adc_voltage) if d.adc_state in (1,2,5) else 'unavailable'
    current=number(d.current_a) if d.adc_state==2 else 'unavailable'
    kalman_valid=d.kalman_status&4 and not d.kalman_status&(16|32|64|128|256)
    kalman=number(d.kalman_rad) if kalman_valid else 'unavailable'
    kalman_velocity=number(d.kalman_velocity_rad_s) if kalman_valid else 'unavailable'
    return (f't={d.timestamp_ms} id={d.motor_id} epoch={d.epoch} enabled={bool(d.flags&1)} armed={bool(d.flags&2)} '
        f'test={bool(d.flags&16)} limits={bool(d.flags&32)} target={number(d.target_rad)} ref={number(d.reference_rad)} '
        f'counts={d.raw_count} encoder={encoder} velocity={velocity} PWM={number(d.pwm_percent)} '
        f'ADC={raw}/{state} active={bool(d.flags&64)} age={d.adc_age_ms} error=0x{d.adc_error:x} '
        f'V={voltage} A={current} '
        f'Kalman={kalman}/{kalman_velocity} status={d.kalman_status} faults={faults(d.faults)}')

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port',required=True)
    parser.add_argument('--motor',type=int,choices=range(8),default=1)
    parser.add_argument('--device-id',type=int,default=1)
    parser.add_argument('--duration',type=float,default=0,help='optional read-only duration; 0 is interactive')
    args=parser.parse_args()
    if not math.isfinite(args.duration) or args.duration<0: parser.error('nonnegative finite duration required')
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
        print(HELP); print('Physical cutoff/current limiting remain necessary. No automatic arming.')
        if not args.duration: threading.Thread(target=console,daemon=True).start()
        decoder=Decoder(); epoch=None; last_diag=0.; last_rx=time.monotonic(); next_hello=0.; request=int(time.time()*1000)&0xffffffff
        pending=None; began=time.monotonic()
        def send(data):
            if port.write(data)!=len(data): raise OSError('short serial write')
        send(diagnostic_frame(args.motor))
        while not done.is_set() and (not args.duration or time.monotonic()-began<args.duration):
            now=time.monotonic()
            if urgent.is_set():
                urgent.clear(); send(stop_frame()); pending=None
                while not commands.empty(): commands.get_nowait()
            if now>=next_hello: send(hello_frame(args.device_id)); next_hello=now+.5
            for msg in decoder.feed(port.read(min(port.in_waiting or 1,256)),now):
                last_rx=now
                if msg.WhichOneof('message')=='diagnostic':
                    epoch=msg.diagnostic.epoch; last_diag=now; print(describe(msg.diagnostic))
                else:
                    try: result_name=pb.CommandReply.Result.Name(msg.reply.result)
                    except ValueError: result_name=f'UNKNOWN({msg.reply.result})'
                    print(f'request={msg.reply.request_id} result={result_name} '
                          f'epoch={msg.reply.epoch} faults={faults(msg.reply.faults)}')
                    epoch=msg.reply.epoch
                    if pending and msg.reply.request_id==pending[0]: pending=None
            if pending and now-pending[1]>1:
                print('command acknowledgement timeout; outcome unknown; sending STOP')
                send(stop_frame()); pending=None; epoch=None
                while not commands.empty(): commands.get_nowait()
            if now-last_rx>1.5:
                print('diagnostic timeout; sending STOP'); send(stop_frame()); epoch=None; last_rx=now
            if pending is None:
                try: text=commands.get_nowait()
                except queue.Empty: continue
                try:
                    request+=1; data=command_frame(text,args.motor,epoch if now-last_diag<.6 else None,request)
                    send(data)
                    if data[2]==0x1a: pending=(request,now)
                except (ValueError,OverflowError) as exc: print(f'rejected locally: {exc}')
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
