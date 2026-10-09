#!/usr/bin/env python3
"""Explicit manual SIM v2 client, UART4 115200 8N1. Never a HW client."""
import argparse
import time
from sim.client import SerialWorker,submit_command,milliseconds
from sim.model_server import ModelConnection

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port',required=True,help='confirmed SIM UART port; never auto-enumerated')
    parser.add_argument('--model-port',type=int,default=48160,help='independent rotor model localhost UDP port')
    args=parser.parse_args(); model=ModelConnection(args.model_port)
    worker=SerialWorker(args.port,on_state=model.update); worker.start()
    print('SIM v2. Wait for Hello ACK; no automatic activation. Commands:')
    print('status | config | recover | arm | enable 0xff | target ID rad limit | stop | reconnect | quit')
    try:
        while worker.running:
            text=input('sim> ').strip()
            if text=='quit': break
            if text=='status':
                with worker.peer.lock:
                    print('session',worker.peer.session,'live',worker.peer.live,'configured',worker.peer.configured,
                          'run',worker.peer.run,'permitted',worker.peer.permitted_run,'causes',worker.peer.causes,
                          'replies',worker.peer.replies,'watchdog',worker.peer.watchdog)
                continue
            if text=='reconnect':
                worker.peer.connect(max(worker.peer.session+1,time.time_ns()),milliseconds()); continue
            try: print('queued; inspect status for reply' if submit_command(worker.peer,text) else 'refused/full; not delivered')
            except ValueError as exc: print(exc)
    except (KeyboardInterrupt,EOFError): pass
    finally: worker.close(); model.close()
    if worker.error: raise worker.error
if __name__=='__main__': main()
