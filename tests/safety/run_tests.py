"""Host tests of production interlock/control/PID/guard/driver and feedback.

HAL/BSP/clock are mocked. The asynchronous fixture emulates absent delivery;
the second binary also exercises the actual HW backend refresh/output path.
Function instrumentation injects a stop after the REAL PID has calculated.
No production test hook or alternative implementation of safety logic.
"""
from pathlib import Path
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
OUT = ROOT / "build/safety-tests"
CONTROL = ROOT / "fw/motors/motor_control"


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    cc = os.environ.get("HOST_CC", "gcc")
    flags = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-g", "-O0",
             "-finstrument-functions", "-I", HERE / "mocks", "-I", CONTROL,
             "-I", ROOT / "fw/bsp/encoder", "-I", ROOT / "fw/comm",
             "-I", ROOT / "fw/comm-stack", "-I", ROOT / "fw/app"]
    sources = [CONTROL / (name + ".c") for name in (
        "motor_control", "motor_safety", "motor_guard", "motor_feedback",
        "drv8848", "motor_outputs", "motor_pid", "motor_pid_config")]
    sources.append(ROOT / "fw/comm/comms_session.c")
    for mode in ("async", "hw", "disarm", "encoder-integration"):
        binary = OUT / (mode + (".exe" if os.name == "nt" else ""))
        extra = ["-DTEST_HW_BACKEND", CONTROL / "motor_backend_hw.c"] if mode in ("hw", "encoder-integration") else []
        subprocess.run([str(a) for a in [cc, *flags, *extra, *sources,
                       HERE / {"disarm": "test_disarm.c", "encoder-integration": "test_encoder_integration.c"}.get(mode, "test_safety.c"), "-o", binary, "-lm"]], cwd=ROOT, check=True)
        subprocess.run([str(binary)], cwd=ROOT, check=True)

    sys.path.insert(0, str(ROOT / "tests/startup"))
    from sanitizers import run_sanitized
    run_sanitized(ROOT, OUT, [*flags, "-DTEST_HW_BACKEND", CONTROL / "motor_backend_hw.c", *sources, HERE / "test_encoder_integration.c", "-lm"], [[]])

if __name__ == "__main__":
    main()
