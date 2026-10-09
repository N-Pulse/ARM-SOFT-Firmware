from pathlib import Path
import csv
import json
import struct
from bench import Bench, OUT
from sim import protocol as p

def closed_loop(b):
    b.advance(100); b.stopped()
    b.peer.request(p.ARM) # no session: rejected by host endpoint
    b.connect()
    b.activate()
    goals=[0.3+0.05*i for i in range(8)]
    b.target([(i,goals[i],60) for i in (7,2,5,0,6,1,4,3)])
    b.advance(5000)
    errors=[abs(x-y) for x,y in zip(goals,b.model.position)]
    assert max(errors)<0.02,(errors,b.fw.state,b.fw.motors)
    assert len(set(round(x,2) for x in b.model.position))==8
    assert b.fw.state['causes']==0
    before=list(b.model.position)
    b.command(p.STOP); b.stopped(); b.advance(40)
    assert all(x==0 for x in b.model.applied)
    assert all(x>0.25 for x in b.model.position) # STOP is never position zero.
    b.command(p.RECOVER); b.stopped()
    b.command(p.ARM); b.advance(80); b.command(p.ENABLE,b'\xff'); b.advance(100)
    assert all(m['output']==0 and m['target']==m['ref'] for m in b.fw.motors)
    return {'max_error_rad':max(errors),'position_at_stop':before,
            'up_max_pending':b.up.max_pending,'down_max_pending':b.down.max_pending,
            'up_max_delay_ms':b.up.max_delay,'down_max_delay_ms':b.down.max_delay}

def startup_reconnect(b):
    b.advance(100); b.stopped()
    # Validly framed commands before Hello cannot activate anything.
    b.raw(p.ARM,session=99,sequence=1,stamp=b.now); b.stopped()
    b.peer.connect(10,b.now); b.advance(40)
    b.command(p.ARM,success=False)
    b.command(p.CONFIG,bytes((1,0,255,0)),feedback=False)
    b.command(p.ARM,success=False,feedback=False) # no measurement has been delivered yet
    b.advance(80); b.activate(); b.target([(0,0.7,60)]); b.advance(300)
    old_session=b.peer.session; old_run=b.fw.state['run']; old_output=b.last_output_frame
    b.peer.connect(11,b.now); b.advance(40); b.stopped()
    assert b.fw.state['causes'] & 128
    b.command(p.ARM,success=False)
    b.force_mask=0; b.command(p.CONFIG,bytes((1,0,255,0))); b.advance(80)
    b.command(p.RECOVER); b.stopped(); b.advance(80)
    assert b.peer.permitted_run is None and not any(b.model.applied)
    b.raw(p.HELLO,session=old_session,sequence=0,stamp=0)
    b.raw(p.FEEDBACK,p.feedback(old_run,0,b.model.feedback()),session=old_session,sequence=1000)
    if old_output: b.peer.receive(old_output,b.now)
    b.stopped(); assert not any(b.model.applied)
    assert b.fw.state['session']==11

def partial_routing(b):
    b.connect()
    b.feedback_ids=[7,2,0]
    b.advance(120)
    assert all(b.fw.motors[i]['age']<=100 for i in (0,2,7))
    assert all(b.fw.motors[i]['age']>100 for i in (1,3,4,5,6))
    b.command(p.ARM,success=False)
    b.feedback_ids=[6,1,5,4,3,7,0,2]; b.advance(80); b.activate()
    # Only the addressed target entries change, in arbitrary arrival order.
    b.target([(7,-0.4,60),(2,0.5,60),(5,-0.3,60)])
    b.advance(200)
    assert b.model.position[7]<0 and b.model.position[2]>0 and b.model.position[5]<0
    assert all(abs(b.model.position[i])<1e-9 for i in (0,1,3,4,6))
    age=b.fw.motors[0]['age']; measured=b.fw.motors[0]['measured']
    b.feedback_fields=p.APPLIED; b.feedback_ids=[0]; b.advance(40)
    assert b.fw.motors[0]['measured']==measured and b.fw.motors[0]['age']>age

