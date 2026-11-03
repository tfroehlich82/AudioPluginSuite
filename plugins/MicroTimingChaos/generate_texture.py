#!/usr/bin/env python3
"""
Generate 4K textures for Micro-Timing Chaos editor.
Outputs:
  Resources/textures/sequencer_bg_4k.png  — dark gradient with subtle noise
  Resources/textures/bpm_sync_4k.png      — icon background
  Resources/textures/sequencer_grid_4k.png — grid pattern
"""

import os
from PIL import Image, ImageDraw, ImageFont

TEXTURES_DIR = "C:/Users/tfroe/Documents/PROJECTS/AudioPluginSuite/plugins/MicroTimingChaos/Resources/textures"
SIZE = (4096, 2458)
ACCENT = "#7DFF5A"
ACCENT_DARK = "#5A7F3E"
ACCENT_DIM = "#1A4A2E"
BG = "#0B0E13"
NOISE = 0.02

def make_sequencer_bg():
    """Dark gradient with subtle noise."""
    img = Image.new("RGBA", SIZE, BG)
    d = ImageDraw.Draw(img)

    # Gradient from top-left to bottom-right
    width, height = SIZE
    for y in range(0, height, 64):
        for x in range(0, width, 64):
            t = (x * height + y * width) / (width * height)
            c = BG
            r = int(0x0B + t * (0x1F - 0x0B))
            g = int(0x0E + t * (0x26 - 0x0E))
            b = int(0x13 + t * (0x31 - 0x13))
            d.rectangle(
                [x, y, x + 64, y + 64],
                fill=(r, g, b, 255),
            )

    # Subtle noise overlay
    noise = Image.new("L", SIZE, 0)
    for py in range(0, height, 32):
        for px in range(0, width, 32):
            # Random noise with low amplitude
            noise_val = int((NOISE * 255) * (1 if (px + py) % 7 == 0 else 0))
            noise.putpixel((px, py), noise_val)

    img_noise = Image.new("RGBA", SIZE)
    img_noise.paste(noise, None, noise)
    img_noise = img_noise.convert("RGBA")
    alpha = int(255 * NOISE)
    img.paste(img_noise, None, Image.new("L", SIZE, (0, 0, 0, alpha)))

    # Border
    d.rectangle(
        [10, 10, width - 10, height - 10],
        fill=(0x2A, 0x33, 0x41, 255),
        outline=(0x15, 0x1A, 0x22, 255),
        width=2,
        rounded=True,
    )

    img.save(os.path.join(TEXTURES_DIR, "sequencer_bg_4k.png"))
    print("  sequencer_bg_4k.png")


def make_sequencer_grid():
    """Grid pattern for the step view."""
    img = Image.new("RGBA", SIZE, BG)
    d = ImageDraw.Draw(img)

    # Subtle grid lines
    for y in range(0, height, 96):
        d.rectangle([0, y, width, y + 1], fill=(0x1E, 0x24, 0x2F, 64))
    for x in range(0, width, 96):
        d.rectangle([x, 0, x + 1, height], fill=(0x1E, 0x24, 0x2F, 64))

    # Accent dots
    accent_radius = 32
    for i in range(0, width, 64):
        for j in range(0, height, 64):
            d.ellipse(
                [i + 32 - 16, j + 32 - 16, i + 32 + 16, j + 32 + 16],
                fill=(0x3A, 0x4A, 0x5A, 128),
            )

    img.save(os.path.join(TEXTURES_DIR, "sequencer_grid_4k.png"))
    print("  sequencer_grid_4k.png")


def make_bpm_sync():
    """BPM sync icon background."""
    img = Image.new("RGBA", SIZE, BG)
    d = ImageDraw.Draw(img)

    # Circular accent
    cx, cy = width / 2, height / 2
    radius = min(width, height) / 3
    d.ellipse(
        [cx - radius, cy - radius, cx + radius, cy + radius],
        fill=(0x1A, 0x2A, 0x3A, 255),
        outline=(0x3A, 0x4A, 0x5A, 255),
        width=3,
        rounded=True,
    )

    # Icon: sync symbol
    sync_x = cx
    sync_y = cy
    inner_radius = radius * 0.6
    d.ellipse(
        [sync_x - inner_radius, sync_y - inner_radius, sync_x + inner_radius, sync_y + inner_radius],
        fill=(0x00, 0x00, 0x00, 128),
    )

    # Two arcs representing sync
    angle = 1.0
    arc_radius = inner_radius * 0.65
    for n in range(12):
        a1 = angle * n
        a2 = angle * (n + 1)
        x1 = cx + arc_radius * 0.7 * (cos(a1) * 0.5 - sin(a1) * 0.5)
        y1 = cy + arc_radius * 0.7 * (cos(a1) * 0.5 + sin(a1) * 0.5)
        x2 = cx + arc_radius * 0.7 * (cos(a2) * 0.5 - sin(a2) * 0.5)
        y2 = cy + arc_radius * 0.7 * (cos(a2) * 0.5 + sin(a2) * 0.5)
        d.line([(x1, y1), (x2, y2)], fill=(0x3A, 0x4A, 0x5A, 192), width=3)

    img.save(os.path.join(TEXTURES_DIR, "bpm_sync_4k.png"))
    print("  bpm_sync_4k.png")


if __name__ == "__main__":
    os.makedirs(TEXTURES_DIR, exist_ok=True)
    make_sequencer_bg()
    make_sequencer_grid()
    make_bpm_sync()
    print("Done.")
