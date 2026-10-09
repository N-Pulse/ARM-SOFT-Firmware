#!/usr/bin/env python3
"""Run the independent fictitious rotor model; localhost UDP only."""
import argparse
import time
from sim.model_server import ModelServer

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port',type=int,default=48160)
    args=parser.parse_args(); server=ModelServer(args.port)
    print('Fictitious rotor-v1 model on localhost UDP',server.port,flush=True)
    try:
        while True: server.poll(); time.sleep(.002)
    except KeyboardInterrupt: pass
    finally: server.close()
if __name__=='__main__': main()
