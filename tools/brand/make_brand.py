"""Generates the FROTH Audio brand artwork: the brand mark and one lockup per plugin.

Run from the repository root:

    python -I tools/brand/make_brand.py

Output goes to modules/suite_ui/brand/. The PNGs are rendered at 4x the size they are shown at,
so they stay sharp when the editor is scaled down. Re-run this script after changing a name or a
colour here, then rebuild, so the BinaryData embeds the new files.

The brand is "FROTH" in a bubble-O wordmark, with "Audio" beside it. Change BRAND_WORD and
BRAND_SUFFIX below to rename it. The typeface is Windows' Bahnschrift, read from the system fonts
folder. Check its licence before shipping the artwork.
"""

import os
from PIL import Image, ImageDraw, ImageFilter, ImageFont

BRAND_WORD = "FROTH"
BRAND_SUFFIX = "Audio"
FONT_PATH = "C:/Windows/Fonts/bahnschrift.ttf"
OUT_DIR = os.path.join(os.path.dirname(__file__), "..", "..", "modules", "suite_ui", "brand")

# Lockup height in pixels, at 4x the size it is drawn at in the editor (about 40 px there).
SCALE = 4
LOCKUP_H = 40 * SCALE

NEUTRAL = (221, 228, 238)      # SuiteUI::Colours::text
BUBBLE_WHITE = (226, 246, 255)

# One lockup per plugin: file name, product line, accent colour (the editor's accent)
PLUGINS = [
    ("lockup_spectral_drift.png", "SPECTRAL DRIFT", (45, 226, 255)),         # cyan  #2DE2FF
    ("lockup_dynamic_saturation.png", "DYNAMIC SATURATION", (255, 159, 28)),  # amber #FF9F1C
    ("lockup_micro_timing_chaos.png", "MICRO-TIMING CHAOS", (125, 255, 90)),  # lime  #7DFF5A
]


def font(size, weight):
    f = ImageFont.truetype(FONT_PATH, size)
    f.set_variation_by_name(weight)
    return f


def cap_box(f):
    """Top and bottom of the capital H, relative to the drawing origin."""
    box = f.getbbox("H")
    return box[1], box[3]


def spaced_width(text, f, spacing):
    return sum(f.getlength(ch) for ch in text) + spacing * max(0, len(text) - 1)


def gradient(size, top, bottom):
    """A vertical gradient image, the same size as the text mask."""
    w, h = size
    img = Image.new("RGBA", (w, h))
    px = img.load()
    for y in range(h):
        t = y / max(1, h - 1)
        c = tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3)) + (255,)
        for x in range(w):
            px[x, y] = c
    return img


