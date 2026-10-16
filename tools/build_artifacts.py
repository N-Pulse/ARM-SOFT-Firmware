"""Small portable filesystem operations for GNU Make (including Windows)."""
from pathlib import Path
import shutil
import sys
import hashlib
import json
import re

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
    elif action == "finger-manifest":
        source, motor_text = args
        if motor_text not in ('0', '1', '2', '3', '4', 'all'):
            raise ValueError("Single-finger image requires motor ID 0..4")
        selectable = motor_text == 'all'
        motor = None if selectable else int(motor_text)
        directory = build_path(source)
        profile = 'HW-fingers-optin' if selectable else f'HW-single-{motor}-optin'
        flags = (directory / 'compile.flags').read_bytes()
        definitions = re.findall(rb'(?:^|[\s|])-D([A-Za-z_]\w*)=([^\s|]+)', flags)
        if len(dict(definitions)) != len(definitions):
            raise ValueError("Duplicate build definitions")
        expected = {'N_PULSE_OLD_HW': '1', 'N_PULSE_TARGET_RET6': '1', 'N_PULSE_DUAL_BOARD_LINK': '1',
                    'N_PULSE_OLD_HW_MOTION_OPT_IN': '1', 'N_PULSE_HW_SINGLE_MOTOR_TEST': '1',
                    'N_PULSE_ENABLE_CURRENT_ACQUISITION': '0', 'N_PULSE_THUMB_EXTI_MOTION_QUALIFIED': '0',
                    'N_PULSE_TIMING_DIAGNOSTICS': '0'}
        if not selectable:
            expected['N_PULSE_HW_TEST_MOTOR'] = motor_text
        values = dict(definitions)
        diagnostic=values.get(b'N_PULSE_HW_FINGER_DIAGNOSTIC',b'0')==b'1'
        if diagnostic: profile+='-diagnostic'
        if directory != ROOT/'build'/profile or (diagnostic and not selectable):
            raise ValueError('Refusing a finger manifest outside its dedicated build directory')
        if values.get(b'N_PULSE_HW_FINGER_DIAGNOSTIC',b'0') not in (b'0',b'1'):
            raise ValueError('Invalid diagnostic profile flag')
        if diagnostic and any(values.get(k.encode()) not in (b'0',b'1') for k in
                ('N_PULSE_DIAG_CHECK_GUARD','N_PULSE_DIAG_CHECK_TRAVEL','N_PULSE_DIAG_CHECK_STEP','N_PULSE_DIAG_CHECK_PREFLIGHT','N_PULSE_DIAG_CHECK_ENCODER')):
            raise ValueError('Missing or invalid diagnostic checks')
        if (b'MOTOR_BACKEND_SIM' in flags or any(values.get(k.encode()) != v.encode() for k, v in expected.items())
                or values.get(b'N_PULSE_HW_SELECTABLE_FINGER', b'0') != (b'1' if selectable else b'0')
                or not re.fullmatch(rb'(?:[1-9]|[1-3][0-9]|40)' if diagnostic else rb'(?:[1-9]|10)', values.get(b'N_PULSE_HW_TEST_PWM_CAP', b''))):
            raise ValueError("Refusing inconsistent single-finger build flags")
        elf = (directory / 'n-pulse.elf').read_bytes()
        if len(elf) < 52 or elf[:6] != b'\x7fELF\x01\x01' or elf[16:20] != b'\x02\x00\x28\x00':
            raise ValueError("An executable 32-bit little-endian ARM ELF is required")
        manifest = {'schema': 1, 'mode': 'HW', 'hardware': 'STM32G474RET6', 'milestone': 'OLD_HW_LOT4',
                    'physical_outputs': 'single_finger_diagnostic' if diagnostic else 'single_finger_commissioning', 'finger_motor_id': motor,
                    'finger_selection': 'runtime' if selectable else 'build',
                    'thumb_motion': 'bench_test_max_5_percent' if selectable else 'requires_exti_qualification',
                    'elf_sha256': hashlib.sha256(elf).hexdigest(), 'flags_sha256': hashlib.sha256(flags).hexdigest()}
        if diagnostic:
            manifest['thumb_motion']='diagnostic_direct_pwm_max_40_percent'
            manifest['diagnostic_max_run_ms']=2000
            manifest['diagnostic_checks']={name:values[('N_PULSE_DIAG_CHECK_'+name.upper()).encode()]==b'1'
                                           for name in ('guard','travel','step','preflight','encoder')}
            manifest['diagnostic_motion']='measured' if manifest['diagnostic_checks']['encoder'] else 'timed_estimate'
        (directory / 'build-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
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
        if path.parent != ROOT / "build" or not re.fullmatch(r'(HW|SIM)(-single-[0-4](-optin)?)?(-timing)?|HW-fingers-optin(-diagnostic)?', path.name):
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
