#!/usr/bin/env python3
"""Generates the 25x25 launcher menu icon: a checklist card with a check mark.
Rendered at 4x and downsampled for anti-aliasing. Dark ink on transparent so it
reads on the white launcher (same idea as PebbleKuma's outlined icon)."""
import os
from PIL import Image, ImageDraw

SS = 4
W = H = 25
img = Image.new("RGBA", (W * SS, H * SS), (0, 0, 0, 0))
d = ImageDraw.Draw(img)

INK = (40, 40, 40, 255)      # near-black, visible on white launcher
ACCENT = (228, 67, 50, 255)  # Todoist-ish red for the check

def s(v):
    return int(round(v * SS))

# Rounded "card" outline.
d.rounded_rectangle([s(3), s(2), s(22), s(23)], radius=s(3), outline=INK, width=s(1.4))

# Three list lines.
for y in (7, 12, 17):
    d.line([s(7), s(y), s(18), s(y)], fill=INK, width=s(1.4))

# A check mark over the card (accent), bottom-right.
d.line([s(13), s(18), s(16), s(21)], fill=ACCENT, width=s(2.2))
d.line([s(16), s(21), s(22), s(13)], fill=ACCENT, width=s(2.2))

out = img.resize((W, H), Image.LANCZOS)
os.makedirs("resources/images", exist_ok=True)
out.save("resources/images/menu_icon.png")
print("wrote resources/images/menu_icon.png")
