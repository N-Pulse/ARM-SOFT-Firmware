#!/usr/bin/env python3
"""The legacy SIM-v1 EMG demo is retired. This entry point uses explicit SIM v2.

No legacy SelectMode, position preview, raw beacons or automatic heartbeat-only
activation is supported. Use tools/uart4_hw_client.py for the unchanged HW wire
format. Importing this module opens no port and does not import pyserial.
"""
from sim_client import main
if __name__=='__main__': main()
