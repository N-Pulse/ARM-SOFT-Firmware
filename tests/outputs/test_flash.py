"""Run the real script in an isolated fixture with a simulated executable only.

No STM32 tool or hardware is invoked, including enumeration, reset or connection.
"""
from pathlib import Path
import hashlib
import json
import os
import shutil
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
FIXTURE = ROOT/'build/output-tests/flash-fixture'

def main():
    FIXTURE.mkdir(parents=True, exist_ok=True)
    script = FIXTURE/'flash.ps1'; shutil.copy2(ROOT/'flash.ps1',script)
    fake = FIXTURE/'fake_programmer.exe'
    subprocess.run([os.environ.get('HOST_CC','gcc'),'-std=c11','-Wall','-Wextra','-Werror',
                    str(HERE/'fake_programmer.c'),'-o',str(fake)],check=True)
    hw=FIXTURE/'build/HW'; hw.mkdir(parents=True,exist_ok=True)
    elf=hw/'n-pulse.elf'; flags=hw/'compile.flags'; manifest=hw/'build-manifest.json'
    header=bytearray(52); header[:6]=b'\x7fELF\x01\x01'; struct.pack_into('<HH',header,16,2,40)
    def artifact(mode='HW', compile_flags=None):
        elf.write_bytes(header)
        flags.write_text(compile_flags or '-DN_PULSE_OLD_HW=1 -DN_PULSE_TARGET_RET6=1 -DN_PULSE_DUAL_BOARD_LINK=1 -DN_PULSE_OLD_HW_MOTION_OPT_IN=0 -DN_PULSE_THUMB_EXTI_MOTION_QUALIFIED=0 -DN_PULSE_TIMING_DIAGNOSTICS=0 -DN_PULSE_ENABLE_CURRENT_ACQUISITION=0 -DN_PULSE_HW_SINGLE_MOTOR_TEST=0 | test\n')
        manifest.write_text(json.dumps({'schema':1,'mode':mode,'hardware':'STM32G474RET6','milestone':'OLD_HW_LOT4','physical_outputs':'permanently_disabled',
            'elf_sha256':hashlib.sha256(elf.read_bytes()).hexdigest(),
            'flags_sha256':hashlib.sha256(flags.read_bytes()).hexdigest()}))
    log=FIXTURE/'fake.log'; count=0
    def case(name,ok,args=(),env=None,program=False,expected=None,serial='SNA',image=None):
        nonlocal count
        log.write_text('')
        process_env={k:v for k,v in os.environ.items() if not k.startswith('FAKE_')}
        process_env['FAKE_PROGRAMMER_LOG']=str(log); process_env.update(env or {})
        command=['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',str(script),
                 '-ProgrammerPath',str(fake),*args]
        result=subprocess.run(command,cwd=FIXTURE,env=process_env,capture_output=True,text=True)
        assert (result.returncode==0)==ok,(name,result.stdout,result.stderr)
        if not ok: assert 'REFUS/ECHEC:' in result.stdout,(name,result.stdout,result.stderr)
        calls=log.read_text(); assert ('ARG -c\n' in calls)==program,(name,calls)
        if program:
            assert calls.count('ARG -c\n')==1 and f'ARG sn={serial}\n' in calls
            assert f'ARG sn={"SNB" if serial=="SNA" else "SNA"}\n' not in calls
            assert f'ARG {image or elf}\n' in calls and 'ARG -v\n' in calls
        if expected: assert expected in result.stdout,(name,result.stdout)
        count+=1; print('PASS flash mock:',name)
        return result,calls
    artifact()
    case('missing explicit serial',False,expected='numero de serie')
    case('invalid serial',False,['-SerialNumber','SNA;BAD'],expected='alphanumerique')
    case('serial absent',False,['-SerialNumber','SNC','-DryRun'],expected='apparait 0 fois')
    case('prefix is not exact serial',False,['-SerialNumber','SN','-DryRun'],expected='apparait 0 fois')
    case('duplicate serial ambiguous',False,['-SerialNumber','SNA','-DryRun'],
         {'FAKE_SERIAL_LINES':'ST-LINK SN : SNA\nST-LINK SN : SNA'},expected='apparait 2 fois')
    case('enumeration error',False,['-SerialNumber','SNA','-DryRun'],{'FAKE_LIST_CODE':'7'},expected='Echec enumeration')
    r,calls=case('dry-run selects exactly one; no connect/write/reset',True,
                ['-SerialNumber','SNA','-DryRun'],expected='DRY-RUN')
    assert calls=='CALL\nARG -l\nARG stlink\n' and f'"{fake}"' in r.stdout and f'"{elf}"' in r.stdout
    case('one simulated write+verify+reset, other probe untouched',True,
         ['-SerialNumber','SNA'],program=True)
    case('programmer nonzero exit',False,['-SerialNumber','SNA'],{'FAKE_PROGRAM_CODE':'8'},program=True)
    case('tool Error line despite zero exit',False,['-SerialNumber','SNA'],{'FAKE_PROGRAM_ERROR':'1'},program=True)
    case('SIM path refused',False,['-SerialNumber','SNA','-ElfPath',str(FIXTURE/'build/SIM/n-pulse.elf'),'-DryRun'])
    case('legacy alias refused',False,['-SerialNumber','SNA','-ElfPath',str(FIXTURE/'Debug/N-PULSE FIRMWARE.elf'),'-DryRun'])
    case('commissioning path refused',False,['-SerialNumber','SNA','-ElfPath',str(FIXTURE/'build/HW-single-1/n-pulse.elf'),'-DryRun'])
    artifact(compile_flags='-DN_PULSE_ENABLE_CURRENT_ACQUISITION=0 -DN_PULSE_HW_SINGLE_MOTOR_TEST=1 | test\n')
    case('commissioning image at canonical path refused',False,['-SerialNumber','SNA','-DryRun'])
    artifact()
    elf.write_bytes(b'SIM substituted for HW')
    case('substituted artifact fails digest',False,['-SerialNumber','SNA','-DryRun'])
    artifact(mode='SIM')
    case('SIM manifest refused at HW path',False,['-SerialNumber','SNA','-DryRun'])
    artifact(compile_flags='-DMOTOR_BACKEND_SIM=1 -DN_PULSE_ENABLE_CURRENT_ACQUISITION=0 | test\n')
    case('SIM compile flags refused',False,['-SerialNumber','SNA','-DryRun'])
    artifact(compile_flags='-DN_PULSE_ENABLE_CURRENT_ACQUISITION=1 | test\n')
    case('experimental ADC build refused',False,['-SerialNumber','SNA','-DryRun'])
    artifact(); flags.write_text('changed flags')
    case('changed flags digest refused',False,['-SerialNumber','SNA','-DryRun'])
    artifact(); original_flags=flags.read_text()
    for flag in ('N_PULSE_OLD_HW_MOTION_OPT_IN','N_PULSE_TIMING_DIAGNOSTICS','N_PULSE_THUMB_EXTI_MOTION_QUALIFIED'):
        artifact(compile_flags=original_flags.replace(flag+'=0',flag+'=1'))
        case(flag+' refused',False,['-SerialNumber','SNA','-DryRun'])
    artifact(); info=json.loads(manifest.read_text()); info['physical_outputs']='enabled'; manifest.write_text(json.dumps(info))
    case('non inhibited manifest refused',False,['-SerialNumber','SNA','-DryRun'])
    artifact(); header[18]=62; artifact()
    case('non ARM ELF refused',False,['-SerialNumber','SNA','-DryRun'])

    # Dedicated images must be named explicitly and retain hashes, motor/PWM
    # containment and the same exact-probe selection as ordinary firmware.
    header[18]=40; artifact()
    tool_dir=FIXTURE/'tools'; tool_dir.mkdir(exist_ok=True)
    builder=tool_dir/'build_artifacts.py'; shutil.copy2(ROOT/'tools/build_artifacts.py',builder)
    finger=FIXTURE/'build/HW-single-1-optin'; finger.mkdir(parents=True,exist_ok=True)
    finger_elf=finger/'n-pulse.elf'; finger_flags=finger/'compile.flags'; finger_manifest=finger/'build-manifest.json'
    def finger_artifact():
        finger_elf.write_bytes(header)
        text=flags.read_text().replace('N_PULSE_OLD_HW_MOTION_OPT_IN=0','N_PULSE_OLD_HW_MOTION_OPT_IN=1')
        text=text.replace('N_PULSE_HW_SINGLE_MOTOR_TEST=0','N_PULSE_HW_SINGLE_MOTOR_TEST=1')
        finger_flags.write_text(text+' -DN_PULSE_HW_TEST_MOTOR=1 -DN_PULSE_HW_TEST_PWM_CAP=5\n')
        subprocess.run([sys.executable,'-B',builder,'finger-manifest','build/HW-single-1-optin','1'],check=True)
    finger_artifact()
    info=json.loads(finger_manifest.read_text())
    assert info['finger_motor_id']==1 and info['physical_outputs']=='single_finger_commissioning'
    case('finger image requires explicit -Finger',False,
         ['-SerialNumber','SNA','-ElfPath',str(finger_elf),'-DryRun'])
    case('finger profile refuses ordinary ELF',False,
         ['-Finger','index','-SerialNumber','SNA','-ElfPath',str(elf),'-DryRun'])
    r,calls=case('finger dry-run enumerates ST-LINK only',True,
         ['-Finger','index','-SerialNumber','SNA','-DryRun'],expected='DRY-RUN')
    assert calls=='CALL\nARG -l\nARG stlink\n' and str(finger_elf) in r.stdout
    for serial in ('SNA','SNB'):
        case('same finger ELF on selected probe '+serial,True,
             ['-Finger','index','-SerialNumber',serial],program=True,serial=serial,image=finger_elf)
    case('finger exact serial required',False,
         ['-Finger','index','-SerialNumber','SN','-DryRun'],expected='apparait 0 fois')
    finger_elf.write_bytes(bytes(header)+b'changed')
    case('finger substituted ELF digest refused',False,['-Finger','index','-SerialNumber','SNA','-DryRun'])
    finger_artifact(); finger_flags.write_text(finger_flags.read_text()+' changed')
    case('finger substituted flags digest refused',False,['-Finger','index','-SerialNumber','SNA','-DryRun'])
    for old,new in (('N_PULSE_HW_TEST_MOTOR=1','N_PULSE_HW_TEST_MOTOR=2'),
                    ('N_PULSE_HW_TEST_PWM_CAP=5','N_PULSE_HW_TEST_PWM_CAP=11'),
                    ('N_PULSE_OLD_HW_MOTION_OPT_IN=1','N_PULSE_OLD_HW_MOTION_OPT_IN=0'),
                    ('N_PULSE_HW_SINGLE_MOTOR_TEST=1','N_PULSE_HW_SINGLE_MOTOR_TEST=0'),
                    ('N_PULSE_ENABLE_CURRENT_ACQUISITION=0','N_PULSE_ENABLE_CURRENT_ACQUISITION=1'),
                    ('N_PULSE_THUMB_EXTI_MOTION_QUALIFIED=0','N_PULSE_THUMB_EXTI_MOTION_QUALIFIED=1'),
                    ('N_PULSE_TIMING_DIAGNOSTICS=0','N_PULSE_TIMING_DIAGNOSTICS=1'),
                    (' | test',' -DMOTOR_BACKEND_SIM=1 | test'),
                    (' | test',' -DN_PULSE_HW_SINGLE_MOTOR_TEST=0 | test')):
        finger_artifact(); finger_flags.write_text(finger_flags.read_text().replace(old,new))
        # Even if someone refreshed the receipt's digest, the profile gates reject it.
        info=json.loads(finger_manifest.read_text()); info['flags_sha256']=hashlib.sha256(finger_flags.read_bytes()).hexdigest()
        finger_manifest.write_text(json.dumps(info))
        case('finger invalid flag refused: '+new,False,['-Finger','index','-SerialNumber','SNA','-DryRun'])
        rejected=subprocess.run([sys.executable,'-B',builder,'finger-manifest','build/HW-single-1-optin','1'],
                                capture_output=True,text=True)
        assert rejected.returncode!=0,('manifest accepted invalid flags',new)
    for field,value in (('finger_motor_id',2),('physical_outputs','permanently_disabled')):
        finger_artifact(); info=json.loads(finger_manifest.read_text()); info[field]=value
        finger_manifest.write_text(json.dumps(info))
        case('finger wrong receipt '+field+' refused',False,['-Finger','index','-SerialNumber','SNA','-DryRun'])

    diagnostic=FIXTURE/'build/HW-fingers-optin-diagnostic'
    diagnostic.mkdir(parents=True,exist_ok=True)
    diagnostic_elf=diagnostic/'n-pulse.elf'
    diagnostic_flags=diagnostic/'compile.flags'
    diagnostic_manifest=diagnostic/'build-manifest.json'
    def diagnostic_artifact(checks=False):
        finger_artifact()
        diagnostic_elf.write_bytes(header)
        text=finger_flags.read_text().replace('N_PULSE_HW_TEST_PWM_CAP=5','N_PULSE_HW_TEST_PWM_CAP=40')
        text+=' -DN_PULSE_HW_SELECTABLE_FINGER=1 -DN_PULSE_HW_FINGER_DIAGNOSTIC=1'
        text+=''.join(f' -DN_PULSE_DIAG_CHECK_{name}={int(checks)}' for name in ('GUARD','TRAVEL','STEP','PREFLIGHT','ENCODER'))
        diagnostic_flags.write_text(text+'\n')
        subprocess.run([sys.executable,'-B',builder,'finger-manifest','build/HW-fingers-optin-diagnostic','all'],check=True)
    diagnostic_artifact()
    info=json.loads(diagnostic_manifest.read_text())
    assert info['diagnostic_max_run_ms']==2000 and not any(info['diagnostic_checks'].values())
    case('diagnostic requires explicit -Fingers -Diagnostic',False,
         ['-Fingers','-SerialNumber','SNA','-ElfPath',str(diagnostic_elf),'-DryRun'])
    case('diagnostic requires common finger profile',False,
         ['-Diagnostic','-Finger','index','-SerialNumber','SNA','-DryRun'])
    r,calls=case('diagnostic dry-run enumerates only',True,
         ['-Fingers','-Diagnostic','-SerialNumber','SNA','-DryRun'],expected='DRY-RUN')
    assert calls=='CALL\nARG -l\nARG stlink\n' and str(diagnostic_elf) in r.stdout
    for serial in ('SNA','SNB'):
        case('same diagnostic ELF on selected probe '+serial,True,
             ['-Fingers','-Diagnostic','-SerialNumber',serial],program=True,serial=serial,image=diagnostic_elf)
    diagnostic_artifact(checks=True)
    case('diagnostic checks enabled accepted',True,
         ['-Fingers','-Diagnostic','-SerialNumber','SNA','-DryRun'])
    for field,value in (('diagnostic_max_run_ms',3000),('diagnostic_checks',{'guard':False,'travel':True,'step':True,'preflight':True})):
        diagnostic_artifact(checks=True)
        info=json.loads(diagnostic_manifest.read_text()); info[field]=value
        diagnostic_manifest.write_text(json.dumps(info))
        case('diagnostic incorrect receipt '+field+' refused',False,
             ['-Fingers','-Diagnostic','-SerialNumber','SNA','-DryRun'])
    diagnostic_artifact()
    diagnostic_flags.write_text(diagnostic_flags.read_text().replace('N_PULSE_HW_TEST_PWM_CAP=40','N_PULSE_HW_TEST_PWM_CAP=41'))
    info=json.loads(diagnostic_manifest.read_text()); info['flags_sha256']=hashlib.sha256(diagnostic_flags.read_bytes()).hexdigest()
    diagnostic_manifest.write_text(json.dumps(info))
    case('diagnostic PWM above 40 refused',False,
         ['-Fingers','-Diagnostic','-SerialNumber','SNA','-DryRun'])
    print(f'{count} script scenarios passed using ONLY the simulated executable.')

if __name__=='__main__': main()
