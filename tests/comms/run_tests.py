"""Maintained communication regressions for CRC handshake and streaming intents.
The historical FIFO/PING fixture is retained in test_comms.c for reference only.
No serial port or network listener is opened.
"""
from pathlib import Path
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[2]
def main():
    out=ROOT/'build/comms-tests'
    out.mkdir(parents=True,exist_ok=True)
    subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror',
                    '-fsanitize=address,undefined','-g',
                    '-Itests/comms/mocks','-Itests/safety/mocks',
                    '-Ifw/comm','-Ifw/motors/motor_control',
                    'fw/comm/haptics_uart2.c','tests/comms/test_haptics_uart2.c',
                    '-o',str(out/'haptics-uart2')],cwd=ROOT,check=True)
    subprocess.run([str(out/'haptics-uart2')],cwd=ROOT,check=True)
    for args in (("tests/lifecycle/run_tests.py","--handshake","--sanitize"),
                 ("tests/lifecycle/run_tests.py","--sanitize"),
                 ("tests/lifecycle/test_host_codec.py",),
                 ("tests/lifecycle/test_decoding_client.py",)):
        subprocess.run([sys.executable,*args],cwd=ROOT,check=True)
    print("Current handshake/streaming/protobuf regressions passed")
if __name__=="__main__": main()
