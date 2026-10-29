#!/usr/bin/env python3
"""Build/run host tests and validate C-produced JSON using the real PC parser."""
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import motor_bench


def main():
    compiler = shutil.which("gcc") or shutil.which("clang")
    if not compiler:
        raise SystemExit("Native GCC or Clang is required for the C software tests")
    output = ROOT / "build" / "bench-tests"
    output.mkdir(parents=True, exist_ok=True)
    common = ["fw/bench/bench.c", "fw/bench/bench_encoder.c",
              "fw/motors/motor_control/motor_pid.c"]
    for name in ("core", "protocol", "hw"):
        executable = output / (name + (".exe" if sys.platform == "win32" else ""))
        sources = common + [f"tests/bench/test_{name}.c"]
        includes = ["-Ifw/bench", "-Ifw/motors/motor_control"]
        if name == "hw":
            sources = ["fw/bench/bench_hw.c", "tests/bench/test_hw.c"]
            includes.insert(0, "-Itests/bench/mocks")
        if name == "protocol":
            sources.append("fw/bench/bench_protocol.c")
        command = [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", *includes,
                   *sources, "-lm", "-o", str(executable)]
        print(f"Building native {name} tests", flush=True)
        subprocess.run(command, cwd=ROOT, check=True)
        result = subprocess.run([str(executable)], cwd=ROOT, check=True,
                                capture_output=True, text=True)
        if name == "protocol":
            packets = [line for line in result.stdout.splitlines() if line.startswith("{")]
            if not packets:
                raise RuntimeError("protocol tests emitted no JSON packets")
            for line in packets:
                motor_bench.parse_packet(line)
            print(f"Native protocol tests passed; {len(packets)} packets accepted by PC parser", flush=True)
            (output / "protocol_output.jsonl").write_text("\n".join(packets) + "\n", encoding="ascii")
        else:
            print(result.stdout.strip(), flush=True)
    subprocess.run([sys.executable, "-m", "unittest", "discover", "-s", "tests/bench",
                    "-p", "test_*.py", "-v"], cwd=ROOT, check=True)
    print("All bench software tests passed; no physical hardware was tested.")


if __name__ == "__main__":
    main()
