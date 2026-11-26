"""Shared, feedback-driven motion sequences for the desktop UI and console."""
from dataclasses import dataclass
import math
import threading
import time


class MotionCancelled(RuntimeError):
    pass


def check_cancelled(session, cancel):
    if cancel.is_set() or session.stop_event.is_set():
        raise MotionCancelled("Opération arrêtée / connexion fermée.")


def positive(value, name):
    if type(value) not in (int, float) or not math.isfinite(value) or value <= 0:
        raise ValueError(f"{name} : saisir un nombre strictement positif.")
    return value


@dataclass(frozen=True)
class Step:
    label: str
    flex: float
    rotation: float

    @property
    def targets(self):
        # Opposite directions for flexion; same direction for rotation.
        return (self.rotation + self.flex, self.rotation - self.flex)


@dataclass(frozen=True)
class Cycle:
    count: int
    flexion: float
    extension: float
    rotation: float = 180.0

    def __post_init__(self):
        if type(self.count) is not int or self.count < 1:
            raise ValueError("Nombre de cycles : saisir un entier supérieur ou égal à 1.")
        positive(self.flexion, "Amplitude de flexion")
        positive(self.extension, "Amplitude d'extension")
        positive(self.rotation, "Angle de rotation")

    @property
    def steps(self):
        return (Step("Flexion", self.flexion, 0),
                Step("Extension", -self.extension, 0),
                Step("Retour au neutre", 0, 0),
                Step("Rotation", 0, self.rotation),
                Step("Retour au neutre", 0, 0))


@dataclass(frozen=True)
class MotionProfile:
    speed: float
    acceleration: float

    def __post_init__(self):
        positive(self.speed, "Vitesse de consigne")
        positive(self.acceleration, "Accélération de consigne")

    def timing(self, distance):
        if distance <= 0:
            return 0.0, 0.0
        ramp = min(self.speed / self.acceleration, math.sqrt(distance / self.acceleration))
        cruise = max(0.0, (distance - self.acceleration * ramp * ramp) / (self.acceleration * ramp))
        return ramp, cruise

    def duration(self, distance):
        ramp, cruise = self.timing(distance)
        return 2 * ramp + cruise


def validate_profile(status, steps, profile):
    if profile is None:
        return
    previous = tuple(m["target_deg"] if status["target_reached"] else m["position_deg"]
                     for m in status["motors"])
    for step in steps:
        travel = tuple(abs(t - p) for t, p in zip(step.targets, previous))
        distance = max(travel)
        if distance:
            for i, (delta, motor) in enumerate(zip(travel, status["motors"]), 1):
                if not delta:
                    continue
                limits = motor["configuration"]["limits"]
                if profile.duration(distance) + 2 >= limits[4] / 1000:
                    raise ValueError(f"{step.label} : trajet trop long pour le délai M{i} de {limits[4] / 1000:g} s. Passer par le neutre ou adapter le profil.")
        previous = step.targets


def validate_steps(status, steps):
    """Check the entire route against the actual board configuration first.

    Firmware remains authoritative, including measured-position checks at each
    arrival. Only this short route is stored, regardless of repetition count.
    """
    if not status["armed"] or status["fault"] != "none":
        raise ValueError("Armer les moteurs sans défaut avant de lancer un mouvement.")
    for i, motor in enumerate(status["motors"]):
        cfg = motor.get("configuration", {})
        if not all(motor.get(k) for k in ("calibrated", "zeroed", "feedback_valid")) or not all(
                cfg.get(k) for k in ("limits_set", "pid_set", "safety_set")):
            raise ValueError(f"M{i + 1} : calibration, zéro et configuration requis.")
        low, high, max_step, _, _ = cfg["limits"]
        previous = motor["target_deg"]
        for j, step in enumerate(steps):
            target = step.targets[i]
            if not math.isfinite(target) or not low <= target <= high:
                raise ValueError(f"{step.label} : cible M{i + 1} = {target:g}° hors limites [{low:g}, {high:g}]°.")
            if abs(target - previous) > max_step or (j == 0 and abs(target - motor["position_deg"]) > max_step):
                raise ValueError(f"{step.label} : déplacement M{i + 1} supérieur à {max_step:g}° par consigne.")
            previous = target


