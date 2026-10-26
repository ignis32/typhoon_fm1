#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 ignis32
# typhoon_fm1's site (GitHub Pages: .github/workflows/pages.yml), three pages apart:
#   install/  the firmware installer (fm1-base's, for this package)
#   editor/   the preset editor (web/editor/, with web/presets/ beside it; ?sim runs it on editor/typhoon.wasm)
#   sim/      the firmware in the browser, with sound (fm1-base's emulator page: ./build.sh --web)
# and fm1-base's front page (app/site/landing.md), firmware/, words/.
#   sh tools/site.sh PACKAGE.fwsc VERSION OUT_DIR
# Needs, built first: app/build/emu (./build.sh --web) and build/sim/typhoon.wasm (web/sim/build.sh).
set -eu
cd "$(dirname "$0")/.."
[ $# -eq 3 ] || { sed -n '4,10p' "$0"; exit 2; }
PKG=$1 VER=$2 OUT=$3
[ -f build/sim/typhoon.wasm ] || { echo "site: no build/sim/typhoon.wasm (sh web/sim/build.sh)" >&2; exit 1; }
rm -rf "$OUT"
python3 base/tools/make_site.py app "$PKG" "$VER" "$OUT"
mv "$OUT/emu" "$OUT/sim"                                   # (fm1-base calls it emu/)
mkdir -p "$OUT/editor" "$OUT/presets"
cp web/editor/index.html web/editor/app.js web/editor/core.mjs web/editor/simport.mjs "$OUT/editor/"
cp web/presets/tsx.mjs "$OUT/presets/"
cp build/sim/typhoon.wasm "$OUT/editor/"
touch "$OUT/.nojekyll"
echo "site: $OUT: install/, editor/, sim/"
