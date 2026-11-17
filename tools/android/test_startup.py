"""Execute the real PRIVATE MM3D entry through whole-AOT on USB Android.

Runs only to the first guest SVC, with a block budget and remote timeout.
No graphics, guest service handlers, RomFS, APK changes or savedata writes.
"""
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def main():
    stage = ROOT / 'local/android-startup-stage'
    manifest = json.loads((stage / 'manifest.json').read_text())
    config = json.loads((ROOT / 'config/android-base.json').read_text())
    scope = 'real_title_entry_to_first_svc_not_game_boot'
    if manifest['base'] != config or manifest.get('private_game_derived') is not True or manifest['scope'] != scope:
        raise ValueError('Startup staging does not match the pinned private base')
    names = {'mm3d_startup_probe': 'startup', 'code.bin': 'startup', 'process-manifest.json': 'startup',
             'libtriaevum_title_aot.so': 'title', 'libc++_shared.so': 'title'}
    if set(manifest['files']) != set(names):
        raise ValueError('Unexpected private startup files')
    paths = {}
    for name, info in manifest['files'].items():
        if info['stage'] != names[name]:
            raise ValueError('Unexpected private startup source directory')
        path = ROOT / ('local/android-' + info['stage'] + '-stage') / name
        if path.stat().st_size != info['bytes'] or hashlib.sha256(path.read_bytes()).hexdigest() != info['sha256']:
            raise ValueError(f'Staged file changed: {name}')
        paths[name] = path

    def adb(*args, check=True):
        return subprocess.run(['adb', '-d', *args], check=check, capture_output=True,
                              text=True, encoding='utf-8', errors='replace', timeout=120)

    adb('get-state')
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    remote = '/data/local/tmp/mm3d-private-startup-' + stamp
    adb('shell', 'mkdir', '-p', remote)
    adb('shell', 'chmod', '700', remote)
    for name, path in paths.items():
        adb('push', str(path), remote + '/' + name)
        adb('shell', 'chmod', '700' if name.endswith('.so') or name == 'mm3d_startup_probe' else '600', remote + '/' + name)
        actual = adb('shell', 'sha256sum', remote + '/' + name).stdout.split()[0]
        if actual != manifest['files'][name]['sha256']:
            raise ValueError(f'Device copy differs: {name}')
    arguments = ['./mm3d_startup_probe', './libtriaevum_title_aot.so', './code.bin', './process-manifest.json']
    command = 'cd ' + shlex.quote(remote) + ' && LD_LIBRARY_PATH=. ' + ' '.join(map(shlex.quote, arguments))
    result = adb('shell', 'timeout 60 sh -c ' + shlex.quote(command), check=False)
    report = {'scope': scope, 'utc': stamp, 'base_commit': config['commit'],
              'source_manifest_sha256': manifest['source_manifest_sha256'],
              'layout_builder_sha256': manifest['layout_builder_sha256'],
              'files': manifest['files'], 'remote_hashes_verified': True,
              'exit_code': result.returncode, 'stdout': result.stdout, 'stderr': result.stderr,
              'model': adb('shell', 'getprop', 'ro.product.model').stdout.strip()}
    path = ROOT / 'local/android-logs' / ('startup-' + stamp + '.json')
    path.parent.mkdir(exist_ok=True)
    path.write_text(json.dumps(report, indent=2) + '\n')
    print(f'Private MM3D startup: {"PASS" if result.returncode == 0 else "FAIL"} ({result.returncode})')
    print(f'Private report: {path}')
    print(result.stdout + result.stderr)
    return int(result.returncode != 0)


if __name__ == '__main__':
    raise SystemExit(main())
