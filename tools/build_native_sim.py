"""Build the actual production C SIM controller with the PC HAL boundary."""
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tests/simulation'))
from bench import build
if __name__=='__main__': print(build('firmware-realtime'))
