"""Native controller/protocol/register tests, then Python client tests. No hardware."""
from pathlib import Path
import importlib.util
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
import wrist_bench

def main():
    cc = shutil.which("gcc") or shutil.which("clang")
    if not cc:
        raise SystemExit("Native GCC or Clang required")
    out = ROOT / "build/tests"
    out.mkdir(parents=True, exist_ok=True)
    for name in ("controller", "hw"):
        sources = ["firmware/bench_hw.c", "tests/test_hw.c"] if name == "hw" else [
            "firmware/wrist.c", "firmware/bench_encoder.c", "firmware/motor_pid.c",
            "firmware/wrist_protocol.c", "tests/test_controller.c"]
        exe = out / (name + (".exe" if sys.platform == "win32" else ""))
        subprocess.run([cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-Itests/mocks", "-Ifirmware",
                        *sources, "-lm", "-o", str(exe)], cwd=ROOT, check=True)
        result = subprocess.run([str(exe)], cwd=ROOT, text=True, capture_output=True)
        if result.returncode:
            raise RuntimeError(f"{name}: {result.stdout}\n{result.stderr}")
        packets = [line for line in result.stdout.splitlines() if line.startswith("{")]
        for line in packets:
            wrist_bench.parse_packet(line)
        if name == "controller":
            if not packets:
                raise RuntimeError("Missing production protocol output")
            (out / "protocol.jsonl").write_text("\n".join(packets)+"\n", encoding="utf-8")
        print(f"{name}: passed ({len(packets)} JSON packets validated)", flush=True)
    subprocess.run([sys.executable, "-m", "unittest", "discover", "-s", "tests", "-p", "test_*.py", "-v"],
                   cwd=ROOT, check=True)
    print("Software checks passed; no real motor or wiring validated.")

if __name__ == "__main__":
    main()
