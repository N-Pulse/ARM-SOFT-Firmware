#!/usr/bin/env python3
"""Run fresh Gazebo whole-hand trials for a Kp/Kd grid, then plot measured results.

Run on the Ubuntu VM with ROS and the project overlay sourced. This is an
operator test, not a firmware change. Temporary SIM gain/world/launch edits are
backed up and restored, including a rebuild of the original configuration.
"""
from __future__ import annotations

import argparse
import ast
import contextlib
import csv
from datetime import datetime
import fcntl
import hashlib
import html
import importlib.util
import itertools
import json
import math
import os
from pathlib import Path
import re
import signal
import subprocess
import sys
import time
import xml.etree.ElementTree as ET

MOTORS = ('thumb', 'index', 'middle', 'ring', 'little', 'wrist_x', 'wrist_y', 'palm')
TITLES = ('Thumb', 'Index', 'Middle', 'Ring', 'Little', 'Wrist motor A', 'Wrist motor B', 'Palm motor')
REPRESENTATIVE = ('THUMB_CLOSE', 'INDEX_CLOSE', 'MIDDLE_CLOSE', 'RING_CLOSE',
                  'LITTLE_CLOSE', 'WRIST_PALM', 'WRIST_PALM', 'CLOSE_HAND')
STAGES = ('HOLD_CURRENT', 'INDEX_OPEN', 'INDEX_CLOSE', 'MIDDLE_OPEN', 'MIDDLE_CLOSE',
          'RING_OPEN', 'RING_CLOSE', 'LITTLE_OPEN', 'LITTLE_CLOSE', 'THUMB_OPEN',
          'THUMB_CLOSE', 'OPEN_HAND', 'CLOSE_HAND', 'PINCH', 'WRIST_PALM',
          'WRIST_DORSI', 'ROTATE_WRIST_R', 'ROTATE_WRIST_L', 'NEUTRAL')
CONTEXT = 'Zero gravity; provisional mechanics; native C control; Ki=0'
MAX_GAP = 0.25


def digest(data):
    return hashlib.sha256(data).hexdigest()


def dump(path, value):
    path.write_text(json.dumps(value, indent=2, allow_nan=False) + '\n')


def replace_gains(source, kp, kd):
    if not all(math.isfinite(v) and 0 <= v <= 3.4028234e38 for v in (kp,kd)):
        raise ValueError('Gains must be finite, non-negative C floats.')
    tree = ast.parse(source)
    matches = [n for n in tree.body if isinstance(n, ast.Assign)
               and any(isinstance(t, ast.Name) and t.id == 'SIM_PID_GAINS' for t in n.targets)]
    if len(matches) != 1:
        raise ValueError('Expected one top-level SIM_PID_GAINS assignment. Use the editable-gains generator.')
    n = matches[0]
    lines = source.splitlines(keepends=True)
    replacement = f'SIM_PID_GAINS = {{"kp": {kp!r}, "ki": 0.0, "kd": {kd!r}}}\n'
    updated = ''.join(lines[:n.lineno-1]) + replacement + ''.join(lines[n.end_lineno:])
    ast.parse(updated)
    return updated


def zero_gravity(source):
    root = ET.fromstring(source)
    if len(root.findall('./world/gravity')) != 1:
        raise ValueError('Expected exactly one world and gravity element in the current bench world.')
    updated, count = re.subn(r'(<gravity\b[^>]*>)[^<]*(</gravity>)', r'\g<1>0 0 0\2', source)
    if count != 1:
        raise ValueError('Unexpected gravity formatting.')
    return updated


def headless_launch(source):
    # Current launch passes '-r ' + world to ros_gz_sim. Change only that literal.
    tree = ast.parse(source)
    matches = [n for n in ast.walk(tree) if isinstance(n, ast.Constant) and n.value == '-r ']
    if len(matches) != 1:
        raise ValueError('Cannot identify the Gazebo -r argument. Use --gui with this launch version.')
    n = matches[0]
    lines = source.splitlines(keepends=True)
    if n.lineno != n.end_lineno:
        raise ValueError('Unexpected launch string formatting.')
    line = lines[n.lineno-1]
    # AST column offsets are UTF-8 byte offsets.
    encoded = line.encode()
    lines[n.lineno-1] = (encoded[:n.col_offset] + b"'-r -s '" + encoded[n.end_col_offset:]).decode()
    updated = ''.join(lines)
    ast.parse(updated)
    return updated


def read_csv(path):
    if not path.exists():
        return []
    with path.open(newline='') as f:
        return [r for r in csv.DictReader(f) if None not in r and all(v is not None for v in r.values())]


