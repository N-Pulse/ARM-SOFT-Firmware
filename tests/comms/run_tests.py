"""Compile real parser/protobuf/session/interlock/UART worker with mocked I/O.

No serial port is opened. Sanitizers are probed locally; none are installed.
"""
from pathlib import Path
import os
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
OUT = ROOT / 'build/comms-tests'
CTRL = ROOT / 'fw/motors/motor_control'
COMM = ROOT / 'fw/comm'
PROTO = ROOT / 'Middlewares/Third_Party/EmbeddedProto/src'

def run(args):
    subprocess.run([str(x) for x in args], cwd=ROOT, check=True)

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    cc, cxx = os.environ.get('HOST_CC', 'gcc'), os.environ.get('HOST_CXX', 'g++')
    includes = [HERE / 'mocks', ROOT / 'tests/safety/mocks', CTRL, COMM,
                ROOT / 'fw/comm-stack', ROOT / 'fw/app', ROOT / 'fw/bsp/encoder',
                ROOT / 'fw/motors/motor_map', PROTO]
    flags = ['-Wall', '-Wextra', '-Werror', '-g', '-O0']
    for p in includes: flags += ['-I', p]
    sources = [CTRL / (s+'.c') for s in ('motor_control', 'motor_safety', 'motor_guard',
        'motor_feedback', 'drv8848', 'motor_outputs', 'motor_pid', 'motor_pid_config')]
    sources += [COMM / (s+'.c') for s in ('comm', 'comms_parser', 'comms_session',
        'uart4_transport', 'board_uart_compat')]
    sources += [ROOT / 'fw/app/intent_router.c', ROOT / 'fw/motors/motor_map/motor_map.c',
                HERE / 'test_comms.c']
    objects = []
    for index, source in enumerate(sources):
        obj = OUT / f'c{index}.o'
        run([cc, '-std=c11', '-finstrument-functions', *flags, '-c', source, '-o', obj])
        objects.append(obj)
    cpp_sources = [ROOT / 'fw/comm-stack/rx.cpp', ROOT / 'fw/comm-stack/tx.cpp', *PROTO.glob('*.cpp')]
    for index, source in enumerate(cpp_sources):
        obj = OUT / f'cpp{index}.o'
        # GCC 6 does not implement EmbeddedProto's maybe_unused attribute.
        run([cxx, '-std=c++14', *flags, '-Wno-attributes', '-Wno-unused-parameter',
             '-c', source, '-o', obj])
        objects.append(obj)
    binary = OUT / ('comms.exe' if os.name == 'nt' else 'comms')
    run([cxx, *objects, '-o', binary, '-lm']); run([binary])

    # Try ASan/UBSan on installed compilers. A compile AND execution are required.
    probe = OUT / 'sanitizer_probe.c'; probe.write_text('int main(void){return 0;}\n')
    candidates = [cc]
    clang = shutil.which('clang')
    if clang: candidates.append(clang)
    chosen = None
    for compiler in candidates:
        target = OUT / ('sanitizer_probe.exe' if os.name == 'nt' else 'sanitizer_probe')
        args = [compiler, '-fsanitize=address,undefined', str(probe), '-o', str(target)]
        built = subprocess.run(args, capture_output=True, text=True)
        (OUT / 'sanitizer-probe.log').write_text(built.stdout + built.stderr)
        if built.returncode == 0 and subprocess.run([str(target)], capture_output=True).returncode == 0:
            chosen = compiler; break
    sim = OUT / ('sim-bounds.exe' if os.name == 'nt' else 'sim-bounds')
    san_flags = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer'] if chosen else []
    run([chosen or cc, '-std=c11', *flags, *san_flags, CTRL / 'motor_sim_feedback.c',
         HERE / 'test_sim_bounds.c', '-o', sim, '-lm']); run([sim])
    result = 'ASan+UBSan SIM bounds passed' if chosen else 'ASan/UBSan unavailable locally; SIM bounds tested without sanitizers'
    print(result); (OUT / 'sanitizer-result.txt').write_text(result + '\n')
    # SIM v1 TX semantics were deliberately replaced by v2. Test actual staging,
    # priority STOP, late output rejection and native closed loop in lot 6.
    run(['python', ROOT / 'tools/run_simulation.py', '--case',
         'stop_saturation,inflight_stop,tx_timeout', '--no-sanitizers'])
    # Exercise the new host codec only, without importing pyserial/opening a port.
    import importlib.util
    spec = importlib.util.spec_from_file_location('uart4_hw_client', ROOT / 'tools/uart4_hw_client.py')
    client = importlib.util.module_from_spec(spec)
    try:
        spec.loader.exec_module(client)
    except ModuleNotFoundError as exc:
        if exc.name not in ('google', 'google.protobuf'):
            raise
        print('Optional HW host codec SKIPPED: Python protobuf is not installed; no serial port opened')
        return
    assert client.action_frame(1) == bytes.fromhex('aa0612040a020801')
    assert client.action_frame(0) == bytes.fromhex('aa0412020a00')
    hello = client.hello_frame()
    msg = client.pb.DeviceMessage(); msg.ParseFromString(hello[2:])
    assert hello[0] == 0xaa and hello[1] == len(hello) - 2 and msg.hello.proto_ver == 1
    print('HW host codec: existing protobuf bindings, Hello and action IDs 0/1 passed; no serial port opened')

if __name__ == '__main__': main()
