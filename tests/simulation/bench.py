"""Compiles and runs the production SIM firmware over virtual, rate-limited UART.
Only HAL, RTOS scheduling and clock are mocked in native_firmware.c.
"""
from pathlib import Path
import csv
import os
import subprocess
import sys
import math
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from sim import protocol as p
from sim.peer import Peer
from sim.model import RotorModel
OUT = ROOT / 'build/simulation'
CTRL = ROOT / 'fw/motors/motor_control'
COMM = ROOT / 'fw/comm'

def compiler_arguments():
    includes = [ROOT/'tests/comms/mocks', ROOT/'tests/safety/mocks', CTRL, COMM,
                ROOT/'fw/comm-stack', ROOT/'fw/app', ROOT/'fw/bsp/encoder']
    args = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-g', '-O0', '-DMOTOR_BACKEND_SIM=1']
    for folder in includes:
        args += ['-I', str(folder)]
    args += [str(CTRL/(name+'.c')) for name in ('motor_control','motor_safety','motor_guard',
        'motor_feedback','motor_backend_sim','motor_pid')]
    args += [str(COMM/(name+'.c')) for name in ('comm','comms_session','uart4_transport',
        'board_uart_compat','sim_protocol','sim_session')]
    args += [str(ROOT/'tests/simulation/native_firmware.c'), '-lm']
    return args

def build():
    OUT.mkdir(parents=True, exist_ok=True)
    binary = OUT / ('firmware.exe' if os.name == 'nt' else 'firmware')
    subprocess.run([os.environ.get('HOST_CC','gcc'), *compiler_arguments(), '-o', str(binary)], cwd=ROOT, check=True)
    return binary

class Firmware:
    def __init__(self, binary):
        self.process = subprocess.Popen([str(binary)], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                        stderr=subprocess.PIPE, text=True, bufsize=1, cwd=ROOT)
        self.frames = []
        self.state = {}; self.motors = []; self.diag = []
    def call(self, text):
        self.process.stdin.write(text+'\n'); self.process.stdin.flush()
        motors = []
        while True:
            line = self.process.stdout.readline().strip()
            if not line:
                error = self.process.stderr.read()
                raise AssertionError('native firmware stopped: '+error)
            fields = line.split()
            if fields[0] == 'E':
                self.motors = motors; return self.state
            if fields[0] == 'F': self.frames.append(bytes.fromhex(fields[1]))
            if fields[0] == 'S':
                self.state = dict(zip(('time','causes','session','run','live','configured','armed'),map(int,fields[1:])))
            if fields[0] == 'M':
                _, mid, target, ref, measured, output, enabled, valid, age = fields
                motors.append(dict(id=int(mid),target=float(target),ref=float(ref),measured=float(measured),
                                   output=float(output),enabled=int(enabled),valid=int(valid),age=int(age)))
            if fields[0] == 'D': self.diag = list(map(int,fields[1:]))
    def close(self):
        if self.process.poll() is None:
            self.process.stdin.write('QUIT\n'); self.process.stdin.flush()
        self.process.communicate(timeout=5)
        assert self.process.returncode == 0

class Link:
    """8N1 at the actual 115200 baud, independently in each direction."""
    def __init__(self):
        self.pending = []; self.available = 0.; self.bytes = 0; self.max_pending = 0; self.max_delay = 0.
    def offer(self, frame, now, latency=0, drop=False):
        self.bytes += len(frame)
        if drop: return
        start = max(float(now), self.available)
        self.available = start + len(frame)*10*1000/p.BAUD
        due = self.available + latency
        self.pending.append((due,frame)); self.max_pending = max(self.max_pending,len(self.pending))
        self.max_delay = max(self.max_delay,due-now)
    def receive(self, now):
        ready = [frame for due,frame in self.pending if due <= now]
        self.pending = [(due,frame) for due,frame in self.pending if due > now]
        return ready

