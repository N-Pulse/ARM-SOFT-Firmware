"""UART4 HW lifecycle client. Requires pyserial only; no port opens on import.

Maintains Hello/PONG, prints status and deferred safety events. Motion is sent
only with an explicit --command allow-start and/or --action option.
"""
import argparse
import time
DEVICE_CONTROL_FIELD=6
STATUS_FIELD=5
SAFETY_FIELD=6
PROBE_FIELD=7
HELLO_FIELD=8
COMMANDS={'allow-start':1,'deny-start':2,'stop':3,'restart':4,'query':7,'estop':8}
STATES=['INIT_HANDSHAKE','INIT_MODULES','INIT_RETURN','READY','WORK_WAIT','WORK_EXECUTING','STOPPING','STOPPED','FAULT']
CAUSES=['MANUAL/ESTOP','GUARD_STALL','POSITION_OR_FORCE_FEEDBACK','CONTROL_DELAY','FORCE_LIMIT','DRIVER','PID','LINK','INIT','MOTION_TIMEOUT']
MOTORS=['THUMB','INDEX','MIDDLE','RING','LITTLE','WRIST_X','WRIST_Y','PALM']
RESULTS=['OK','WRONG_STATE','QUEUE_FULL','BAD_SESSION','STALE','RECOVERY_DENIED','INIT_FAILED','MOTION_TIMEOUT','BAD_VERSION','PEER_CHANGED','TRANSPORT','LINK_TIMEOUT']
def varint(value):
    if not 0<=value<=0xffffffff: raise ValueError('uint32 required')
    out=bytearray()
    while value>=128: out.append((value&127)|128); value>>=7
    out.append(value); return bytes(out)
def read_varint(data,i):
    value=0
    for k in range(5):
        if i>=len(data): raise ValueError('truncated varint')
        b=data[i]; i+=1
        if k==4 and b&240: raise ValueError('uint32 overflow')
        value|=(b&127)<<(7*k)
        if not b&128: return value,i
    raise ValueError('bad varint')
def message(field,values):
    inner=b''.join(varint(k*8)+varint(v) for k,v in values.items())
    payload=varint(field*8+2)+varint(len(inner))+inner
    if not 1<=len(payload)<=128: raise ValueError('payload too long')
    return b'\xaa'+bytes([len(payload)])+payload

def hello_frame(peer=1): return message(1,{1:peer,2:1,3:1})
def control_frame(command,request_id,peer=1): return message(DEVICE_CONTROL_FIELD,{1:command,2:request_id,3:peer})
def action_frame(action):
    if action not in range(17): raise ValueError('unknown action')
    # DeviceMessage.Data.ClassificationData, including proto3 zero omission.
    inner=b'' if action==0 else b'\x08'+varint(action)
    data=b'\x0a'+varint(len(inner))+inner
    p=b'\x12'+varint(len(data))+data
    return b'\xaa'+bytes([len(p)])+p

def decode(payload):
    tag,i=read_varint(payload,0); n,i=read_varint(payload,i)
    if tag&7!=2 or n!=len(payload)-i: raise ValueError('invalid envelope')
    if tag//8 not in (STATUS_FIELD,SAFETY_FIELD,PROBE_FIELD,HELLO_FIELD):
        return tag//8,{}
    fields={}
    while i<len(payload):
        key,i=read_varint(payload,i)
        if key&7!=0 or key//8 in fields: raise ValueError('invalid scalar fields')
        value,i=read_varint(payload,i); fields[key//8]=value
    return tag//8,fields

def state_name(value): return STATES[value] if value<len(STATES) else str(value)
def describe(tag,f):
    if tag==STATUS_FIELD:
        result=f.get(2,0)
        return (f"state={state_name(f.get(1,0))} result={RESULTS[result] if result<len(RESULTS) else result} "
                f"request={f.get(3,0)} queue={f.get(4,0)} dropped={f.get(5,0)} "
                f"causes=0x{f.get(6,0):x} action={f.get(7,0)} done={bool(f.get(8,0))} detail={f.get(9,0)}")
    if tag==SAFETY_FIELD:
        causes=','.join(name for i,name in enumerate(CAUSES) if f.get(2,0)&(1<<i))
        motors=','.join(name for i,name in enumerate(MOTORS) if f.get(3,0)&(1<<i))
        return f"SAFETY t={f.get(1,0)}ms cause={causes} motors={motors} from={state_name(f.get(5,0))} epoch={f.get(4,0)}; outputs inhibited, PID reset"
    return f'type={tag} fields={f}'

def main():
    import serial
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--port',required=True); ap.add_argument('--duration',type=float,default=30)
    ap.add_argument('--peer-id',type=int,default=1); ap.add_argument('--command',choices=COMMANDS)
    ap.add_argument('--action',type=int,choices=range(17))
    ap.add_argument('--request-id-start',type=int,default=int(time.time()))
    args=ap.parse_args()
    if args.duration<=0 or not 0<args.peer_id<=0xffffffff or not 0<args.request_id_start<0xffffffff:
        ap.error('positive duration, peer ID and request ID required')
    if args.action is not None and args.command not in (None,'allow-start'):
        ap.error('--action can only accompany allow-start or an already working session')
    request=args.request_id_start; pending=args.command; action=args.action
    with serial.Serial(args.port,115200,timeout=.05,write_timeout=.1) as port:
        def send(data):
            if port.write(data)!=len(data): raise RuntimeError('short UART write')
        end=time.monotonic()+args.duration; hello_at=0; rx=bytearray()
        while time.monotonic()<end:
            now=time.monotonic()
            if now>=hello_at: send(hello_frame(args.peer_id)); hello_at=now+.5
            rx.extend(port.read(min(port.in_waiting or 1,256)))
            while rx:
                if rx[0]!=0xaa: del rx[0]; continue
                if len(rx)<2: break
                n=rx[1]
                if not 1<=n<=128: del rx[0]; continue
                if len(rx)<n+2: break
                payload=bytes(rx[2:n+2]); del rx[:n+2]
                try: tag,f=decode(payload)
                except ValueError as e: print('RX rejected:',e); continue
                if tag not in (STATUS_FIELD,SAFETY_FIELD,PROBE_FIELD,HELLO_FIELD):
                    continue
                if tag==PROBE_FIELD and f.get(1)==5:
                    if f.get(3)!=args.peer_id: raise RuntimeError('PING peer mismatch')
                    send(control_frame(6,f[2],args.peer_id)); continue
                print(describe(tag,f),flush=True)
                if tag!=STATUS_FIELD: continue
                state=f.get(1,0)
                admissible=(pending in ('allow-start','deny-start') and state==3 or
                            pending=='restart' and state in (7,8) or
                            pending in ('query','estop') or
                            pending=='stop' and state in (1,2,3,4,5,6,7))
                if pending and admissible:
                    send(control_frame(COMMANDS[pending],request,args.peer_id)); request+=1; pending=None
                if action is not None and state in (4,5):
                    send(action_frame(action)); action=None
        if pending or action is not None: print('Requested command/action was not sent: required state was not reached.')
if __name__=='__main__': main()
