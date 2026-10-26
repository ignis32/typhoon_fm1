#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 ignis32
"""typhoon's presets over USB-MIDI (firmware/src/core/sysex.c has the protocol; web/presets/tsx.mjs is the same in JS).

  tools/presets.py list                         every slot: number, name, user / factory
  tools/presets.py backup [FILE] [--slot N]     the user's presets (or slot N) into FILE
                                                (default: typhoon-presets-<date>.json here)
  tools/presets.py restore FILE [--slot N] [--to M]
                                                the presets in FILE back into their slots (or only N, or N into M)
  tools/presets.py rename N NAME                slot N's name (up to 12: A-Z 0-9 space - . /)
  tools/presets.py delete N                     slot N plays its factory preset (or INIT) again
  tools/presets.py stats [SECONDS]              the audio's CPU load once a second (average, highest), for SECONDS
                                                (default 10): play meanwhile to see what a sound costs

Slots are numbered as on the screen, 1..96 (the device says how many). Writes just write (no question on the
device); a write waits while a note sounds. The FM-1 is found by its ALSA card (Felucca / typhoon); --port /dev/snd/midiC1D0 picks one.
A backup is JSON: each preset's slot, name and its 512-byte record (hex), as the device stores it.
"""
import argparse, glob, json, os, re, select, sys, time
from pathlib import Path

VER = 1
CMD = dict(INFO=0, LIST=1, READ=2, WBEGIN=3, WDATA=4, WCOMMIT=5, DELETE=6, RENAME=7, STATS=8,
           LAYOUT=9, DESCRIBE=10, GET=11, SET=12, TEXT=13, ENGINES=14, GROUP=15, LOAD=16, SAVE=17, STATE=18)
STATUS = ["OK", "EMPTY", "BUSY", "BAD", "FAIL", "CRC"]
SLOT, NAME, CHUNK = 512, 12, 64


def check_tables():
    """In the tree: the command and status numbers above must be core/sysex.c's (a protocol change shows here)."""
    src = Path(__file__).resolve().parent.parent / "firmware" / "src" / "core" / "sysex.c"
    if not src.exists():
        return
    text = re.sub(r"/\*.*?\*/", " ", src.read_text(), flags=re.S)
    for prefix, ours in (("TSX_", list(CMD)), ("TS_", STATUS)):
        m = re.search(r"enum\s*\{\s*(" + prefix + r"\w+(?:\s*,\s*\w+)*)\s*\}", text)
        names = [n.strip()[len(prefix):] for n in m.group(1).split(",")] if m else []
        if names != ours:
            sys.exit(f"presets.py: its {prefix}* table {ours} is not core/sysex.c's {names}: update CMD / STATUS")


def pack(data):
    out, value, bits = [], 0, 0
    for b in data:
        value |= b << bits
        bits += 8
        while bits >= 7:
            out.append(value & 127)
            value >>= 7
            bits -= 7
    if bits:
        out.append(value & 127)
    return out


def unpack(seven):
    out, value, bits = [], 0, 0
    for b in seven:
        value |= b << bits
        bits += 7
        if bits >= 8:
            out.append(value & 255)
            value >>= 8
            bits -= 8
    return None if value or bits >= 7 else out


def frame(cmd, seq, data=()):
    body = [0x7D, ord("T"), ord("R"), VER, cmd, seq & 127] + pack(data)
    return bytes([0xF0] + body + [(-sum(body)) & 127, 0xF7])


def crc32(data):
    c = 0xFFFFFFFF
    for b in data:
        c ^= b
        for _ in range(8):
            c = (c >> 1) ^ 0xEDB88320 if c & 1 else c >> 1
    return ~c & 0xFFFFFFFF


def u16(v):
    return [v & 255, v >> 8 & 255]


def u32(v):
    return [v >> s & 255 for s in (0, 8, 16, 24)]


def name_of(b):
    b = bytes(b)
    return b.split(b"\0")[0].decode("ascii", "replace")


