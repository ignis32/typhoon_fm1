#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 ignis32
# engine_flags.sh [PROFILE | a,b,c]: the -D flags that switch a build's engines on (engines/engines.h).
# A profile is profiles/<PROFILE>.txt (one engine per line, # comments); a comma list names engines directly.
# A line "include NAME" takes profile NAME's lines there. (Bluetooth is app/fm1.toml's, not a profile's.)
# Used by ./build.sh (the device: build/engines/typhoon_engines.h) and the simulator (web/sim/build.sh's SIM_CFLAGS),
# so they agree.
#   engine_flags.sh --list PROFILE    the profile's lines, includes expanded
set -eu
cd "$(dirname "$0")/.."
expand() {   # expand NAME DEPTH: profile NAME's names, one a line
    [ "$2" -lt 5 ] || { echo "engine_flags: includes nested too deep at $1 (a loop?)" >&2; exit 1; }
    [ -f "profiles/$1.txt" ] || { echo "engine_flags: no profile $1 (profiles/$1.txt)" >&2; exit 1; }
    while read -r a b; do
        case $a in ''|\#*) continue ;; esac
        if [ "$a" = include ]; then expand "${b%%#*}" $(($2 + 1)); else echo "$a"; fi
    done < "profiles/$1.txt"
}
if [ "${1:-}" = --list ]; then
    expand "${2:-default}" 0
    exit 0
fi
sel=${1:-${TYPHOON_PROFILE:-default}}
case "$sel" in
    *,*|"") list=$(echo "$sel" | tr ',' '\n') ;;
    *) if [ -f "profiles/$sel.txt" ]; then list=$(expand "$sel" 0); else list=$sel; fi ;;
esac
known=$(sed -n 's/^#if TYPHOON_ENG_\([A-Z0-9_]*\)$/\1/p' firmware/src/engines/engines.h | sort -u)
out=""
for e in $list; do
    E=$(echo "$e" | tr 'a-z' 'A-Z')
    echo "$known" | grep -qx "$E" || { echo "engine_flags: unknown engine '$e' (known: $(echo $known | tr 'A-Z' 'a-z'))" >&2; exit 1; }
    out="$out -DTYPHOON_ENG_$E=1"
done
[ -n "$out" ] || { echo "engine_flags: no engine in '$sel'" >&2; exit 1; }
echo $out
