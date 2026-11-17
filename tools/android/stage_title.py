"""Stage the PRIVATE game-derived ARM64 module and a ROM-free loader probe.

Output is under local/ and must never be published with the repository/APK demo.
"""
import hashlib
import json
import re
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]


def main():
    build, ndk, private = map(Path, sys.argv[1:])
    config = json.loads((ROOT / 'config/android-base.json').read_text())
    preparation = json.loads((private / 'preparation.json').read_text())
    if preparation['upstream_commit'] != config['commit'] or preparation['recipe'] != config['rom_recipe']:
        raise ValueError('Translated sources belong to a different base/ROM recipe')
    source_manifest = private / 'translated/TITLE_SOURCE_MANIFEST.json'
    if hashlib.sha256(source_manifest.read_bytes()).hexdigest() != preparation['source_manifest_sha256']:
        raise ValueError('Translated source manifest changed after preparation')
    translated = json.loads(source_manifest.read_text())
    for name, expected in translated['files'].items():
        if not re.fullmatch(r'[A-Za-z0-9_]+\.(cpp|h)', name):
            raise ValueError('Invalid translated source name')
        if hashlib.sha256((source_manifest.parent / name).read_bytes()).hexdigest() != expected:
            raise ValueError(f'Translated source changed during compilation: {name}')
    output = ROOT / 'local/android-title-stage'
    output.mkdir(exist_ok=True)
    (output / 'manifest.json').unlink(missing_ok=True)
    prebuilt = ndk / 'toolchains/llvm/prebuilt/linux-x86_64'
    sources = {'libtriaevum_title_aot.so': build / 'upstream-native/libtriaevum_title_aot.so',
               'mm3d_title_probe': build / 'mm3d_title_probe',
               'libc++_shared.so': prebuilt / 'sysroot/usr/lib/aarch64-linux-android/libc++_shared.so'}
    contract = json.loads((private.parent.parent / 'source/TriAevum/titles/mm3d/a32/whole_aot_product_manifest.json').read_text())
    manifest = {'base': config, 'private_game_derived': True, 'scope': 'title_abi_validation_not_game_boot',
                'source_manifest_sha256': preparation['source_manifest_sha256'],
                'functions': preparation['functions'],
                'expected_entry_count': contract['closure']['counts']['dispatcher_entries'],
                'entrypoint': contract['source_image']['entrypoint'], 'files': {}}
    manifest['compiler'] = subprocess.check_output([str(prebuilt / 'bin/clang++'), '--version'], text=True).splitlines()[0]
    manifest['title_flags'] = '-O1 -ffp-model=strict -g0; arm64-v8a; android-29; 16 KiB LOAD'
    for name, source in sources.items():
        header = source.read_bytes()[:20]
        if header[:5] != b'\x7fELF\x02' or header[18:20] != b'\xb7\x00':
            raise ValueError(f'Not an ELF64 AArch64 file: {source}')
        headers = subprocess.check_output([str(prebuilt / 'bin/llvm-readelf'), '--program-headers', '--wide', str(source)], text=True)
        alignments = [int(line.split()[-1], 16) for line in headers.splitlines() if line.strip().startswith('LOAD ')]
        if not alignments or min(alignments) < 16384:
            raise ValueError(f'LOAD alignment below 16 KiB: {source}')
        destination = output / name
        shutil.copy2(source, destination)
        manifest['files'][name] = {'sha256': hashlib.sha256(destination.read_bytes()).hexdigest(),
                                   'bytes': destination.stat().st_size, 'load_alignments': alignments}
    exports = subprocess.check_output([str(prebuilt / 'bin/llvm-nm'), '-D', '--defined-only',
                                      str(output / 'libtriaevum_title_aot.so')], text=True)
    if not all(name in exports.split() for name in ('triaevum_title_aot_query', 'triaevum_title_whole_aot_query')):
        raise ValueError('Private title module lacks required exported ABI queries')
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(f'PRIVATE title staged: {output}; not a boot qualification or public artifact')


if __name__ == '__main__':
    main()
