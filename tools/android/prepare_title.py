"""Prepare PRIVATE MM3D whole-AOT sources using the pinned upstream frontend.

Run with Linux Python in WSL. ROM-derived code, IR and translated C++ stay under
the ignored WSL private workspace. This neither builds an APK nor boots MM3D.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]


def digest(path):
    result = hashlib.sha256()
    with path.open('rb') as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b''):
            result.update(chunk)
    return result.hexdigest()


def write_json(path, value):
    temporary = path.with_suffix(path.suffix + '.tmp')
    temporary.write_text(json.dumps(value, indent=2, sort_keys=True) + '\n', encoding='utf-8')
    temporary.replace(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--phase', choices=('ir', 'sources'), default='sources')
    args = parser.parse_args()
    if sys.platform != 'linux':
        parser.error('Run through WSL: wsl -d Ubuntu -- python3 tools/android/prepare_title.py')
    import fcntl
    config = json.loads((ROOT / 'config/android-base.json').read_text())
    work = Path(os.environ.get('MM3D_ANDROID_WORK', Path.home() / 'mm3d-android')).expanduser().resolve()
    upstream = work / 'source/TriAevum'
    commit = subprocess.check_output(['git', '-C', str(upstream), 'rev-parse', 'HEAD'], text=True).strip()
    dirty = subprocess.check_output(['git', '-C', str(upstream), 'status', '--porcelain', '--untracked-files=no'], text=True)
    if commit != config['commit'] or dirty:
        raise ValueError('The upstream checkout must be clean and match config/android-base.json')
    if config['title'] != 'mm3d':
        raise ValueError('This preparation supports the verified MM3D EUR recipe only')
    private = work / 'private' / ('mm3d-eur-' + commit[:7])
    private.mkdir(parents=True, exist_ok=True)
    # Refuse a location not excluded from the WSL project's Git tree.
    subprocess.run(['git', '-C', str(work), 'check-ignore', '-q', str(private)], check=True)
    preparation_lock = (private / '.prepare.lock').open('a+')
    try:
        fcntl.flock(preparation_lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError as error:
        raise RuntimeError('Another private title preparation is already running') from error
    contract_path = upstream / 'titles/mm3d/a32/whole_aot_product_manifest.json'
    contract = json.loads(contract_path.read_text())
    inputs = private / 'inputs'
    inputs.mkdir(exist_ok=True)
    for role, name in (('code', 'code.bin'), ('exheader', 'exheader.bin')):
        source = ROOT / 'extracted/mm3d-eur-rev1' / name
        expected = contract['source_image'][role]
        if source.stat().st_size != expected['bytes'] or digest(source) != expected['sha256']:
            raise ValueError(f'Local {name} does not match the pinned MM3D contract')
        destination = inputs / name
        if not destination.exists() or digest(destination) != expected['sha256']:
            shutil.copy2(source, destination)
    print('Verified MM3D code/ExHeader; private workspace:', private, flush=True)
    sys.path.insert(0, str(upstream))
    from tools.triaevum_release.whole_aot_source_backend import SourceImage, prepare_whole_aot_ir
    def image(role, name):
        expected = contract['source_image'][role]
        return SourceImage(inputs / name, expected['bytes'], expected['sha256'])
    started = time.monotonic()
    print('Extracting/reusing structural whole-AOT IR and enforcing upstream closure audit...', flush=True)
    result = prepare_whole_aot_ir(recipe=config['rom_recipe'], title='mm3d',
                                  code=image('code', 'code.bin'), exheader=image('exheader', 'exheader.bin'),
                                  cache_root=private / 'ir-cache')
    write_json(private / 'ir-result.json', result)
    print('IR audit passed; cache status:', result['status'], flush=True)
    if args.phase == 'ir':
        print('Structural IR verified:', json.dumps(result['audit'], sort_keys=True), flush=True)
        preparation_lock.close()
        return
    from tools.triaevum_release import whole_aot_plugin_backend
    generator, _, _, _ = whole_aot_plugin_backend._load_generator()
    codegen = contract['codegen']
    output = private / 'translated'
    output.mkdir(exist_ok=True)
    selection = upstream / 'titles/mm3d/a32/whole_aot_functions.json'
    print(f'Generating/reusing {codegen["shard_count"]} C++ shards ({codegen["shard_strategy"]})...', flush=True)
    generated = generator.generate(Path(result['directory']) / 'aot_program.json', selection,
                                   inputs / 'code.bin', output,
                                   shard_count=codegen['shard_count'], shard_strategy=codegen['shard_strategy'])
    files = generated['files']
    if not files or len(generated['functions']) != contract['closure']['counts']['selected_functions']:
        raise ValueError('Generated function inventory differs from the MM3D contract')
    for name, expected in files.items():
        if not re.fullmatch(r'[A-Za-z0-9_]+\.(cpp|h)', name) or digest(output / name) != expected:
            raise ValueError(f'Invalid or changed generated source: {name}')
    translated = {'format': 'triaevum_translated_title_source_v1', 'title': 'mm3d',
                  'recipe': config['rom_recipe'], 'upstream_commit': commit,
                  'private_local_generation': True, 'ir_cache_key': result['cache_key'],
                  'program_sha256': generated['program_sha256'],
                  'selection_sha256': generated['selection_sha256'],
                  'generator_sha256': generated['generator_sha256'],
                  'code_sha256': contract['source_image']['code']['sha256'], 'files': files}
    write_json(output / 'TITLE_SOURCE_MANIFEST.json', translated)
    summary = {'upstream_commit': commit, 'title': 'mm3d', 'recipe': config['rom_recipe'],
               'scope': 'private_translated_sources_not_android_module_or_game_boot',
               'translated_directory': str(output), 'files': len(files),
               'functions': len(generated['functions']), 'seconds': round(time.monotonic() - started, 3),
               'source_manifest_sha256': digest(output / 'TITLE_SOURCE_MANIFEST.json'), 'ir': result}
    write_json(private / 'preparation.json', summary)
    print(f'PRIVATE translated sources ready: {len(files)} files, {len(generated["functions"])} functions', flush=True)
    print('Source manifest:', output / 'TITLE_SOURCE_MANIFEST.json', flush=True)
    preparation_lock.close()


if __name__ == '__main__':
    main()
