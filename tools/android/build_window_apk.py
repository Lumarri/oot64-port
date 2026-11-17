"""Build/sign the ROM-free NativeActivity APK with the installed Android SDK.

Windows packaging; ARM64 compilation uses the existing WSL/NDK build. No Gradle,
Java app code, ROM, network download or private title module is required.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import zipfile
from datetime import datetime, timezone

ROOT = Path(__file__).resolve().parents[2]
BUILD_TOOLS = '36.0.0'
PLATFORM = 'android-34'


def run(arguments, **kwargs):
    subprocess.run([str(arg) for arg in arguments], check=True, cwd=ROOT, **kwargs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--skip-native', action='store_true', help='Package already staged, hash-verified libraries')
    parser.add_argument('--sdk', type=Path, default=os.environ.get('ANDROID_HOME') or os.environ.get('ANDROID_SDK_ROOT'))
    parser.add_argument('--jdk', type=Path, default=os.environ.get('JAVA_HOME'))
    args = parser.parse_args()
    if not args.sdk or not args.jdk:
        parser.error('Set ANDROID_HOME/JAVA_HOME or pass --sdk/--jdk')
    sdk, jdk = args.sdk, args.jdk
    build_tools = sdk / 'build-tools' / BUILD_TOOLS
    android_jar = sdk / 'platforms' / PLATFORM / 'android.jar'
    java, keytool = jdk / 'bin/java.exe', jdk / 'bin/keytool.exe'
    for tool in (android_jar, java, keytool, build_tools / 'aapt2.exe',
                 build_tools / 'zipalign.exe', build_tools / 'lib/apksigner.jar'):
        if not tool.is_file():
            raise FileNotFoundError(tool)
    if not args.skip_native:
        run(['wsl', '-d', 'Ubuntu', '--', 'env', 'MM3D_BUILD_WINDOW_APP=ON', 'bash', 'tools/android/build_native.sh'])
    stage = ROOT / 'local/android-window-native-stage'
    native = json.loads((stage / 'manifest.json').read_text())
    config = json.loads((ROOT / 'config/android-base.json').read_text())
    if native['base'] != config or set(native['files']) != {'libmm3d_window.so', 'libc++_shared.so'}:
        raise ValueError('Native window staging does not match the pinned base/libraries')
    for name, info in native['files'].items():
        if hashlib.sha256((stage / name).read_bytes()).hexdigest() != info['sha256']:
            raise ValueError(f'Native library changed: {name}')
        if min(info['load_alignments']) < 16384:
            raise ValueError(f'Native library is not 16 KiB aligned: {name}')
    work = ROOT / 'local/android-window-build' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    work.mkdir(parents=True)
    resources, unsigned, aligned = (work / name for name in ('resources.apk', 'unsigned.apk', 'aligned.apk'))
    run([build_tools / 'aapt2.exe', 'link', '-I', android_jar, '--manifest',
         ROOT / 'ports/android/window_app/AndroidManifest.xml', '--min-sdk-version', '29',
         '--target-sdk-version', '34', '--version-code', '1', '--version-name', '0.1-window-prototype',
         '-o', resources])
    with zipfile.ZipFile(resources) as resource_zip, zipfile.ZipFile(unsigned, 'w', compression=zipfile.ZIP_STORED) as apk:
        for entry in resource_zip.infolist():
            apk.writestr(entry.filename, resource_zip.read(entry))
        for name in native['files']:
            apk.write(stage / name, 'lib/arm64-v8a/' + name)
        for license_file in (ROOT / 'ports/android/window_app/licenses').iterdir():
            if license_file.is_file():
                apk.write(license_file, 'assets/licenses/' + license_file.name)
    run([build_tools / 'zipalign.exe', '-f', '-P', '16', '4', unsigned, aligned])
    signing = ROOT / 'local/android-app-signing'
    signing.mkdir(exist_ok=True)
    key = signing / 'window-debug.jks'
    if not key.exists():
        run([keytool, '-genkeypair', '-keystore', key, '-storetype', 'PKCS12', '-alias', 'androiddebugkey',
             '-storepass', 'android', '-keypass', 'android', '-keyalg', 'RSA', '-keysize', '2048',
             '-validity', '10000', '-dname', 'CN=Android Debug,O=Android,C=US'])
    output = ROOT / 'local/android-apk'
    output.mkdir(exist_ok=True)
    artifact = output / 'mm3d-window-debug.apk'
    signer = [java, '-jar', build_tools / 'lib/apksigner.jar']
    run(signer + ['sign', '--ks', key, '--ks-key-alias', 'androiddebugkey', '--ks-pass', 'pass:android',
                  '--key-pass', 'pass:android', '--v1-signing-enabled', 'false', '--out', artifact, aligned])
    run(signer + ['verify', '--verbose', artifact])
    run([build_tools / 'zipalign.exe', '-c', '-P', '16', '4', artifact])
    with zipfile.ZipFile(artifact) as apk:
        allowed = {'AndroidManifest.xml', 'resources.arsc', *(f'lib/arm64-v8a/{name}' for name in native['files'])}
        allowed.update('assets/licenses/' + file.name for file in (ROOT / 'ports/android/window_app/licenses').iterdir() if file.is_file())
        if set(apk.namelist()) != allowed:
            raise ValueError('APK contains unexpected entries')
        for name, info in native['files'].items():
            if hashlib.sha256(apk.read('lib/arm64-v8a/' + name)).hexdigest() != info['sha256']:
                raise ValueError(f'Packaged library changed: {name}')
    manifest = {'package': 'org.lumarri.mm3d.prototype', 'activity': 'android.app.NativeActivity',
                'scope': 'visible_synthetic_prototype_not_gameplay', 'apk_sha256': hashlib.sha256(artifact.read_bytes()).hexdigest(),
                'bytes': artifact.stat().st_size, 'sdk_build_tools': BUILD_TOOLS, 'compile_platform': PLATFORM,
                'native': native, 'signed': 'local_debug_key_not_for_release'}
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(f'Development APK: {artifact}')
    print('Scope: animated synthetic TEV texture; no game boot or title assets.')


if __name__ == '__main__':
    main()