class Device:
    def __init__(self, port):
        self.port = port
        self.fd = os.open(port, os.O_RDWR | os.O_NONBLOCK)
        self.buf = b""
        self.seq = int(time.time()) & 127

    def frames(self, timeout):
        """SysEx messages that come within timeout s (other MIDI is skipped)."""
        end = time.time() + timeout
        while True:
            i = self.buf.find(b"\xF0")
            j = self.buf.find(b"\xF7", i) if i >= 0 else -1
            if i >= 0 and j >= 0:
                m, self.buf = self.buf[i:j + 1], self.buf[j + 1:]
                yield m
                continue
            left = end - time.time()
            if left <= 0:
                return
            r, _, _ = select.select([self.fd], [], [], left)
            if r:
                try:
                    self.buf += os.read(self.fd, 4096)
                except BlockingIOError:
                    pass
                if len(self.buf) > 65536:
                    self.buf = self.buf[-4096:]

    def request(self, cmd, data=(), quiet=False):
        """(status, data); a lost message is sent again (the device answers a repeat from its cache), BUSY waits."""
        self.seq = (self.seq + 1) & 127
        msg, said, t0 = frame(cmd, self.seq, data), False, time.time()
        while True:
            for _ in range(5):
                os.write(self.fd, msg)
                for m in self.frames(1.0):
                    m = list(m)
                    if len(m) < 10 or m[1:5] != [0x7D, ord("T"), ord("R"), VER] or m[5] != cmd | 0x40 or m[6] != self.seq:
                        continue
                    if sum(m[1:-1]) & 127:
                        continue
                    d = unpack(m[7:-2])
                    if d:
                        st = STATUS[d[0]] if d[0] < len(STATUS) else f"?{d[0]}"
                        break
                else:
                    continue
                break
            else:
                sys.exit(f"no answer from {self.port} (is it typhoon with the presets protocol, version {VER}?)")
            if st != "BUSY":
                return st, d[1:]
            if not said and not quiet:
                print("  waiting for silence (a note sounds on the FM-1) ...", flush=True)
                said = True
            if time.time() - t0 > 60:
                sys.exit("the FM-1 stayed busy (a note sounding) for a minute: gave up")
            time.sleep(0.3)

    def ok(self, cmd, data=(), what=""):
        st, d = self.request(cmd, data)
        if st != "OK":
            sys.exit(f"{what or 'request'}: the FM-1 says {st}")
        return d


def find_port(want):
    if want:
        return want
    cards = Path("/proc/asound/cards").read_text() if Path("/proc/asound/cards").exists() else ""
    for line in cards.splitlines():
        m = re.match(r"\s*(\d+)\s+\[", line)
        if m and re.search(r"felucca|typhoon|fm-?1", line, re.I):
            for dev in sorted(glob.glob(f"/dev/snd/midiC{m.group(1)}D*")):
                return dev
    sys.exit("no FM-1 MIDI port found (/proc/asound/cards); give one with --port /dev/snd/midiCnD0")


def info(dev):
    d = dev.ok(CMD["INFO"], what="INFO")
    if name_of(d[7:15]) != "TYPHOON" or d[0] != VER:
        sys.exit(f"{dev.port} is not typhoon (presets protocol {VER})")
    return dict(slots=d[1], record=d[2] | d[3] << 8, cur=d[4], edited=d[5], factory=d[6],
                version=name_of(d[15:23]) if len(d) >= 23 else "?")


def slots(dev, n):
    out = []
    for first in range(0, n, 4):
        k = min(4, n - first)
        d = dev.ok(CMD["LIST"], [first, k], "LIST")
        for i in range(k):
            out.append(dict(slot=first + i + 1, user=bool(d[i * 13]), name=name_of(d[i * 13 + 1:i * 13 + 13])))
    return out


def read(dev, k):
    rec = []
    for at in range(0, SLOT, CHUNK):
        st, d = dev.request(CMD["READ"], [k - 1] + u16(at) + [CHUNK])
        if st == "EMPTY":
            return None
        if st != "OK":
            sys.exit(f"READ slot {k}: the FM-1 says {st}")
        rec += d
    if crc32(rec[8:]) != int.from_bytes(bytes(rec[4:8]), "little"):
        sys.exit(f"slot {k}: the record read has a bad CRC (try again)")
    return rec


def write(dev, k, rec):
    dev.ok(CMD["WBEGIN"], [k - 1], "WRITE BEGIN")
    for at in range(0, SLOT, CHUNK):
        dev.ok(CMD["WDATA"], u16(at) + rec[at:at + CHUNK], "WRITE DATA")
    dev.ok(CMD["WCOMMIT"], u32(crc32(rec)), f"WRITE slot {k}")


def slot_arg(v, n=96):
    k = int(v)
    if not 1 <= k <= n:
        sys.exit(f"slot {k}: 1..{n}")
    return k


