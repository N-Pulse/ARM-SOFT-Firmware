#!/usr/bin/env python3
"""One command builds production C and executes the PC closed-loop scenarios.
No hardware, ROS2, Gazebo, pyserial or protobuf is needed.
"""
from pathlib import Path
import argparse
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tests/simulation'))
from bench import build, ROOT, OUT
import os
import subprocess
from scenarios import run_cases

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--case',default='all')
    parser.add_argument('--no-sanitizers',action='store_true')
    args=parser.parse_args()
    binary=build()
    protocol=OUT/("protocol.exe" if os.name=="nt" else "protocol")
    subprocess.run([os.environ.get("HOST_CC","gcc"),"-std=c11","-Wall","-Wextra","-Werror","-I","fw/comm",
                    "fw/comm/sim_protocol.c","tests/simulation/test_protocol.c","-o",str(protocol)],cwd=ROOT,check=True)
    subprocess.run([str(protocol)],check=True)
    from test_host import run
    run()
    run_cases(binary,args.case)
    if not args.no_sanitizers:
        from sanitizer import run_available
        run_available(args.case)
if __name__=='__main__': main()
