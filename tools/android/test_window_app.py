"""Install/test the visible synthetic NativeActivity prototype on the USB phone.

Captures only our foreground activity. Saves filtered app logs and private PNGs;
tests background/resume and rotation, restoring the prior user-rotation setting.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]
PACKAGE = 'org.lumarri.mm3d.prototype'
COMPONENT = PACKAGE + '/android.app.NativeActivity'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--install', action='store_true', help='Install/update the local development APK first')
    args = parser.parse_args()
    artifact = ROOT / 'local/android-apk/mm3d-window-debug.apk'
    manifest = json.loads((artifact.parent / 'manifest.json').read_text())
    if manifest['package'] != PACKAGE or hashlib.sha256(artifact.read_bytes()).hexdigest() != manifest['apk_sha256']:
        raise ValueError('APK does not match its manifest')
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    directory = ROOT / 'local/android-logs' / ('window-' + stamp)
    directory.mkdir(parents=True)
    report = {'scope': 'visible_synthetic_not_gameplay', 'apk_sha256': manifest['apk_sha256'], 'checks': {}}

    def adb(*arguments, binary=False, check=True):
        return subprocess.run(['adb', '-d', *arguments], check=check, capture_output=True,
                              timeout=45, **({} if binary else {'text': True, 'encoding': 'utf-8', 'errors': 'replace'}))

    adb('get-state')
    if args.install:
        adb('install', '-r', str(artifact))
    device_time = float(adb('shell', 'date', '+%s').stdout.strip())

    def logs():
        text = adb('logcat', '-d', '-v', 'epoch', '-s', 'MM3DWindow:I', '*:S').stdout
        return '\n'.join(line for line in text.splitlines()
                         if re.match(r'^\s*\d+\.\d+', line) and float(line.split()[0]) >= device_time)

    def wait_for(predicate, name):
        until = time.monotonic() + 25
        while time.monotonic() < until:
            text = logs()
            if 'fatal_startup' in text or 'fatal_render' in text:
                raise RuntimeError(text)
            if predicate(text):
                report['checks'][name] = True
                print(f'{name}: PASS', flush=True)
                return
            time.sleep(0.5)
        report['checks'][name] = False
        raise TimeoutError(name)

    def screenshot(name):
        focus = adb('shell', 'dumpsys', 'window').stdout
        if not any(PACKAGE in line and 'mCurrentFocus' in line for line in focus.splitlines()):
            raise RuntimeError('Our activity is not focused; no screenshot captured')
        image = adb('exec-out', 'screencap', '-p', binary=True).stdout
        if not image.startswith(b'\x89PNG\r\n\x1a\n'):
            raise ValueError('Screenshot is not a PNG')
        (directory / name).write_bytes(image)

    rotation = adb('shell', 'cmd', 'window', 'user-rotation').stdout.strip()
    if not re.fullmatch(r'(free|lock [0-3])', rotation):
        raise ValueError('Unrecognized rotation state')
    try:
        adb('shell', 'am', 'force-stop', PACKAGE)
        adb('shell', 'cmd', 'window', 'user-rotation', 'lock', '0')
        adb('shell', 'am', 'start', '-W', '-n', COMPONENT)
        wait_for(lambda text: 'frame_presented' in text and 'frame_readback' in text, 'visible_window_swap_readback')
        screenshot('portrait.png')
        before = logs().count('frame_presented')
        adb('shell', 'input', 'keyevent', 'KEYCODE_HOME')
        wait_for(lambda text: 'activity_paused' in text and 'context_released' in text, 'background_surface_release')
        adb('shell', 'am', 'start', '-W', '-n', COMPONENT)
        wait_for(lambda text: text.count('frame_presented') > before, 'resume_context_recreation')
        before = logs().count('frame_presented')
        adb('shell', 'cmd', 'window', 'user-rotation', 'lock', '1')
        wait_for(lambda text: text.count('frame_presented') > before, 'rotation_context_recreation')
        screenshot('landscape.png')
        report['passed'] = True
    finally:
        adb('shell', 'cmd', 'window', 'user-rotation', *rotation.split())
        (directory / 'app.log').write_text(logs() + '\n', encoding='utf-8')
        report['log'] = 'app.log'
        (directory / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
        print(f'Private window report: {directory}', flush=True)


if __name__ == '__main__':
    main()
