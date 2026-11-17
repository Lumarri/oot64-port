"""Stage PRIVATE code/layout and an ARM64 probe for real MM3D startup.

Uses the upstream ExHeader layout builder. RomFS is read only for its service
window metadata; it is neither staged nor sent to Android by this route.
"""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    build, ndk, private = map(Path, sys.argv[1:])
    config = json.loads((ROOT / 'config/android-base.json').read_text())
    title = json.loads((ROOT / 'local/android-title-stage/manifest.json').read_text())
    if title['base'] != config or title.get('private_game_derived') is not True:
        raise ValueError('Build and stage the pinned private title first')
    upstream = private.parent.parent / 'source/TriAevum'
    commit = subprocess.check_output(['git', '-C', str(upstream), 'rev-parse', 'HEAD'], text=True).strip()
    dirty = subprocess.check_output(['git', '-C', str(upstream), 'status', '--porcelain', '--untracked-files=no'], text=True)
    if commit != config['commit'] or dirty:
        raise ValueError('The startup layout builder must be the clean pinned upstream')
    contract = json.loads((upstream / 'titles/mm3d/a32/whole_aot_product_manifest.json').read_text())
    inputs = private / 'inputs'
    for role, name in (('code', 'code.bin'), ('exheader', 'exheader.bin')):
        expected = contract['source_image'][role]
        if (inputs / name).stat().st_size != expected['bytes'] or sha(inputs / name) != expected['sha256']:
            raise ValueError(f'Private {name} differs from the verified recipe')
    sys.path.insert(0, str(upstream))
    from tools.oot3d.native_a32_runtime.build_process_manifest import build_manifest
    builder = upstream / 'tools/oot3d/native_a32_runtime/build_process_manifest.py'
    layout = build_manifest(inputs / 'exheader.bin', inputs / 'code.bin',
                            ROOT / 'extracted/mm3d-eur-rev1/romfs.bin')
    if layout['process']['entrypoint'] != title['entrypoint']:
        raise ValueError('Process layout and private title have different entrypoints')
    # Portable relative references; this probe does not mount/open RomFS.
    for field, name in (('exheader_path', 'exheader.bin'), ('code_bin_path', 'code.bin'),
                         ('romfs_image_path', 'romfs.bin')):
        layout['source'][field] = name
    output = ROOT / 'local/android-startup-stage'
    output.mkdir(exist_ok=True)
    (output / 'manifest.json').unlink(missing_ok=True)
    (output / 'process-manifest.json').write_text(json.dumps(layout, indent=2, sort_keys=True) + '\n')
    shutil.copy2(inputs / 'code.bin', output / 'code.bin')
    source = build / 'mm3d_startup_probe'
    prebuilt = ndk / 'toolchains/llvm/prebuilt/linux-x86_64/bin'
    header = source.read_bytes()[:20]
    if header[:5] != b'\x7fELF\x02' or header[18:20] != b'\xb7\x00':
        raise ValueError('Startup probe is not ELF64 AArch64')
    headers = subprocess.check_output([str(prebuilt / 'llvm-readelf'), '--program-headers', '--wide', str(source)], text=True)
    alignments = [int(line.split()[-1], 16) for line in headers.splitlines() if line.strip().startswith('LOAD ')]
    if not alignments or min(alignments) < 16384:
        raise ValueError('Startup probe LOAD alignment is below 16 KiB')
    shutil.copy2(source, output / source.name)
    manifest = {'base': config, 'private_game_derived': True,
                'scope': 'real_title_entry_to_first_svc_not_game_boot',
                'source_manifest_sha256': title['source_manifest_sha256'],
                'layout_builder_sha256': sha(builder), 'files': {}}
    for name in ('mm3d_startup_probe', 'code.bin', 'process-manifest.json'):
        path = output / name
        manifest['files'][name] = {'stage': 'startup', 'sha256': sha(path), 'bytes': path.stat().st_size}
    manifest['files']['mm3d_startup_probe']['load_alignments'] = alignments
    for name in ('libtriaevum_title_aot.so', 'libc++_shared.so'):
        manifest['files'][name] = {'stage': 'title', **title['files'][name]}
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(f'PRIVATE startup staged: {output}; CPU only, no CTR services or game boot')


if __name__ == '__main__':
    main()
