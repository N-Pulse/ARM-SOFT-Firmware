"""Pure codec, mapping, one-writer and independent model process regressions."""
from pathlib import Path
import collections
import importlib.util
import json
import math
import os
import socket
import struct
import subprocess
import sys
import threading
import time
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
from sim import protocol as p
from sim.peer import Peer
from sim.ros_adapter import EffortAdapter
from sim.client import SerialWorker,milliseconds

def rejects(fn):
    try: fn()
    except (ValueError,OverflowError): return
    raise AssertionError('invalid value accepted')

def run():
    for module_path in ('tools/ros2_bridge/serial_bridge.py','tools/serial_emg_client.py'):
        spec=importlib.util.spec_from_file_location('safe_import',ROOT/module_path)
        module=importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
    for value in (math.nan,math.inf,-math.inf,1e100):
        rejects(lambda:p.targets([(0,value,60)]))
        rejects(lambda:p.feedback(1,0,[(0,11,0.,value,0,0.)]))
    rejects(lambda:p.targets([(0,0.,60),(0,1.,60)]))
    rejects(lambda:p.feedback(1,0,[(8,11,0.,0.,0,0.)]))
    rejects(lambda:p.feedback(1,0,[(0,1,0.,0.,0,1.)]))
    rejects(lambda:p.feedback(1,0,[(0,15,0.,0.,65536,1.)]))
    wire=p.encode(p.HEARTBEAT,1,1,0)
    parser=p.Parser(); assert parser.feed(wire[:5],0)==[]
    assert parser.feed(wire[5:],5)[0][:4]==(p.HEARTBEAT,1,1,0)
    for index in (2,-1):
        bad=bytearray(wire); bad[index]^=1; assert not p.Parser().feed(bytes(bad),0)
    parser=p.Parser(); parser.feed(wire[:5],0); assert not parser.feed(wire[5:],16)
    peer=Peer(); published=[]
    adapter=EffortAdapter(peer,[str(i) for i in range(8)],[1,2,3,4,5,6,7,8],published.append)
    adapter.applied_state([10,-20,30,0,0,0,0,0],10)
    entries=adapter.joint_state(['7','2','0'],[.7,.2,0.],[],10)
    assert [e[0] for e in entries]==[0,2,7] and all(e[1]==9 for e in entries)
    body=p.feedback(1,0,entries); assert body[6]==3 and len(body)==7+3*14
    assert entries[1][-1]==30 and entries[1][3]==0 and not entries[1][1]&p.VELOCITY
    adapter.applied_joint_state(["7","2"],[8.,6.],5)
    assert adapter.applied[7]==(1.,5) and adapter.applied[2]==(2.,5)
    entries=adapter.joint_state(["2"],[.3],[.1],20,20)
    assert peer.latest_feedback[1]==5 # oldest included source stamps all fields conservatively
    rejects(lambda:adapter.joint_state(["2"],[.3],[],0,60))
    previous=peer.latest_feedback
    rejects(lambda:adapter.joint_state(['0'],[math.nan],[],11))
    assert peer.latest_feedback==previous
    peer.permitted_run=1; peer.outputs=[10,-20,30,0,0,0,0,0]; peer.output_revision=1
    adapter.output(peer,10); assert published[-1]==[10,-40,90,0,0,0,0,0]
    peer.request(p.STOP); adapter.output(peer,11); assert published[-1]==[0.]*8

    # Unsolicited armed status cannot grant permission. Explicit ARM ACK can.
    peer=Peer(); peer.connect(1,0); assert peer.outgoing(0)[0][3]==p.HELLO
    def status(seq,stamp,run=0,armed=0,config=0,req=0,ack=0,result=1):
        return p.encode(p.HELLO_ACK if req==p.HELLO else p.STATUS,1,seq,stamp,
            struct.pack('<IIIBBBBIB',run,0,0,0,armed,config,req,ack,result))
    peer.receive(status(1,0,req=p.HELLO),1); assert peer.live
    peer.receive(status(2,22,run=1,armed=1,config=1),2)
    assert peer.rx_sequence==1 # future by 21 ms exceeds bounded clock skew
    peer.receive(status(2,21,run=1,armed=1,config=1),2)
    assert peer.rx_sequence==2 # future by 20 ms: bounded serial jitter
    peer.receive(status(2,1,run=1,armed=1,config=1),2); assert peer.permitted_run is None
    assert peer.request(p.ARM); frames=peer.outgoing(2); arm_seq=peer.pending_arm
    peer.receive(status(3,2,run=1,armed=1,config=1,req=p.ARM,ack=arm_seq),3)
    assert peer.permitted_run==1
    output=p.encode(p.OUTPUT,1,4,3,struct.pack('<IIB8h',1,0,255,1000,-2000,3000,0,0,0,0,0))
    peer.receive(output,4); assert peer.outputs[:3]==[10.,-20.,30.]
    for seq in range(5,9): peer.receive(status(seq,seq*20,run=1,armed=1,config=1),seq*20+1)
    peer.poll(125); assert peer.permitted_run is None and not any(peer.outputs)
    peer.receive(p.encode(p.OUTPUT,1,9,125,struct.pack('<IIB8h',1,0,255,*([5000]*8))),126)
    assert not any(peer.outputs)
    assert p.newer(0,0xffffffff) and not p.newer(0xffffffff,0)

    class FakeSerial:
        def __init__(self): self.rx=bytearray(); self.writes=[]; self.owner_ids=[]; self.parser=p.Parser(); self.closed=False
        def read(self,n):
            data=bytes(self.rx[:n]); del self.rx[:n]; return data
        def write(self,frame):
            self.owner_ids.append(threading.get_ident()); self.writes.append(frame)
            for kind,sid,seq,stamp,body in self.parser.feed(frame,milliseconds()):
                if kind==p.HELLO:
                    response=struct.pack('<IIIBBBBIB',0,0,0,0,0,0,p.HELLO,0,1)
                    self.rx.extend(p.encode(p.HELLO_ACK,sid,1,milliseconds()&0xffffffff,response))
            return len(frame)
        def close(self): self.closed=True
    fake=FakeSerial(); worker=SerialWorker('VIRTUAL',transport=fake); worker.start()
    deadline=time.monotonic()+1
    while not worker.peer.live and time.monotonic()<deadline: time.sleep(.002)
    assert worker.peer.live
    def producer():
        for _ in range(100):
            worker.peer.submit_feedback([(0,11,0.,0.,0,0.)],milliseconds())
            worker.peer.request(p.CONFIG,b'\x01\x00\xff\x00')
    producers=[threading.Thread(target=producer) for _ in range(3)]
    for thread in producers: thread.start()
    for thread in producers: thread.join()
    time.sleep(.12); worker.close()
    assert worker.error is None and fake.closed and len(set(fake.owner_ids))==1
    parser=p.Parser(); parsed=[]
    for f in fake.writes: parsed+=parser.feed(f,0)
    assert len(parsed)==len(fake.writes) and worker.peer.replaced>0
    assert all(len(f)<=151 for f in fake.writes)

    # A separate real OS process keeps integrating and cancels u after its own
    # deadline while the parent sends nothing. Only localhost UDP is used.
    proc=subprocess.Popen([sys.executable,str(ROOT/'tools/sim_model_server.py'),'--port','0'],
        stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,cwd=ROOT)
    sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM); sock.settimeout(1)
    try:
        port=int(proc.stdout.readline().split()[-1]); address=('127.0.0.1',port)
        def send(kind,seq,**kw):
            sock.sendto(json.dumps({'type':kind,'session':1,'sequence':seq,'ms':milliseconds(),**kw}).encode(),address)
        send('begin',1); send('arm',2,run=1); send('output',3,run=1,values=[50.]*8); send('query',4)
        first=json.loads(sock.recvfrom(4096)[0]); assert first['applied']==[50.]*8
        time.sleep(.18) # intentionally cross command AND session deadlines
        send('query',5); stopped=json.loads(sock.recvfrom(4096)[0])
        assert stopped['watchdog'] and not stopped['live']
        assert stopped['applied']==[0.]*8 and stopped['run'] is None
        assert any(v!=0 for v in stopped['velocity']) # coast, no instantaneous velocity reset
        send('output',6,run=1,values=[50.]*8); send('query',7)
        late=json.loads(sock.recvfrom(4096)[0])
        assert late['applied']==[0.]*8 and not late['live']
        send('arm',8,run=2); send('query',9)
        expired=json.loads(sock.recvfrom(4096)[0])
        assert expired['run'] is None and not expired['live'] # diagnostics cannot rearm
    finally:
        sock.close(); proc.terminate(); proc.communicate(timeout=3)
    print('PASS host codec/mapping/partial fields/nonfinite rejection/STOP/watchdogs, one writer with 3 producers, independent model subprocess; no ROS or serial port')
if __name__=='__main__': run()
