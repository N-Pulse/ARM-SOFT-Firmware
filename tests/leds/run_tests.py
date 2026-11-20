from pathlib import Path
import os
import subprocess
ROOT=Path(__file__).resolve().parents[2]
out=ROOT/'build/led-board-tests'
out.mkdir(parents=True,exist_ok=True)
subprocess.run([os.environ.get('HOST_CC','clang'),'-std=c11','-Wall','-Wextra','-Werror',
                '-fsanitize=address,undefined','-g','-Itests/leds/mocks',
                '-Ifw/bsp/status_leds','fw/bsp/status_leds/status_leds_board.c',
                'fw/bsp/status_leds/status_leds_codec.c','tests/leds/test_board.c',
                '-o',str(out/'led-board')],cwd=ROOT,check=True)
subprocess.run([str(out/'led-board')],cwd=ROOT,check=True)
