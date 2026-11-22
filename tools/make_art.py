"""Draws the Blocksmith 2 icon, Home Menu banner and banner sound in code
(kickoff answer F-4: art is drawn in code, nothing copied).

    py -3 tools/make_art.py meta

Writes into the given folder:
    icon.png     48 x 48   Home Menu icon (a grass block)
    banner.png   256 x 128 top-screen banner shown when the icon is picked
    banner.wav   short silent clip (bannertool needs a sound)

Same input, same bytes out, so make only redraws when this file changes.
"""

import struct
import sys
import wave
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

GRASS_TOP = (96, 168, 64)
GRASS_TOP_DARK = (78, 142, 50)
GRASS_SIDE = (70, 130, 46)
DIRT = (134, 96, 67)
DIRT_DARK = (106, 74, 50)
DIRT_LIGHT = (156, 116, 84)


def speckle(x, y, seed):
    """Small fixed pattern so the block faces are not flat (no random module,
    so every run gives the same pixels)."""
    return ((x * 73856093) ^ (y * 19349663) ^ (seed * 83492791)) & 0xFF


def draw_block(size):
    """A grass block seen from slightly above: top face plus two side faces."""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    px = img.load()
    cx = size / 2
    top_h = size * 0.25     # height of the top diamond's half
    side_h = size * 0.5     # height of the side faces
    y0 = size * 0.06

    for y in range(size):
        for x in range(size):
            fx, fy = x + 0.5, y + 0.5
            dx = abs(fx - cx) / cx
            # Top face: a diamond.
            if fy >= y0 and abs(fx - cx) / cx + abs(fy - (y0 + top_h)) / top_h <= 1.0:
                s = speckle(x, y, 1)
                px[x, y] = GRASS_TOP_DARK if s < 60 else GRASS_TOP
                continue
            # Side faces: below the diamond's lower edges.
            top_edge = y0 + top_h + (1.0 - dx) * top_h
            if fy >= top_edge and fy <= top_edge + side_h and dx <= 1.0:
                left = fx < cx
                depth = fy - top_edge
                s = speckle(x, y, 2 if left else 3)
                if depth < size * 0.09 + (s & 3):
                    col = GRASS_SIDE
                else:
                    col = DIRT_DARK if s < 50 else (DIRT_LIGHT if s > 220 else DIRT)
                if not left:  # right face is in shade
                    col = tuple(int(c * 0.78) for c in col)
                px[x, y] = col + (255,)
    return img


def make_icon(path):
    draw_block(48).save(path, optimize=False)


def make_banner(path):
    w, h = 256, 128
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    # Sky fading down to a strip of ground.
    for y in range(h):
        t = y / (h - 1)
        d.line([(0, y), (w, y)], fill=(int(110 + 60 * t), int(170 + 40 * t), 235, 255))
    ground = 100
    d.rectangle([0, ground, w, h], fill=DIRT + (255,))
    d.rectangle([0, ground, w, ground + 6], fill=GRASS_SIDE + (255,))
    for x in range(0, w, 4):
        for y in range(ground + 8, h, 4):
            if speckle(x, y, 4) < 50:
                d.rectangle([x, y, x + 1, y + 1], fill=DIRT_DARK + (255,))

    block = draw_block(64)
    img.alpha_composite(block, (14, ground - 58))

    font = ImageFont.load_default(size=26)
    small = ImageFont.load_default(size=13)
    title = "Blocksmith 2"
    d.text((92, 32), title, font=font, fill=(30, 40, 30, 255), stroke_width=0)
    d.text((90, 30), title, font=font, fill=(255, 255, 255, 255))
    d.text((92, 64), "Build, mine and explore", font=small, fill=(255, 255, 255, 255))
    img.save(path, optimize=False)


def make_wav(path):
    rate = 22050
    frames = rate // 2  # half a second of silence
    with wave.open(str(path), "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(rate)
        wf.writeframes(struct.pack("<h", 0) * frames)


def main():
    if len(sys.argv) != 2:
        print("usage: py -3 tools/make_art.py <output folder>")
        return 2
    out = Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)
    make_icon(out / "icon.png")
    make_banner(out / "banner.png")
    make_wav(out / "banner.wav")
    return 0


if __name__ == "__main__":
    sys.exit(main())