def draw_bubble_o(mask_draw, glint_draw, box, thickness):
    """The O of FROTH as a bubble: a ring, a smaller bubble inside it, and a glint on the top left."""
    x0, y0, x1, y1 = box
    mask_draw.ellipse(box, outline=255, width=thickness)

    w, h = x1 - x0, y1 - y0
    cx, cy = x0 + w * 0.58, y0 + h * 0.60
    r = min(w, h) * 0.16
    mask_draw.ellipse([cx - r, cy - r, cx + r, cy + r], outline=255, width=max(2, thickness // 2))

    glint_draw.arc([x0 + w * 0.16, y0 + h * 0.16, x1 - w * 0.16, y1 - h * 0.16],
                   start=195, end=255, fill=255, width=max(2, thickness // 2))


def brand_mark(size):
    """A bubble cluster: one large bubble with two smaller ones, each with a glint."""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    s = float(size)
    stroke = max(3, size // 26)

    def bubble(cx, cy, r, alpha):
        box = [cx * s - r * s, cy * s - r * s, cx * s + r * s, cy * s + r * s]
        d.ellipse(box, outline=BUBBLE_WHITE + (alpha,), width=stroke)
        glint = [box[0] + r * s * 0.35, box[1] + r * s * 0.35, box[2] - r * s * 0.35, box[3] - r * s * 0.35]
        d.arc(glint, start=200, end=250, fill=(255, 255, 255, 255), width=stroke)

    bubble(0.42, 0.56, 0.32, 255)
    bubble(0.82, 0.22, 0.13, 220)
    bubble(0.84, 0.74, 0.08, 200)
    return img


def lockup(product, accent, out_path):
    brand_font = font(int(LOCKUP_H * 0.80), "Bold")
    suffix_font = font(int(LOCKUP_H * 0.30), "Bold")
    product_font = font(int(LOCKUP_H * 0.34), "Bold")

    spacing_brand = LOCKUP_H * 0.02
    spacing_suffix = LOCKUP_H * 0.06
    spacing_product = LOCKUP_H * 0.03

    brand_top, brand_bottom = cap_box(brand_font)
    brand_cap = brand_bottom - brand_top
    product_top, product_bottom = cap_box(product_font)
    product_cap = product_bottom - product_top

    pad = int(LOCKUP_H * 0.25)
    gap_lines = int(LOCKUP_H * 0.10)
    text_h = brand_cap + gap_lines + product_cap
    mark_size = int(text_h * 1.10)

    # Vertical layout: brand cap top at y0, product cap below it
    y0 = pad
    brand_y = y0 - brand_top
    suffix_y = brand_y + brand_bottom - (suffix_font.getbbox("H")[3])
    product_y = y0 + brand_cap + gap_lines - product_top

    text_x = pad + mark_size + int(LOCKUP_H * 0.20)

    brand_w = spaced_width(BRAND_WORD, brand_font, spacing_brand)
    suffix_x = text_x + brand_w + int(LOCKUP_H * 0.22)
    suffix_w = spaced_width(BRAND_SUFFIX, suffix_font, spacing_suffix)
    product_w = spaced_width(product, product_font, spacing_product)
    width = int(max(suffix_x + suffix_w, text_x + product_w) + pad)
    height = int(y0 + text_h + 2 * pad)

    # Masks: the brand word (with the bubble O), its glint, the suffix and the product line
    brand_mask = Image.new("L", (width, height), 0)
    bd = ImageDraw.Draw(brand_mask)
    glint_mask = Image.new("L", (width, height), 0)
    gd = ImageDraw.Draw(glint_mask)

    x = text_x
    for ch in BRAND_WORD:
        if ch == "O":
            ox0, oy0, ox1, oy1 = brand_font.getbbox("O")
            box = [x + ox0, brand_y + oy0, x + ox1, brand_y + oy1]
            thickness = max(4, int((oy1 - oy0) * 0.16))
            draw_bubble_o(bd, gd, box, thickness)
        else:
            bd.text((x, brand_y), ch, font=brand_font, fill=255)
        x += brand_font.getlength(ch) + spacing_brand

    suffix_mask = Image.new("L", (width, height), 0)
    sx = suffix_x
    for ch in BRAND_SUFFIX:
        ImageDraw.Draw(suffix_mask).text((sx, suffix_y), ch, font=suffix_font, fill=255)
        sx += suffix_font.getlength(ch) + spacing_suffix

    product_mask = Image.new("L", (width, height), 0)
    px = text_x
    for ch in product:
        ImageDraw.Draw(product_mask).text((px, product_y), ch, font=product_font, fill=255)
        px += product_font.getlength(ch) + spacing_product

    out = Image.new("RGBA", (width, height), (0, 0, 0, 0))

    # Glow of the brand word in the accent colour, behind everything
    glow = Image.new("RGBA", (width, height), accent + (0,))
    glow.putalpha(brand_mask.filter(ImageFilter.GaussianBlur(LOCKUP_H * 0.07)).point(lambda v: int(v * 0.6)))
    out.alpha_composite(glow)

    # The brand word: accent gradient, with the glint on the bubble
    brand_layer = gradient((width, height), tuple(min(255, c + 70) for c in accent), accent)
    brand_layer.putalpha(brand_mask)
    out.alpha_composite(brand_layer)

    glint_layer = Image.new("RGBA", (width, height), (255, 255, 255, 230))
    glint_layer.putalpha(glint_mask)
    out.alpha_composite(glint_layer)

    suffix_layer = Image.new("RGBA", (width, height), NEUTRAL + (255,))
    suffix_layer.putalpha(suffix_mask)
    out.alpha_composite(suffix_layer)

    product_layer = Image.new("RGBA", (width, height), NEUTRAL + (255,))
    product_layer.putalpha(product_mask)
    out.alpha_composite(product_layer)

    # The mark, vertically centred on the text block
    mark = brand_mark(mark_size)
    out.alpha_composite(mark, (pad, (height - mark_size) // 2))

    # Trim to the artwork, then pad evenly, so the lockup is centred in its own image
    out = out.crop(out.getbbox())
    margin = pad // 2
    framed = Image.new("RGBA", (out.width + 2 * margin, out.height + 2 * margin), (0, 0, 0, 0))
    framed.alpha_composite(out, (margin, margin))

    framed.save(out_path)
    return framed.size


def main():
    os.makedirs(OUT_DIR, exist_ok=True)

    mark_path = os.path.join(OUT_DIR, "brand_mark.png")
    brand_mark(LOCKUP_H).save(mark_path)
    print("wrote", os.path.normpath(mark_path))

    for file_name, product, accent in PLUGINS:
        path = os.path.join(OUT_DIR, file_name)
        size = lockup(product, accent, path)
        print("wrote", os.path.normpath(path), size)


if __name__ == "__main__":
    main()
