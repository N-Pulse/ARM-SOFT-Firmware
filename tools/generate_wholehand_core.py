#!/usr/bin/env python3
"""Generate the native boundary with explicitly configured simulation gains.
The production code is read, never modified. Controller and HAL harness hashes
are recorded so the bench can be associated with its exact input sources.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re

# EDIT THESE THREE VALUES FOR GAZEBO TESTS, then rebuild and restart Gazebo.
# Applied to all eight motors in the generated SIM copy only.
# The production fw/motors/motor_control/motor_pid_config.c is never edited.
# Units: kp = %/rad, ki = %/(rad*s), kd = %*s/rad.
# Set SIM_PID_GAINS = None to use the production coefficient table instead.
SIM_PID_GAINS = {"kp": 5.0, "ki": 0.0, "kd": 1}


def generate(root,out,api):
    gains = SIM_PID_GAINS
    if gains is not None:
        if set(gains) != {'kp', 'ki', 'kd'}:
            raise ValueError('SIM_PID_GAINS must contain exactly kp, ki and kd.')
        gains = {name: float(value) for name, value in gains.items()}
        if any(not math.isfinite(value) or value<0 or value>3.4028234e38 for value in gains.values()):
            raise ValueError('Simulation gains must be finite, non-negative and fit in a C float.')
    control_path=root/'fw/motors/motor_control/motor_control.c'
    harness_path=root/'tests/simulation/native_firmware.c'
    source=control_path.read_text();harness=harness_path.read_text()
    generated,count=re.subn(r'const\s+motor_pid_config_t\s+sim_rotor_pid\s*=\s*\{.*?\};',
        'motor_pid_config_t sim_rotor_pid = g_motor_pid_config[i];\n'
        '        sim_rotor_pid.max_dt_s = N_PULSE_NATIVE_SIM_PID_MAX_DT_S;',source,flags=re.S)
    if count!=1:raise ValueError('Expected one SIM PID initializer.')
    if gains is not None:
        generated=generated.replace(
            'motor_pid_config_t sim_rotor_pid = g_motor_pid_config[i];',
            'motor_pid_config_t sim_rotor_pid = g_motor_pid_config[i];\n'
            + '\n'.join(f'        sim_rotor_pid.{name} = {gains[name]:.9e}f;' for name in ('kp','ki','kd')),1)
    old='int main(int argc,char **argv)'
    if harness.count(old)!=1:raise ValueError('Unexpected native HAL harness main.')
    harness=harness.replace(old,'int npulse_unused_cli_main(int argc,char **argv)',1)
    harness+='\n'+api.read_text()
    out.mkdir(parents=True,exist_ok=True)
    (out/'motor_control.c').write_text('/* GENERATED: production algorithm; explicit SIM gains; original unchanged. */\n'+generated)
    (out/'native_firmware.c').write_text(harness)
    hashes={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (control_path,harness_path,root/'fw/motors/motor_control/motor_pid_config.c')}
    (out/'inputs.json').write_text(json.dumps(dict(profile='lockstep-wholehand',hashes=hashes,
        simulation_pid_gains=gains,
        note='Native HAL/RTOS boundary; simulated time. Not a hardware timing test.'),indent=2)+'\n')
    print('Native whole-hand gains: '+('production table' if gains is None else
        ', '.join(f'{name}={gains[name]:g}' for name in ('kp','ki','kd'))+' on all eight motors (SIM only)'))


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    args=p.parse_args()
    generate(args.root,args.out,args.root/'simulation/ros2_ws/src/npulse_sim_control/src/hand_api.c.inc')
