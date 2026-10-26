#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 ignis32
# Build typhoon: fm1-base's build (base/tools/build.py, base/ a git submodule) on app/ (its settings: app/fm1.toml).
#   ./build.sh [--engines PROFILE | a,b,c] [--out DIR] [--release X.Y] [--host | --web]
#   --engines  the engines (profiles/PROFILE.txt or a list; default $TYPHOON_PROFILE, else "default"), through
#              tools/engine_flags.sh into build/engines/typhoon_engines.h (app/app.h takes it)
#   --out DIR  the package (DIR/typhoon_fm1.fwsc, a release's under its own name), the image, ELF, listing and DIR/report.json
#              (what the CI and the installer read: sizes, identity, engines, the checks); default build/
#   --release  refused while a file in the image has a GPL SPDX line (with Bluetooth, none may: LICENSING.md)
#   JIELI_TOOLCHAIN  JieLi Linux toolchain (default: ~/.jieli/toolchain)
#   AC79_SDK         JieLi AC79 SDK checkout (default: ~/fw-AC79_AIoT_SDK)
#   PYTHON           a Python with Pillow
# The screen keeps typhoon's Terminus faces (X0X_FONTSET=terminus: base/tools/gen_font.py).
set -e
cd "$(dirname "$0")"
export JIELI_TOOLCHAIN="${JIELI_TOOLCHAIN:-$HOME/.jieli/toolchain}"
export AC79_SDK="${AC79_SDK:-$HOME/fw-AC79_AIoT_SDK}"
export X0X_FONTSET="${X0X_FONTSET:-terminus}"
PY="${PYTHON:-python3}"
[ -f base/bsp/chip/usb.c ] || git submodule update --init --recursive   # (fm1-base, felucca-bsp: a fresh checkout has none)
[ -f base/tools/build.py ] || { echo "build.sh: no fm1-base in base/ (git submodule update --init --recursive)"; exit 1; }
"$PY" -c 'import PIL' 2>/dev/null || { echo "build.sh: $PY has no Pillow (pip3 install Pillow)"; exit 1; }
sh tools/patches.sh apply                  # typhoon's changes to fm1-base and felucca-bsp (patches/README.md)

SEL=${TYPHOON_PROFILE:-default}
OUTDIR=build
ARGS=""
DEV=1
REL=""
while [ $# -gt 0 ]; do
    case $1 in
        --engines|--profile) SEL=$2; shift 2 ;;
        --out) OUTDIR=$2; shift 2 ;;
        --host|--web) DEV=0; ARGS="$ARGS $1"; shift ;;
        --release) REL=$2; ARGS="$ARGS $1 $2"; shift 2 ;;
        *) ARGS="$ARGS $1"; shift ;;
    esac
done

# the engines: build/engines/typhoon_engines.h (always written, so no other build's choice lingers)
FLAGS=$(sh tools/engine_flags.sh "$SEL")
mkdir -p build/engines
{
    echo "/* ./build.sh --engines $SEL (tools/engine_flags.sh) */"
    for f in $FLAGS; do
        case $f in -DTYPHOON_ENG_*=1) n=${f#-D}; echo "#define ${n%=1} 1" ;; esac
    done
} > build/engines/typhoon_engines.h

[ $DEV = 1 ] || exec "$PY" base/tools/build.py app $ARGS
# a release links JieLi's binary Bluetooth libraries: no GPL file may be in the image (LICENSING.md)
if [ -n "$REL" ]; then
    GPL=$(grep -rlE --include='*.[chS]' 'SPDX-License-Identifier: [^*]*GPL' app/*.c app/*.h firmware/src firmware/gen || true)
    [ -z "$GPL" ] || { echo "build.sh: --release refused: GPL code in the image (LICENSING.md):"; echo "$GPL" | sed 's/^/  /'; exit 1; }
fi
mkdir -p "$OUTDIR"
LOG="$OUTDIR/build.out"
{ "$PY" base/tools/build.py app $ARGS 2>&1; echo $? > "$LOG.rc"; } | tee "$LOG"
rc=$(cat "$LOG.rc")
rm -f "$LOG.rc"
[ "$rc" = 0 ] || exit "$rc"
"$PY" - "$OUTDIR" "$SEL" "$FLAGS" "$REL" <<'EOF'
# the build's outputs into --out, and its report (the old tools/build.py's report.json, the same fields)
import json, re, shutil, sys
from pathlib import Path
out, sel, flags, rel = Path(sys.argv[1]), sys.argv[2], sys.argv[3].split(), sys.argv[4]
log = (out / "build.out").read_text()
m = lambda pat: re.search(pat, log, re.M)
pkg = m(r"^package\s+(\S+)\s+(\d+) B, identity (\S+)")
img = m(r"image (\d+) B; RAM \.data\+\.bss (\d+) B of (\d+); pool (\d+) B of (\d+)")
ldr = m(r"^loader\s+\S+\s+(\d+) B")
parts = m(r"^parts\s+(.*?)\s*(?:\(not built in|$)")
slot = re.search(r"^APP_SLOT = (0x[0-9A-Fa-f]+)", Path("base/tools/fm1pkg_make.py").read_text(), re.M)
src = Path(pkg[1])
src = Path("app/build") / src.name                 # (the build ran in this tree: its path may be the container's)
name = src.name if rel else "typhoon_fm1.fwsc"     # a release keeps its name
for f in ("typhoon_fm1.bin", "typhoon_fm1.elf", "typhoon_fm1.dis"):
    if (Path("app/build") / f).exists() and out.resolve() != Path("app/build").resolve():
        shutil.copy(Path("app/build") / f, out / f)
shutil.copy(src, out / name)
report = dict(
    engines=[f[len("-DTYPHOON_ENG_"):-2].lower() for f in flags if f.startswith("-DTYPHOON_ENG_")],
    profile=sel, ble=bool(parts and "Bluetooth MIDI" in parts[1]),
    image=int(img[1]), image_max=int(slot[1], 16), ram=int(img[2]), ram_max=int(img[3]), pool=int(img[4]), pool_max=int(img[5]),
    identity=pkg[3], version=rel.upper() or "DEV",
    package=name, package_size=int(pkg[2]), loader=int(ldr[1]) if ldr else 0,
    checks=[re.sub(r"^\s*ok\s+", "", l) for l in log.splitlines() if re.match(r"^\s+ok\s", l)])
(out / "report.json").write_text(json.dumps(report, indent=1) + "\n")
print(f"report   {out / 'report.json'}: {name}, {report['identity']}, engines {' '.join(report['engines'])}")
EOF
