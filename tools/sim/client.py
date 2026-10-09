"""One serial I/O thread owns all frame writes. Imports open no serial device."""
import collections
import os
import shlex
import struct
import threading
import time
from . import protocol as p
from .peer import Peer

def milliseconds():
    return time.monotonic_ns()//1000000

def submit_command(peer, text):
    """Explicit manual SIM-only commands; return whether queued, never delivered."""
    words=shlex.split(text)
    if not words: return False
    command=words[0]
    if command=='config' and len(words) in (1,2):
        mask=int(words[1],0) if len(words)==2 else 0
        if not 0 <= mask <= 255: raise ValueError('force mask 0..255')
        return peer.request(p.CONFIG,struct.pack('<HBB',1,255,mask))
    if command in ('recover','arm','stop') and len(words)==1:
        return peer.request({'recover':p.RECOVER,'arm':p.ARM,'stop':p.STOP}[command])
    if command=='enable' and len(words)==2:
        mask=int(words[1],0)
        if not 1 <= mask <= 255: raise ValueError('axis mask 1..255')
        return peer.request(p.ENABLE,bytes((mask,)))
    if command=='target' and len(words)==4:
        return peer.request(p.TARGET,p.targets([(int(words[1]),float(words[2]),int(words[3]))]))
    raise ValueError('config [force_mask] | recover | arm | enable mask | target ID rad limit | stop')

class SerialWorker:
    def __init__(self,port,*,transport=None,on_state=None,clock=milliseconds):
        self.peer=Peer(); self.clock=clock; self.on_state=on_state
        self.transport=transport; self.port=port; self.running=False; self.error=None
        self.thread=None
    def start(self):
        if self.transport is None:
            import serial # optional dependency, only when an explicit port is opened
            options={"exclusive":True} if os.name!="nt" else {}
            self.transport=serial.Serial(self.port,baudrate=p.BAUD,timeout=0,write_timeout=0.025,**options)
        self.peer.connect(time.time_ns(),self.clock())
        self.running=True
        self.thread=threading.Thread(target=self._loop,name='sim-uart-owner',daemon=True)
        self.thread.start()
    def _loop(self):
        pending=collections.deque(); available=0.
        try:
            while self.running:
                now=self.clock()
                data=self.transport.read(256)
                if data: self.peer.receive(data,now)
                self.peer.poll(now)
                if self.on_state: self.on_state(self.peer,now)
                with self.peer.lock:
                    if self.peer.stop_pending: pending.clear()
                    if not pending: pending.extend(self.peer.outgoing(now))
                if pending and now >= available:
                    frame=pending.popleft()
                    if self.transport.write(frame)!=len(frame):
                        raise OSError('partial serial write')
                    available=now+len(frame)*10000/p.BAUD
                time.sleep(0.001)
        except Exception as exc:
            self.error=exc
            with self.peer.lock:
                self.peer.live=False; self.peer._safe('I/O failure')
            if self.on_state: self.on_state(self.peer,self.clock())
        finally:
            self.running=False
    def close(self):
        self.peer.request(p.STOP)
        # The remote model watchdog is the fallback if this offer cannot be sent.
        time.sleep(0.03)
        self.running=False
        if self.thread: self.thread.join(timeout=1)
        if self.transport: self.transport.close()
