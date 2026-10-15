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
        flags.write_text(compile_flags or '-DN_PULSE_ENABLE_CURRENT_ACQUISITION=0 -DN_PULSE_HW_SINGLE_MOTOR_TEST=0 | test\n')
        manifest.write_text(json.dumps({'schema':1,'mode':mode,
            'elf_sha256':hashlib.sha256(elf.read_bytes()).hexdigest(),
            'flags_sha256':hashlib.sha256(flags.read_bytes()).hexdigest()}))
    log=FIXTURE/'fake.log'; count=0
    def case(name,ok,args=(),env=None,program=False,expected=None):
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
            assert calls.count('ARG -c\n')==1 and 'ARG sn=SNA\n' in calls and 'sn=SNB' not in calls
            assert f'ARG {elf}\n' in calls and 'ARG -v\n' in calls
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
    assert calls=='CALL\nARG -l\n' and f'"{fake}"' in r.stdout and f'"{elf}"' in r.stdout
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
    artifact(); header[18]=62; artifact()
    case('non ARM ELF refused',False,['-SerialNumber','SNA','-DryRun'])
    print(f'{count} script scenarios passed using ONLY the simulated executable.')

if __name__=='__main__': main()
