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
# Every page shows the build it comes from (version, commit, UTC time; build.json beside them): BUILD_COMMIT and
# BUILD_DATE when the build system gives them, else git's commit (-dirty for local changes) and now.
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
COMMIT=${BUILD_COMMIT:-$(git rev-parse --short=7 HEAD 2>/dev/null || echo unknown)}
[ -n "${BUILD_COMMIT:-}" ] || git diff --quiet HEAD 2>/dev/null || COMMIT="$COMMIT-dirty"
DATE=${BUILD_DATE:-$(date -u +"%Y-%m-%d %H:%M UTC")}
python3 - "$OUT" "$VER" "$COMMIT" "$DATE" <<'PY'
import html, json, sys
from pathlib import Path
out, ver, commit, date = Path(sys.argv[1]), *sys.argv[2:]
(out / "build.json").write_text(json.dumps(dict(name="typhoon_fm1", version=ver, commit=commit, date=date)) + "\n")
stamp = html.escape(f"typhoon_fm1 {ver} · {commit} · {date}")
badge = (f'<div id="build-stamp" title="the build this page comes from" style="position:fixed;right:8px;bottom:6px;'
         'z-index:9999;font:11px/1.4 ui-monospace,Menlo,Consolas,monospace;padding:2px 7px;border-radius:6px;'
         f'background:rgba(127,127,127,.2);color:inherit;opacity:.8;pointer-events:none">{stamp}</div>\n')
for page in ("index.html", "install/index.html", "editor/index.html", "sim/index.html"):
    f = out / page
    t = f.read_text()
    i = t.rfind("</body>")
    f.write_text(t[:i] + badge + t[i:] if i >= 0 else t + badge)
PY
echo "site: $OUT: install/, editor/, sim/ ($COMMIT, $DATE)"
