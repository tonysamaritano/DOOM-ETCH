#!/bin/sh
# Run the existing engine -> G-code -> physical trace chain for one E1M1 frame.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
bin="$repo/build/test"
wad="$repo/test-data/doom1.wad"
output="$repo/build/viewer"
for program in etch-export etch-gcode-run; do
    if [ ! -x "$bin/$program" ]; then
        printf 'Build first: cmake --preset test && cmake --build --preset test\n' >&2
        exit 1
    fi
done
if [ ! -f "$wad" ]; then
    printf 'Place your shareware doom1.wad in %s/test-data/\n' "$repo" >&2
    exit 1
fi
mkdir -p "$output"
"$bin/etch-export" "$wad" "$output/e1m1"
"$bin/etch-gcode-run" "$output/e1m1.gcode" --trace "$output/e1m1.json"
printf '\nViewer: http://127.0.0.1:8770/viewer/index.html\n'
