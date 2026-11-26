#!/usr/bin/env python3
"""Generate the native boundary and select HW coefficients at CMake configure.
The production code is read, never modified. Controller and HAL harness hashes
are recorded so the bench can be associated with its exact input sources.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re


def generate(root,out,api):
    control_path=root/'fw/motors/motor_control/motor_control.c'
    harness_path=root/'tests/simulation/native_firmware.c'
    source=control_path.read_text();harness=harness_path.read_text()
    generated,count=re.subn(r'const\s+motor_pid_config_t\s+sim_rotor_pid\s*=\s*\{.*?\};',
        'motor_pid_config_t sim_rotor_pid = g_motor_pid_config[i];\n'
        '        sim_rotor_pid.max_dt_s = N_PULSE_NATIVE_SIM_PID_MAX_DT_S;',source,flags=re.S)
    if count!=1:raise ValueError('Expected one SIM PID initializer.')
    old='int main(int argc,char **argv)'
    if harness.count(old)!=1:raise ValueError('Unexpected native HAL harness main.')
    harness=harness.replace(old,'int npulse_unused_cli_main(int argc,char **argv)',1)
    harness+='\n'+api.read_text()
    out.mkdir(parents=True,exist_ok=True)
    (out/'motor_control.c').write_text('/* GENERATED: production algorithm with HW gains; original unchanged. */\n'+generated)
    (out/'native_firmware.c').write_text(harness)
    hashes={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (control_path,harness_path,root/'fw/motors/motor_control/motor_pid_config.c')}
    (out/'inputs.json').write_text(json.dumps(dict(profile='lockstep-index',hashes=hashes,
        note='Native HAL/RTOS boundary; simulated time. Not a hardware timing test.'),indent=2)+'\n')


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    args=p.parse_args()
    generate(args.root,args.out,args.root/'simulation/ros2_ws/src/npulse_sim_control/src/sync_api.c.inc')
