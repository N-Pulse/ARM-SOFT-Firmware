"""Current decoding peer: SYNC -> HELLO -> ACK/NACK -> repeated DATA intents.

Only main() opens a serial port. Uses stdlib codecs; pyserial is optional until
CLI execution. No remote stop/restart/ESTOP/approval commands are sent.
"""
from dataclasses import dataclass
import argparse
import binascii
import time

from intent_actions import ACTION_COUNT
SYNC=bytes.fromhex('000001dcbd')

@dataclass(frozen=True)
class Parameters:
    proto_ver: int = 1
    status_len: int = 2
    data_len: int = 6
    hb_period_ms: int = 100
    def wire(self):
        values=(self.proto_ver,self.status_len,self.data_len,self.hb_period_ms)
        if any(not 0<=x<=255 for x in values) or not self.hb_period_ms or self.status_len+self.data_len>128:
            raise ValueError('Invalid one-byte handshake parameters')
        return bytes(values)

def crc16(data): return binascii.crc_hqx(data,0xffff)
def packet(kind,sequence,payload):
    if len(payload)>253: raise ValueError('Packet payload too large')
    head=bytes((kind,sequence&255,len(payload)+2))+bytes(payload)
    return head+crc16(head).to_bytes(2,'little')
def hello_frame(sequence,parameters): return packet(1,sequence,parameters.wire())
def data_frame(action,sequence,parameters,state=0):
    parameters.wire()
    if action not in range(ACTION_COUNT): raise ValueError('Unknown action')
    if parameters.data_len!=6:
        raise ValueError('Firmware data_len must be configured to 6 for the explicit-action protobuf codec')
    status=bytes([state])+bytes(parameters.status_len-1) if parameters.status_len else b''
    # Explicit proto3 zero encoding keeps every action vector six bytes long.
    vector=bytes((0x12,4,0x0a,2,8,action))
    return packet(0x10,sequence,status+vector)

class Parser:
    """Bounded raw-handshake and legacy diagnostic stream parser."""
    def __init__(self): self.data=bytearray(); self.stamps=[]
    def feed(self,data,now):
        if self.stamps and (now-self.stamps[-1]>.1 or now-self.stamps[0]>.1):
            self.data.clear(); self.stamps.clear()
        self.data.extend(data); self.stamps.extend([now]*len(data))
        frames=[]
        def consume(n):
            del self.data[:n]; del self.stamps[:n]
        while self.data:
            kind=self.data[0]
            if kind not in (0,2,3,0xaa): consume(1); continue
            if kind==0xaa:
                if len(self.data)<2: break
                n=self.data[1]+2
                if not 3<=n<=130: consume(1); continue
            else:
                if len(self.data)<3: break
                n=5 if kind==0 else 9
                if self.data[2]!=(1 if kind==0 else 6): consume(1); continue
            if len(self.data)<n: break
            f=bytes(self.data[:n])
            valid=(f==SYNC if kind==0 else kind==0xaa or int.from_bytes(f[-2:],'little')==crc16(f[:-2]))
            if valid: frames.append(f); consume(n)
            else: consume(1)
        return frames

class Peer:
    def __init__(self,parameters=None,action=None,period_ms=30):
        self.parameters=parameters or Parameters(); self.parameters.wire()
        if action is not None and action not in range(ACTION_COUNT): raise ValueError('Unknown action')
        if period_ms<=0: raise ValueError('Positive DATA period required')
        self.action=action; self.period_ms=period_ms; self.active=False
        self.sequence=0; self.hello_sequence=0; self.next_send=0; self.waiting=False
    def receive(self,frame,now):
        if frame==SYNC:
            self.active=False; self.waiting=True; self.hello_sequence=(self.hello_sequence+1)&255
            return hello_frame(self.hello_sequence,self.parameters)
        if self.waiting and len(frame)==9 and frame[0] in (2,3) and frame[1]==self.hello_sequence:
            if int.from_bytes(frame[-2:],'little')!=crc16(frame[:-2]) or frame[2]!=6: return None
            self.parameters=Parameters(*frame[3:7]); self.parameters.wire()
            self.active=True; self.waiting=False; self.next_send=now
        return None
    def service(self,now):
        if not self.active or self.action is None or now<self.next_send: return None
        f=data_frame(self.action,self.sequence,self.parameters)
        self.sequence=(self.sequence+1)&255
        period=min(self.period_ms,max(1,self.parameters.hb_period_ms//2))
        self.next_send=now+period/1000
        return f

def main():
    import serial
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--port',required=True); ap.add_argument('--duration',type=float,default=30)
    ap.add_argument('--action',type=int,choices=range(ACTION_COUNT))
    ap.add_argument('--period-ms',type=int,default=30)
    ap.add_argument('--proto-ver',type=int,default=1); ap.add_argument('--status-len',type=int,default=2)
    ap.add_argument('--data-len',type=int,default=6); ap.add_argument('--hb-period-ms',type=int,default=100)
    args=ap.parse_args()
    if args.duration<=0: ap.error('Positive duration required')
    try: peer=Peer(Parameters(args.proto_ver,args.status_len,args.data_len,args.hb_period_ms),args.action,args.period_ms)
    except ValueError as error: ap.error(str(error))
    parser=Parser()
    with serial.Serial(args.port,115200,timeout=.001,write_timeout=.1) as port:
        def send(frame):
            if frame and port.write(frame)!=len(frame): raise RuntimeError('Short serial write')
        end=time.monotonic()+args.duration
        while time.monotonic()<end:
            now=time.monotonic()
            for frame in parser.feed(port.read(min(port.in_waiting or 1,256)),now):
                send(peer.receive(frame,now))
                if frame[0] in (2,3): print('ACK' if frame[0]==2 else 'NACK',peer.parameters,flush=True)
                elif frame[0]==0xaa:
                    from lifecycle_client import decode,describe
                    try: print(describe(*decode(frame[2:])),flush=True)
                    except ValueError: pass
            try: send(peer.service(time.monotonic()))
            except ValueError as error: raise SystemExit(str(error)) from error
if __name__=='__main__': main()
