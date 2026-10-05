#!/usr/bin/env python3
"""Compile .nxscene DSL → .nxb binary (+ optional C header)."""

from __future__ import annotations

import argparse
import re
import shlex
import struct
from dataclasses import dataclass, field
from pathlib import Path
from typing import List, Optional


def shlex_split(line: str) -> List[str]:
    """Split preserving quoted strings."""
    return shlex.split(line, posix=True)

NXB_MAGIC = 0x3142584E
NXB_VERSION = 1

SEG_COLOR = 1
SEG_FTEXT = 2
SEG_GRAPH8 = 3

OBJ_RECT = 1
OBJ_CIRCLE = 2
OBJ_TEXT = 3
OBJ_NUM = 4

OBJ_F_VISIBLE = 0x01
OBJ_F_GRAPH = 0x02


def rgb_to_r3g3b2(rgb: int) -> int:
    r = (rgb >> 16) & 0xFF
    g = (rgb >> 8) & 0xFF
    b = rgb & 0xFF
    return ((r & 0xE0) >> 0) | ((g & 0xE0) >> 3) | ((b & 0xC0) >> 6)


def parse_color(tok: str) -> int:
    if tok.startswith("#"):
        h = tok[1:]
        if len(h) != 6:
            raise ValueError(f"bad color {tok}")
        return rgb_to_r3g3b2(int(h, 16))
    return int(tok, 0) & 0xFF


@dataclass
class Seg:
    kind: int
    width: int
    bg: int = 0
    font_id: int = 1
    buf_id: int = 0
    rows: int = 0


@dataclass
class Strip:
    height: int
    segs: List[Seg] = field(default_factory=list)
    page_id: int = 0


@dataclass
class Obj:
    type: int
    name: str
    page_id: int
    x: int = 0
    y: int = 0
    w: int = 0
    h: int = 0
    fill: int = 0
    stroke: int = 0
    stroke_w: int = 0
    color: int = 255
    font_id: int = 1
    text: str = ""
    val: int = 0
    digits: int = 0
    flags: int = OBJ_F_VISIBLE


@dataclass
class Page:
    name: str
    id: int
    bg: int = 0
    strips: List[Strip] = field(default_factory=list)
    objs: List[Obj] = field(default_factory=list)


@dataclass
class Scene:
    width: int = 320
    height: int = 240
    pages: List[Page] = field(default_factory=list)


KV_RE = re.compile(r'([A-Za-z_][A-Za-z0-9_]*)\s*=\s*("([^"]*)"|#?[A-Za-z0-9_]+)')


def parse_kv(parts: List[str]) -> dict:
    joined = " ".join(parts)
    out = {}
    for m in KV_RE.finditer(joined):
        k = m.group(1)
        raw = m.group(2)
        if raw.startswith('"'):
            out[k] = raw[1:-1]
        elif raw.startswith("#"):
            out[k] = parse_color(raw)
        else:
            try:
                out[k] = int(raw, 0)
            except ValueError:
                out[k] = raw
    return out


def font_height(font_id: int) -> int:
    return 16 if font_id == 1 else 8


