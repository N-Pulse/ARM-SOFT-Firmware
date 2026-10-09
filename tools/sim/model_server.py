"""Independent local UDP motor model process with its own clock and watchdog.
Only localhost virtual transport is supported. No serial or ROS dependency.
"""
import json
import math
import socket
import time
from .model import RotorModel
from .client import milliseconds
from .protocol import finite, newer

class ModelServer:
    def __init__(self,port=0):
        self.socket=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
        self.socket.bind(('127.0.0.1',port)); self.socket.setblocking(False)
        self.port=self.socket.getsockname()[1]
        self.model=RotorModel(); self.session=self.sequence=self.last_run=0
        self.last_packet=0; self.live=False; self.address=None; self.last_step=milliseconds()
    def packet(self,data,address,now):
        try:
            if len(data)>2048: return
            m=json.loads(data)
            sid,seq,stamp=m['session'],m['sequence'],m['ms']
            if not isinstance(sid,int) or not 0<sid<1<<64 or not isinstance(seq,int) or not 0<=seq<1<<32 or not isinstance(stamp,int) or not 0<=now-stamp<=80: return
            if m['type']=='begin':
                if sid<=self.session: return
                self.model.stop(); self.session=sid; self.sequence=seq; self.last_run=0
                self.live=True; self.address=address
            elif sid!=self.session or address!=self.address or not newer(seq,self.sequence): return
            else:
                kind=m['type']
                # Read-only diagnostics remain available after expiration.
                # They never restore live, permission or an expired run.
                if not self.live and kind!='query': return
                if kind=='arm':
                    run=m['run']
                    if not isinstance(run,int) or not 0<run<1<<32 or not newer(run,self.last_run): return
                    self.last_run=run; self.model.arm(run,now)
                elif kind=='output':
                    values=m['values']
                    if len(values)!=8: return
                    prepared=[finite(v,100) for v in values]
                    if not self.model.apply(m['run'],prepared,now): return
                elif kind=='stop': self.model.stop()
                elif kind!='query': return
                self.sequence=seq
            self.last_packet=now
            if m['type']=='query':
                answer={'session':self.session,'run':self.model.run,'position':self.model.position,
                        'velocity':self.model.velocity,'applied':self.model.applied,'ms':now,
                        'watchdog':self.model.watchdog,'live':self.live}
                try: self.socket.sendto(json.dumps(answer).encode(),address)
                except (BlockingIOError,OSError): pass
        except (ValueError,KeyError,TypeError,OverflowError): return
    def poll(self,now=None):
        now=milliseconds() if now is None else now
        # Advance the independent model clock before applying a later session
        # expiry. A delayed poll still integrates only up to the command
        # deadline under power, then coasts; it cannot erase earlier inertia.
        while now-self.last_step>=10:
            self.last_step+=10; self.model.step(self.last_step)
        if self.live and now-self.last_packet>150:
            self.live=False; self.model.stop()
        for _ in range(32):
            try: data,address=self.socket.recvfrom(2049)
            except BlockingIOError: break
            self.packet(data,address,now)
    def close(self): self.model.stop(); self.socket.close()

class ModelConnection:
    """Host adapter; server remains alive and inhibits if this process hangs/dies."""
    def __init__(self,port):
        self.socket=socket.socket(socket.AF_INET,socket.SOCK_DGRAM); self.socket.setblocking(False)
        self.address=('127.0.0.1',port); self.session=0; self.seq=0
        self.permission=None; self.revision=0; self.last_query=-20
    def send(self,kind,now,**kwargs):
        self.seq=(self.seq+1)&0xffffffff
        message={'type':kind,'session':self.session,'sequence':self.seq,'ms':now,**kwargs}
        try: self.socket.sendto(json.dumps(message).encode(),self.address)
        except (BlockingIOError,OSError): pass
    def update(self,peer,now):
        if peer.session!=self.session:
            self.session=peer.session; self.seq=0; self.permission=None
            self.send('begin',now)
        if peer.permitted_run!=self.permission:
            self.permission=peer.permitted_run
            self.send('stop' if self.permission is None else 'arm',now,run=self.permission)
        if peer.output_revision!=self.revision:
            self.revision=peer.output_revision
            if self.permission is not None:
                self.send('output',now,run=self.permission,values=peer.outputs)
        if now-self.last_query>=20:
            self.last_query=now; self.send('query',now)
        for _ in range(8):
            try: data,address=self.socket.recvfrom(4096)
            except BlockingIOError: break
            try:
                m=json.loads(data)
                if address!=self.address or m['session']!=self.session or not 0<=now-m['ms']<=50: continue
                entries=[(i,11,m['position'][i],m['velocity'][i],0,m['applied'][i]) for i in range(8)]
                peer.submit_feedback(entries,m['ms'])
            except (ValueError,KeyError,TypeError,IndexError): continue
    def close(self): self.socket.close()
