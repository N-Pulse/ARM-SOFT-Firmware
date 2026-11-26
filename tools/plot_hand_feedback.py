#!/usr/bin/env python3
"""Presentation figures from measured whole-hand CSVs. No smoothing or synthetic results."""
import argparse
import csv
import json
import math
from pathlib import Path

MOTORS = ('thumb', 'index', 'middle', 'ring', 'little', 'wrist_x', 'wrist_y', 'palm')
TITLES = ('Thumb', 'Index', 'Middle', 'Ring', 'Little', 'Wrist motor A', 'Wrist motor B', 'Palm motor')
GROUPS = ((0, 2), (2, 3), (5, 3), (8, 3), (11, 3))
BLUE, INK, ORANGE, RED, GREEN = '#0072B2', '#172B45', '#D55E00', '#C4414B', '#008875'
CAUSES = {1:'MANUAL', 2:'GUARD', 4:'FEEDBACK', 8:'CONTROL_DELAY', 16:'FORCE',
          32:'DRIVER', 64:'PID', 128:'LINK', 256:'INIT', 512:'MOTION_TIMEOUT'}


def read_trial(path, requested_run=None):
    with path.open(newline='') as file:
        reader = csv.DictReader(file)
        required = {'sim_ms','phase','run','causes','revision','command'}
        required |= {f'{m}_{f}' for m in MOTORS for f in
                     ('target','position','requested_percent','applied_percent','feedback_age_ms')}
        missing = required - set(reader.fieldnames or [])
        if missing:
            raise ValueError(f'{path}: incompatible CSV; missing: {", ".join(sorted(missing))}')
        rows = list(reader)
    active_runs = sorted({int(r['run']) for r in rows if int(r['phase']) == 2})
    if not active_runs:
        raise ValueError(f'{path}: no active C firmware samples (phase=2).')
    run = requested_run if requested_run is not None else active_runs[-1]
    if run not in active_runs:
        raise ValueError(f'{path}: run {run} has no active samples. Available: {active_runs}')
    # Contiguous activation only: no interpolation across STOP, restart or clock reset.
    start = next(i for i,r in enumerate(rows) if int(r['run'])==run and int(r['phase'])==2)
    stop = start
    while stop < len(rows) and int(rows[stop]['run'])==run and int(rows[stop]['phase'])==2:
        stop += 1
    active = rows[start:stop]
    display = list(active)
    last_ms = float(active[-1]['sim_ms'])
    for r in rows[stop:]:
        ms = float(r['sim_ms'])
        if int(r['run']) != run or ms < last_ms or ms > last_ms + 750:
            break
        display.append(r)
    previous = None
    for r in display:
        ms = float(r['sim_ms'])
        if not math.isfinite(ms) or (previous is not None and ms < previous):
            raise ValueError(f'Simulation clock moved backwards: {previous} -> {ms} ms. Cannot combine measurements across a reset.')
        previous = ms
        for m in MOTORS:
            for f in ('target','position','requested_percent','applied_percent','feedback_age_ms'):
                if not math.isfinite(float(r[f'{m}_{f}'])):
                    raise ValueError(f'Non-finite {m}_{f}; cannot plot trustworthy data.')
    origin = float(active[0]['sim_ms'])
    faults = [(float(r['sim_ms'])-origin)/1000 for r in display if int(r['phase'])==3]
    first_fault = faults[0] if faults else None
    fault_row = next((r for r in display if int(r['phase'])==3), None)
    fault_cause = int(fault_row['causes']) if fault_row else 0
    return dict(path=path,run=run,available_runs=active_runs,rows=display,active=active,
                time=[(float(r['sim_ms'])-origin)/1000 for r in display],
                fault=first_fault,fault_cause=fault_cause)


def values(trial, motor, field):
    scale = 180/math.pi if field in ('position','target','reference','error') else 1
    result = []
    for r in trial['rows']:
        if field in ('target','reference','error') and int(r['phase']) != 2:
            result.append(float('nan'))  # Fault cleanup resets targets: never plot them as a new request.
        elif field == 'error':
            result.append((float(r[f'{motor}_target'])-float(r[f'{motor}_position']))*scale)
        else:
            result.append(float(r[f'{motor}_{field}'])*scale)
    return result


