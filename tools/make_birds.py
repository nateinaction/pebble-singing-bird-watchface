#!/usr/bin/env python3
"""Generate 12 Texas-songbird silhouette icons for the Texas Songbird watchface.

Each bird is a stylised left-facing side profile built from primitives, drawn at
4x supersample then downsampled for clean edges. Output: 72x72 RGBA PNGs with a
transparent background (the watchface renders them over solid black).

Hour -> bird mapping (index 0 = 12 o'clock, clockwise):
  0 Northern Mockingbird   1 Painted Bunting        2 Scissor-tailed Flycatcher
  3 Vermilion Flycatcher   4 Eastern Screech-Owl    5 Carolina Wren
  6 Bewick's Wren          7 American Robin          8 Northern Parula
  9 Golden-cheeked Warbler 10 Blue Jay              11 Northern Cardinal
"""
import os
from PIL import Image, ImageDraw

S = 4                      # supersample factor
SZ = 72                    # final icon size (px)
N = SZ * S

OUT = os.path.join(os.path.dirname(__file__), "..", "resources", "images", "birds")

# palette
BONE = (224, 220, 208, 255)      # default silhouette (warm off-white)
GRAY = (150, 150, 150, 255)
DGRAY = (90, 90, 90, 255)
BROWN = (150, 96, 54, 255)
WARMBR = (176, 112, 60, 255)
RED = (200, 30, 24, 255)
VERM = (230, 40, 20, 255)
BLUE = (40, 90, 190, 255)
GREEN = (60, 150, 60, 255)
YELLOW = (230, 200, 40, 255)
BLACK = (30, 30, 30, 255)
WHITE = (240, 240, 240, 255)
ORANGE = (210, 110, 40, 255)


def _s(v):
    return int(round(v * S))