class Bench:
    def __init__(self,binary,case,trace=None):
        self.fw=Firmware(binary); self.peer=Peer(); self.model=RotorModel(); self.now=0
        self.up=Link(); self.down=Link(); self.case=case; self.trace=trace
        self.last_permitted=None; self.last_revision=0; self.force_mask=0; self.force=0
        self.pause_model=False; self.feedback_ids=None; self.feedback_fields=None
        self.last_output_frame=None; self.feedback_number=0
        self.events=[]; self.previous_local=False; self.previous_model_watchdog=False
    def sync_model(self):
        if self.peer.permitted_run != self.last_permitted:
            self.last_permitted=self.peer.permitted_run
            if self.last_permitted is None:
                self.model.stop(); self.events.append(("remote_inhibit",self.now))
            else: self.model.arm(self.last_permitted,self.now)
        if self.peer.output_revision != self.last_revision:
            self.last_revision=self.peer.output_revision
            self.model.apply(self.peer.permitted_run,self.peer.outputs,self.now)
    def step(self,*,feedback=True,heartbeats=True,up=True,down=True,control=True,safety=True,
             drain=True,mode=0,latency_up=0,latency_down=0,latency_feedback=0,drop_feedback_every=0,endpoint=True):
        self.now += 10
        self.model.step(self.now)
        if self.pause_model:
            self.model.position=[0.0]*8; self.model.velocity=[0.0]*8
        self.fw.call('TIME '+str(self.now & 0xffffffff))
        if endpoint:
            self.peer.poll(self.now)
            for frame in self.down.receive(self.now):
                self.peer.receive(frame,self.now)
            self.sync_model()
            if feedback and self.now % 20 == 0:
                entries=self.model.feedback(self.force_mask,self.force,self.feedback_ids)
                if self.feedback_fields is not None:
                    entries=[(i,self.feedback_fields,pos if self.feedback_fields&1 else 0.,
                              vel if self.feedback_fields&2 else 0.,f if self.feedback_fields&4 else 0,
                              a if self.feedback_fields&8 else 0.) for i,_,pos,vel,f,a in entries]
                self.peer.submit_feedback(entries,self.now)
            for frame in self.peer.outgoing(self.now):
                is_feedback=frame[3]==p.FEEDBACK
                if is_feedback: self.feedback_number+=1
                drop=not up or (not heartbeats and frame[3]==p.HEARTBEAT) or (is_feedback and drop_feedback_every and self.feedback_number%drop_feedback_every==0)
                self.up.offer(frame,self.now,latency_up+(latency_feedback if is_feedback else 0),drop)
        for frame in self.up.receive(self.now): self.fw.call('RX '+frame.hex())
        self.fw.call('STEP '+f'{int(control)} {int(safety and self.now%20==0)} {int(drain)} {mode}')
        for frame in self.fw.frames:
            if frame[3]==p.OUTPUT: self.last_output_frame=frame
            self.down.offer(frame,self.now,latency_down,not down or not endpoint)
        self.fw.frames.clear()
        if self.trace:
            for m in self.fw.motors:
                i=m['id']
                self.trace.writerow([self.case,self.now,self.fw.state['session'],self.fw.state['run'],i,
                    m['target'],m['ref'],m['measured'],self.model.position[i],self.model.velocity[i],
                    m['output'],self.model.applied[i],m['enabled'],self.fw.state['armed'],
                    self.fw.state['causes'],m['age'],self.peer.watchdog,int(self.model.watchdog)])
        if self.previous_local and not self.fw.state['armed']:
            self.events.append(('local_inhibit',self.now))
        if not self.previous_model_watchdog and self.model.watchdog:
            self.events.append(('model_watchdog',self.now))
        self.previous_local=bool(self.fw.state['armed']); self.previous_model_watchdog=self.model.watchdog
        assert self.fw.diag[-2]==0
    def advance(self,ms,**kwargs):
        for _ in range(math.ceil(ms/10)): self.step(**kwargs)
    def command(self,kind,body=b'',success=True,**step_options):
        assert self.peer.request(kind,body)
        seq=(self.peer.sequence+1)&0xffffffff
        for _ in range(20):
            self.step(**step_options)
            if (kind,seq) in self.peer.replies:
                result=self.peer.replies[(kind,seq)]
                assert result==success,(kind,seq,result,self.fw.state)
                return result
        raise AssertionError(('no command reply',kind,seq,self.fw.state,self.peer.replies))
    def connect(self,sid=1,force_mask=0):
        self.peer.connect(sid,self.now)
        for _ in range(8):
            self.step()
            if self.peer.live: break
        assert self.peer.live and not self.fw.state['armed']
        self.force_mask=force_mask
        self.command(p.CONFIG,bytes((1,0,255,force_mask)))
        self.advance(80)
    def activate(self,mask=255):
        self.command(p.RECOVER)
        self.command(p.ARM)
        self.advance(80)
        self.command(p.ENABLE,bytes((mask,)))
        self.advance(20)
        assert all(m['output']==0.0 for m in self.fw.motors)
    def target(self,entries): self.command(p.TARGET,p.targets(entries))
    def raw(self,kind,body=b'',session=None,sequence=None,stamp=None,wire=None):
        if sequence is None:
            self.peer.sequence=(self.peer.sequence+1)&0xffffffff; sequence=self.peer.sequence
        if wire is None:
            wire=p.encode(kind,self.peer.session if session is None else session,sequence,
                          self.peer.fw_stamp if stamp is None else stamp,body)
        self.fw.call('RX '+wire.hex()); self.fw.call('STEP 1 1 1 0')
        for frame in self.fw.frames: self.down.offer(frame,self.now)
        self.fw.frames.clear()
        return wire
    def stopped(self):
        assert not self.fw.state['armed'] and all(not m['enabled'] and m['output']==0 for m in self.fw.motors)
    def close(self): self.fw.close()