def gapped(time, data, max_gap):
    x,y = [],[]
    for i,(t,v) in enumerate(zip(time,data)):
        if i and t-time[i-1] > max_gap:
            x.append(float('nan'));y.append(float('nan'))
        x.append(t);y.append(v)
    return x,y


def cause_text(mask):
    names = [name for bit,name in CAUSES.items() if mask & bit]
    return '+'.join(names) or str(mask)


def style(plt):
    plt.rcParams.update({
        'font.family':'DejaVu Sans','font.size':11,'axes.titlesize':12,'axes.titleweight':'bold',
        'axes.labelsize':10,'axes.edgecolor':'#C7D0DB','axes.labelcolor':INK,
        'text.color':INK,'xtick.color':'#526175','ytick.color':'#526175',
        'axes.spines.top':False,'axes.spines.right':False,'axes.grid':True,
        'grid.color':'#E7ECF2','grid.linewidth':.7,'grid.alpha':1,
        'figure.facecolor':'white','axes.facecolor':'white','savefig.facecolor':'white',
        'pdf.fonttype':42,'ps.fonttype':42,'lines.linewidth':1.8})


def decorate(ax, trial, max_gap):
    ax.set_axisbelow(True)
    ax.margins(x=.015)
    if trial['fault'] is not None:
        ax.axvline(trial['fault'],color=RED,linestyle=':',linewidth=1.4)
        ax.axvspan(trial['fault'],trial['time'][-1],color=RED,alpha=.07)
    gaps = sum(b-a>max_gap for a,b in zip(trial['time'],trial['time'][1:]))
    return gaps


def save(fig, output, stem, dpi, plt):
    for ext in ('png','pdf'):
        path = output / f'{stem}.{ext}'
        fig.savefig(path,dpi=dpi)
        print(path)
    plt.close(fig)


def header(fig, title, trial, args, comparison=False):
    fig.suptitle(title,x=.055,y=.97,ha='left',fontsize=21,fontweight='bold',color=INK)
    detail = f'{args.context}  |  C firmware run {trial["run"]}'
    fig.text(.055,.925,detail,fontsize=10,color='#526175')
    if trial['fault'] is not None:
        status=f'FAULT: {cause_text(trial["fault_cause"])} at {trial["fault"]:.2f} s'
        color=RED
    else:
        status='No fault recorded in this activation'
        color=GREEN
    fig.text(.945,.925,status,ha='right',fontsize=10,color=color,fontweight='bold')
    footer = (f'Source: {trial["path"].name} | Measured samples; no smoothing. '
              'Wrist plots use differential motor coordinates.')
    if comparison:
        footer += ' Trials aligned at first active sample, not synchronized events.'
    fig.text(.055,.025,footer,fontsize=8,color='#66758A',wrap=True)


