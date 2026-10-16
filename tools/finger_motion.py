"""Feedback-paced targets and an explicitly requested single-finger test."""
import math
import shlex

FINGER_IDS = {'thumb': 0, 'index': 1, 'middle': 2, 'ring': 3, 'little': 4}
# Nominal Moon DCU10025P12 + PG10C scale in the common -Fingers firmware.
DEFAULT_ENCODER_QUANTUM = math.tau / (12 * 4 * 61)

def raw_finger(text, default_pwm=3):
    words=shlex.split(text)
    if not words or words[0]!='raw' or len(words)==1:
        return None
    code=int(words[1])
    if code in range(8) and len(words)==2:
        return None  # Existing raw encoder diagnostics.
    if code not in (*range(10,15),*range(20,25)) or not 2<=len(words)<=4:
        raise ValueError('raw 10..14/20..24 [ANGLE_DEG [PWM]]')
    angle=float(words[2]) if len(words)>2 else (0. if code>=20 else 30.)
    pwm=int(words[3]) if len(words)>3 else default_pwm
    if not math.isfinite(angle) or not 0<=angle<=90 or not 1<=pwm<=10:
        raise ValueError('finite angle within 0..90 degrees and PWM within 1..10 required')
    return code%10,angle,pwm

def degree_limits(text):
    words = shlex.split(text)
    if len(words) != 6:
        raise ValueError('limits-deg MIN MAX STEP PWM RATE (angles and rate in degrees)')
    minimum, maximum, step, pwm, rate = map(float, words[1:])
    if not all(math.isfinite(v) for v in (minimum, maximum, step, pwm, rate)) or not pwm.is_integer():
        raise ValueError('finite limits and integer PWM required')
    return 'limits {} {} {} {} {}'.format(
        math.radians(minimum), math.radians(maximum), math.radians(step), int(pwm), math.radians(rate))

class QuickFingerTest:
    """One operator-requested setup/move sequence. No retry or automatic rearm."""
    def __init__(self, angle, pwm, maximum=10, diagnostic=False, from_angle=None):
        if not math.isfinite(maximum) or not 0 < maximum <= 90:
            raise ValueError('--max-angle must be within 0..90 degrees, excluding zero')
        if angle is None or not math.isfinite(angle) or not 0 <= angle <= maximum:
            raise ValueError('--run requires --angle within 0..--max-angle (default 10 degrees)')
        if not 1 <= pwm <= (40 if diagnostic else 5):
            raise ValueError('--run PWM must be within 1..5 percent, or 1..40 with --diagnostic')
        self.diagnostic=diagnostic
        if from_angle is not None and (not diagnostic or not math.isfinite(from_angle) or not 0<=from_angle<=maximum):
            raise ValueError('--from-angle requires --diagnostic and an estimated position within 0..--max-angle')
        self.from_angle=from_angle
        self.maximum=math.radians(maximum)
        self.commands=('link-recover',degree_limits(f'limits-deg 0 {maximum} 5 {pwm} 45'))+\
            ((f'estimate {math.radians(from_angle)}',) if from_angle is not None else ())+\
            ('preflight verified','arm','enable','move')
        self.index=0; self.ready_after=math.inf; self.deadline=math.inf

    def begin(self, now):
        self.ready_after=now; self.deadline=now+3

    def acknowledged(self, now):
        self.ready_after=now; self.deadline=now+3

    def next_command(self, d, epoch, diagnostic_time, now):
        if self.index==len(self.commands): return None
        if now>self.deadline:
            raise ValueError('test setup timed out waiting for fresh encoder/status; no retry')
        if d is None or d.epoch!=epoch or diagnostic_time<self.ready_after or now-diagnostic_time>.3:
            return None
        if not d.flags&(1<<23): raise ValueError('--run requires the current -Fingers image on BOTH boards')
        if self.diagnostic and not d.flags&(1<<9): raise ValueError('--diagnostic requires build/flash -Fingers -Diagnostic on BOTH boards')
        if not self.diagnostic and d.flags&(1<<9): raise ValueError('diagnostic firmware loaded: pass --diagnostic explicitly or flash the normal -Fingers image')
        if not d.flags&(1<<16): raise ValueError('connect the PC to the MASTER COM port')
        if d.flags&(1<<22): raise ValueError('thumb is blocked by the loaded firmware; rebuild/reflash the current -Fingers image on BOTH boards for the bench test')
        timed=self.diagnostic and bool(d.flags&(1<<14))
        if self.from_angle is not None and not timed: raise ValueError('--from-angle requires the new timed diagnostic firmware')
        if not timed and d.flags&28!=28: return None
        if not math.isfinite(d.reference_rad if timed else d.encoder_rad): return None
        if not self.diagnostic and not 0<=d.encoder_rad<=self.maximum:
            raise ValueError('measured position is outside the requested test limits')
        if self.index and d.faults: return None  # One recovery only; wait for its feedback.
        command=self.commands[self.index]
        required=0 if command.startswith(('link-recover','limits ')) else 32
        if command in ('arm','enable','move'): required |= 1<<20
        if command in ('enable','move'): required |= 2
        if command=='move': required |= 1
        if d.flags&required!=required: return None
        self.index+=1
        self.ready_after=math.inf  # Next step waits for ACK AND refreshed status.
        return command

