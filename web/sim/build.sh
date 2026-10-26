#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 ignis32
# The simulator: web/sim/typhoon_sim.c (the app on fm1-base's host platform) to WebAssembly, wasm32-wasi, a reactor
# (no main: JavaScript drives it). Needs the device build's generated headers (app/build/gen: the fonts, the screen
# reader's words link): ./build.sh first.
#   web/sim/build.sh [OUT]   OUT/typhoon.wasm (default build/sim)
set -eu
cd "$(dirname "$0")/../.."
OUT=${1:-build/sim}
ZIG=${ZIG:-zig}
GEN=app/build/gen
[ -f "$GEN/felucca_font.h" ] || { echo "sim: no $GEN/felucca_font.h (the device build makes it: ./build.sh)"; exit 1; }
sh tools/patches.sh apply   # (patches/: typhoon's changes to fm1-base and felucca-bsp, the device build's too)
mkdir -p "$OUT"
# the exports: every EXPORT_AS(name) in the glue (zig cc for wasm32-wasi keeps only what the link is told to export)
EXP=$(sed -n '/^#define/!s/.*EXPORT_AS(\([a-z0-9_]*\)).*/-Wl,--export=\1/p' web/sim/typhoon_sim.c | tr '\n' ' ')
$ZIG cc --target=wasm32-wasi -mexec-model=reactor -O2 -ffp-contract=off -std=gnu99 \
    -DPLAT_HOST -DPLAT_WEB -DFM1B_BLE=1 -DFM1B_BLE_CENTRAL=1 \
    -Ibase/web/emu -Ibase/host -Ibase/device -Ibase/include -Ibase/lib -Iapp -I"$GEN" -Ifirmware/src -Ifirmware/gen \
    -Wall -Wno-unused-function -Wno-unused-parameter -Wno-unused-variable -Wno-unused-but-set-variable ${SIM_CFLAGS:-} \
    $EXP -Wl,-z,stack-size=262144 \
    -o "$OUT/typhoon.wasm" web/sim/typhoon_sim.c -lm
echo "sim: $OUT ($(wc -c < "$OUT/typhoon.wasm" | tr -d ' ') B wasm)"
