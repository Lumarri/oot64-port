#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
work="${MM3D_ANDROID_WORK:-$HOME/mm3d-android}"
mapfile -t locked < <(python3 - "$root/config/android-base.json" <<'PY'
import json, sys
c = json.load(open(sys.argv[1]))
for key in ('ndk_archive', 'ndk_archive_sha1', 'ndk_directory', 'ndk_revision'):
    print(c[key])
PY
)
archive="$work/tools/${locked[0]}"
destination="$work/tools/${locked[2]}"
if [[ -f "$destination/source.properties" ]]; then
    grep -Fq "Pkg.Revision = ${locked[3]}" "$destination/source.properties"
    test -f "$destination/build/cmake/android.toolchain.cmake"
    printf 'NDK ready: %s\n' "$destination"
    exit 0
fi
command -v unzip >/dev/null
mkdir -p "$work/tools"
if [[ ! -f "$archive" ]] || ! printf '%s  %s\n' "${locked[1]}" "$archive" | sha1sum --check --status; then
    curl --fail --location --retry 3 --output "$archive" \
        "https://dl.google.com/android/repository/${locked[0]}"
fi
printf '%s  %s\n' "${locked[1]}" "$archive" | sha1sum --check
if [[ -e "$destination" ]]; then
    printf 'Incomplete NDK directory exists: %s. Move it aside before retrying.\n' "$destination" >&2
    exit 1
fi
unzip -q "$archive" -d "$work/tools"
grep -Fq "Pkg.Revision = ${locked[3]}" "$destination/source.properties"
printf 'NDK ready: %s\n' "$destination"
