"""Fictitious eight independent rotors, NOT a wrist differential model.

u is the embedded PID's signed percent. Euler integration at exactly 10 ms:
acceleration = 0.2*u - 4*velocity. STOP cancels u; inertia then decays.
The model owns its own command watchdog, even if the host endpoint is paused.
"""
class RotorModel:
    STEP_S = 0.01
    ACCEL_PER_PERCENT = 0.2
    DAMPING_PER_S = 4.0
    WATCHDOG_MS = 100
    def __init__(self):
        self.position = [0.0]*8; self.velocity = [0.0]*8; self.applied = [0.0]*8
        self.run = None; self.last_command = 0; self.watchdog = False
    def arm(self, run, now):
        self.run = run; self.last_command = now; self.applied = [0.0]*8; self.watchdog = False
    def stop(self):
        self.run = None; self.applied = [0.0]*8
    def apply(self, run, outputs, now):
        if self.run is not None and now-self.last_command > self.WATCHDOG_MS:
            self.stop(); self.watchdog = True
        if self.run != run or run is None:
            return False
        self.applied = list(outputs); self.last_command = now
        return True
    def step(self, now):
        if self.run is not None and now-self.last_command > self.WATCHDOG_MS:
            self.stop(); self.watchdog = True
        for i in range(8):
            self.velocity[i] += (self.ACCEL_PER_PERCENT*self.applied[i] - self.DAMPING_PER_S*self.velocity[i])*self.STEP_S
            self.position[i] += self.velocity[i]*self.STEP_S
    def feedback(self, force_mask=0, force=0, ids=None):
        return [(i, 11 | (4 if force_mask & (1<<i) else 0), self.position[i], self.velocity[i],
                 force if force_mask & (1<<i) else 0, self.applied[i]) for i in (range(8) if ids is None else ids)]
