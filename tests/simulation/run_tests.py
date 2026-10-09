"""Alias for the documented one-command native SIM bench."""
from pathlib import Path
import runpy
runpy.run_path(str(Path(__file__).resolve().parents[2]/'tools/run_simulation.py'),run_name='__main__')
