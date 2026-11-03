#!/usr/bin/env python3
# Generate the installer artwork: the WiX UI banner and dialog bitmaps and the
# product icon used by ARP and the Start Menu shortcut.
#
#   python3 data/msi/gen-artwork.py
#
# Replaces banner.bmp, dialog.bmp and msitools.ico in this directory.

import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).resolve().parent

BACKGROUND = (28, 32, 46)
ACCENT = (166, 38, 57)
FOREGROUND = (255, 255, 255)

FONTS = (
    "/usr/share/fonts/google-droid/DroidSans-Bold.ttf",
    "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans-Bold.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
)


def font(size):
    for path in FONTS:
        try:
            return ImageFont.truetype(path, size)
        except OSError:
            continue
    return ImageFont.load_default()


def centre(draw, box, text, size, fill=FOREGROUND):
    fnt = font(size)
    x0, y0, x1, y1 = box
    left, top, right, bottom = draw.textbbox((0, 0), text, font=fnt)
    draw.text((x0 + (x1 - x0 - (right - left)) / 2 - left,
               y0 + (y1 - y0 - (bottom - top)) / 2 - top),
              text, font=fnt, fill=fill)


def save_bitmap(image, path):
    # Windows Installer wants an uncompressed bitmap with no alpha channel;
    # an 8 bit palette keeps the package small.
    image.convert("RGB").convert("P", palette=Image.ADAPTIVE, colors=256).save(
        path, format="BMP", dpi=(96, 96))


def banner():
    # 493x58, drawn scaled into the 370x44 header strip. WixUI paints the
    # dialog Title and Description over the left part, so keep it light and
    # put the artwork on the right.
    image = Image.new("RGB", (493, 58), FOREGROUND)
    draw = ImageDraw.Draw(image)
    draw.rectangle([425, 0, 492, 57], fill=ACCENT)
    centre(draw, (425, 0, 492, 57), "msi", 26)
    save_bitmap(image, HERE / "banner.bmp")


def dialog():
    # 493x312, drawn scaled into 370x234 on the exit and error dialogs, whose
    # text starts at x=135 (about x=180 here).
    image = Image.new("RGB", (493, 312), FOREGROUND)
    draw = ImageDraw.Draw(image)
    draw.rectangle([0, 0, 170, 312], fill=BACKGROUND)
    draw.rectangle([170, 0, 178, 312], fill=ACCENT)
    centre(draw, (0, 150, 170, 190), "msi", 64)
    centre(draw, (0, 210, 170, 240), "tools", 36)
    save_bitmap(image, HERE / "dialog.bmp")


def icon():
    image = Image.new("RGBA", (256, 256), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.rounded_rectangle([8, 8, 248, 248], radius=40, fill=ACCENT)
    centre(draw, (0, 60, 256, 200), "msi", 120)
    # uncompressed entries for every size: Windows Installer stores the icon
    # as is and older shells cannot read PNG compressed entries
    image.save(HERE / "msitools.ico",
               sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (256, 256)],
               bitmap_format="bmp")


def main():
    banner()
    dialog()
    icon()
    print("wrote banner.bmp, dialog.bmp and msitools.ico to", HERE)
    return 0


if __name__ == "__main__":
    sys.exit(main())
