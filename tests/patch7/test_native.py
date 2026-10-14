"""Real monotonic C process + existing Peer/effort adapter; no Gazebo/model."""
from pathlib import Path
import sys
import time
import struct
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
sys.path.insert(0,str(ROOT/'tests/simulation'))
from bench import build
from sim.client import SerialWorker,submit_command,milliseconds
from sim.native_transport import NativeTransport
from sim import protocol as p
from sim.ros_adapter import SourceClock
from sim.peer import Peer

def run():
    peer=Peer(continuous_clock=True); peer.connect(1,1000)
    body=struct.pack('<IIIBBBBIB',0,0,0,0,0,0,p.HELLO,0,1)
    peer.receive(p.encode(p.HELLO_ACK,1,1,101,body),1001)
    assert peer.request(p.CONFIG,b'\x01\x00\xff\x00')
    status=struct.pack('<IIIBBBBIB',0,0,0,0,0,0,0,0,0)
    peer.receive(p.encode(p.STATUS,1,2,120,status),1020)
    assert len(peer.commands)==1 # repeated disabled status must not erase an explicit operator request
    peer.hello_pending=False
    assert all(frame[3] in (p.CONFIG,p.HEARTBEAT) for frame in peer.outgoing(1050))
    stamp=p.Parser().feed(peer.outgoing(1090)[0],0)[0][3]
    assert stamp==170 # estimated C time 190, strict 20 ms scheduling margin
    assert peer.request(p.ARM)
    fault=struct.pack('<IIIBBBBIB',1,1,1,0,0,0,0,0,0)
    peer.receive(p.encode(p.STATUS,1,3,191,fault),1091)
    assert not peer.commands and peer.permitted_run is None
    clock=SourceClock(); assert clock.capture(1000000000,1010000000,100,'joints')==90
    assert clock.capture(1020000000,1030000000,120,'joints')==110
    for stamp,ros,wall in ((1020000000,1030000000,130),(1040000000,1040000000,400),
                            (0,0,130),(1100000000,1300000000,200)):
        try: clock.capture(stamp,ros,wall,'joints')
        except ValueError: pass
        else: raise AssertionError('clock policy accepted stale/repeated/paused/jumped source')
    transport=NativeTransport(build('firmware-realtime')); worker=SerialWorker(None,transport=transport)
    worker.start()
    def wait(condition,seconds=1):
        deadline=time.monotonic()+seconds
        while not condition():
            worker.peer.submit_feedback([(i,11,0.,0.,0,0.) for i in range(8)],milliseconds())
            if worker.error: raise AssertionError(worker.error)
            if time.monotonic()>deadline: raise AssertionError(('native IPC timeout',worker.peer.causes,worker.peer.replies,worker.peer.watchdog,worker.peer.live,worker.error))
            time.sleep(.005)
    def command(text):
        previous=set(worker.peer.replies)
        assert submit_command(worker.peer,text)
        wait(lambda:bool(set(worker.peer.replies)-previous))
        key=next(iter(set(worker.peer.replies)-previous))
        assert worker.peer.replies[key],(text,key,worker.peer.causes)
    try:
        wait(lambda:worker.peer.live); assert worker.peer.permitted_run is None
        command('config')
        wait(lambda:worker.peer.configured)
        until=time.monotonic()+.06
        wait(lambda:time.monotonic()>=until)
        command('recover')
        until=time.monotonic()+.06
        wait(lambda:time.monotonic()>=until)
        command('arm')
        # Continue feeding while the ARM generation acquires new samples.
        until=time.monotonic()+.06
        wait(lambda:time.monotonic()>=until)
        command('enable 255')
        command('target 7 0.02 3'); wait(lambda:worker.peer.outputs[7]>0)
        assert worker.peer.enabled==255 and worker.peer.permitted_run is not None
        command('intent ROTATE_WRIST_R'); wait(lambda:worker.peer.outputs[5]>0 and worker.peer.outputs[6]>0)
        worker.peer.request(p.STOP); wait(lambda:worker.peer.causes!=0)
        assert worker.peer.permitted_run is None and not any(worker.peer.outputs)
        until=time.monotonic()+.06
        wait(lambda:time.monotonic()>=until)
        command('recover'); assert worker.peer.permitted_run is None
        # Stop host heartbeat without closing child: its own C monotonic clock
        # must expire the session. A later resume cannot restore actuation.
        worker.running=False; worker.thread.join(timeout=1)
        time.sleep(.2)
        wire=b''; deadline=time.monotonic()+.1
        while time.monotonic()<deadline:
            wire+=transport.read(256); time.sleep(.002)
        frames=p.Parser().feed(wire,milliseconds())
        assert any(kind==p.STOP for kind,*_ in frames),frames
        print('PASS native binary IPC: eight axes, real C PID effort, same explicit commands/intent, STOP/recovery, independent monotonic session watchdog; ROS 1x timestamp policy')
    finally: worker.close()
if __name__=='__main__': run()