class FingerMotion:
    def __init__(self, motor):
        if motor not in range(5):
            raise ValueError('single-finger motion requires motor ID 0..4')
        self.motor = motor
        self.limits = None
        self.encoder_quantum = None
        self.active = False
        self.completed = False

    def accept_limits(self, text):
        """Called only after the firmware ACKed this exact limits request."""
        words = shlex.split(text)
        if len(words) == 6 and words[0] == 'limits':
            self.limits = tuple(map(float, words[1:]))

    def cancel(self):
        self.active = False
        self.completed = False

    def accept_calibration(self, text):
        """An optional acknowledged override replaces the nominal resolution."""
        words = shlex.split(text)
        if len(words) == 6 and words[0] == 'calibrate':
            self.encoder_quantum = math.tau / (int(words[1]) * int(words[2]) * float(words[3]))

    def start(self, degrees, pwm, diagnostic):
        if not math.isfinite(degrees) or not 0 <= degrees <= 90:
            raise ValueError('angle must be finite and within 0..90 degrees')
        if not isinstance(pwm, int) or not 1 <= pwm <= 10:
            raise ValueError('PWM must be an integer within 1..10 percent')
        if self.limits is None:
            raise ValueError('enter limits/limits-deg and wait for OK in this console first')
        self._check(diagnostic)
        minimum, maximum, step, ceiling, _ = self.limits
        goal = math.radians(degrees)
        if not minimum <= goal <= maximum or pwm > ceiling:
            raise ValueError('requested angle/PWM exceeds the acknowledged limits')
        if diagnostic.flags & (1 << 21) and step < .005:
            raise ValueError('remote feedback resolution requires a step limit of at least 0.005 rad')
        self.goal, self.pwm, self.step = goal, pwm, step
        self.epoch = diagnostic.epoch
        self.waypoint = diagnostic.target_rad
        self.tolerance = max(.0015 if diagnostic.flags & (1 << 21) else .00001, min(.005, step * .1))
        quantum = self.encoder_quantum
        if quantum is None and diagnostic.flags & (1 << 23):
            quantum = DEFAULT_ENCODER_QUANTUM
        if quantum is not None:
            # A target between encoder counts cannot converge more closely
            # than half a count, plus the remote 1-mrad rounding error.
            self.tolerance = max(self.tolerance, quantum / 2 +
                                 (.0005 if diagnostic.flags & (1 << 21) else 0) + 1e-7)
            if .4 * step <= self.tolerance:
                raise ValueError('step limit is too small for the encoder resolution')
        self.active, self.completed = True, False

    def _check(self, d):
        if d is None or d.motor_id != self.motor:
            raise ValueError('wait for selected-finger diagnostics; check the -Finger firmware build')
        if d.flags & (1 << 18):
            raise ValueError('ordinary firmware inhibits motion; build with -Finger NAME')
        if d.flags & (1 << 22):
            raise ValueError('thumb is blocked by the loaded firmware; rebuild/reflash the current -Fingers image on BOTH boards for the bench test')
        if d.faults:
            raise ValueError('STOP/fault is latched; inspect status and recover explicitly')
        if (d.flags & 63) != 63 or not d.flags & (1 << 20):
            raise ValueError('calibration, limits, fresh feedback, preflight, arm and enable are required')
        if not all(math.isfinite(v) for v in (d.encoder_rad, d.reference_rad, d.target_rad)):
            raise ValueError('finite encoder, reference and target feedback required')

    def next_target(self, d):
        if not self.active:
            return None
        self._check(d)
        if d.epoch != self.epoch:
            raise ValueError('STOP epoch changed; movement cancelled')
        # Wait for acknowledgement AND measured/reference convergence at each
        # waypoint. A new target never jumps ahead of the measured finger.
        if any(abs(v - self.waypoint) > self.tolerance for v in (d.target_rad, d.encoder_rad, d.reference_rad)):
            return None
        if abs(self.goal - d.encoder_rad) <= self.tolerance and abs(self.goal - d.reference_rad) <= self.tolerance:
            self.active, self.completed = False, True
            return None
        # Leave margin for transport latency and the slave's 1-mrad telemetry.
        distance = .4 * self.step
        lower = max(d.encoder_rad, d.reference_rad) - distance
        upper = min(d.encoder_rad, d.reference_rad) + distance
        target = min(upper, max(lower, self.goal))
        if abs(target - self.waypoint) <= self.tolerance:
            return None
        self.waypoint = target
        return target

