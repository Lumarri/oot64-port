#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
work="${MM3D_ANDROID_WORK:-$HOME/mm3d-android}"
source_dir="$work/source/TriAevum"
mapfile -t locked < <(python3 - "$root/config/android-base.json" <<'PY'
import json, sys
config = json.load(open(sys.argv[1]))
for key in ('commit', 'ndk_directory', 'ndk_revision'):
    print(config[key])
PY
)
commit="${locked[0]}"
if [[ "$(git -C "$source_dir" rev-parse HEAD)" != "$commit" ]] ||
   [[ -n "$(git -C "$source_dir" status --porcelain --untracked-files=no)" ]]; then
    echo 'Title builds require the clean pinned TriAevum checkout.' >&2
    exit 1
fi
private="$work/private/mm3d-eur-${commit:0:7}"
ndk="${ANDROID_NDK_HOME:-$work/tools/${locked[1]}}"
grep -Fq "Pkg.Revision = ${locked[2]}" "$ndk/source.properties"
build="$work/build/title-mm3d-${commit:0:7}"
translated=""
targets=(mm3d_title_probe mm3d_startup_probe)
if [[ "${1:-}" != '--probe-only' ]]; then
    python3 -B "$root/tools/android/prepare_title.py" --phase sources
    translated="$private/translated"
    targets+=(triaevum_android_title)
fi
mkdir -p "$root/local/android-logs"
cmake -S "$root/ports/android/title_module" -B "$build" -G Ninja \
    "-DCMAKE_TOOLCHAIN_FILE=$ndk/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-29 -DANDROID_STL=c++_shared \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_SCAN_FOR_MODULES=OFF \
    "-DTRIAEVUM_SOURCE_DIR=$source_dir" -DTRIAEVUM_ANDROID_TITLE=mm3d \
    -DTRIAEVUM_ANDROID_BUILD_NRI=OFF -DTRIAEVUM_ANDROID_TITLE_OPTIMIZATION=1 -DTRIAEVUM_ANDROID_TITLE_DEBUG_INFO=OFF \
    "-DTRIAEVUM_TRANSLATED_TITLE_DIR=$translated" \
    "-DFETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON=$work/build/native-${commit:0:7}/_deps/nlohmann_json-src"
cmake --build "$build" --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-2}" --target "${targets[@]}" \
    2>&1 | tee "$root/local/android-logs/title-build.log"
if [[ -n "$translated" ]]; then
    python3 -B "$root/tools/android/stage_title.py" "$build" "$ndk" "$private"
    python3 -B "$root/tools/android/stage_startup.py" "$build" "$ndk" "$private"
fi