def parse_scene(text: str) -> Scene:
    scene = Scene()
    page: Optional[Page] = None
    in_plan = False
    strip: Optional[Strip] = None

    for lineno, raw in enumerate(text.splitlines(), 1):
        # Strip comments: '#' only when not part of #RRGGBB color
        line = re.sub(r"(?<![0-9A-Fa-f])#(?![0-9A-Fa-f]{6}\b).*", "", raw).strip()
        if not line:
            continue
        parts = shlex_split(line)
        op = parts[0]

        try:
            if op == "meta":
                continue
            if op == "size" and len(parts) >= 3:
                scene.width = int(parts[1])
                scene.height = int(parts[2])
                continue
            if op == "font":
                continue
            if op == "page":
                kv = parse_kv(parts[3:])
                page = Page(name=parts[1], id=int(parts[2]), bg=int(kv.get("bg", 0)))
                scene.pages.append(page)
                in_plan = False
                strip = None
                continue
            if page is None:
                raise ValueError("statement before page")

            if op == "plan":
                in_plan = True
                strip = None
                continue
            if in_plan and op == "strip":
                strip = Strip(height=int(parts[1]), page_id=page.id)
                page.strips.append(strip)
                continue
            if in_plan and op == "seg":
                if strip is None:
                    raise ValueError("seg without strip")
                kind_s = parts[1]
                width = int(parts[2])
                kv = parse_kv(parts[3:])
                if kind_s == "color":
                    seg = Seg(SEG_COLOR, width, bg=int(kv.get("bg", page.bg)))
                elif kind_s == "ftext":
                    font_id = int(kv.get("font", 1))
                    buf = str(kv.get("buf", "ftext0"))
                    buf_id = 0 if buf.endswith("0") else 1
                    rows = max(1, strip.height // font_height(font_id))
                    seg = Seg(
                        SEG_FTEXT,
                        width,
                        bg=int(kv.get("bg", page.bg)),
                        font_id=font_id,
                        buf_id=buf_id,
                        rows=rows,
                    )
                elif kind_s == "graph8":
                    seg = Seg(SEG_GRAPH8, width, buf_id=0, rows=strip.height)
                else:
                    raise ValueError(f"unknown seg {kind_s}")
                strip.segs.append(seg)
                continue

            if op in ("rect", "circle", "text", "num"):
                in_plan = False
                strip = None

            if op == "rect":
                name = parts[1]
                kv = parse_kv(parts[2:])
                flags = OBJ_F_VISIBLE | (OBJ_F_GRAPH if "graph" in parts else 0)
                page.objs.append(
                    Obj(
                        OBJ_RECT,
                        name,
                        page.id,
                        x=int(kv.get("x", 0)),
                        y=int(kv.get("y", 0)),
                        w=int(kv.get("w", 0)),
                        h=int(kv.get("h", 0)),
                        fill=int(kv.get("fill", 0)),
                        stroke=int(kv.get("stroke", 0)),
                        stroke_w=int(kv.get("stroke_w", 0)),
                        flags=flags,
                    )
                )
                continue
            if op == "circle":
                name = parts[1]
                kv = parse_kv(parts[2:])
                flags = OBJ_F_VISIBLE | (OBJ_F_GRAPH if "graph" in parts else 0)
                page.objs.append(
                    Obj(
                        OBJ_CIRCLE,
                        name,
                        page.id,
                        x=int(kv.get("x", 0)),
                        y=int(kv.get("y", 0)),
                        w=int(kv.get("r", 0)),
                        fill=int(kv.get("fill", 0)),
                        stroke=int(kv.get("stroke", 0)),
                        stroke_w=int(kv.get("stroke_w", 0)),
                        flags=flags,
                    )
                )
                continue
            if op == "text":
                name = parts[1]
                kv_parts = []
                text = ""
                for p in parts[2:]:
                    if "=" in p:
                        kv_parts.append(p)
                    else:
                        text = p
                kv = parse_kv(kv_parts)
                page.objs.append(
                    Obj(
                        OBJ_TEXT,
                        name,
                        page.id,
                        x=int(kv.get("x", 0)),
                        y=int(kv.get("y", 0)),
                        font_id=int(kv.get("font", 1)),
                        color=int(kv.get("color", 255)),
                        text=text,
                    )
                )
                continue
            if op == "num":
                name = parts[1]
                kv = parse_kv(parts[2:])
                page.objs.append(
                    Obj(
                        OBJ_NUM,
                        name,
                        page.id,
                        x=int(kv.get("x", 0)),
                        y=int(kv.get("y", 0)),
                        font_id=int(kv.get("font", 1)),
                        color=int(kv.get("color", 255)),
                        val=int(kv.get("val", 0)),
                        digits=int(kv.get("digits", 0)),
                    )
                )
                continue

            raise ValueError(f"unknown op {op}")
        except Exception as e:
            raise SystemExit(f"{lineno}: {e}") from e

    return scene


class Pool:
    def __init__(self) -> None:
        self.data = bytearray(b"\0")

    def add(self, s: str) -> int:
        b = s.encode("utf-8") + b"\0"
        idx = self.data.find(b)
        if idx >= 0:
            return idx
        off = len(self.data)
        self.data.extend(b)
        return off


def compile_scene(scene: Scene) -> bytes:
    names = Pool()
    strings = Pool()

    page_recs = []
    strip_recs = []
    seg_recs = []
    obj_recs = []

    for p in scene.pages:
        strip_first = len(strip_recs)
        for st in p.strips:
            seg_first = len(seg_recs)
            for sg in st.segs:
                seg_recs.append(sg)
            strip_recs.append((st.height, seg_first, len(st.segs), p.id))

        obj_first = len(obj_recs)
        for o in p.objs:
            name_off = names.add(o.name)
            str_off = strings.add(o.text) if o.type == OBJ_TEXT else 0
            obj_recs.append((o, name_off, str_off))

        page_recs.append(
            (
                p.id,
                p.bg,
                names.add(p.name),
                obj_first,
                len(p.objs),
                strip_first,
                len(p.strips),
            )
        )

    out = bytearray()
    out += struct.pack(
        "<IHHHHHHHHHH",
        NXB_MAGIC,
        NXB_VERSION,
        scene.width,
        scene.height,
        len(page_recs),
        len(obj_recs),
        len(strip_recs),
        len(seg_recs),
        len(names.data),
        len(strings.data),
        0,
    )

    for p in page_recs:
        out += struct.pack("<BBHHHHBB", p[0], p[1], p[2], p[3], p[4], p[5], p[6], 0)

    for st in strip_recs:
        out += struct.pack("<HHBBH", st[0], st[1], st[2], st[3], 0)

    for sg in seg_recs:
        out += struct.pack("<BBBBHH", sg.kind, sg.bg, sg.font_id, sg.buf_id, sg.width, sg.rows)

    for o, name_off, str_off in obj_recs:
        out += struct.pack(
            "<BBBBHhhhhBBBBHiBBBB",
            o.type,
            o.flags,
            o.page_id,
            o.font_id,
            name_off,
            o.x,
            o.y,
            o.w,
            o.h,
            o.fill,
            o.stroke,
            o.stroke_w,
            o.color,
            str_off,
            o.val,
            o.digits,
            0,
            0,
            0,
        )

    out += names.data
    out += strings.data
    return bytes(out)


def write_header(blob: bytes, path: Path, symbol: str) -> None:
    lines = [
        "#pragma once",
        "#include <stdint.h>",
        f"static const uint8_t {symbol}[] = {{",
    ]
    for i in range(0, len(blob), 12):
        chunk = ", ".join(f"0x{b:02x}" for b in blob[i : i + 12])
        lines.append(f"  {chunk},")
    lines.append("};")
    lines.append(f"static const uint32_t {symbol}_len = {len(blob)};")
    path.write_text("\n".join(lines) + "\n")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("input", type=Path)
    ap.add_argument("-o", "--output", type=Path, required=True)
    ap.add_argument("--header", type=Path, help="Also emit C header with byte array")
    ap.add_argument("--symbol", default="demo_main_nxb")
    args = ap.parse_args()

    scene = parse_scene(args.input.read_text())
    blob = compile_scene(scene)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(blob)
    if args.header:
        args.header.parent.mkdir(parents=True, exist_ok=True)
        write_header(blob, args.header, args.symbol)
    print(f"wrote {args.output} ({len(blob)} bytes), pages={len(scene.pages)} objs={sum(len(p.objs) for p in scene.pages)}")


if __name__ == "__main__":
    main()