def plot_overview(plt, trial, baseline, args, kind):
    fig,axes=plt.subplots(2,4,figsize=(17.78,10),sharex=True)
    fig.subplots_adjust(left=.055,right=.965,bottom=.12,top=.81,wspace=.3,hspace=.42)
    titles={'position':'Target tracking — all eight motors',
            'error':'Tracking error — target minus measurement',
            'effort':'Motor effort — requested and applied'}
    header(fig,titles[kind],trial,args,baseline is not None)
    handles={}
    for i,(motor,ax) in enumerate(zip(MOTORS,axes.flat)):
        ax.set_title(TITLES[i],loc='left',pad=10)
        if kind=='position':
            traces=[('target',INK,'--','C target'),('position',BLUE,'-',args.label)]
        elif kind=='error':
            traces=[('error',BLUE,'-',args.label)]
            ax.axhspan(-args.guide_deg,args.guide_deg,color=GREEN,alpha=.07)
            ax.axhline(0,color='#7B8A9D',linewidth=.8)
        else:
            traces=[('requested_percent',INK,'--','C requested'),('applied_percent',BLUE,'-','Adapter applied')]
            ax.axhline(0,color='#7B8A9D',linewidth=.8)
        for field,color,line,label in traces:
            h,=ax.plot(*gapped(trial['time'],values(trial,motor,field),args.max_gap),
                      color=color,linestyle=line,label=label,zorder=3)
            handles[label]=h
        if baseline is not None and kind!='effort':
            field='position' if kind=='position' else 'error'
            h,=ax.plot(*gapped(baseline['time'],values(baseline,motor,field),args.max_gap),
                      color=ORANGE,alpha=.85,label=args.compare_label)
            handles[args.compare_label]=h
        decorate(ax,trial,args.max_gap)
        ax.set_ylabel('Command (%)' if kind=='effort' else 'Angle (deg)' if kind=='position' else 'Error (deg)')
        if i>=4:ax.set_xlabel('Simulated time (s)')
        if kind=='error':
            low,high=ax.get_ylim(); extent=max(abs(low),abs(high),args.guide_deg*1.2)
            ax.set_ylim(-extent,extent)
        else:
            low,high=ax.get_ylim(); minimum=2 if kind=='position' else 1
            if high-low < minimum:
                middle=(low+high)/2;ax.set_ylim(middle-minimum/2,middle+minimum/2)
    fig.legend(list(handles.values()),list(handles),loc='upper left',bbox_to_anchor=(.047,.905),
               frameon=False,ncol=4,fontsize=10)
    if kind=='error':
        fig.text(.945,.865,f'Shaded guide: ±{args.guide_deg:g}° (not a hardware specification)',
                 ha='right',fontsize=9,color='#526175')
    save(fig,args.output,{'position':'01_target_tracking','error':'02_tracking_error','effort':'03_motor_effort'}[kind],args.dpi,plt)


def plot_detail(plt, trial, args):
    motor=args.motor
    fig,axes=plt.subplots(3,1,figsize=(16,9),sharex=True,gridspec_kw={'height_ratios':[2,1,1]})
    fig.subplots_adjust(left=.075,right=.96,bottom=.11,top=.80,hspace=.24)
    header(fig,f'{TITLES[MOTORS.index(motor)]} — feedback and correction',trial,args)
    for field,color,line,label in [('target',INK,'--','C target'),('position',BLUE,'-','Measured position')]:
        axes[0].plot(*gapped(trial['time'],values(trial,motor,field),args.max_gap),color=color,linestyle=line,label=label)
    axes[0].legend(loc='upper left',frameon=False,ncol=2)
    axes[0].set_ylabel('Angle (deg)')
    axes[1].plot(*gapped(trial['time'],values(trial,motor,'error'),args.max_gap),color=BLUE)
    axes[1].axhspan(-args.guide_deg,args.guide_deg,color=GREEN,alpha=.08)
    axes[1].axhline(0,color='#7B8A9D',linewidth=.7);axes[1].set_ylabel('Error (deg)')
    for field,color,line,label in [('requested_percent',INK,'--','C requested'),('applied_percent',BLUE,'-','Adapter applied')]:
        axes[2].plot(*gapped(trial['time'],values(trial,motor,field),args.max_gap),color=color,linestyle=line,label=label)
    axes[2].set_ylabel('Command (%)');axes[2].set_xlabel('Simulated time (s)')
    axes[2].legend(loc='upper right',frameon=False,ncol=2,fontsize=9)
    for ax in axes:decorate(ax,trial,args.max_gap)
    events=[];previous=None
    for t,r in zip(trial['time'],trial['rows']):
        if int(r['phase'])==2 and r['revision']!=previous:
            previous=r['revision'];events.append((t,r['command']))
    if len(events)<=6:
        for t,label in events:
            axes[0].axvline(t,color='#BBC6D4',linewidth=.7,zorder=1)
            axes[0].text(t,.99,label.replace('_',' '),transform=axes[0].get_xaxis_transform(),
                         rotation=90,va='top',ha='right',fontsize=8,color='#66758A')
    save(fig,args.output,'04_detail_'+motor,args.dpi,plt)