def main():
    ap = argparse.ArgumentParser(description="typhoon presets over USB-MIDI", epilog=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", help="the raw MIDI device (default: the FM-1's, found by name)")
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("list")
    b = sub.add_parser("backup")
    b.add_argument("file", nargs="?")
    b.add_argument("--slot", type=int)
    r = sub.add_parser("restore")
    r.add_argument("file")
    r.add_argument("--slot", type=int)
    r.add_argument("--to", type=int)
    n = sub.add_parser("rename")
    n.add_argument("slot", type=int)
    n.add_argument("name")
    d = sub.add_parser("delete")
    d.add_argument("slot", type=int)
    s = sub.add_parser("stats")
    s.add_argument("seconds", nargs="?", type=int, default=10)
    a = ap.parse_args()

    check_tables()
    dev = Device(find_port(a.port))
    inf = info(dev)
    if a.cmd == "list":
        print(f"typhoon {inf['version']} on {dev.port}")
        for s in slots(dev, inf["slots"]):
            mark = ">" if s["slot"] == inf["cur"] + 1 else " "
            print(f"{mark}{s['slot']:2}  {s['name']:12}  {'user' if s['user'] else 'factory' if s['slot'] <= inf['factory'] else '-'}")
    elif a.cmd == "backup":
        want = [slot_arg(a.slot, inf["slots"])] if a.slot else [s["slot"] for s in slots(dev, inf["slots"]) if s["user"]]
        out = []
        for k in want:
            rec = read(dev, k)
            if rec is None:
                sys.exit(f"slot {k} holds nothing saved (a factory preset is in the firmware)")
            out.append(dict(slot=k, name=name_of(rec[16:28]), record=bytes(rec).hex()))
            print(f"  {k:2}  {out[-1]['name']}")
        f = Path(a.file or time.strftime("typhoon-presets-%Y-%m-%d_%H%M%S.json"))
        f.write_text(json.dumps(dict(format="typhoon-presets", version=1, taken=time.strftime("%Y-%m-%d %H:%M:%S"),
                                     presets=out), indent=1) + "\n")
        print(f"{len(out)} preset(s) -> {f}")
    elif a.cmd == "restore":
        data = json.loads(Path(a.file).read_text())
        if data.get("format") not in ("typhoon-presets", "tornado-presets"):   # (tornado: typhoon's earlier name)
            sys.exit(f"{a.file}: not a typhoon presets backup")
        ps = [p for p in data["presets"] if not a.slot or p["slot"] == a.slot]
        if not ps:
            sys.exit(f"{a.file}: no preset in slot {a.slot}")
        if a.to and len(ps) != 1:
            sys.exit("--to needs one preset (--slot N)")
        for p in ps:
            rec = list(bytes.fromhex(p["record"]))
            if len(rec) != SLOT or crc32(rec[8:]) != int.from_bytes(bytes(rec[4:8]), "little"):
                sys.exit(f"{a.file}: slot {p['slot']}'s record is damaged")
            k = slot_arg(a.to or p["slot"], inf["slots"])
            write(dev, k, rec)
            print(f"  {k:2}  {p['name']}")
        print(f"{len(ps)} preset(s) written")
    elif a.cmd == "rename":
        k = slot_arg(a.slot, inf["slots"])
        nm = a.name.upper()
        if len(nm) > NAME or not re.fullmatch(r"[A-Z0-9 ./-]*", nm):
            sys.exit("a name: up to 12 of A-Z 0-9 space - . /")
        st, _ = dev.request(CMD["RENAME"], [k - 1] + list(nm.encode().ljust(NAME, b"\0")))
        if st == "EMPTY":
            sys.exit(f"slot {k} holds nothing saved: save it on the FM-1 first")
        if st != "OK":
            sys.exit(f"RENAME: the FM-1 says {st}")
        print(f"  {k:2}  {nm}")
    elif a.cmd == "stats":
        dev.ok(CMD["STATS"], what="STATS")             # (the highest starts again)
        for _ in range(a.seconds):
            time.sleep(1)
            d = dev.ok(CMD["STATS"], what="STATS")
            avg, peak = d[0] | d[1] << 8, d[2] | d[3] << 8
            print(f"  audio load {avg / 10:5.1f} %   highest {peak / 10:5.1f} %", flush=True)
    elif a.cmd == "delete":
        k = slot_arg(a.slot, inf["slots"])
        dev.ok(CMD["DELETE"], [k - 1], "DELETE")
        print(f"  {k:2}  deleted: it plays its factory preset again" if k <= inf["factory"] else f"  {k:2}  deleted")


if __name__ == "__main__":
    main()