def feedback_loss(b):
    b.connect(); b.activate(); b.target([(0,0.8,60)]); b.advance(200)
    b.advance(180,feedback=False,heartbeats=True)
    b.stopped(); assert b.fw.state['causes'] & 4
    assert b.fw.state['live'] # heartbeat and measurement freshness are distinct
    assert not any(b.model.applied)
    b.command(p.RECOVER,success=False,feedback=False)
    b.advance(100); b.command(p.RECOVER); b.stopped()
    b.advance(100); assert not any(b.model.applied)

def direction_loss(b,direction):
    b.connect(); b.activate(); b.target([(i,0.8,70) for i in range(8)]); b.advance(250)
    start=b.now; first_event=len(b.events)
    kwargs={'down':False} if direction=='down' else {'up':False} if direction=='up' else {'up':False,'down':False}
    b.advance(280,**kwargs)
    b.stopped(); assert b.fw.state['causes'] & (4|128)
    assert not any(b.model.applied) and b.peer.permitted_run is None
    assert b.now-start==280
    # Restoring traffic cannot restore a session or the old actuation.
    b.advance(150); b.stopped(); assert not any(b.model.applied)
    events=b.events[first_event:]
    local=min(t for kind,t in events if kind=='local_inhibit')-start
    remote=min(t for kind,t in events if kind in ('remote_inhibit','model_watchdog'))-start
    assert local<=240 and remote<=180,(direction,events)
    return {'cut_ms':start,'local_inhibit_latency_ms':local,'remote_inhibit_latency_ms':remote,'directions':direction}

def host_process_pause(b):
    b.connect(); b.activate(); b.target([(0,0.9,60)]); b.advance(200)
    assert any(b.model.applied)
    b.advance(130,endpoint=False)
    assert b.model.watchdog and not any(b.model.applied)
    b.stopped()
    return {'model_watchdog_without_peer_execution':True}

def malformed_packets(b):
    b.connect(); b.activate(); before=[m['target'] for m in b.fw.motors]
    bad_targets=[bytes((2,0))+struct.pack('<fB',0.4,60), # count mismatch
        bytes((2,0))+struct.pack('<fB',0.4,60)+bytes((0,))+struct.pack('<fB',0.5,60),
        bytes((1,8))+struct.pack('<fB',0.4,60), bytes((1,0))+struct.pack('<fB',float('nan'),60),
        bytes((1,0))+struct.pack('<fB',float('inf'),60), bytes((1,0))+struct.pack('<fB',2.1,60),
        bytes((1,0))+struct.pack('<fB',0.4,101)]
    old=b.fw.diag[3]
    for body in bad_targets:
        b.raw(p.TARGET,body)
        assert [m['target'] for m in b.fw.motors]==before
    assert b.fw.diag[3]>=old+len(bad_targets)
    good=p.encode(p.TARGET,b.peer.session,b.peer.sequence+1,b.peer.fw_stamp,p.targets([(0,0.4,60)]))
    bad=bytearray(good); bad[-1]^=0x80
    b.raw(p.TARGET,wire=bytes(bad)); assert [m['target'] for m in b.fw.motors]==before
    bad=bytearray(good); bad[2]=1; b.raw(p.TARGET,wire=bytes(bad))
    # Unknown type, excessive length and the distinct HW/legacy SIM protocols.
    b.raw(99,b''); b.raw(p.TARGET,wire=bytes.fromhex('a55a0207ff'))
    b.raw(p.TARGET,wire=bytes.fromhex('aa0612040a020801aa01020019'))
    assert [m['target'] for m in b.fw.motors]==before
    b.advance(60); b.target([(0,0.4,60)])
    duplicate=b.raw(p.TARGET,p.targets([(0,0.5,60)]))
    after=b.fw.motors[0]['target']; b.raw(p.TARGET,wire=duplicate)
    assert b.fw.motors[0]['target']==after
    b.raw(p.TARGET,p.targets([(0,0.9,60)]),sequence=b.peer.sequence-2)
    b.raw(p.TARGET,p.targets([(0,0.9,60)]),stamp=(b.peer.fw_stamp-100)&0xffffffff)
    assert b.fw.motors[0]['target']==after
    # Nonfinite feedback is rejected as an entire packet and inhibits active axes.
    entries=b.model.feedback()
    body=bytearray(p.feedback(b.fw.state['run'],0,entries))
    struct.pack_into('<f',body,7+2,float('nan'))
    b.raw(p.FEEDBACK,bytes(body)); b.stopped(); assert b.fw.state['causes']&4
    return {'invalid_target_packets':len(bad_targets),'crc_version_length_ids_nonfinite_old_sequences':True}

