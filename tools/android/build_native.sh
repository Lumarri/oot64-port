#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
work="${MM3D_ANDROID_WORK:-$HOME/mm3d-android}"
source_dir="$work/source/TriAevum"
mapfile -t locked < <(python3 - "$root/config/android-base.json" <<'PY'
import json, sys
c = json.load(open(sys.argv[1]))
for key in ('repository', 'commit', 'title', 'ndk_directory', 'ndk_revision'):
    print(c[key])
PY
)
for tool in git cmake ninja python3; do command -v "$tool" >/dev/null; done
export ANDROID_NDK_HOME="${ANDROID_NDK_HOME:-$work/tools/${locked[3]}}"
grep -Fq "Pkg.Revision = ${locked[4]}" "$ANDROID_NDK_HOME/source.properties"
test -f "$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake"
if [[ ! -d "$source_dir" ]]; then
    mkdir -p "$work/source"
    # Reuse the downloaded Git objects, but create an independent Linux checkout.
    if [[ -d "$root/local/TriAevum/.git" ]]; then
        git clone --no-hardlinks --no-checkout "$root/local/TriAevum" "$source_dir"
        git -C "$source_dir" remote set-url origin "${locked[0]}"
    else
        git init "$source_dir"
        git -C "$source_dir" remote add origin "${locked[0]}"
    fi
    git -C "$source_dir" config core.autocrlf false
    if ! git -C "$source_dir" cat-file -e "${locked[1]}^{commit}" 2>/dev/null; then
        git -C "$source_dir" fetch --depth 1 origin "${locked[1]}"
    fi
    git -C "$source_dir" checkout --detach "${locked[1]}"
fi
if [[ "$(git -C "$source_dir" rev-parse HEAD)" != "${locked[1]}" ]]; then
    echo 'The local TriAevum checkout does not match config/android-base.json.' >&2
    exit 1
fi
if [[ -n "$(git -C "$source_dir" status --porcelain --untracked-files=no)" ]]; then
    echo 'The pinned TriAevum checkout has tracked changes; preserve/review them first.' >&2
    exit 1
fi
export TRIAEVUM_BUILD_DIR="$work/build/native-${locked[1]:0:7}"
# WSL currently has ~4 GiB RAM; keep compiler memory bounded.
export CMAKE_BUILD_PARALLEL_LEVEL="${CMAKE_BUILD_PARALLEL_LEVEL:-2}"
export TRIAEVUM_BUILD_TITLE=0
export TRIAEVUM_TRANSLATED_TITLE_DIR=""
mkdir -p "$root/local/android-logs"
cmake -S "$root/ports/android/compat_probe" -B "$TRIAEVUM_BUILD_DIR/compat" -G Ninja \
    "-DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-29 \
    "-DTRIAEVUM_SOURCE_DIR=$source_dir" \
    -DANDROID_STL=c++_shared -DCMAKE_BUILD_TYPE=Release
cmake --build "$TRIAEVUM_BUILD_DIR/compat" --parallel "$CMAKE_BUILD_PARALLEL_LEVEL"
bash "$source_dir/scripts/build-android-native.sh" \
    "-DTRIAEVUM_ANDROID_TITLE=${locked[2]}" \
    2>&1 | tee "$root/local/android-logs/native-build.log"
python3 "$root/tools/android/stage_native.py" "$TRIAEVUM_BUILD_DIR" "$ANDROID_NDK_HOME"
