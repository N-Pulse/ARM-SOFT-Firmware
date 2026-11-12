"""UART4 HW lifecycle client. Requires pyserial only; no port opens on import.

Protobuf encode/decode helpers retain wire compatibility for regression tests.
The CLI delegates to the current SYNC/HELLO/DATA link; no remote controls are sent.
"""
import argparse
import time
DEVICE_CONTROL_FIELD=6
STATUS_FIELD=5
SAFETY_FIELD=6
PROBE_FIELD=7
HELLO_FIELD=8
STATES=['INIT_HANDSHAKE','INIT_MODULES','INIT_RETURN','RESERVED_3','WORK','RESERVED_5','RESERVED_6','RESERVED_7','FAULT']
CAUSES=['MANUAL/ESTOP','GUARD_STALL','POSITION_OR_FORCE_FEEDBACK','CONTROL_DELAY','FORCE_LIMIT','DRIVER','PID','LINK','INIT','MOTION_TIMEOUT','POSITION_LIMIT']
MOTORS=['THUMB','INDEX','MIDDLE','RING','LITTLE','WRIST_X','WRIST_Y','PALM']
RESULTS=['OK','WRONG_STATE','RESERVED_2','BAD_SESSION','STALE','RECOVERY_DENIED','INIT_FAILED','MOTION_TIMEOUT','BAD_VERSION','PEER_CHANGED','TRANSPORT','LINK_TIMEOUT']
from intent_actions import ACTION_COUNT

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
    if action not in range(ACTION_COUNT): raise ValueError('unknown action')
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
                f"request={f.get(3,0)} "
                f"causes=0x{f.get(6,0):x} action={f.get(7,0)} detail={f.get(9,0)}")
    if tag==SAFETY_FIELD:
        causes=','.join(name for i,name in enumerate(CAUSES) if f.get(2,0)&(1<<i))
        motors=','.join(name for i,name in enumerate(MOTORS) if f.get(3,0)&(1<<i))
        return f"SAFETY t={f.get(1,0)}ms cause={causes} motors={motors} from={state_name(f.get(5,0))} epoch={f.get(4,0)}; outputs inhibited, PID reset"
    return f'type={tag} fields={f}'

def main():
    from decoding_link import main as run_current_link
    run_current_link()
if __name__=='__main__': main()