def active_rows(rows):
    # Exactly one contiguous native activation; no stitching of restarts or clock resets.
    start = next((i for i, r in enumerate(rows) if r.get('phase') == '2'), None)
    if start is None:
        return []
    run = rows[start]['run']
    selected = []
    previous = None
    for r in rows[start:]:
        if r['phase'] != '2' or r['run'] != run:
            break
        ms = float(r['sim_ms'])
        if previous is not None and ms < previous:
            raise ValueError('Physics time moved backwards inside an activation.')
        previous = ms
        for m in MOTORS:
            for field in ('target', 'position', 'velocity', 'requested_percent',
                          'applied_percent', 'feedback_age_ms', 'kp', 'ki', 'kd'):
                if not math.isfinite(float(r[f'{m}_{field}'])):
                    raise ValueError(f'Non-finite {m}_{field}.')
        selected.append(r)
    return selected


def split_stages(rows):
    groups = []
    for row in rows:
        key = (row['revision'], row['command'])
        if not groups or groups[-1][0] != key:
            groups.append((key, []))
        groups[-1][1].append(row)
    return groups


def metrics_for_stage(samples, motor, previous_target):
    t0 = float(samples[0]['sim_ms']) / 1000
    t = [float(r['sim_ms'])/1000-t0 for r in samples]
    q = [math.degrees(float(r[f'{motor}_position'])) for r in samples]
    targets = [math.degrees(float(r[f'{motor}_target'])) for r in samples]
    if max(targets)-min(targets) > 1e-3:
        raise ValueError('A motor target changed within one command revision.')
    target = targets[0]
    error = [target-p for p in q]
    intervals = [(i, b-a) for i, (a, b) in enumerate(zip(t, t[1:])) if 0 < b-a <= MAX_GAP]
    duration = sum(dt for _, dt in intervals)
    rmse = math.sqrt(sum(error[i]**2*dt for i, dt in intervals)/duration) if duration else None
    gaps = sum(b-a > MAX_GAP for a, b in zip(t, t[1:]))
    step = target-previous_target if previous_target is not None else 0.0
    moving = abs(step) >= 1.0
    overshoot = None
    rise = None
    settling = None
    if moving:
        direction = math.copysign(1, step)
        overshoot = max(0.0, max(direction*(p-target) for p in q))
        progress = [(p-previous_target)/step for p in q]
        t10 = next((x for x, p in zip(t, progress) if p >= .1), None)
        t90 = next((x for x, p in zip(t, progress) if p >= .9), None)
        if t10 is not None and t90 is not None:
            rise = t90-t10
        # Final continuously observed position band, held >=0.5 simulated second.
        # A gap invalidates continuity, even when both end samples are in-band.
        band_start = None
        for i, e in enumerate(error):
            if abs(e) > 2:
                band_start = None
            elif band_start is None or (i and t[i]-t[i-1] > MAX_GAP):
                band_start = t[i]
        if band_start is not None and t[-1]-band_start >= .5:
            settling = band_start
    tail = [e for x, e in zip(t, error) if x >= max(0, t[-1]-.5)]
    return dict(moving=moving, target_deg=target, initial_position_deg=q[0],
                step_deg=step, observed_duration_s=t[-1], rmse_deg=rmse,
                overshoot_deg=overshoot, rise_10_90_s=rise,
                settling_2deg_s=settling, final_error_deg=error[-1],
                tail_peak_to_peak_deg=max(tail)-min(tail), telemetry_gaps=gaps,
                max_feedback_age_ms=max(float(r[f'{motor}_feedback_age_ms']) for r in samples),
                max_applied_percent=max(abs(float(r[f'{motor}_applied_percent'])) for r in samples))


def verify_trace(rows, kp, kd):
    active = active_rows(rows)
    if not active:
        raise ValueError('No active C firmware samples in CSV.')
    for r in active:
        for motor in MOTORS:
            for name, expected in (('kp', kp), ('ki', 0.0), ('kd', kd)):
                got = float(r[f'{motor}_{name}'])
                if not math.isclose(got, expected, rel_tol=1e-6, abs_tol=1e-6):
                    raise ValueError(f'GAIN MISMATCH: {motor} {name}={got}, expected {expected}.')
    return active