def parser_fragmentation(b):
    b.connect(); b.activate()
    b.peer.sequence+=1
    frame=p.encode(p.TARGET,b.peer.session,b.peer.sequence,b.peer.fw_stamp,p.targets([(3,0.7,60)]))
    # Noise and a broken candidate, then byte-wise fragmented valid input.
    b.fw.call('RX 55a500a55a0207ff')
    for chunk in (frame[:1],frame[1:4],frame[4:11],frame[11:]):
        b.fw.call('RX '+chunk.hex()); b.fw.call('STEP 1 1 1 0')
    assert abs(b.fw.motors[3]['target']-0.7)<1e-6
    # Consecutive complete frames must both be parsed in a single drain.
    frames=[]
    for mid in (6,1):
        b.peer.sequence+=1
        frames.append(p.encode(p.TARGET,b.peer.session,b.peer.sequence,b.peer.fw_stamp,p.targets([(mid,0.2+mid*0.05,60)])))
    b.fw.call('RX '+b''.join(frames).hex()); b.fw.call('STEP 1 1 1 0')
    assert b.fw.motors[6]['target']>0.49 and b.fw.motors[1]['target']>0.24
    # Interbyte timeout: no portion of the interrupted command applies.
    b.peer.sequence+=1
    f=p.encode(p.TARGET,b.peer.session,b.peer.sequence,b.peer.fw_stamp,p.targets([(4,0.9,60)]))
    b.fw.call('RX '+f[:8].hex()); b.fw.call('STEP 1 1 0 0')
    b.advance(20); b.fw.call('RX '+f[8:].hex()); b.fw.call('STEP 1 1 0 0')
    assert b.fw.motors[4]['target']==0
    b.advance(60); b.target([(4,0.5,60)])
    # UART error and overflow request stop synchronously before the parser runs.
    b.fw.call('ERROR'); b.stopped(); assert b.fw.state['causes']&128
    b.advance(30); assert not b.fw.state['live']

def overflow(b):
    b.connect(); b.activate(); b.target([(0,0.7,70)])
    b.fw.call('RX '+('55'*258)); b.stopped(); assert b.fw.diag[7]==1
    b.advance(200); assert not any(b.model.applied)

def stop_saturation(b):
    b.connect(); b.activate(); b.target([(i,0.8,70) for i in range(8)]); b.advance(100)
    old=b.last_output_frame
    b.fw.call('FILL'); assert b.fw.diag[-1]==8
    b.advance(200,drain=False)
    assert b.fw.diag[4]>0 # replaceable latest output/status, no growing backlog
    b.fw.call('STOP'); b.stopped()
    b.advance(60)
    assert not any(b.model.applied)
    if old: b.peer.receive(old,b.now)
    b.advance(100); b.stopped(); assert not any(b.model.applied)
    return {'ordinary_queue_full':8,'replaced_frames':b.fw.diag[4]}

def inflight_stop(b):
    b.connect(); b.activate(); b.target([(0,0.8,70)]); b.advance(100)
    b.advance(40,mode=3)
    b.stopped(); b.advance(150)
    assert not any(b.model.applied)
    return {'stop_pending_at_HAL_commit':True}

def tx_timeout(b):
    b.connect(); b.activate(); b.target([(0,0.8,70)]); b.advance(100)
    b.advance(40,mode=2); assert b.fw.diag[6]>0
    b.fw.call('STOP'); b.stopped(); b.advance(180)
    assert not any(b.model.applied)

def tx_expiration(b):
    b.connect(); b.activate(); b.target([(0,0.8,70)]); b.advance(80)
    b.advance(40,drain=False)
    expired=b.fw.diag[5]
    # Pause producers at the scheduler boundary, then let the real TX worker
    # examine its old mailbox without replacing it with a fresh output first.
    b.now+=50; b.fw.call('TIME '+str(b.now)); b.fw.call('TX'); frames=list(b.fw.frames)
    assert b.fw.diag[5]>expired
    assert all(p.Parser().feed(f,b.now)[0][0]!=p.OUTPUT for f in frames)
    b.fw.call('STOP'); b.advance(180); b.stopped()
    assert not any(b.model.applied)
    return {'expired_output_dropped':True}

