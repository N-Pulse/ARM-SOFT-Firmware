#!/usr/bin/env python3
"""One fresh zero-gravity whole-hand benchmark, reusing test_pid_sweep.py.

Measures simulated/wall-clock speed from the physics timestamps in the CSV and
one ROS START-to-active acknowledgement. The latter includes native arming and
telemetry publication; it is NOT EEG/EMG-to-motion latency or a hard-real-time test.
"""
from __future__ import annotations
import argparse
import csv
from datetime import datetime
import importlib.util
import json
import math
from pathlib import Path
import statistics
import sys
import time


def percentile(values, fraction):
    if not values:
        return None
    values = sorted(values)
    position = (len(values)-1)*fraction
    lo, hi = math.floor(position), math.ceil(position)
    return values[lo]+(values[hi]-values[lo])*(position-lo)


def speed_metrics(rows):
    start = next((i for i,r in enumerate(rows) if r.get('phase')=='2'), None)
    if start is None:
        raise ValueError('No active firmware samples.')
    active = []
    run = rows[start]['run']
    for row in rows[start:]:
        if row['phase']!='2' or row['run']!=run:
            break
        active.append(row)
    wall = [int(r['wall_ns'])/1e9 for r in active]
    sim = [float(r['sim_ms'])/1000 for r in active]
    if len(wall)<2 or wall[-1]<=wall[0] or sim[-1]<=sim[0]:
        raise ValueError('Not enough progressing samples for a speed measurement.')
    if any(b<=a for a,b in zip(wall,wall[1:])) or any(b<a for a,b in zip(sim,sim[1:])):
        raise ValueError('Clock discontinuity; refusing to combine resets.')
    windows=[]
    first=0
    for i in range(1,len(wall)):
        if wall[i]-wall[first]>=1:
            windows.append((sim[i]-sim[first])/(wall[i]-wall[first]))
            first=i
    return dict(active_samples=len(active),active_wall_s=wall[-1]-wall[0],
        active_sim_s=sim[-1]-sim[0],real_time_factor=(sim[-1]-sim[0])/(wall[-1]-wall[0]),
        one_wall_second_windows=len(windows),rtf_window_p10=percentile(windows,.1),
        rtf_window_median=percentile(windows,.5),rtf_window_p90=percentile(windows,.9),
        note='Ratio uses paired physics-sample timestamps during one active run. '
             'RTF=1 means equal simulated/wall duration. This does not measure every control tick or certify deadlines.')


def start_probe(destination):
    # Runs in the isolated ROS domain inherited from the sweep.
    import rclpy
    from rclpy.qos import QoSProfile,ReliabilityPolicy
    from npulse_sim_control.msg import HandCommand,HandState
    rclpy.init()
    node=rclpy.create_node('npulse_start_timing_probe')
    publisher=node.create_publisher(HandCommand,'/fw_hand_controller/hand_command',1)
    sent=started=active=last_state=None
    ages=[]
    bad_ages=0
    result={'measurement':'ROS START publisher to observed native-firmware activation',
        'scope':'Includes arming and telemetry. Not EEG/EMG-to-motion latency.',
        'monotonic_clock':time.get_clock_info('monotonic').implementation}

    def receive(msg):
        nonlocal started,active,last_state,bad_ages
        received=time.monotonic_ns()
        last_state=msg
        if sent is None:
            return
        if msg.phase in (1,2) and started is None:
            started=received
        if msg.phase==2:
            if active is None:
                active=received
            age=(received-int(msg.wall_ns))/1e6
            if 0<=age<=5000:
                ages.append(age)
            else:
                bad_ages+=1
        if msg.phase==3:
            raise RuntimeError(f'Firmware fault during timing probe: causes={msg.causes}')

    subscription=node.create_subscription(HandState,'/fw_hand_controller/hand_state',receive,
        QoSProfile(depth=1,reliability=ReliabilityPolicy.BEST_EFFORT))
    try:
        deadline=time.monotonic()+25
        while time.monotonic()<deadline:
            rclpy.spin_once(node,timeout_sec=.02)
            if sent is None and last_state is not None and publisher.get_subscription_count()>0:
                if last_state.phase==2:
                    raise RuntimeError('Controller was already active before START; not a fresh trial.')
                command=HandCommand();command.enabled=True
                sent=time.monotonic_ns()
                publisher.publish(command)
            if active is not None and (len(ages)>=20 or time.monotonic_ns()-active>1_000_000_000):
                break
        if sent is None or active is None:
            raise RuntimeError('No START-to-active transition observed within 25 seconds.')
        result.update(start_to_first_started_state_observed_ms=(started-sent)/1e6,
            start_to_active_state_observed_ms=(active-sent)/1e6,
            telemetry_samples_near_start=len(ages),invalid_cross_clock_samples=bad_ages,
            physics_sample_to_probe_receipt_p50_ms=percentile(ages,.5),
            physics_sample_to_probe_receipt_p95_ms=percentile(ages,.95),
            telemetry_note='First active samples near START, including ROS publication. '
                           'Not native C feedback age or control jitter. One START is not a worst-case bound.')
        destination.write_text(json.dumps(result,indent=2)+'\n')
        print(json.dumps(result,indent=2),flush=True)
    finally:
        node.destroy_node()
        rclpy.shutdown()


