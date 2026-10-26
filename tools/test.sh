#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 ignis32
# The checks (tests/sim/*.test.mjs) on the simulator (web/sim: the app on fm1-base's host platform, WebAssembly):
# after ./build.sh (its generated headers). Needs zig 0.14.1 and Node 20; without zig on the PATH it is fetched into
# build/zig (the release workflow's runner has none).
set -eu
cd "$(dirname "$0")/.."
ZIG_V=0.14.1
if ! command -v zig >/dev/null 2>&1; then
    Z=build/zig/zig-x86_64-linux-$ZIG_V
    [ -x "$Z/zig" ] || { mkdir -p build/zig && curl -fsSL "https://ziglang.org/download/$ZIG_V/zig-x86_64-linux-$ZIG_V.tar.xz" | tar -xJ -C build/zig; }
    PATH="$PWD/$Z:$PATH"
fi
export ZIG_GLOBAL_CACHE_DIR="${ZIG_GLOBAL_CACHE_DIR:-$PWD/build/zig-cache}" ZIG_LOCAL_CACHE_DIR="${ZIG_LOCAL_CACHE_DIR:-$PWD/build/zig-cache}"
sh web/sim/build.sh build/sim
node tests/sim_test.mjs