def load_plotter(root):
    path = root/'tools/plot_hand_feedback.py'
    spec = importlib.util.spec_from_file_location('npulse_existing_plotter', path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def render_trial(root, directory, meta):
    plotter = load_plotter(root)
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    args = argparse.Namespace(output=directory/'plots', context=CONTEXT,
        label=f'Measured: Kp={meta["kp"]:g}, Kd={meta["kd"]:g}',
        motor='index', max_gap=MAX_GAP, guide_deg=2.0, dpi=160)
    args.output.mkdir(exist_ok=True)
    trial = plotter.read_trial(directory/'trace.csv')
    plotter.style(plt)
    # Avoid printing 22 paths for every trial.
    with (directory/'plot.log').open('w') as log, contextlib.redirect_stdout(log):
        for kind in ('position', 'error', 'effort'):
            plotter.plot_overview(plt, trial, None, args, kind)
        for motor in MOTORS:
            args.motor = motor
            plotter.plot_detail(plt, trial, args)
        plotter.write_metrics(trial, args)


def write_table(path, rows):
    if not rows:
        return
    fields = list(dict.fromkeys(key for row in rows for key in row))
    with path.open('w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def render_comparison(output, records):
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    plt.rcParams.update({'font.family':'DejaVu Sans', 'font.size':10,
        'axes.spines.top':False, 'axes.spines.right':False,
        'axes.grid':True, 'grid.alpha':.18, 'pdf.fonttype':42})
    fig, axes = plt.subplots(2, 4, figsize=(17.78, 10))
    fig.subplots_adjust(left=.065, right=.975, top=.79, bottom=.13, hspace=.43, wspace=.32)
    fig.suptitle('PID tuning: command-aligned responses', x=.065, y=.965,
                 ha='left', fontsize=23, fontweight='bold', color='#172B45')
    fig.text(.065,.922,CONTEXT, color='#526175')
    colors = plt.get_cmap('tab10').colors
    handles = {}
    for i, (motor, title, command) in enumerate(zip(MOTORS, TITLES, REPRESENTATIVE)):
        ax = axes.flat[i]
        reference_target = None
        longest = 0.0
        count = 0
        for j, record in enumerate(records):
            if not record.get('comparable'):
                continue
            group = next((s for (_, name), s in record['groups'] if name == command), None)
            if not group:
                continue
            target = math.degrees(float(group[0][f'{motor}_target']))
            if reference_target is None:
                reference_target = target
            if abs(target-reference_target) > .01:
                continue  # Different targets are not a matched comparison.
            origin = float(group[0]['sim_ms'])
            t = [(float(r['sim_ms'])-origin)/1000 for r in group]
            q = [math.degrees(float(r[f'{motor}_position'])) for r in group]
            xx, yy = [], []
            for k, (x,y) in enumerate(zip(t,q)):
                if k and x-t[k-1] > MAX_GAP:
                    xx.append(float('nan')); yy.append(float('nan'))
                xx.append(x); yy.append(y)
            longest = max(longest, t[-1])
            label = f'Kp {record["kp"]:g}, Kd {record["kd"]:g}'
            if record.get('repeat',1) != 1:
                label += f' (repeat {record["repeat"]})'
            if record['status'] != 'PASSED':
                label += ' [incomplete trial]'
            line, = ax.plot(xx, yy, color=colors[j%len(colors)], lw=1.6, label=label)
            handles[label] = line
            count += 1
        if count:
            line, = ax.plot([0,longest], [reference_target]*2, '--', color='#172B45', lw=1.3, label='C target')
            handles['C target'] = line
        else:
            ax.text(.5,.5,'No comparable recorded stage',ha='center',va='center',transform=ax.transAxes)
        ax.set_title(f'{title}\n{command}',fontsize=11,fontweight='bold')
        ax.set_xlabel('Time from command observation (sim s)')
        ax.set_ylabel('Motor angle (deg)')
    if handles:
        fig.legend(handles.values(), handles.keys(), loc='upper center',
                   bbox_to_anchor=(.52,.89), ncol=4, frameon=False, fontsize=9)
    fig.text(.065,.035,'Measured samples, no smoothing. Each panel uses one named command, aligned at its first CSV sample.\n'
             'Wrist A/B are differential motor coordinates. Curve length depends on stage completion; missing stages are not successes.',
             fontsize=9,color='#526175')
    for ext in ('png','pdf'):
        fig.savefig(output/f'comparison_8_motors.{ext}',dpi=180)
    plt.close(fig)


def analyse(root, output):
    records, all_metrics, trial_table = [], [], []
    reference_initial = None
    sweep = json.loads((output/'sweep.json').read_text())
    for trial_info in sweep['trials']:
        directory = output/trial_info['trial']
        metadata = directory/'trial.json'
        if not metadata.exists():
            continue
        meta = json.loads(metadata.read_text())
        row = {k:meta.get(k) for k in ('trial','kp','ki','kd','repeat','status','reason','passed_stages')}
        try:
            active = verify_trace(read_csv(directory/'trace.csv'),meta['kp'],meta['kd'])
            initial = [math.degrees(float(active[0][f'{m}_position'])) for m in MOTORS]
            if reference_initial is None:
                reference_initial = initial
            difference = max(abs(a-b) for a,b in zip(initial,reference_initial))
            comparable = difference <= .5
            row['initial_pose_difference_deg'] = difference
            row['comparable_initial_pose'] = comparable
            groups = split_stages(active)
            previous = {m:None for m in MOTORS}
            metrics = []
            for (revision, command), samples in groups:
                for motor in MOTORS:
                    item = dict(trial=meta['trial'],kp=meta['kp'],ki=0,kd=meta['kd'],
                                status=meta['status'],revision=int(revision),command=command,motor=motor,
                                stage_passed=int(revision) <= int(meta.get('passed_stages',0)))
                    item.update(metrics_for_stage(samples,motor,previous[motor]))
                    previous[motor] = item['target_deg']
                    metrics.append(item)
            write_table(directory/'stage_metrics.csv',metrics)
            all_metrics.extend(metrics)
            print(f'  Plotting all eight motors: {meta["trial"]}',flush=True)
            render_trial(root,directory,meta)
            records.append(dict(meta,groups=groups,comparable=comparable))
        except (ValueError,OSError,KeyError,ImportError) as error:
            row['analysis_error'] = str(error)
            row['status'] = 'DATA_OR_PLOT_ERROR'
            row['reason'] = str(error)
            print(f'  Analysis issue in {meta["trial"]}: {error}',flush=True)
        trial_table.append(row)
    write_table(output/'trial_summary.csv',trial_table)
    write_table(output/'stage_metrics.csv',all_metrics)
    render_comparison(output,records)
    table = ''.join('<tr>'+''.join(f'<td>{html.escape(str(r.get(k,"")))}</td>' for k in
        ('trial','kp','kd','status','passed_stages','reason'))+'</tr>' for r in trial_table)
    sections = []
    for record in records:
        trial = record['trial']
        links = ' · '.join(f'<a href="{trial}/plots/04_detail_{m}.png">{title}</a>'
                           for m,title in zip(MOTORS,TITLES))
        sections.append(f'<h2>{html.escape(trial)}</h2><p>{links}</p>'
            f'<p><a href="{trial}/trace.csv">CSV</a> · '
            f'<a href="{trial}/stage_metrics.csv">Stage metrics</a> · '
            f'<a href="{trial}/gazebo.log">Gazebo log</a></p>'
            f'<img src="{trial}/plots/01_target_tracking.png">')
    hardware_status = 'verified unchanged' if sweep.get('hardware_sources_unchanged') else 'not verified unchanged; see sweep.json'
    restoration = html.escape(str(sweep.get('restoration','unknown')))
    report = '''<!doctype html><html lang="en"><meta charset="utf-8"><title>Npulse PID sweep</title>
<style>body{font:16px system-ui,sans-serif;max-width:1300px;margin:40px auto;padding:0 24px;color:#172b45}
img{width:100%;height:auto}table{border-collapse:collapse;width:100%}td,th{padding:9px;border-bottom:1px solid #dce4ec;text-align:left}
a{color:#0072b2}p{line-height:1.5}</style><h1>Npulse PID sweep</h1>
<p>Zero gravity. Native C control. Ki=0. Simplified, uncalibrated mechanics.</p>
<p>Hardware sources: '''+hardware_status+'''. Restoration: '''+restoration+'''.</p>
<p>Each trial starts a fresh Gazebo instance. The compared stages use identical named targets. These are gain comparisons with feedback active in every trial.</p>
<table><thead><tr><th>Trial</th><th>Kp</th><th>Kd</th><th>Outcome</th><th>Passed stages</th><th>Reason</th></tr></thead><tbody>'''+table+'''</tbody></table>
<h2>Command-aligned comparison</h2><img src="comparison_8_motors.png">
<p><a href="comparison_8_motors.pdf">Comparison PDF</a> · <a href="trial_summary.csv">Trial summary</a> · <a href="stage_metrics.csv">All stage metrics</a></p>
<p>Metrics use simulated seconds and degrees. Overshoot and sampled 10–90% rise time apply only to target steps of at least 1°. Settling requires the final observed error to stay within ±2° for at least 0.5 s. Blank values mean not observed or not applicable. Telemetry gaps over 0.25 s break continuity. Incomplete trials must not be ranked as successful tuning results.</p>
'''+''.join(sections)+'</html>'
    (output/'index.html').write_text(report)
    print(f'Report: {output/"index.html"}',flush=True)


class Transaction:
    """Persist originals before mutation; never overwrite an unexpected user edit."""
    def __init__(self, root, output, paths=None):
        root, output = root.resolve(), output.resolve()
        self.root = root
        self.output = output
        self.path = output/'restore_manifest.json'
        if paths is None:
            self.data = json.loads(self.path.read_text())
            if Path(self.data['root']).resolve() != root:
                raise ValueError('Recovery manifest belongs to a different project root.')
        else:
            (output/'originals').mkdir()
            self.data = {'root':str(root), 'restored':False, 'files':[]}
            for i, path in enumerate(paths):
                path = path.resolve()
                original = path.read_bytes()
                backup = output/'originals'/f'{i:02d}-{path.name}'
                backup.write_bytes(original)
                self.data['files'].append(dict(path=str(path.relative_to(root)),
                    backup=str(backup.relative_to(output)), original_sha256=digest(original),
                    allowed_sha256=[digest(original)]))
            dump(self.path,self.data)

    def edit(self, path, text):
        path = path.resolve()
        entry = next(e for e in self.data['files'] if e['path'] == str(path.relative_to(self.root)))
        if digest(path.read_bytes()) not in entry['allowed_sha256']:
            raise ValueError(f'{path} changed outside the sweep. Refusing to overwrite it.')
        data = text.encode()
        entry['allowed_sha256'].append(digest(data))
        dump(self.path,self.data)  # Recovery knows intended content before writing it.
        path.write_bytes(data)

    def restore(self):
        # Validate every file before restoring any, including backup integrity.
        pending = []
        for entry in self.data['files']:
            path = (self.root/entry['path']).resolve()
            if not path.is_relative_to(self.root):
                raise ValueError('Recovery path outside project root.')
            original = (self.output/entry['backup']).read_bytes()
            if digest(original) != entry['original_sha256']:
                raise ValueError(f'Backup checksum mismatch: {path}')
            if digest(path.read_bytes()) not in entry['allowed_sha256']:
                raise ValueError(f'{path} was edited externally. Restore it manually from originals/.')
            pending.append((path,original))
        for path, data in pending:
            path.write_bytes(data)
        self.data['restored'] = True
        dump(self.path,self.data)


def terminate(process):
    """Stop only the process group created by this script, including descendants."""
    for sig, wait in ((signal.SIGINT,8), (signal.SIGTERM,4), (signal.SIGKILL,2)):
        try:
            os.killpg(process.pid,sig)
        except ProcessLookupError:
            break
        with contextlib.suppress(subprocess.TimeoutExpired):
            process.wait(timeout=wait)
        try:
            os.killpg(process.pid,0)
        except ProcessLookupError:
            break
    with contextlib.suppress(subprocess.TimeoutExpired):
        process.wait(timeout=1)


def run_command(command, cwd, env, log_path, timeout):
    with log_path.open('a') as log:
        log.write('\n$ '+' '.join(map(str,command))+'\n');log.flush()
        p = subprocess.Popen(command,cwd=cwd,env=env,stdout=log,
                             stderr=subprocess.STDOUT,start_new_session=True)
        try:
            start = time.monotonic()
            notice = start
            while p.poll() is None:
                now = time.monotonic()
                if now-start > timeout:
                    raise TimeoutError(f'Command timeout; see {log_path}')
                if now-notice > 25:
                    print(f'  Still running ({int(now-start)} s): {command[0]} — log {log_path.name}',flush=True)
                    notice = now
                time.sleep(.2)
            if p.returncode:
                raise RuntimeError(f'Command exited {p.returncode}; see {log_path}')
        finally:
            terminate(p)


def build(root, env, log):
    run_command(['colcon','build','--symlink-install','--packages-select',
                 'npulse_sim_control','prosthesis_description','--cmake-force-configure'],
                root/'simulation/ros2_ws',env,log,600)


def existing_gazebo():
    result = subprocess.run(['ps','-eo','pid=,uid=,args='],text=True,capture_output=True,check=True)
    pattern = re.compile(r'(?:^|/|\s)(?:gz\s+sim|gz-sim(?:-[\w-]+)?)(?:\s|$)')
    matches = []
    for line in result.stdout.splitlines():
        parts = line.strip().split(None,2)
        if len(parts) == 3 and int(parts[1]) == os.getuid() and pattern.search(parts[2]):
            matches.append(line.strip())
    return matches


def run_trial(root, directory, env, startup_timeout, trial_timeout):
    log_path = directory/'gazebo.log'
    csv_path = None
    reason = ''
    with log_path.open('w') as log:
        p = subprocess.Popen(['ros2','launch','prosthesis_description','gazebo_hand_lockstep.launch.py'],
            cwd=root/'simulation/ros2_ws',env=env,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
        try:
            start = time.monotonic()
            while True:
                if p.poll() is not None:
                    raise RuntimeError('Gazebo launch exited during startup; see gazebo.log.')
                text = log_path.read_text(errors='replace')
                if 'Failed to configure controller' in text or 'Error activating controller' in text:
                    raise RuntimeError('Controller startup failed; see gazebo.log.')
                match = re.search(r'AUTO_TEST ready; CSV=([^;\r\n]+)',text)
                if match:
                    csv_path = Path(match[1])
                    rows = read_csv(csv_path)
                    if len(rows) >= 2 and float(rows[-1]['sim_ms']) > float(rows[0]['sim_ms']):
                        break  # Physics and active controller telemetry both progressed.
                if time.monotonic()-start > startup_timeout:
                    raise TimeoutError('No progressing automatic-test CSV. Check automatic_test and Gazebo startup.')
                time.sleep(.25)
            print('  Physics ready. Sending one explicit START.',flush=True)
            run_command(['ros2','topic','pub','--once','/fw_hand_controller/hand_command',
                         'npulse_sim_control/msg/HandCommand','{enabled: true}'],
                        root,env,directory/'operator.log',30)
            start = notice = time.monotonic()
            terminal = None
            while terminal is None:
                text = log_path.read_text(errors='replace')
                failure = re.search(r'AUTO_TEST FAIL[^\r\n]*',text)
                if failure:
                    terminal = 'FAILED';reason = failure[0]
                elif 'COMPLETE' in text and 'STOP requested' in text:
                    terminal = 'PASSED'
                elif p.poll() is not None:
                    raise RuntimeError('Gazebo launch exited during the test.')
                elif time.monotonic()-start > trial_timeout:
                    raise TimeoutError('Wall-time test deadline reached.')
                if time.monotonic()-notice > 20:
                    passes = re.findall(r'AUTO_TEST PASS (\w+)',text)
                    print(f'  Progress: {len(passes)}/19 stages; last={passes[-1] if passes else "arming"}',flush=True)
                    notice = time.monotonic()
                if terminal is None:
                    time.sleep(.25)
            # Allow the wall-timer CSV writer to record the terminal outcome before shutdown.
            deadline = time.monotonic()+5
            wanted = '2' if terminal == 'PASSED' else '3'
            while time.monotonic() < deadline:
                rows = read_csv(csv_path)
                if any(r.get('test_outcome') == wanted for r in rows):
                    break
                time.sleep(.1)
            else:
                terminal = 'INCOMPLETE'
                reason = 'Terminal controller event was not confirmed in CSV.'
            passes = re.findall(r'AUTO_TEST PASS (\w+)',log_path.read_text(errors='replace'))
            if terminal == 'PASSED' and tuple(passes) != STAGES:
                terminal = 'INCOMPLETE'
                reason = 'Completed stage sequence differs from the expected 19-stage bench.'
            return dict(status=terminal,reason=reason,passed_stages=len(passes))
        finally:
            terminate(p)
            if csv_path is not None and csv_path.exists():
                # Copy only after the writer has been stopped.
                (directory/'trace.csv').write_bytes(csv_path.read_bytes())


def preflight(root, args):
    if sys.platform != 'linux':
        raise ValueError('Run the sweep on the Ubuntu ROS/Gazebo VM. Offline --analyse-only also works elsewhere.')
    if os.environ.get('ROS_DISTRO') != 'jazzy':
        raise ValueError('Source /opt/ros/jazzy/setup.bash and the project overlay first.')
    pending = root/'build/simulation/pid-sweep-pending.json'
    if pending.exists():
        previous = json.loads(pending.read_text())['output']
        raise ValueError(f'An earlier sweep needs recovery first: python3 tools/test_pid_sweep.py --restore {previous}')
    import matplotlib  # Fail before editing files if plotting is unavailable.
    import yaml
    base = root/'simulation/ros2_ws/src/prosthesis_description'
    generator = root/'tools/generate_wholehand_core.py'
    world = base/'worlds/prosthesis_hand_zero_gravity.sdf'
    launch = base/'launch/gazebo_hand_lockstep.launch.py'
    config = base/'config/prosthesis_hand_lockstep.yaml'
    for path in (generator,world,launch,config,root/'tools/plot_hand_feedback.py',
                 root/'simulation/ros2_ws/src/npulse_sim_control/src/hand_api.c.inc'):
        if not path.is_file():
            raise ValueError(f'Missing existing bench file: {path}')
    parameters = yaml.safe_load(config.read_text())['fw_hand_controller']['ros__parameters']
    if parameters.get('automatic_test') is not True:
        raise ValueError('Set automatic_test: true in the existing whole-hand YAML before running this sweep.')
    replace_gains(generator.read_text(),5.0,1.0)
    zero_gravity(world.read_text())
    if world.name not in launch.read_text():
        raise ValueError('The current launch does not reference the expected whole-hand world.')
    if not args.gui:
        headless_launch(launch.read_text())
    plotter = load_plotter(root)
    for name in ('read_trial','style','plot_overview','plot_detail','write_metrics'):
        if not hasattr(plotter,name):
            raise ValueError('Existing plotting script has an incompatible interface.')
    processes = existing_gazebo()
    if processes:
        raise ValueError('Close the existing Gazebo run with Ctrl+C first. No unrelated processes will be killed.\n'+'\n'.join(processes))
    return generator,world,launch


def run_sweep(root, output, args):
    root, output = root.resolve(), output.resolve()
    paths = preflight(root,args)
    generator,world,launch = paths
    plan = [(kp,kd,repeat) for kp,kd in itertools.product(args.kp,args.kd)
            for repeat in range(1,args.repeat+1)]
    if len(plan) > 30:
        raise ValueError('At most 30 fresh launches per sweep. Reduce the grid or repeats.')
    print(f'Plan: {len(plan)} fresh trials, Ki=0, gravity=0, '+('GUI' if args.gui else 'headless'),flush=True)
    for kp,kd,repeat in plan:
        print(f'  Kp={kp:g}, Kd={kd:g}, repeat={repeat}',flush=True)
    if args.dry_run:
        print('Preflight passed. No project files changed. Run without --dry-run to execute.',flush=True)
        return
    output.mkdir(parents=True,exist_ok=False)
    original = {p:p.read_text() for p in paths}
    tx = Transaction(root,output,paths)
    env = os.environ.copy()
    # Isolate this bench from the user's normal ROS domain and Gazebo partition.
    domain = args.ros_domain_id
    env['ROS_DOMAIN_ID'] = str(domain)
    env['GZ_PARTITION'] = f'npulse-pid-sweep-{os.getpid()}-{time.time_ns()}'
    share = root/'simulation/ros2_ws/install/prosthesis_description/share'
    env['GZ_SIM_RESOURCE_PATH'] = str(share)+(os.pathsep+env['GZ_SIM_RESOURCE_PATH'] if env.get('GZ_SIM_RESOURCE_PATH') else '')
    env['RCUTILS_COLORIZED_OUTPUT'] = '0'
    hardware = list((root/'fw/motors/motor_control').glob('*.c'))
    hardware += list((root/'fw/motors/motor_control').glob('*.h'))
    hardware += [root/'fw/app/intent_router.c',root/'fw/motors/motor_map/motor_map.c']
    hardware_hashes = {str(p.relative_to(root)):digest(p.read_bytes()) for p in hardware}
    source_paths = [p for p in (root/'simulation/ros2_ws/src/npulse_sim_control').rglob('*')
                    if p.suffix in ('.cpp','.hpp','.inc','.h','.cmake','.yaml') and p.is_file()]
    source_paths += [p for p in (root/'simulation/ros2_ws/src/prosthesis_description').rglob('*')
                     if p.suffix in ('.xacro','.yaml') and p.is_file()]
    metadata = dict(root=str(root),conditions=CONTEXT,ki=0,gravity=[0,0,0],
        ros_domain_id=domain,plan=[dict(kp=k,kd=d,repeat=r) for k,d,r in plan],
        hardware_sha256=hardware_hashes,
        bench_sha256={str(p.relative_to(root)):digest(p.read_bytes()) for p in source_paths},
        trials=[],restoration='pending')
    dump(output/'sweep.json',metadata)
    pending = root/'build/simulation/pid-sweep-pending.json'
    dump(pending,dict(output=str(output)))
    interrupted = False
    try:
        tx.edit(world,zero_gravity(original[world]))
        if not args.gui:
            tx.edit(launch,headless_launch(original[launch]))
        for i,(kp,kd,repeat) in enumerate(plan,1):
            name = f'{i:02d}-kp{kp:g}-kd{kd:g}-r{repeat}'
            directory = output/name;directory.mkdir()
            trial = dict(trial=name,kp=kp,ki=0.0,kd=kd,repeat=repeat,status='PENDING',passed_stages=0,reason='')
            metadata['trials'].append(trial)
            print(f'[{i}/{len(plan)}] {name}: building native C...',flush=True)
            try:
                for relative,sha in {**metadata['bench_sha256'], **hardware_hashes}.items():
                    if digest((root/relative).read_bytes()) != sha:
                        raise ValueError(f'Bench source changed during the sweep: {relative}')
                tx.edit(generator,replace_gains(original[generator],kp,kd))
                build(root,env,directory/'build.log')
                installed_world = share/'prosthesis_description/worlds'/world.name
                actual_gravity = ET.parse(installed_world).getroot().find('./world/gravity')
                if actual_gravity is None or len(actual_gravity.text.split()) != 3 or any(float(x) != 0 for x in actual_gravity.text.split()):
                    raise ValueError('Installed world is not zero gravity. Refusing an unmatched trial.')
                native_inputs = root/'simulation/ros2_ws/build/npulse_sim_control/native_wholehand/inputs.json'
                if not native_inputs.is_file():
                    raise ValueError('Generated native inputs.json is missing after build.')
                (directory/'native_inputs.json').write_bytes(native_inputs.read_bytes())
            except KeyboardInterrupt:
                trial.update(status='INTERRUPTED',reason='Operator interrupted the build.')
                dump(directory/'trial.json',trial);dump(output/'sweep.json',metadata)
                raise
            except (RuntimeError,TimeoutError,ValueError) as error:
                trial.update(status='BUILD_FAILED',reason=str(error))
                dump(directory/'trial.json',trial);dump(output/'sweep.json',metadata)
                # A build/configuration problem is not a bad gain. Abort the sweep.
                raise
            try:
                trial.update(run_trial(root,directory,env,args.startup_timeout,args.trial_timeout))
                active = verify_trace(read_csv(directory/'trace.csv'),kp,kd)
                trial['gains_verified'] = True
                trial['initial_positions_deg'] = [math.degrees(float(active[0][f'{m}_position'])) for m in MOTORS]
            except KeyboardInterrupt:
                trial.update(status='INTERRUPTED',reason='Operator interrupted the trial.')
                raise
            except (RuntimeError,TimeoutError,ValueError,OSError,KeyError) as error:
                trial.update(status='ERROR',reason=str(error))
            finally:
                dump(directory/'trial.json',trial);dump(output/'sweep.json',metadata)
            print(f'  {trial["status"]}: {trial["passed_stages"]}/19 stages. {trial["reason"]}',flush=True)
            # Continue after recorded controller failures in a NEW process, never re-arm a faulted trial.
            if trial['status'] == 'ERROR':
                raise RuntimeError('Infrastructure/data validation error; sweep stopped instead of treating it as tuning.')
            remaining = existing_gazebo()
            if remaining:
                raise RuntimeError('A Gazebo process remains after shutdown. Sweep stopped; close it before restarting.')
    except KeyboardInterrupt:
        interrupted = True
        metadata['interrupted'] = True
        print('\nInterrupted. Stopping the owned simulation and restoring original configuration...',flush=True)
    finally:
        print('Restoring original SIM files and rebuilding the original configuration...',flush=True)
        try:
            tx.restore()
            build(root,env,output/'restore-build.log')
            metadata['restoration'] = 'sources_and_build_restored'
            pending.unlink(missing_ok=True)
        except (Exception,KeyboardInterrupt) as error:
            metadata['restoration'] = 'manual_recovery_required'
            metadata['restore_error'] = str(error)
            print(f'RESTORATION NEEDS ATTENTION: {error}\nRecovery: python3 tools/test_pid_sweep.py --restore {output}',flush=True)
        changed = [name for name,sha in hardware_hashes.items() if digest((root/name).read_bytes()) != sha]
        metadata['hardware_sources_unchanged'] = not changed
        if changed:
            metadata['hardware_changes_detected'] = changed
        dump(output/'sweep.json',metadata)
        try:
            print('Rendering available measured trials...',flush=True)
            analyse(root,output)
        except Exception as error:
            metadata['analysis_error'] = str(error)
            dump(output/'sweep.json',metadata)
            print(f'Plotting error: {error}\nRetry: python3 tools/test_pid_sweep.py --analyse-only {output}',flush=True)
    if metadata['restoration'] != 'sources_and_build_restored':
        raise RuntimeError('Original build not confirmed restored. See restore-build.log and recovery command above.')
    if interrupted:
        return 130
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__,formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    parser.add_argument('--kp',type=float,nargs='+',default=[5.0,10.0])
    parser.add_argument('--kd',type=float,nargs='+',default=[0.0,0.5,1.0])
    parser.add_argument('--repeat',type=int,default=1)
    parser.add_argument('--gui',action='store_true',help='Show Gazebo. Default runs server-only for faster VM tests.')
    parser.add_argument('--output',type=Path)
    parser.add_argument('--dry-run',action='store_true',help='Check the bench and print the plan, without edits or runs.')
    parser.add_argument('--startup-timeout',type=float,default=150)
    parser.add_argument('--trial-timeout',type=float,default=600,help='Wall seconds per test, not simulation seconds.')
    parser.add_argument('--ros-domain-id',type=int,default=87,help='Private ROS domain for this sweep. Default 87.')
    parser.add_argument('--analyse-only',type=Path,help='Regenerate plots from an existing sweep; no ROS or rebuild.')
    parser.add_argument('--restore',type=Path,help='Restore source backups and rebuild the configuration saved before a sweep.')
    args = parser.parse_args()
    root = args.root.expanduser().resolve()
    try:
        if args.analyse_only:
            analyse(root,args.analyse_only.expanduser().resolve())
            return 0
        if not 1 <= args.repeat <= 5 or not 1 <= args.ros_domain_id <= 100:
            parser.error('repeat must be 1..5 and ros-domain-id 1..100.')
        if not all(math.isfinite(v) and 0 <= v <= 3.4028234e38 for v in args.kp+args.kd):
            parser.error('Gains must be finite, non-negative C floats.')
        if len(set(args.kp)) != len(args.kp) or len(set(args.kd)) != len(args.kd):
            parser.error('Remove duplicate gain values.')
        if not all(math.isfinite(v) and v > 0 for v in (args.startup_timeout,args.trial_timeout)):
            parser.error('Timeouts must be finite and positive.')
        lock_path = root/'build/simulation/pid-sweep.lock'
        lock_path.parent.mkdir(parents=True,exist_ok=True)
        with lock_path.open('w') as lock:
            try:
                fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
            except BlockingIOError:
                raise ValueError('Another PID sweep or restore is already running in this project.')
            if args.restore:
                output = args.restore.expanduser().resolve()
                tx = Transaction(root,output)
                tx.restore()
                build(root,os.environ.copy(),output/'restore-build.log')
                metadata = json.loads((output/'sweep.json').read_text())
                metadata['restoration'] = 'sources_and_build_restored'
                dump(output/'sweep.json',metadata)
                pending = root/'build/simulation/pid-sweep-pending.json'
                if pending.exists() and Path(json.loads(pending.read_text())['output']).resolve() == output:
                    pending.unlink()
                print('Original files and build restored.')
                return 0
            output = (args.output or root/'results/hand-feedback'/datetime.now().strftime('pid-sweep-%Y%m%d-%H%M%S')).expanduser().resolve()
            return run_sweep(root,output,args)
    except (ValueError,RuntimeError,TimeoutError,OSError,KeyError,ImportError) as error:
        print(f'ERROR: {error}',file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