def tx_failure(b,mode):
    b.connect(); b.activate(); b.target([(0,0.8,70)]); b.advance(100)
    b.advance(180,mode=mode)
    assert not any(b.model.applied) and b.peer.permitted_run is None
    if mode==4: assert b.fw.diag[6]>0
    b.fw.call('STOP'); b.stopped(); b.advance(180)
    assert not any(b.model.applied)
    return {'HAL_error':mode==1,'abort_quarantine':mode==4}

def rx_rearm_failure(b):
    b.connect(); b.activate(); b.target([(0,0.8,70)]); b.advance(100)
    b.advance(60,mode=5); b.stopped()
    assert b.fw.state['causes']&128 and not b.fw.state['live']
    b.advance(180); b.stopped(); assert not any(b.model.applied)
    return {'failed_RX_rearm_inhibits_before_parser':True}

def delay_and_force(b):
    b.connect(force_mask=255); b.activate(); b.target([(0,0.8,70)]); b.advance(100)
    b.advance(100,control=False)
    b.stopped(); assert b.fw.state['causes']&8
    b.advance(80); b.command(p.RECOVER); b.command(p.ARM); b.advance(80); b.command(p.ENABLE,b'\xff')
    b.target([(0,0.9,70)]); b.force=1600; b.advance(80)
    b.stopped(); assert b.fw.state['causes']&16
    b.command(p.RECOVER,success=False)
    b.force=0; b.advance(80); b.command(p.RECOVER); b.stopped()

def force_loss(b):
    b.connect(force_mask=255); b.activate(); b.target([(0,0.7,60)]); b.advance(100)
    b.feedback_fields=11 # position/velocity/applied continue; force alone is missing
    b.advance(160); b.stopped(); assert b.fw.state['causes']&4
    assert b.fw.state['live']

def sim_guard(b):
    b.connect(); b.activate(); b.pause_model=True
    b.target([(6,0.8,70)]); b.advance(600)
    b.stopped(); assert b.fw.state['causes']&2
    b.command(p.RECOVER,success=False)
    b.pause_model=False; b.model.position[6]=0.01; b.advance(80)
    b.command(p.RECOVER); b.stopped()

def latency_budget(b):
    b.connect(); b.activate(); b.target([(i,0.6,60) for i in range(8)])
    b.advance(10000,latency_up=10,latency_down=10)
    assert b.fw.state['causes']==0 and max(abs(x-0.6) for x in b.model.position)<0.02
    assert b.up.max_pending<=5 and b.down.max_pending<=5
    duration=b.now/1000
    rate_up=b.up.bytes/duration; rate_down=b.down.bytes/duration
    assert rate_up*10 < p.BAUD and rate_down*10 < p.BAUD
    return {'up_bytes_per_second':rate_up,'down_bytes_per_second':rate_down,
            'up_max_pending':b.up.max_pending,'down_max_pending':b.down.max_pending,
            'extra_each_direction_ms':10,'duration_ms':10000}

def late_feedback(b):
    b.connect(); b.activate(); b.target([(0,0.8,60)]); b.advance(200)
    b.advance(240,latency_feedback=110)
    b.stopped(); assert b.fw.state['causes']&4 and b.fw.state['live']
    assert not any(b.model.applied)
    return {'feedback_extra_delay_ms':110,'heartbeat_extra_delay_ms':0}

def packet_loss(b):
    b.connect(); b.activate(); b.target([(i,0.5,60) for i in range(8)])
    b.advance(6000,drop_feedback_every=4,latency_up=10,latency_down=10)
    assert b.fw.state['causes']==0 and max(abs(v-.5) for v in b.model.position)<.02
    return {'feedback_dropped_fraction':0.25,'extra_each_direction_ms':10}

