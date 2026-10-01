#!/usr/bin/env python3
# Verify package contracts with local HTTP and Gateway fixtures on a native runtime.
import argparse
import configparser
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]  # Public package source and test fixtures.


# Execute a bounded test and reject engine diagnostics even when exit status is zero.
def execute(engine, project, gd, expected):
    command = [str(engine), '--allow-net', '--allow-ext', 'run', 'late_test.gd'] if gd else [str(engine), '--headless', '--path', str(project), '--script', 'res://late_test.gd']
    result = subprocess.run(command, cwd=project, capture_output=True, text=True, timeout=90)
    output = result.stdout + result.stderr
    if result.returncode or expected not in output or 'ERROR' in output or 'leaked' in output:
        raise RuntimeError(output)
    print(expected)


# Exercise each declared native configuration and the script package independently.
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--engine', type=Path, required=True)
    parser.add_argument('--artifacts', type=Path, required=True)
    parser.add_argument('--platform', choices=['macos', 'linux', 'windows'], required=True)
    parser.add_argument('--gd', action='store_true')
    args = parser.parse_args()
    (ROOT / 'tmp').mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(dir=ROOT / 'tmp', prefix='native-') as folder:
        temp = Path(folder)
        for variant in ('debug', 'release'):
            project = temp / variant
            shutil.copytree(ROOT / 'tests/ext/supabase', project)
            manifest = configparser.ConfigParser(interpolation=None)
            manifest.optionxform = str
            manifest.read(ROOT / 'extensions/supabase/supabase.gdextension')
            chosen = {key: value for key, value in manifest['libraries'].items()
                      if key.startswith(args.platform + '.' + variant + '.')}
            if len(chosen) != 1:
                raise ValueError('expected one platform library')
            key, relative = next(iter(chosen.items()))
            leaf = Path(relative.strip('"')).name
            (project / 'bin').mkdir()
            shutil.copyfile(args.artifacts / leaf, project / 'bin' / leaf)
            manifest['libraries'] = {key.replace('.' + variant + '.', '.debug.'): relative,
                                     key.replace('.' + variant + '.', '.release.'): relative}
            with (project / 'supabase.gdextension').open('w') as stream:
                manifest.write(stream)
            execute(args.engine.resolve(), project, args.gd, 'checks=21 failures=0')
        project = temp / 'discord'
        shutil.copytree(ROOT / 'tests/ext/discord', project)
        shutil.copytree(ROOT / 'extensions/discord/src', project / 'discord')
        execute(args.engine.resolve(), project, args.gd, 'checks=26 failures=0')
    print('native package contracts: ok')


if __name__ == '__main__':
    main()
