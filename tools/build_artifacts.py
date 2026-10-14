"""Small portable filesystem operations for GNU Make (including Windows)."""
from pathlib import Path
import shutil
import sys
import hashlib
import json

ROOT = Path(__file__).resolve().parents[1]


def build_path(value):
    path = (ROOT / value).resolve()
    if not path.is_relative_to(ROOT / "build"):
        raise ValueError(f"Refusing operation outside build/: {path}")
    return path


def main():
    action, *args = sys.argv[1:]
    if action == "mkdir":
        build_path(args[0]).mkdir(parents=True, exist_ok=True)
    elif action == "stamp":
        path = build_path(args[0])
        content = args[1] + "\n"
        if not path.exists() or path.read_text() != content:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content)
    elif action == "objects":
        build_path(args[0]).write_text("\n".join(f'"{p}"' for p in args[1:]) + "\n")
    elif action == "publish":
        config, source = args
        if config not in ("HW", "SIM"):
            raise ValueError("Unknown configuration")
        directory = build_path(source)
        flags = (directory / "compile.flags").read_bytes()
        sim_defined = b"-DMOTOR_BACKEND_SIM=1" in flags
        if (sim_defined != (config == "SIM") or b"-DN_PULSE_ENABLE_CURRENT_ACQUISITION=0" not in flags
                or b"-DN_PULSE_HW_SINGLE_MOTOR_TEST=0" not in flags
                or b"-DN_PULSE_OLD_HW_MOTION_OPT_IN=0" not in flags
                or b"-DN_PULSE_TIMING_DIAGNOSTICS=0" not in flags
                or directory != ROOT / "build" / config):
            raise ValueError("Refusing inconsistent/default-ADC build publication")
        manifest = {"schema": 1, "mode": config,
                    "hardware": "STM32G474RET6", "milestone": "OLD_HW_LOT4",
                    "physical_outputs": "permanently_disabled",
                    "elf_sha256": hashlib.sha256((directory / "n-pulse.elf").read_bytes()).hexdigest(),
                    "flags_sha256": hashlib.sha256(flags).hexdigest()}
        (directory / "build-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
        destination = ROOT / "Debug" / ("SIM" if config == "SIM" else "")
        destination.mkdir(parents=True, exist_ok=True)
        for extension in ("elf", "map", "list"):
            shutil.copy2(build_path(source) / f"n-pulse.{extension}",
                         destination / f"N-PULSE FIRMWARE.{extension}")
    elif action == "clean":
        path = build_path(args[0])
        import re
        if path.parent != ROOT / "build" or not re.fullmatch(r'(HW|SIM)(-single-[0-4](-optin)?)?(-timing)?', path.name):
            raise ValueError(f"Refusing unknown build configuration: {path}")
        # Never touch old Debug objects, tracked .su files, or sources.
        # Remove only generated artifacts in the selected configuration.
        if path.exists():
            for artifact in path.rglob("*"):
                if artifact.is_file() and (artifact.suffix in (
                        ".o", ".d", ".su", ".cyclo", ".elf", ".map", ".list")
                        or artifact.name in ("compile.flags", "build-manifest.json")):
                    artifact.unlink()
    else:
        raise ValueError(f"Unknown operation: {action}")


if __name__ == "__main__":
    main()