def sequence_wrap(b):
    b.connect()
    accepted=b.fw.diag[0]
    for value in (0x7ffffffe,0xfffffffc,0xfffffffd,0xfffffffe,0xffffffff,0,1):
        b.peer.sequence=value
        b.raw(p.HEARTBEAT,sequence=value)
        assert b.fw.diag[0]==accepted+1,(value,b.fw.diag)
        accepted+=1
    b.raw(p.HEARTBEAT,sequence=0xffffffff); assert b.fw.diag[0]==accepted
    b.raw(p.HEARTBEAT,sequence=1); assert b.fw.diag[0]==accepted
    # Whole session/feedback/control roundtrip spanning HAL's uint32 wrap.
    b.close(); b.fw=__import__('bench').Firmware(b.fw.process.args[0])
    b.peer=__import__('sim.peer',fromlist=['Peer']).Peer(); b.model=__import__('sim.model',fromlist=['RotorModel']).RotorModel()
    b.up=__import__('bench').Link(); b.down=__import__('bench').Link()
    b.last_permitted=None; b.last_revision=0; b.now=0xfffffff0
    b.connect(sid=2); b.activate(); b.target([(0,0.4,60)]); b.advance(1000)
    assert b.fw.state['causes']==0 and b.model.position[0]>.15
    return {'input_sequence_wrap':True,'HAL_millisecond_wrap':True}

def atomic_feedback_reject(b):
    b.connect(); before=[m['measured'] for m in b.fw.motors]
    valid=p.feedback(b.fw.state['run'],0,[(0,11,.9,0.,0,0.),(1,11,.8,0.,0,0.)])
    malformed=[]
    bad=bytearray(valid); bad[7+14]=0; malformed.append(bytes(bad))
    bad=bytearray(valid); bad[7+14]=8; malformed.append(bytes(bad))
    bad=bytearray(valid); bad[6]=86; malformed.append(bytes(bad))
    malformed += [valid[:-1],valid+b'\x00']
    bad=bytearray(valid); bad[8]=0x80; malformed.append(bytes(bad))
    bad=bytearray(valid); bad[8]=1; struct.pack_into('<h',bad,7+12,100); malformed.append(bytes(bad))
    for body in malformed:
        b.raw(p.FEEDBACK,body)
        assert [m['measured'] for m in b.fw.motors]==before
    assert not b.fw.state['armed']
    return {'atomic_rejections':len(malformed)}

CASES={'closed_loop':closed_loop,'startup_reconnect':startup_reconnect,'partial_routing':partial_routing,
    'feedback_loss':feedback_loss,'firmware_to_host_loss':lambda b:direction_loss(b,'down'),
    'host_to_firmware_loss':lambda b:direction_loss(b,'up'),'complete_cut':lambda b:direction_loss(b,'both'),
    'host_process_pause':host_process_pause,'malformed_packets':malformed_packets,
    'parser_fragmentation':parser_fragmentation,'overflow':overflow,'stop_saturation':stop_saturation,
    'inflight_stop':inflight_stop,'tx_timeout':tx_timeout,'tx_expiration':tx_expiration,
    'tx_hal_error':lambda b:tx_failure(b,1),'tx_abort_failure':lambda b:tx_failure(b,4),
    'rx_rearm_failure':rx_rearm_failure,'control_delay_force':delay_and_force,
    'force_loss':force_loss,'sim_guard':sim_guard,'latency_budget':latency_budget,
    'late_feedback':late_feedback,'packet_loss':packet_loss,'sequence_wrap':sequence_wrap,'atomic_feedback_reject':atomic_feedback_reject}
def run_cases(binary,selected='all',artifact_suffix=''):
    names=list(CASES) if selected=='all' else selected.split(',')
    OUT.mkdir(parents=True,exist_ok=True)
    results={}
    with (OUT/('trace'+artifact_suffix+'.csv')).open('w',newline='',encoding='utf8') as stream:
        trace=csv.writer(stream)
        trace.writerow(['scenario','time_ms','session','run','motor','target_rad','reference_rad','feedback_rad',
            'model_rad','velocity_rad_s','pid_percent','applied_percent','enabled','virtual_armed',
            'stop_causes','measurement_age_ms','peer_stop_reason','model_watchdog'])
        for name in names:
            b=Bench(binary,name,trace)
            try:
                results[name]=CASES[name](b) or {'passed':True}
                print('PASS SIM '+name,flush=True)
            finally: b.close()
    (OUT/('results'+artifact_suffix+'.json')).write_text(json.dumps(results,indent=2)+'\n')
    print('PC software scenarios passed; trace: build/simulation/trace.csv. No hardware or Gazebo test.')
    return results