class DiagnosticFingerMotion(FingerMotion):
    """One direct target; the diagnostic firmware owns PWM and its 2 s cutoff."""
    def _check(self,d):
        if d is None or not d.flags&(1<<14): return super()._check(d)
        if d.motor_id!=self.motor or not d.flags&(1<<9) or d.flags&((1<<18)|(1<<22)):
            raise ValueError('selected timed diagnostic firmware is required')
        if d.faults: raise ValueError('STOP/fault is latched; timed movement cancelled')
        if d.flags&51!=51 or not d.flags&(1<<20):
            raise ValueError('limits, preflight, arm and enable are required')
        if not all(math.isfinite(v) for v in (d.reference_rad,d.target_rad)):
            raise ValueError('estimated reference is unavailable')

    def start(self,degrees,pwm,diagnostic):
        if not math.isfinite(degrees) or not 0<=degrees<=90 or not isinstance(pwm,int) or not 1<=pwm<=40:
            raise ValueError('diagnostic target must be 0..90 degrees and PWM 1..40 percent')
        self._check(diagnostic)
        if not diagnostic.flags&(1<<9): raise ValueError('dedicated diagnostic firmware is required')
        if self.limits is None or pwm>self.limits[3]: raise ValueError('acknowledged diagnostic limits/PWM required')
        self.goal=math.radians(degrees); self.pwm=pwm
        self.timed=bool(diagnostic.flags&(1<<14))
        self.origin=diagnostic.reference_rad if self.timed else diagnostic.encoder_rad; self.epoch=diagnostic.epoch
        self.tolerance=.0016 if diagnostic.flags&(1<<21) else DEFAULT_ENCODER_QUANTUM/2+1e-7
        self.sent=False; self.active=True; self.completed=False

    def next_target(self,d):
        if not self.active: return None
        if self.timed:
            if d is None or d.motor_id!=self.motor or not math.isfinite(d.reference_rad):
                raise ValueError('timed diagnostic reference unavailable')
            reached=abs(d.reference_rad-self.goal)<=self.tolerance
            if reached and (not self.sent or (d.faults&1 and not d.flags&3)) and not d.faults&~(1|128):
                self.active=False; self.completed=True; return None
        elif d is None or d.motor_id!=self.motor or not d.flags&4 or not math.isfinite(d.encoder_rad):
            raise ValueError('diagnostic position feedback unavailable')
        else:
            reached=abs(d.encoder_rad-self.goal)<=self.tolerance or \
            (self.goal>self.origin and d.encoder_rad>=self.goal) or \
            (self.goal<self.origin and d.encoder_rad<=self.goal)
        # A target crossing makes the firmware issue STOP locally. MANUAL and
        # its relayed LINK latch are therefore expected at successful arrival.
        if not self.timed and reached and not d.faults&~(1|128):
            self.active=False; self.completed=True; return None
        self._check(d)
        if d.epoch!=self.epoch: raise ValueError('diagnostic STOP before the requested angle')
        if not self.sent:
            self.sent=True; return self.goal
        return None
