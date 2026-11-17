#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
work="${MM3D_ANDROID_WORK:-$HOME/mm3d-android}"
source_dir="$work/source/TriAevum"
commit="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["commit"])' "$root/config/android-base.json")"
if [[ "$(git -C "$source_dir" rev-parse HEAD)" != "$commit" ]] ||
   [[ -n "$(git -C "$source_dir" status --porcelain --untracked-files=no)" ]]; then
    echo 'Host probe requires the clean pinned TriAevum checkout.' >&2
    exit 1
fi
build_dir="$work/build/gles-host-${commit:0:7}"
mkdir -p "$root/local/android-logs"
cmake -S "$root/ports/android/compat_probe" -B "$build_dir" -G Ninja \
    -DMM3D_HOST_GLES_PROBE=ON -DCMAKE_BUILD_TYPE=Release \
    "-DTRIAEVUM_SOURCE_DIR=$source_dir"
cmake --build "$build_dir" --parallel 2
# This deliberately uses software rendering: it cannot qualify Mali hardware.
ctest --test-dir "$build_dir" --verbose \
    2>&1 | tee "$root/local/android-logs/gles-host.log"
