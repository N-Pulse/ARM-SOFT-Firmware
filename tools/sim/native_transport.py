"""SIM v2 binary IPC to a monotonic-clock native C firmware process.
The realtime process never runs a rotor model and owns its own safety clock.
Bounded pipe writes/queues fail closed; frozen/dead children are terminated.
"""
import os
from pathlib import Path
import queue
import subprocess
import threading

class NativeTransport:
    continuous_clock=True # C and Python use the same PC monotonic clock rate
    baud_limited=False # binary IPC has no physical UART serialization delay
    def __init__(self,binary):
        binary=Path(binary).resolve()
        if not binary.is_file(): raise ValueError('build native firmware first: python tools/build_native_sim.py')
        self.process=subprocess.Popen([str(binary),'--realtime'],stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,bufsize=0)
        self.rx=queue.Queue(maxsize=8); self.jobs=queue.Queue(maxsize=1)
        self.closed=False; self.error=None; self.remaining=b''
        self.reader=threading.Thread(target=self._read,name='native-ipc-rx',daemon=True)
        self.writer=threading.Thread(target=self._write,name='native-ipc-tx',daemon=True)
        self.reader.start(); self.writer.start()
    def _fail(self,exc):
        self.error=exc
        if self.process.poll() is None: self.process.terminate()
    def _read(self):
        try:
            while not self.closed:
                data=os.read(self.process.stdout.fileno(),256)
                if not data: raise OSError('native firmware exited')
                self.rx.put_nowait(data)
        except (OSError,queue.Full) as exc:
            if not self.closed: self._fail(exc)
    def _write(self):
        while not self.closed:
            try: data,done,result=self.jobs.get(timeout=.05)
            except queue.Empty: continue
            try:
                written=self.process.stdin.write(data)
                if written!=len(data): raise OSError('short native IPC write')
                result.append(written)
            except OSError as exc: self._fail(exc)
            finally: done.set()
    def read(self,n):
        if self.error: raise OSError(f'native IPC failure: {self.error}')
        if not self.remaining:
            try: self.remaining=self.rx.get_nowait()
            except queue.Empty: return b''
        data,self.remaining=self.remaining[:n],self.remaining[n:]
        return data
    def write(self,data):
        if self.closed or self.error: raise OSError('native IPC unavailable')
        if not 1<=len(data)<=151: raise ValueError('bounded SIM frame required')
        done=threading.Event(); result=[]
        self.jobs.put_nowait((bytes(data),done,result))
        if not done.wait(.025) or not result:
            self._fail(OSError('native IPC write timeout')); raise OSError('native IPC write timeout')
        return result[0]
    def close(self):
        self.closed=True
        if self.process.poll() is None:
            self.process.stdin.close()
            try: self.process.wait(timeout=.2)
            except subprocess.TimeoutExpired: self.process.terminate()
        self.process.wait(timeout=1)
        self.reader.join(timeout=.2); self.writer.join(timeout=.2)
        self.process.stdout.close()
