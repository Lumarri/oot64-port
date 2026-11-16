"""Stage only ROM-free native probes and their libraries for Windows ADB."""
import hashlib
import json
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
FILES = {
    'mm3d_gles_probe': 'compat/mm3d_gles_probe',
    'native_presentation_policy_tests': 'native_presentation_policy_tests',
    'triaevum_android_nri_probe': 'triaevum_android_nri_probe',
    'triaevum_runtime_layout_tests': 'abi/triaevum_runtime_layout_tests',
    'oot3d_native_direct_aot_tests': 'abi/oot3d_native_direct_aot_tests',
    'triaevum_probe_empty_title.so': 'abi/triaevum_probe_empty_title.so',
    'triaevum_input_service_tests': 'module/triaevum_input_service_tests',
    'triaevum_audio_service_tests': 'module/triaevum_audio_service_tests',
    'triaevum_filesystem_service_tests': 'module/triaevum_filesystem_service_tests',
    'triaevum_native_module_loader_tests': 'module/triaevum_native_module_loader_tests',
    'triaevum_mock_module.so': 'module/triaevum_mock_module.so',
}


def stage(build, ndk):
    config = json.loads((ROOT / 'config/android-base.json').read_text())
    directory = ROOT / 'local/android-native-stage'
    directory.mkdir(parents=True, exist_ok=True)
    manifest_path = directory / 'manifest.json'
    # An interrupted restaging must never leave a successful old manifest.
    manifest_path.unlink(missing_ok=True)
    prebuilt = ndk / 'toolchains/llvm/prebuilt/linux-x86_64'
    sources = {name: build / relative for name, relative in FILES.items()}
    sources['libc++_shared.so'] = prebuilt / 'sysroot/usr/lib/aarch64-linux-android/libc++_shared.so'
    manifest = {'base': config, 'scope': 'native_probes_without_game', 'files': {}}
    for name, source in sources.items():
        header = source.read_bytes()[:20]
        if header[:5] != b'\x7fELF\x02' or header[18:20] != b'\xb7\x00':
            raise ValueError(f'Not an ELF64 AArch64 binary: {source}')
        program_headers = subprocess.check_output(
            [str(prebuilt / 'bin/llvm-readelf'), '--program-headers', '--wide', str(source)], text=True)
        alignments = [int(line.split()[-1], 16) for line in program_headers.splitlines()
                      if line.strip().startswith('LOAD ')]
        if not alignments or min(alignments) < 0x4000:
            raise ValueError(f'ELF LOAD alignment below 16 KiB: {source}')
        destination = directory / name
        shutil.copy2(source, destination)
        manifest['files'][name] = {'sha256': hashlib.sha256(destination.read_bytes()).hexdigest(),
                                  'bytes': destination.stat().st_size,
                                  'load_alignments': alignments}
    manifest_path.write_text(json.dumps(manifest, indent=2) + '\n')
    print(f'Staged {len(sources)} ARM64 files: {directory}')


if __name__ == '__main__':
    stage(Path(sys.argv[1]), Path(sys.argv[2]))
