"""Integration checks after both real HW and SIM builds already exist."""
from pathlib import Path
import hashlib
import os
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    make = os.environ.get("MAKE") or shutil.which("make") or shutil.which("mingw32-make")
    if not make:
        raise RuntimeError("GNU Make required")
    hw_objects = list(ROOT.glob("build/HW/obj/**/*.o"))
    sim_objects = list(ROOT.glob("build/SIM/obj/**/*.o"))
    assert hw_objects and sim_objects, "Build both configurations first"
    assert {p.relative_to(ROOT / "build/HW/obj") for p in hw_objects} == {
        p.relative_to(ROOT / "build/SIM/obj") for p in sim_objects}, "Incomplete source rebuild"
    objects = hw_objects + sim_objects
    before = {p: (p.stat().st_mtime_ns, digest(p)) for p in objects}
    hardware_alias = ROOT / "Debug/N-PULSE FIRMWARE.elf"
    hw_hash = digest(ROOT / "build/HW/n-pulse.elf")
    for args in [("SIM_MODE=0",), ("SIM_MODE=1",), ("-C", "Debug", "SIM_MODE=0")]:
        result = subprocess.run([make, *args], cwd=ROOT, capture_output=True, text=True)
        assert result.returncode == 0, result.stdout + result.stderr
        assert ' -c "' not in result.stdout, "Unexpected recompilation after mode switch"
        assert digest(hardware_alias) == hw_hash, "SIM replaced the HW legacy path"
    after = {p: (p.stat().st_mtime_ns, digest(p)) for p in objects}
    assert before == after, "Object content or timestamp changed during mode alternation"
    invalid = subprocess.run([make, "-n", "SIM_MODE=2"], cwd=ROOT, capture_output=True, text=True)
    assert invalid.returncode != 0 and "SIM_MODE must be 0" in invalid.stderr
    for unsafe in ('CURRENT_ACQUISITION=1', 'OLD_HW_MOTION_OPT_IN=1'):
        rejected = subprocess.run([make, '-n', unsafe], cwd=ROOT, capture_output=True, text=True)
        assert rejected.returncode != 0 and 'OLD_HW' in rejected.stderr, unsafe
    print(f"HW -> SIM -> HW/Debug: {len(objects)} isolated objects unchanged; HW alias preserved; invalid mode rejected.")
    print('OLD_HW: unmapped current acquisition and unscoped motion opt-in rejected before compilation.')
    timing_sim = subprocess.run([make,'-n','SIM_MODE=1','TIMING_DIAGNOSTICS=1'],cwd=ROOT,capture_output=True,text=True)
    assert timing_sim.returncode!=0 and 'HW-only' in timing_sim.stderr


if __name__ == "__main__":
    main()
