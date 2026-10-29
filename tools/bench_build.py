"""Portable build filesystem operations, confined to build/bench."""
from pathlib import Path
import shutil
import sys

ROOT = Path(__file__).resolve().parents[1]
OUT = (ROOT / 'build' / 'bench').resolve()

def checked(value):
    path = (ROOT / value).resolve()
    if path != OUT and OUT not in path.parents:
        raise ValueError(f'Path is outside build/bench: {path}')
    return path

def main():
    action, *args = sys.argv[1:]
    if action == 'clean':
        # Resolve and verify absolute deletion target before recursive deletion.
        path = checked('build/bench')
        if path.exists():
            shutil.rmtree(path)
    elif action == 'mkdir':
        checked(args[0]).mkdir(parents=True, exist_ok=True)
    elif action == 'stamp':
        path = checked(args[0]); content = args[1] + '\n'
        path.parent.mkdir(parents=True, exist_ok=True)
        if not path.exists() or path.read_text() != content:
            path.write_text(content)
    elif action == 'objects':
        checked(args[0]).write_text('\n'.join(f'"{p}"' for p in args[1:]) + '\n')
    else:
        raise ValueError(action)

if __name__ == '__main__':
    main()
