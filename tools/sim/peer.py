"""Host SIM endpoint. Producers submit work; one I/O owner constructs/writes frames.

No automatic configuration, recovery, ARM, ENABLE or target on connection.
All times passed here are monotonic integer milliseconds (injectable in the PC bench).
"""
import collections
import struct
import threading
from . import protocol as p

class Peer:
    def __init__(self,*,continuous_clock=False):
        self.continuous_clock=continuous_clock
        self.lock = threading.RLock()
        self.parser = p.Parser()
        self.session = self.sequence = self.rx_sequence = 0
        self.live = self.configured = False
        self.run = self.epoch = self.causes = self.enabled = 0
        self.permitted_run = None
        self.pending_arm = None
        self.commands = collections.deque(maxlen=8)
        self.latest_feedback = None
        self.outputs = [0.0]*8
        self.last_fw = self.fw_stamp = self.last_command = 0
        self.clock_fw = self.clock_local = 0
        self.last_hb = -40
        self.last_feedback_sent = -20
        self.hello_sent = 0
        self.hello_pending = False
        self.stop_pending = False
        self.replies = {}
        self.output_revision = 0
        self.replaced = self.rejected = 0
        self.watchdog = ''

    def connect(self, session, now):
        with self.lock:
            if session <= self.session:
                raise ValueError('each connection requires a greater 64-bit session ID')
            self.session = session; self.sequence = self.rx_sequence = 0
            self.live = self.configured = False
            self.run = self.epoch = self.causes = self.enabled = 0
            self._safe('connect')
            self.commands.clear(); self.latest_feedback = None; self.replies.clear()
            self.hello_pending = True; self.hello_sent = now
            self.last_fw = now; self.fw_stamp = 0; self.stop_pending = False
            self.clock_fw = 0; self.clock_local = now

    def _safe(self, reason):
        self.outputs = [0.0]*8
        self.permitted_run = self.pending_arm = None
        self.enabled = 0
        self.commands.clear()
        self.watchdog = reason

    def poll(self, now):
        with self.lock:
            self.parser.poll(now)
            if self.live and now-self.last_fw > p.LINK_MS:
                self.live = False; self._safe('session watchdog')
                self.commands.clear(); self.latest_feedback = None
            elif self.permitted_run is not None and now-self.last_command > p.COMMAND_MS:
                self._safe('command watchdog')
                self.commands.clear()

    def request(self, kind, body=b''):
        with self.lock:
            if kind == p.STOP:
                self._safe('local stop'); self.commands.clear(); self.latest_feedback = None
                self.stop_pending = True; return True
            if kind not in (p.CONFIG, p.RECOVER, p.ARM, p.ENABLE, p.TARGET, p.INTENT):
                raise ValueError('explicit command required')
            if not self.live or len(self.commands) == self.commands.maxlen:
                return False
            self.commands.append((kind, bytes(body)))
            return True

    def submit_feedback(self, entries, captured_ms):
        with self.lock:
            # Validate the entire batch before replacing the pending sample.
            p.feedback(self.run, 0, entries)
            if self.latest_feedback is not None:
                self.replaced += 1
            self.latest_feedback = (list(entries), captured_ms)

    def outgoing(self, now):
        """Called by the ONE writer only. Complete frames, bounded work, no backlog."""
        with self.lock:
            self.poll(now)
            if self.hello_pending:
                self.hello_pending = False
                return [p.encode(p.HELLO, self.session, 0, 0)]
            if not self.live:
                return []
            work = []
            if self.stop_pending:
                self.stop_pending = False; work.append((p.STOP, b''))
            elif self.commands:
                work.append(self.commands.popleft())
            if self.latest_feedback is not None and now-self.last_feedback_sent >= 20:
                self.last_feedback_sent = now
                entries, captured = self.latest_feedback; self.latest_feedback = None
                if 0 <= now-captured <= p.SAMPLE_MS and self.configured:
                    work.append((p.FEEDBACK, p.feedback(self.run, now-captured, entries)))
            if now-self.last_hb >= 40:
                self.last_hb = now; work.append((p.HEARTBEAT, b''))
            frames = []
            for kind, body in work:
                self.sequence = (self.sequence+1) & 0xffffffff
                if kind == p.ARM:
                    self.pending_arm = self.sequence
                # Native clocks share a rate, but C samples its scheduler tick
                # before servicing IPC. Keep the existing 80 ms lease strict:
                # a 20 ms scheduling margin avoids a future-dated wire stamp.
                stamp=(max(self.clock_fw,self.clock_fw+now-self.clock_local-20)&0xffffffff
                       if self.continuous_clock else self.fw_stamp)
                frames.append(p.encode(kind, self.session, self.sequence, stamp, body))
            return frames

    def receive(self, data, now):
        with self.lock:
            self.poll(now)
            for kind, sid, seq, stamp, body in self.parser.feed(data, now):
                if sid != self.session or not p.newer(seq, self.rx_sequence):
                    self.rejected += 1; continue
                if kind not in (p.HELLO_ACK, p.STATUS, p.OUTPUT, p.STOP):
                    self.rejected += 1; continue
                if kind == p.HELLO_ACK:
                    if self.live or now-self.hello_sent > p.LEASE_MS or len(body) != 21:
                        self.rejected += 1; continue
                elif not self.live:
                    self.rejected += 1; continue
                else:
                    # Fixed Hello clock anchor: permit bounded serial jitter/skew,
                    # never move the anchor forward on a replayed status.
                    age = (self.clock_fw + now-self.clock_local - stamp) & 0xffffffff
                    if age >= 0x80000000:
                        age -= 0x100000000
                    if not -20 <= age <= p.LEASE_MS:
                        self.rejected += 1; continue
                if kind in (p.STATUS, p.HELLO_ACK):
                    if len(body) != 21:
                        self.rejected += 1; continue
                    run, epoch, causes, mask, armed, config, req, ack, result = struct.unpack('<IIIBBBBIB', body)
                    if armed not in (0,1) or config not in (0,1) or result not in (0,1):
                        self.rejected += 1; continue
                    if kind == p.HELLO_ACK and (req != p.HELLO or ack != 0 or not result or armed or config or mask):
                        self.rejected += 1; continue
                    if kind == p.HELLO_ACK:
                        self.clock_fw = stamp; self.clock_local = now
                    self.live = True; self.configured = bool(config)
                    changed=run!=self.run or causes!=self.causes
                    previously_permitted=self.permitted_run is not None or self.enabled!=0
                    self.run, self.epoch, self.causes = run, epoch, causes
                    if (not armed or causes) and (changed or previously_permitted):
                        self._safe('firmware inhibited')
                    if req == p.ARM and ack == self.pending_arm:
                        if result and armed and not causes:
                            self.permitted_run = run; self.last_command = now; self.watchdog = ''
                        self.pending_arm = None
                    if req:
                        self.replies[(req, ack)] = bool(result)
                        if len(self.replies) > 16:
                            self.replies.pop(next(iter(self.replies)))
                elif kind == p.STOP:
                    if len(body) != 8:
                        self.rejected += 1; continue
                    self.run, self.causes = struct.unpack('<II', body)
                    self._safe('firmware stop')
                else:
                    if len(body) != 25:
                        self.rejected += 1; continue
                    run, epoch, mask, *values = struct.unpack('<IIB8h', body)
                    if any(abs(v) > 10000 or (not mask & (1 << i) and v != 0) for i,v in enumerate(values)):
                        self.rejected += 1; continue
                    if self.permitted_run != run or epoch != self.epoch:
                        self.rejected += 1; continue
                    self.outputs = [v/100.0 for v in values]; self.enabled = mask
                    self.last_command = now; self.output_revision += 1
                self.rx_sequence = seq; self.last_fw = now; self.fw_stamp = stamp