def wait_for_target(session, cancel, expected=None, timeout=62):
    # A fresh status after the ACK prevents an old reached packet from advancing
    # a sequence. Then use the stream instead of flooding the UART with status.
    status = session.snapshot(cancel=cancel)
    targets = expected if expected is not None else [m["target_deg"] for m in status["motors"]]
    deadline = time.monotonic() + timeout
    while True:
        check_cancelled(session, cancel)
        if status["fault"] != "none" or not status["armed"]:
            raise RuntimeError(f"Mouvement interrompu : {status['fault']}.")
        if any(not math.isclose(m.get("goal_deg", m["target_deg"]), t, rel_tol=1e-6, abs_tol=1e-5)
               for m, t in zip(status["motors"], targets)):
            raise RuntimeError("Mouvement interrompu par une autre consigne.")
        now = time.monotonic()
        if now - status.get("host_monotonic_s", now) > 1.0:
            raise TimeoutError("Télémétrie absente depuis plus d'une seconde.")
        if status["target_reached"]:
            return
        if now >= deadline:
            raise TimeoutError("La position demandée n'a pas été atteinte dans le délai prévu.")
        cancel.wait(.1)
        current, _ = session.current()
        if current is not None:
            status = current


def execute_step(session, cancel, step, profile=None):
    check_cancelled(session, cancel)
    command = f"move wrist {step.flex:.9g} {step.rotation:.9g}"
    if profile is not None:
        status = session.snapshot(cancel=cancel)
        validate_steps(status, (step,))
        validate_profile(status, (step,), profile)
        command += f" {profile.speed:.9g} {profile.acceleration:.9g}"
    session.command(command, cancel=cancel)
    wait_for_target(session, cancel, step.targets)


def run_cycle(session, cycle, cancel, notify=lambda event: None, telemetry_hz=10, profile=None):
    neutral = Step("Neutre initial", 0, 0)
    check_cancelled(session, cancel)
    status = session.snapshot(cancel=cancel)
    validate_steps(status, (neutral,) + cycle.steps)
    validate_profile(status, (neutral,) + cycle.steps, profile)
    try:
        session.command(f"stream on {telemetry_hz}", cancel=cancel)
        notify(dict(kind="progress", completed=0, total=cycle.count, stage=neutral.label, cycle=0))
        execute_step(session, cancel, neutral, profile)
        for number in range(1, cycle.count + 1):
            for step in cycle.steps:
                notify(dict(kind="progress", completed=number - 1, total=cycle.count,
                            stage=step.label, cycle=number))
                execute_step(session, cancel, step, profile)
            notify(dict(kind="progress", completed=number, total=cycle.count,
                        stage="Cycle terminé", cycle=number))
        check_cancelled(session, cancel)
        return f"{cycle.count} cycle(s) terminé(s). Neutre maintenu ; Arrêter libère les moteurs."
    except MotionCancelled:
        session.priority_stop()
        raise
    except BaseException:
        session.emergency_stop()
        raise


def run_move(session, step, cancel, telemetry_hz=10, profile=None):
    check_cancelled(session, cancel)
    status = session.snapshot(cancel=cancel)
    validate_steps(status, (step,))
    validate_profile(status, (step,), profile)
    try:
        session.command(f"stream on {telemetry_hz}", cancel=cancel)
        execute_step(session, cancel, step, profile)
        return f"{step.label} : position atteinte et maintenue."
    except MotionCancelled:
        session.priority_stop()
        raise
    except BaseException:
        session.emergency_stop()
        raise


class MotionRunner:
    """One worker for configuration/motion; stop bypasses ordinary ACK waits."""
    def __init__(self, session, notify=lambda event: None):
        self.session = session
        self.notify = notify
        self.cancel = threading.Event()
        self.thread = None
        self.lock = threading.Lock()

    @property
    def busy(self):
        return self.thread is not None and self.thread.is_alive()

    def start(self, action):
        with self.lock:
            if self.busy:
                raise ValueError("Une opération est en cours. Attendre sa fin ou cliquer sur Arrêter.")
            self.cancel = threading.Event()
            self.thread = threading.Thread(target=self._run, args=(action, self.cancel), daemon=True)
            self.thread.start()

    def _run(self, action, cancel):
        try:
            message = action(cancel)
            check_cancelled(self.session, cancel)
            self.notify(dict(kind="done", message=message or "Opération terminée."))
        except MotionCancelled as exc:
            self.notify(dict(kind="cancelled", message=str(exc)))
        except Exception as exc:
            self.notify(dict(kind="error", message=str(exc)))

    def stop(self, emergency=False):
        self.cancel.set()
        if emergency:
            self.session.emergency_stop()
        else:
            self.session.priority_stop()

    def join(self, timeout=2):
        if self.thread is not None:
            self.thread.join(timeout)