def write_metrics(trial,args):
    rows=trial['active'];time=[float(r['sim_ms'])/1000 for r in rows]
    dt=[b-a for a,b in zip(time,time[1:])]
    # Time-weighted left-sample error on measured intervals; skip long telemetry gaps.
    valid=[(i,d) for i,d in enumerate(dt) if 0<d<=args.max_gap]
    table=[]
    for i,motor in enumerate(MOTORS):
        errors=[math.degrees(float(r[f'{motor}_target'])-float(r[f'{motor}_position'])) for r in rows]
        duration=sum(d for _,d in valid)
        rmse=math.sqrt(sum(errors[j]**2*d for j,d in valid)/duration) if duration else None
        spread=None
        if i<5 and all(f'joint_{j}_position' in rows[0]
                       for j in range(GROUPS[i][0], GROUPS[i][0]+GROUPS[i][1])):
            first,count=GROUPS[i]
            spread=max(math.degrees(max(float(r[f'joint_{j}_position']) for j in range(first,first+count))-
                                    min(float(r[f'joint_{j}_position']) for j in range(first,first+count))) for r in rows)
        table.append(dict(motor=motor,rmse_deg=rmse,max_abs_error_deg=max(map(abs,errors)),
                          last_active_error_deg=errors[-1],max_phalange_spread_deg=spread,
                          max_abs_applied_percent=max(abs(float(r[f'{motor}_applied_percent'])) for r in rows),
                          max_feedback_age_ms=max(float(r[f'{motor}_feedback_age_ms']) for r in rows)))
    with (args.output/'metrics.csv').open('w',newline='') as file:
        writer=csv.DictWriter(file,fieldnames=list(table[0]));writer.writeheader();writer.writerows(table)
    summary=dict(source=str(trial['path'].resolve()),selected_run=trial['run'],available_runs=trial['available_runs'],
                 active_samples=len(rows),active_duration_sim_s=time[-1]-time[0],
                 max_sample_gap_sim_ms=1000*max(dt,default=0),fault_time_sim_s=trial['fault'],
                 fault_cause=cause_text(trial['fault_cause']) if trial['fault'] is not None else None,
                 context=args.context,metrics=table,
                 note='One contiguous activation. No smoothing. RMSE excludes telemetry gaps exceeding max_gap; '
                      'metrics include transients and are not automatic success criteria. Applied effort is adapter command, not measured torque.')
    (args.output/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(args.output/'metrics.csv');print(args.output/'summary.json')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('csv',type=Path)
    parser.add_argument('--output',type=Path,default=Path('results/hand-feedback'))
    parser.add_argument('--run',type=int,help='Firmware run; defaults to latest run with active samples.')
    parser.add_argument('--motor',choices=MOTORS,default='index',help='Motor for the detailed figure.')
    parser.add_argument('--label',default='Closed loop — measured')
    parser.add_argument('--compare',type=Path,help='Optional second MEASURED trial, same CSV schema.')
    parser.add_argument('--compare-run',type=int)
    parser.add_argument('--compare-label',default='Baseline — measured')
    parser.add_argument('--context',default='Zero gravity | Provisional mechanics | Native C PID')
    parser.add_argument('--guide-deg',type=float,default=2)
    parser.add_argument('--max-gap',type=float,default=.25,help='Break lines across longer simulated-time gaps (s).')
    parser.add_argument('--dpi',type=int,default=200)
    args=parser.parse_args()
    if not math.isfinite(args.guide_deg) or args.guide_deg<=0 or not math.isfinite(args.max_gap) or args.max_gap<=0:
        parser.error('guide-deg and max-gap must be finite and positive.')
    try:
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
    except ImportError:
        parser.exit(1,'Matplotlib missing. Ubuntu: sudo apt install python3-matplotlib\n')
    try:
        trial=read_trial(args.csv,args.run)
        baseline=read_trial(args.compare,args.compare_run) if args.compare else None
    except (ValueError,OSError,KeyError) as error:
        parser.exit(1,str(error)+'\n')
    args.output.mkdir(parents=True,exist_ok=True)
    style(plt)
    for kind in ('position','error','effort'):plot_overview(plt,trial,baseline,args,kind)
    plot_detail(plt,trial,args)
    write_metrics(trial,args)
    print(f'Plotted C firmware run {trial["run"]}; available: {trial["available_runs"]}.')
    if trial['fault'] is not None:print('FAULT INCLUDED:',cause_text(trial['fault_cause']))
    print('No open-loop result is fabricated: --compare requires a separately recorded trial.')


if __name__=='__main__':
    main()
