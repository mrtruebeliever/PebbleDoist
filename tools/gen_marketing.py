#!/usr/bin/env python3
"""Generates the store marketing icons (rounded-square app-icon logos): a dark
checklist with the top item checked in the Todoist-ish red accent. Rendered at 4x
and downsampled for smooth edges. Mirrors Demi's marketing/ layout."""
import os
from PIL import Image, ImageDraw

SS = 4
ACCENT = (228, 67, 50, 255)   # #E44332
BG_TOP = (38, 40, 46, 255)
BG_BOT = (22, 23, 27, 255)
LINE_DONE = (236, 236, 238, 255)
LINE = (120, 124, 132, 255)
BOX = (150, 154, 162, 255)
WHITE = (255, 255, 255, 255)


def render(size):
    S = size * SS
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    rad = int(S * 0.225)

    # Rounded-square background with a soft vertical gradient.
    grad = Image.new("RGBA", (1, S))
    for y in range(S):
        f = y / float(S - 1)
        grad.putpixel((0, y), tuple(int(BG_TOP[i] + (BG_BOT[i] - BG_TOP[i]) * f) for i in range(4)))
    grad = grad.resize((S, S))
    mask = Image.new("L", (S, S), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, S - 1, S - 1], radius=rad, fill=255)
    img.paste(grad, (0, 0), mask)

    # Three checklist rows.
    box = int(S * 0.135)
    bx = int(S * 0.20)
    lx0 = int(S * 0.40)
    lx1 = int(S * 0.80)
    lw = max(2, int(S * 0.035))
    rows_y = [0.36, 0.52, 0.68]
    for i, ry in enumerate(rows_y):
        cy = int(S * ry)
        top = cy - box // 2
        if i == 0:
            # Completed item: filled accent box + white check + bright line.
            d.rounded_rectangle([bx, top, bx + box, top + box], radius=int(box * 0.28), fill=ACCENT)
            d.line([bx + int(box * 0.24), cy + int(box * 0.02),
                    bx + int(box * 0.44), cy + int(box * 0.24),
                    bx + int(box * 0.80), cy - int(box * 0.28)], fill=WHITE, width=max(2, int(S * 0.03)), joint="curve")
            d.line([lx0, cy, lx1, cy], fill=LINE_DONE, width=lw)
        else:
            d.rounded_rectangle([bx, top, bx + box, top + box], radius=int(box * 0.28),
                                outline=BOX, width=max(2, int(S * 0.022)))
            d.line([lx0, cy, int(S * (0.80 - 0.06 * i)), cy], fill=LINE, width=lw)

    return img.resize((size, size), Image.LANCZOS)


os.makedirs("marketing", exist_ok=True)
for sz in (80, 144):
    render(sz).save("marketing/icon-%d.png" % sz)
    print("wrote marketing/icon-%d.png" % sz)
