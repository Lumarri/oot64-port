"""Load the PRIVATE MM3D ARM64 module through the upstream loader on USB Android.

Checks the staged module/hashes and ABI/dispatch inventory; does not execute
guest functions, boot the game, install game assets or alter the demo APK.
"""
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def main():
    stage = ROOT / 'local/android-title-stage'
    manifest = json.loads((stage / 'manifest.json').read_text())
    base = json.loads((ROOT / 'config/android-base.json').read_text())
    if manifest['base'] != base or manifest.get('private_game_derived') is not True:
        raise ValueError('Private title does not match the pinned base')
    names = {'libtriaevum_title_aot.so', 'mm3d_title_probe', 'libc++_shared.so'}
    if set(manifest['files']) != names:
        raise ValueError('Unexpected private title staging contents')
    for name, info in manifest['files'].items():
        if hashlib.sha256((stage / name).read_bytes()).hexdigest() != info['sha256']:
            raise ValueError(f'Staged file changed: {name}')

    def adb(*arguments, check=True):
        return subprocess.run(['adb', '-d', *arguments], check=check, capture_output=True,
                              text=True, encoding='utf-8', errors='replace', timeout=120)

    adb('get-state')
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    remote = '/data/local/tmp/mm3d-private-title-' + stamp
    adb('shell', 'mkdir', '-p', remote)
    for name in sorted(names):
        adb('push', str(stage / name), remote + '/' + name)
        adb('shell', 'chmod', '700', remote + '/' + name)
    arguments = ['./mm3d_title_probe', './libtriaevum_title_aot.so', str(manifest['expected_entry_count']), hex(manifest['entrypoint'])]
    command = 'cd ' + shlex.quote(remote) + ' && LD_LIBRARY_PATH=. ' + ' '.join(map(shlex.quote, arguments))
    result = adb('shell', 'timeout 60 sh -c ' + shlex.quote(command), check=False)
    report = {'scope': 'private_title_loaded_not_game_boot', 'utc': stamp,
              'base_commit': base['commit'], 'source_manifest_sha256': manifest['source_manifest_sha256'],
              'files': manifest['files'], 'expected_entry_count': manifest['expected_entry_count'],
              'exit_code': result.returncode, 'stdout': result.stdout, 'stderr': result.stderr,
              'model': adb('shell', 'getprop', 'ro.product.model').stdout.strip()}
    path = ROOT / 'local/android-logs' / ('title-' + stamp + '.json')
    path.parent.mkdir(exist_ok=True)
    path.write_text(json.dumps(report, indent=2) + '\n')
    print(f'Private title ABI: {"PASS" if result.returncode == 0 else "FAIL"} ({result.returncode})')
    print(f'Private report: {path}')
    if result.returncode:
        print(result.stdout + result.stderr)
    return int(result.returncode != 0)


if __name__ == '__main__':
    raise SystemExit(main())
