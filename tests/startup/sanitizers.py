"""Probe execution, not just flags; UBSan is independent of ASan availability."""
from pathlib import Path
import os
import shutil
import subprocess

def run_sanitized(root, out, arguments, cases):
    probe = out / 'sanitizer_probe.c'
    probe.write_text('int main(void){return 0;}\n')
    candidates = [os.environ.get('HOST_CC', 'gcc')]
    if shutil.which('clang'): candidates.append(shutil.which('clang'))
    for kind in ('address,undefined', 'undefined'):
        suffix = kind.replace(',', '-')
        chosen = None
        logs = []
        for compiler in candidates:
            binary = out / ('probe-' + suffix + ('.exe' if os.name == 'nt' else ''))
            result = subprocess.run([compiler, '-fsanitize=' + kind, str(probe), '-o', str(binary)], capture_output=True, text=True)
            logs.append(str(result.args) + '\n' + result.stdout + result.stderr)
            if result.returncode == 0:
                executed = subprocess.run([str(binary)], capture_output=True, text=True)
                logs.append(executed.stdout + executed.stderr)
                if executed.returncode == 0: chosen = compiler; break
        (out / ('probe-' + suffix + '.log')).write_text('\n'.join(logs))
        if not chosen:
            print('SKIP ' + kind + ': compiler/runtime unavailable; see probe-' + suffix + '.log')
            continue
        binary = out / ('numeric-' + suffix + ('.exe' if os.name == 'nt' else ''))
        subprocess.run([chosen, '-fsanitize=' + kind, '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', *map(str, arguments), '-o', str(binary)], cwd=root, check=True)
        for case in cases: subprocess.run([str(binary), *case], cwd=root, check=True)
        print('PASS production numeric/integration with ' + kind)