def draw_base(d, cx, cy, spec):
    """Draw a generic passerine centred near (cx, cy) in supersampled coords.

    spec keys: body, head_r, body_w, body_h, tilt(upright), tail(style,len,color),
    crest, ear_tufts, belly, back, mask, beak(len,hook), eyestripe, wing.
    """
    body = spec["body"]
    up = spec.get("upright", False)

    # --- tail (drawn first, behind body) ---
    tail = spec.get("tail", {})
    tstyle = tail.get("style", "medium")
    tlen = tail.get("len", 22)
    tcol = tail.get("color", body)
    # tail root sits at back of body
    if up:
        rootx, rooty = cx + _s(6), cy + _s(10)
    else:
        rootx, rooty = cx + _s(11), cy + _s(2)
    if tstyle == "forked":
        # long scissor tail: two thin diverging streamers
        for sgn in (-1, 1):
            tip_y = rooty + _s(tlen) * (1 if sgn > 0 else 0.4)
            d.polygon([
                (rootx, rooty - _s(3)), (rootx, rooty + _s(3)),
                (rootx + _s(tlen), tip_y + sgn * _s(6)),
                (rootx + _s(tlen), tip_y + sgn * _s(9)),
            ], fill=tcol)
    elif tstyle == "up":
        # cocked-up wren tail
        ang = _s(tlen)
        d.polygon([
            (rootx - _s(2), rooty), (rootx + _s(4), rooty),
            (rootx + _s(10), rooty - ang), (rootx + _s(4), rooty - ang),
        ], fill=tcol)
    else:
        length = {"short": 14, "medium": 22, "long": 30}.get(tstyle, tlen)
        dy = _s(8) if up else _s(6)
        d.polygon([
            (rootx, rooty - _s(4)), (rootx, rooty + _s(4)),
            (rootx + _s(length), rooty + dy + _s(4)),
            (rootx + _s(length), rooty + dy - _s(1)),
        ], fill=tcol)

    # --- body ---
    bw, bh = _s(spec.get("body_w", 20)), _s(spec.get("body_h", 17))
    if up:
        bbox = [cx - bw // 2, cy - bh, cx + bw // 2, cy + bh]
    else:
        bbox = [cx - bw, cy - bh // 2, cx + bw, cy + bh // 2]
    d.ellipse(bbox, fill=body)

    # belly / back accent overlays
    if spec.get("belly"):
        by0 = cy if not up else cy
        d.pieslice([cx - bw, cy - bh // 2, cx + bw, cy + bh // 2] if not up
                   else [cx - bw // 2, cy - bh, cx + bw // 2, cy + bh],
                   -30, 150, fill=spec["belly"])
    if spec.get("back"):
        d.pieslice([cx - bw, cy - bh // 2, cx + bw, cy + bh // 2] if not up
                   else [cx - bw // 2, cy - bh, cx + bw // 2, cy + bh],
                   150, 330, fill=spec["back"])

    # wing patch
    if spec.get("wing"):
        d.ellipse([cx - _s(10), cy - _s(4), cx + _s(6), cy + _s(6)], fill=spec["wing"])

    # --- head ---
    hr = _s(spec.get("head_r", 9))
    if up:
        hx, hy = cx - _s(1), cy - bh - _s(2)
    else:
        hx, hy = cx - bw + _s(2), cy - bh // 2 - _s(3)
    hcol = spec.get("head", body)
    d.ellipse([hx - hr, hy - hr, hx + hr, hy + hr], fill=hcol)

    # ear tufts (owl)
    if spec.get("ear_tufts"):
        for sgn in (-1, 1):
            ex = hx + sgn * _s(5)
            d.polygon([(ex - _s(3), hy - hr + _s(2)),
                       (ex + _s(3), hy - hr + _s(2)),
                       (ex + sgn * _s(2), hy - hr - _s(8))], fill=hcol)

    # crest
    crest = spec.get("crest")
    if crest:
        ch = _s(crest.get("h", 10))
        ccol = crest.get("color", hcol)
        d.polygon([(hx - _s(2), hy - hr + _s(2)),
                   (hx + _s(6), hy - hr + _s(3)),
                   (hx - _s(3), hy - hr - ch)], fill=ccol)

    # face mask (cardinal / warbler)
    if spec.get("mask"):
        d.ellipse([hx - hr - _s(2), hy - _s(2), hx + _s(3), hy + _s(6)],
                  fill=spec["mask"])
    if spec.get("cheek"):
        d.ellipse([hx - hr, hy - _s(1), hx - _s(1), hy + _s(6)], fill=spec["cheek"])

    # eyestripe (wrens)
    if spec.get("eyestripe"):
        d.line([(hx - hr, hy - _s(2)), (hx + _s(5), hy - _s(3))],
               fill=WHITE, width=_s(2))

    # beak
    beak = spec.get("beak", {})
    blen = _s(beak.get("len", 7))
    bcol = beak.get("color", DGRAY)
    if beak.get("hook"):
        d.polygon([(hx - hr + _s(1), hy - _s(1)), (hx - hr + _s(1), hy + _s(4)),
                   (hx - hr - blen, hy + _s(4)), (hx - hr - blen, hy + _s(6))],
                  fill=bcol)
    else:
        d.polygon([(hx - hr + _s(1), hy - _s(3)), (hx - hr + _s(1), hy + _s(3)),
                   (hx - hr - blen, hy + _s(1))], fill=bcol)

    # eye
    ecol = BLACK if hcol[0] + hcol[1] + hcol[2] > 300 else WHITE
    d.ellipse([hx - _s(4), hy - _s(3), hx - _s(1), hy], fill=ecol)

    # legs
    lcol = spec.get("legs", DGRAY)
    ly = cy + (bh if up else bh // 2)
    for off in (-4, 3):
        d.line([(cx + _s(off), ly - _s(2)), (cx + _s(off) - _s(1), ly + _s(8))],
               fill=lcol, width=_s(2))


# --- per-species specs -----------------------------------------------------
BIRDS = {
    0: dict(  # Northern Mockingbird — slim gray, long tail, gray wing
        body=(178, 178, 172, 255), head=(178, 178, 172, 255), body_w=19, body_h=14,
        head_r=8, tail=dict(style="long", color=(150, 150, 145, 255)),
        wing=(120, 120, 116, 255), beak=dict(len=7)),
    1: dict(  # Painted Bunting — blue head, green back, red belly
        body=RED, head=BLUE, back=GREEN, belly=RED, body_w=17, body_h=15,
        head_r=8, tail=dict(style="medium", color=GREEN), beak=dict(len=6)),
    2: dict(  # Scissor-tailed Flycatcher — pale gray, long forked tail
        body=(198, 198, 198, 255), head=(210, 210, 210, 255), belly=(225, 190, 190, 255),
        body_w=15, body_h=12, head_r=7, tail=dict(style="forked", len=34,
        color=(60, 60, 60, 255)), beak=dict(len=6)),
    3: dict(  # Vermilion Flycatcher — vermilion body/crest, dark back+mask
        body=VERM, head=VERM, back=(60, 45, 45, 255), body_w=16, body_h=14,
        head_r=8, crest=dict(h=7, color=VERM), mask=(50, 40, 40, 255),
        tail=dict(style="medium", color=(60, 45, 45, 255)), beak=dict(len=6)),
    4: dict(  # Eastern Screech-Owl — upright, ear tufts, big head, short tail
        body=(150, 120, 92, 255), head=(150, 120, 92, 255), upright=True,
        body_w=22, body_h=17, head_r=12, ear_tufts=True,
        tail=dict(style="short", color=(130, 104, 78, 255)),
        beak=dict(len=4, hook=True), legs=(120, 96, 70, 255)),
    5: dict(  # Carolina Wren — plump warm brown, cocked tail, eyestripe
        body=WARMBR, head=WARMBR, belly=(210, 170, 120, 255), body_w=17, body_h=15,
        head_r=8, tail=dict(style="up", len=16, color=WARMBR), eyestripe=True,
        beak=dict(len=8), legs=BROWN),
    6: dict(  # Bewick's Wren — slimmer gray-brown, long cocked tail, eyestripe
        body=(140, 120, 96, 255), head=(140, 120, 96, 255), body_w=15, body_h=12,
        head_r=7, tail=dict(style="up", len=22, color=(120, 102, 80, 255)),
        eyestripe=True, beak=dict(len=8), legs=BROWN),
    7: dict(  # American Robin — upright, dark head, gray back, orange belly
        body=(110, 110, 110, 255), head=(60, 60, 60, 255), belly=ORANGE,
        upright=True, body_w=19, body_h=16, head_r=9,
        tail=dict(style="medium", color=(90, 90, 90, 255)),
        beak=dict(len=6, color=YELLOW), legs=(120, 90, 60, 255)),
    8: dict(  # Northern Parula — tiny, blue-gray back, yellow throat, short tail
        body=YELLOW, head=(110, 140, 175, 255), back=(110, 140, 175, 255),
        belly=YELLOW, body_w=13, body_h=12, head_r=7,
        tail=dict(style="short", color=(110, 140, 175, 255)), beak=dict(len=5)),
    9: dict(  # Golden-cheeked Warbler — black body, yellow cheek, slim
        body=BLACK, head=BLACK, cheek=YELLOW, body_w=14, body_h=12, head_r=7,
        tail=dict(style="medium", color=BLACK), beak=dict(len=5)),
    10: dict(  # Blue Jay — blue, crest, white belly
        body=BLUE, head=BLUE, belly=(225, 225, 235, 255), body_w=18, body_h=15,
        head_r=9, crest=dict(h=11, color=BLUE), mask=(40, 40, 40, 255),
        tail=dict(style="long", color=(30, 70, 160, 255)), beak=dict(len=6)),
    11: dict(  # Northern Cardinal — red, crest, black mask, orange beak
        body=RED, head=RED, body_w=18, body_h=15, head_r=9,
        crest=dict(h=12, color=RED), mask=BLACK,
        tail=dict(style="long", color=(170, 24, 20, 255)),
        beak=dict(len=7, color=ORANGE)),
}


def main():
    os.makedirs(OUT, exist_ok=True)
    for idx, spec in BIRDS.items():
        img = Image.new("RGBA", (N, N), (0, 0, 0, 0))
        d = ImageDraw.Draw(img)
        draw_base(d, N // 2, N // 2, spec)
        img = img.resize((SZ, SZ), Image.LANCZOS)
        path = os.path.join(OUT, f"bird_{idx:02d}.png")
        img.save(path)
        print("wrote", os.path.relpath(path))


if __name__ == "__main__":
    main()
