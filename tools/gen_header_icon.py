#!/usr/bin/env python3
"""Generates the 22x22 top-bar icon: a monochrome (black) checklist card with a
check mark. Unlike the launcher icon, the check is black too — the header bar is
filled in the red accent, where a red check would disappear. Black ink reads
cleanly on the accent (same black-on-red choice used for the app's text).
Rendered at 4x and downsampled for anti-aliasing, on a transparent background."""
import os
from PIL import Image, ImageDraw

SS = 4
W = H = 22
img = Image.new("RGBA", (W * SS, H * SS), (0, 0, 0, 0))
d = ImageDraw.Draw(img)

INK = (0, 0, 0, 255)  # pure black, reads on the red accent bar

def s(v):
    return int(round(v * SS))

# Rounded "card" outline.
d.rounded_rectangle([s(2), s(2), s(19), s(20)], radius=s(3), outline=INK, width=s(1.4))

# Three list lines.
for y in (7, 11, 15):
    d.line([s(6), s(y), s(15), s(y)], fill=INK, width=s(1.4))

# A check mark over the card (black), bottom-right.
d.line([s(11), s(16), s(14), s(19)], fill=INK, width=s(2.2))
d.line([s(14), s(19), s(19), s(11)], fill=INK, width=s(2.2))

out = img.resize((W, H), Image.LANCZOS)
os.makedirs("resources/images", exist_ok=True)
out.save("resources/images/header_icon.png")
print("wrote resources/images/header_icon.png")
