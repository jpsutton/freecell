#!/usr/bin/env python3
"""Build the app icon from the Classic king of spades face.

Writes packaging/icons/freecell.svg (the source), freecell-<size>.png,
packaging/macos/FreeCell.icns, packaging/windows/freecell.ico and the
Android adaptive launcher icon under packaging/android/res.

Needs rsvg-convert (librsvg) and Pillow:
    python3 tools/make_icons.py
"""
import re
import subprocess
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
CARD = ROOT / 'assets' / 'faces' / 'faithful' / 'KS.svg'
ICONS = ROOT / 'packaging' / 'icons'
SIZES = [16, 24, 32, 48, 64, 128, 256, 512, 1024]
ANDROID_RES = ROOT / 'packaging' / 'android' / 'res'
# Android launcher densities (scale over mdpi).
DENSITIES = {'mdpi': 1, 'hdpi': 1.5, 'xhdpi': 2, 'xxhdpi': 3, 'xxxhdpi': 4}

# XP felt, as on the board.
FELT = '#007F00'


def card_inner() -> str:
    card = CARD.read_text()
    m = re.match(r'<svg[^>]*>', card)
    return card[m.end():card.rindex('</svg>')]


def icon_svg() -> str:
    inner = card_inner()
    # The card is 71x96; centre it at 84% of the icon's height on a felt tile
    # with a thin dark edge so it reads on light and dark backgrounds.
    h = 430
    w = h * 71 / 96
    x = (512 - w) / 2
    y = (512 - h) / 2
    return (
        '<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" '
        'viewBox="0 0 512 512" width="512" height="512">'
        f'<rect x="8" y="8" width="496" height="496" rx="96" fill="{FELT}" stroke="#004000" stroke-width="8"/>'
        f'<svg x="{x:.2f}" y="{y:.2f}" width="{w:.2f}" height="{h}" viewBox="0 0 71 96">'
        f'{inner}'
        '<rect x="0.5" y="0.5" width="70" height="95" rx="3" fill="none" stroke="#000" stroke-width="1"/>'
        '</svg></svg>'
    )


def android_foreground_svg() -> str:
    # Adaptive icon foreground: 108x108dp, of which launchers may mask all but
    # a 66dp circle. A 52dp-high card keeps its corners inside that circle;
    # the felt is the background layer.
    h = 52
    w = h * 71 / 96
    return (
        '<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" '
        'viewBox="0 0 108 108" width="108" height="108">'
        f'<svg x="{(108 - w) / 2:.2f}" y="{(108 - h) / 2:.2f}" width="{w:.2f}" height="{h}" viewBox="0 0 71 96">'
        f'{card_inner()}'
        '<rect x="0.5" y="0.5" width="70" height="95" rx="3" fill="none" stroke="#000" stroke-width="1"/>'
        '</svg></svg>'
    )


def android_icons() -> None:
    src = ANDROID_RES / 'ic_launcher_foreground.svg'
    (ANDROID_RES / 'values').mkdir(parents=True, exist_ok=True)
    (ANDROID_RES / 'mipmap-anydpi-v26').mkdir(parents=True, exist_ok=True)
    src.write_text(android_foreground_svg())
    for name, scale in DENSITIES.items():
        out = ANDROID_RES / f'mipmap-{name}' / 'ic_launcher_foreground.png'
        out.parent.mkdir(parents=True, exist_ok=True)
        size = str(int(108 * scale))
        subprocess.run(['rsvg-convert', '-w', size, '-h', size, '-o', str(out), str(src)], check=True)
    src.unlink()
    (ANDROID_RES / 'values' / 'ic_launcher_background.xml').write_text(
        '<?xml version="1.0" encoding="utf-8"?>\n<resources>\n'
        f'    <color name="ic_launcher_background">{FELT}</color>\n</resources>\n')
    (ANDROID_RES / 'mipmap-anydpi-v26' / 'ic_launcher.xml').write_text(
        '<?xml version="1.0" encoding="utf-8"?>\n'
        '<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">\n'
        '    <background android:drawable="@color/ic_launcher_background"/>\n'
        '    <foreground android:drawable="@mipmap/ic_launcher_foreground"/>\n'
        '</adaptive-icon>\n')


def main() -> None:
    ICONS.mkdir(parents=True, exist_ok=True)
    src = ICONS / 'freecell.svg'
    src.write_text(icon_svg())
    pngs = {}
    for size in SIZES:
        out = ICONS / f'freecell-{size}.png'
        subprocess.run(['rsvg-convert', '-w', str(size), '-h', str(size), '-o', str(out), str(src)],
                       check=True)
        pngs[size] = out
    big = Image.open(pngs[1024])
    big.save(ROOT / 'packaging' / 'macos' / 'FreeCell.icns')
    Image.open(pngs[256]).save(ROOT / 'packaging' / 'windows' / 'freecell.ico',
                               sizes=[(s, s) for s in (16, 24, 32, 48, 64, 128, 256)])
    android_icons()


if __name__ == '__main__':
    main()
