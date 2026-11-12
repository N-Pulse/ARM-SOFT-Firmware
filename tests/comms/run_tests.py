"""Maintained communication regressions for CRC handshake and streaming intents.
The historical FIFO/PING fixture is retained in test_comms.c for reference only.
No serial port or network listener is opened.
"""
from pathlib import Path
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[2]
def main():
    for args in (("tests/lifecycle/run_tests.py","--handshake","--sanitize"),
                 ("tests/lifecycle/run_tests.py","--sanitize"),
                 ("tests/lifecycle/test_host_codec.py",),
                 ("tests/lifecycle/test_decoding_client.py",)):
        subprocess.run([sys.executable,*args],cwd=ROOT,check=True)
    print("Current handshake/streaming/protobuf regressions passed")
if __name__=="__main__": main()
