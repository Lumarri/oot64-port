"""Run staged ROM-free probes over Windows ADB; retain private JSON results.

Uses the single USB phone by default, or --serial. Does not install an APK.
"""
import argparse
import hashlib
import json
import shlex
import subprocess
import time
from datetime import datetime, timezone
from pathlib import Path

from check_device import collect

ROOT = Path(__file__).resolve().parents[2]
CASES = [
    ('presentation_policy', 'native_presentation_policy_tests', []),
    ('runtime_layout', 'triaevum_runtime_layout_tests', []),
    ('input_service', 'triaevum_input_service_tests', []),
    ('audio_service', 'triaevum_audio_service_tests', []),
    ('filesystem_service', 'triaevum_filesystem_service_tests', []),
    ('native_module_loader', 'triaevum_native_module_loader_tests', ['triaevum_mock_module.so']),
    ('empty_title_rejection', 'oot3d_native_direct_aot_tests', ['triaevum_probe_empty_title.so', '--expect-empty']),
    ('nri_device', 'triaevum_android_nri_probe', []),
    ('gles32_draw_and_compute', 'mm3d_gles_probe', []),
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--serial', help='ADB device selector (kept out of the report)')
    parser.add_argument('--case', action='append', choices=[case[0] for case in CASES],
                        help='Run selected probes only; exit status reflects these probes')
    args = parser.parse_args()
    adb = ['adb', '-s', args.serial] if args.serial else ['adb', '-d']

    def run(*arguments, check=True, timeout=90):
        return subprocess.run(adb + list(arguments), check=check, capture_output=True,
                              text=True, encoding='utf-8', errors='replace', timeout=timeout)

    stage = ROOT / 'local/android-native-stage'
    manifest = json.loads((stage / 'manifest.json').read_text())
    config = json.loads((ROOT / 'config/android-base.json').read_text())
    if manifest['base'] != config:
        raise ValueError('The staged binaries do not match the pinned base')
    for name, info in manifest['files'].items():
        if Path(name).name != name:
            raise ValueError('Invalid staging filename')
        if hashlib.sha256((stage / name).read_bytes()).hexdigest() != info['sha256']:
            raise ValueError(f'Staged binary changed: {name}')
    run('get-state')
    device = {key: run('shell', 'getprop', prop).stdout.strip() for key, prop in (
        ('model', 'ro.product.model'), ('android', 'ro.build.version.release'),
        ('api', 'ro.build.version.sdk'), ('abi', 'ro.product.cpu.abilist'))}
    if 'arm64-v8a' not in device['abi'].split(','):
        raise ValueError('The device does not advertise arm64-v8a')
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    remote = f'/data/local/tmp/mm3d-native-{config["commit"][:7]}-{stamp}'
    run('shell', 'mkdir', '-p', remote)
    for name in manifest['files']:
        run('push', str(stage / name), remote + '/' + name)
        run('shell', 'chmod', '700', remote + '/' + name)
    report = {'utc': stamp, 'base_commit': config['commit'], 'device': device,
              'renderer_preflight': collect(adb),
              'scope': 'shell_uid_probes_not_apk_or_gameplay', 'tests': []}
    report_path = ROOT / 'local/android-logs' / f'device-{stamp}.json'
    report_path.parent.mkdir(parents=True, exist_ok=True)
    for label, executable, extra in CASES:
        if args.case and label not in args.case:
            continue
        command = f'cd {shlex.quote(remote)} && TMPDIR={shlex.quote(remote)} LD_LIBRARY_PATH={shlex.quote(remote)} '
        command += ' '.join(shlex.quote('./' + item if not item.startswith('--') else item)
                            for item in [executable, *extra])
        started = time.monotonic()
        # Android timeout bounds the remote process even if the host stops waiting.
        bounded = 'timeout 60 sh -c ' + shlex.quote(command)
        result = run('shell', bounded, check=False)
        record = {'name': label, 'exit_code': result.returncode,
                  'seconds': round(time.monotonic() - started, 3),
                  'stdout': result.stdout, 'stderr': result.stderr}
        report['tests'].append(record)
        report_path.write_text(json.dumps(report, indent=2) + '\n')
        print(f'{label}: {"PASS" if result.returncode == 0 else "FAIL"} ({result.returncode})', flush=True)
        if result.returncode:
            print(result.stdout + result.stderr, flush=True)
    print(f'Private report: {report_path}')
    return int(any(item['exit_code'] for item in report['tests'])
               or (not args.case and not report['renderer_preflight']['known_requirements_met']))


if __name__ == '__main__':
    raise SystemExit(main())
