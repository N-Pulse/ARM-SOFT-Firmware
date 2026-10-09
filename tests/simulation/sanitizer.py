from pathlib import Path
import os
import shutil
import subprocess
from bench import ROOT,OUT,compiler_arguments
from scenarios import run_cases

def run_available(selected):
    probe=OUT/'sanitizer_probe.c'; probe.write_text('int main(void){return 0;}\n')
    candidates=[os.environ.get('HOST_CC','gcc')]
    if shutil.which('clang'): candidates.append(shutil.which('clang'))
    for kind in ('address,undefined','undefined'):
        label=kind.replace(',','-'); chosen=None; logs=[]
        for compiler in candidates:
            binary=OUT/('probe-'+label+('.exe' if os.name=='nt' else ''))
            built=subprocess.run([compiler,'-fsanitize='+kind,str(probe),'-o',str(binary)],capture_output=True,text=True)
            logs.append(str(built.args)+'\n'+built.stdout+built.stderr)
            if built.returncode==0:
                result=subprocess.run([str(binary)],capture_output=True,text=True)
                logs.append(result.stdout+result.stderr)
                if result.returncode==0: chosen=compiler; break
        (OUT/('probe-'+label+'.log')).write_text('\n'.join(logs))
        if not chosen:
            print('SKIP SIM '+kind+': compiler/runtime unavailable; probe-'+label+'.log')
            continue
        binary=OUT/('firmware-'+label+('.exe' if os.name=='nt' else ''))
        flags=['-fsanitize='+kind,'-fno-sanitize-recover=all','-fno-omit-frame-pointer']
        subprocess.run([chosen,*flags,*compiler_arguments(),'-o',str(binary)],cwd=ROOT,check=True)
        run_cases(binary,selected,artifact_suffix='-'+label)
        protocol=OUT/('protocol-'+label+('.exe' if os.name=='nt' else ''))
        subprocess.run([chosen,*flags,'-std=c11','-I','fw/comm','fw/comm/sim_protocol.c',
                       'tests/simulation/test_protocol.c','-o',str(protocol)],cwd=ROOT,check=True)
        subprocess.run([str(protocol)],check=True)
        print('PASS SIM production native/parser '+kind)
