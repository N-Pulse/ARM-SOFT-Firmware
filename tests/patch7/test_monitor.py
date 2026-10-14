"""Host HW schema/client tests; no serial device opened."""
from pathlib import Path
import sys
import math
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
import hw_monitor as h
def run():
    assert h.stop_frame()==bytes.fromhex('aa022200')
    for command in ('limits 0 0.2 0.02 2 0.1','calibrate 1000 4 1 -1 -9223372036854775808',
                    'zero','recover','preflight verified','link-authorize','link-recover','arm','enable','target 0.02 2','disable','estimator-reset verified','estimator-stop'):
        data=h.command_frame(command,1,3,4); assert len(data)<=130
        message=h.pb.DeviceMessage(); message.ParseFromString(data[2:]); assert message.commission.epoch==3
    preflight=h.pb.DeviceMessage(); preflight.ParseFromString(h.command_frame('preflight verified',1,3,4)[2:])
    assert preflight.commission.operation==11 and preflight.commission.stationary_reference_verified
    for command in ('arm','target nan 2','limits 0 1 inf 2 0.1','estimator-reset no','preflight no'):
        try: h.command_frame(command,1,None if command=='arm' else 3,4)
        except ValueError: pass
        else: raise AssertionError(command)
    d=h.pb.MotorDiagnostic()
    for f in d.DESCRIPTOR.fields:
        setattr(d,f.name,-2**63 if f.type==18 else -3.4e38 if f.type==2 else 2**32-1)
    msg=h.pb.ProsthesisMessage(diagnostic=d); data=h.frame(msg)
    assert len(data)==125 # 123 payload + 2-byte envelope, within 128 payload
    decoder=h.Decoder(); output=[]
    for byte in data: output+=decoder.feed(bytes((byte,)),0)
    assert len(output)==1 and output[0].diagnostic.raw_count==-2**63
    decoder.feed(data[:5],0); assert decoder.feed(b'',.2)==[]
    assert len(decoder.feed(b'noise\xaa\xff'+data,.21))==1
    d=h.pb.MotorDiagnostic(current_a=math.nan,adc_voltage=math.nan,encoder_rad=math.nan,
        velocity_rad_s=math.nan,kalman_rad=math.nan,kalman_velocity_rad_s=math.nan)
    text=h.describe(d)
    assert 'A=unavailable' in text and 'ADC=unavailable/INVALID' in text and 'encoder=unavailable' in text
    assert 'A=unavailable' in h.describe(h.pb.MotorDiagnostic())
    d.flags=(1<<21)|(42<<24)
    assert 'source=remote remote_age_ms=42' in h.describe(d)
    assert 'counts=unavailable' in h.describe(d)
    d.flags|=128; d.raw_count=17; assert 'counts=17' in h.describe(d)
    for command,mid in (('link',8),('status 4',4),('raw 1',1)):
        request=h.pb.DeviceMessage(); request.ParseFromString(h.command_frame(command,1,None,0)[2:])
        assert request.diagnostic.motor_id==mid
    status=h.pb.LinkDiagnostic(role=1,local_stops=1,remote_stops=128,flags=1,heartbeat_age_ms=50)
    text=h.describe_link(status)
    assert 'role=MASTER' in text and 'local_STOP=MANUAL' in text and 'remote_STOP=LINK' in text
    response=h.pb.ProsthesisMessage(link=status); wire=h.frame(response)
    decoded=h.Decoder().feed(wire,0); assert len(decoded)==1 and decoded[0].WhichOneof('message')=='link'
    print('PASS monitor/schema: thirteen explicit commands, link recovery/authorization, preflight attestation, remote age, finite validation, signed raw counts, 123-byte worst payload, fragmented/noisy/expired frames, unavailable display')
if __name__=='__main__': run()