def load_runner(root):
    path=root/'tools/test_pid_sweep.py'
    if not path.is_file():
        raise ValueError(f'Missing existing sweep script: {path}')
    spec=importlib.util.spec_from_file_location('npulse_timing_sweep',path)
    runner=importlib.util.module_from_spec(spec);spec.loader.exec_module(runner)
    return runner


def summarize(output):
    metadata=json.loads((output/'sweep.json').read_text())
    trials=[]
    for trial in metadata['trials']:
        directory=output/trial['trial']
        item={k:trial.get(k) for k in ('trial','kp','ki','kd','status','passed_stages','reason')}
        trace=directory/'trace.csv'
        if trace.exists():
            with trace.open(newline='') as f:
                rows=[r for r in csv.DictReader(f) if None not in r and all(v is not None for v in r.values())]
            try:item['speed']=speed_metrics(rows)
            except ValueError as error:item['speed_error']=str(error)
        probe=directory/'start_timing.json'
        if probe.exists():item['start_timing']=json.loads(probe.read_text())
        trials.append(item)
    report=dict(conditions='Zero gravity; native C control; GUI mode not recorded in original sweep metadata.',
        restoration=metadata.get('restoration'),hardware_sources_unchanged=metadata.get('hardware_sources_unchanged'),
        limits='Finite-run observation on this VM. No hard-real-time or external EEG/EMG-to-motion certification.',trials=trials)
    (output/'timing_summary.json').write_text(json.dumps(report,indent=2)+'\n')
    for item in trials:
        print(f'\n{item["trial"]}: {item["status"]}; stages={item["passed_stages"]}')
        if 'speed' in item:
            speed=item['speed']
            print(f'  Simulated duration: {speed["active_sim_s"]:.2f} s')
            print(f'  Wall-clock duration: {speed["active_wall_s"]:.2f} s')
            print(f'  REAL-TIME FACTOR: {speed["real_time_factor"]:.3f}')
            if speed['rtf_window_p10'] is not None:
                print(f'  1 s window RTF p10 / median / p90: {speed["rtf_window_p10"]:.3f} / {speed["rtf_window_median"]:.3f} / {speed["rtf_window_p90"]:.3f}')
        if 'start_timing' in item:
            t=item['start_timing']
            print(f'  START -> active state observed: {t["start_to_active_state_observed_ms"]:.1f} ms (arming + telemetry included)')
            if t['physics_sample_to_probe_receipt_p50_ms'] is not None:
                print(f'  Physics sample -> probe receipt p50 / p95: {t["physics_sample_to_probe_receipt_p50_ms"]:.1f} / {t["physics_sample_to_probe_receipt_p95_ms"]:.1f} ms (near START)')
    print(f'\nSaved: {output/"timing_summary.json"}')
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    parser.add_argument('--output',type=Path)
    parser.add_argument('--headless',action='store_true',help='Measure without the Gazebo GUI; default includes it.')
    parser.add_argument('--repeat',type=int,default=1)
    parser.add_argument('--probe',type=Path,help=argparse.SUPPRESS)
    parser.add_argument('--analyse-only',type=Path,help='Print speed from an existing sweep; latency only if recorded.')
    args=parser.parse_args()
    if args.probe:
        start_probe(args.probe.resolve());return 0
    if args.analyse_only:
        summarize(args.analyse_only.resolve());return 0
    if not 1<=args.repeat<=5:parser.error('repeat must be 1..5')
    root=args.root.resolve()
    runner=load_runner(root)
    output=(args.output or root/'results/hand-feedback'/datetime.now().strftime('timing-%Y%m%d-%H%M%S')).resolve()
    original_command=runner.run_command

    def command_with_probe(command,cwd,env,log_path,timeout):
        if command[:4]==['ros2','topic','pub','--once']:
            original_command([sys.executable,str(Path(__file__).resolve()),'--probe',str(log_path.parent/'start_timing.json')],
                             cwd,env,log_path,35)
        else:
            original_command(command,cwd,env,log_path,timeout)

    runner.run_command=command_with_probe
    runner.analyse=lambda *_:None  # No plot rendering needed for this short timing benchmark.
    options=argparse.Namespace(kp=[5.0],kd=[1.0],repeat=args.repeat,gui=not args.headless,
        dry_run=False,ros_domain_id=87,startup_timeout=150,trial_timeout=600)
    lock_path=root/'build/simulation/pid-sweep.lock';lock_path.parent.mkdir(parents=True,exist_ok=True)
    import fcntl
    with lock_path.open('a') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        try:
            result=runner.run_sweep(root,output,options)
        finally:
            if (output/'sweep.json').exists():
                summarize(output)
                meta=json.loads((output/'timing_summary.json').read_text())
                meta['conditions']='Zero gravity; native C control; '+('headless server' if args.headless else 'Gazebo GUI enabled')
                (output/'timing_summary.json').write_text(json.dumps(meta,indent=2)+'\n')
    return result or 0


if __name__=='__main__':
    try:sys.exit(main())
    except (ValueError,RuntimeError,OSError,ImportError) as error:
        print(f'ERROR: {error}',file=sys.stderr);sys.exit(1)
