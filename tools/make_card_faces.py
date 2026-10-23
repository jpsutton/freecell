#!/usr/bin/env python3
"""Build the 52 card faces, as SVG, for each card style in assets/faces/.

The faces follow Windows XP FreeCell: the same 71x96 layout, pip grid, corner
index and flat colors, but drawn as vectors so they stay sharp at any size.

- Index: 2-unit monoline glyphs modeled on the XP bitmap font.
- Pips: smooth versions of the XP suit shapes.
- Courts: the Byron Knoll art in assets/cards/ (only its four flat color
  layers), recolored to the style's palette and clipped into an XP-style frame.
  Jacks and queens are mirrored, as in XP.

No dependencies beyond Python 3. Run from anywhere:
    python3 tools/make_card_faces.py
"""
import colorsys
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BYRON = ROOT / 'assets' / 'cards'
OUT = ROOT / 'assets' / 'faces'

VARIANTS = {
    'faithful': dict(red='#FF0000', black='#000000', stroke=2.0, border=1.0, radius=3,
                     pal={'red': '#FF0000', 'yellow': '#FFFF00', 'blue': '#0000FF',
                          'black': '#000000', 'white': '#FFFFFF'}),
    'refined': dict(red='#D40019', black='#111111', stroke=2.0, border=0.75, radius=3.5,
                    pal={'red': '#D40019', 'yellow': '#F6CB16', 'blue': '#1C47B8',
                         'black': '#111111', 'white': '#FFFFFF'}),
}
V = None  # the style being built

# Suit pips in a 15x15 box (XP pip size), smooth versions of the XP silhouettes.
PIP = {
    'S': 'M7.5 0C9 2.3 15 5.6 15 9.3C15 11.7 13.3 13.1 11.4 13.1C10 13.1 8.9 12.4 8.3 11.4'
         'C8.4 12.9 9.2 14.2 10.5 15H4.5C5.8 14.2 6.6 12.9 6.7 11.4C6.1 12.4 5 13.1 3.6 13.1'
         'C1.7 13.1 0 11.7 0 9.3C0 5.6 6 2.3 7.5 0Z',
    'H': 'M7.5 15C5.6 12 0 8.6 0 4.3C0 1.8 1.8 0 4.1 0C5.7 0 6.9 0.9 7.5 2.4'
         'C8.1 0.9 9.3 0 10.9 0C13.2 0 15 1.8 15 4.3C15 8.6 9.4 12 7.5 15Z',
    'D': 'M7.5 0L13.5 7.5L7.5 15L1.5 7.5Z',
    'C': 'M7.5 0A3.6 3.6 0 1 1 7.5 7.2A3.6 3.6 0 1 1 7.5 0Z'
         'M3.7 5.6A3.7 3.7 0 1 1 3.7 13A3.7 3.7 0 1 1 3.7 5.6Z'
         'M11.3 5.6A3.7 3.7 0 1 1 11.3 13A3.7 3.7 0 1 1 11.3 5.6Z'
         'M5.6 6.4H9.4V10.4H5.6Z'
         'M6.7 9.4C6.6 12.2 5.8 13.9 4.4 15H10.6C9.2 13.9 8.4 12.2 8.3 9.4Z',
}

# Index glyphs: centerlines for a 2-unit monoline, in card units, drawn on
# the same boxes as the XP bitmap font (y 4..16).
SIX = ('M9.6 5.6C9.1 5.2 8.4 5 7.6 5C5.8 5 5 6.4 5 9V12C5 14 6 15 7.5 15'
       'C9 15 10 14 10 12C10 10.2 9 9.2 7.5 9.2C6 9.2 5 10 5 11.4')
GLYPH = {
    'A': '<path d="M4.6 15L8.5 4.8L12.4 15M5.9 11.6H11.1M3 15H6.4M10.6 15H14"/>',
    '2': '<path d="M5 7.2C5 5.7 6 5 7.5 5C9 5 10 5.8 10 7.2C10 8.6 9.2 9.4 8.2 10.4L5 13.6V15H11"/>',
    '3': '<path d="M4.6 5H10L7 9C9 9 10 10.1 10 12C10 14 9 15 7.5 15C6 15 5.1 14.3 4.8 13.2"/>',
    '4': '<path d="M8.6 16V5.2L4 11.8V12H12"/>',
    '5': '<path d="M10.2 5H5.4L5.1 9.4C5.8 8.8 6.6 8.6 7.5 8.6C9.2 8.6 10 9.8 10 11.8'
         'C10 13.9 9 15 7.5 15C6 15 5.1 14.2 4.9 13.2"/>',
    '6': f'<path d="{SIX}"/>',
    '7': '<path d="M4.6 5H10V5.6C8.6 8 7.4 10.4 7.2 16"/>',
    '8': '<ellipse cx="7.5" cy="7.3" rx="2.3" ry="2.3"/><ellipse cx="7.5" cy="12.3" rx="2.5" ry="2.7"/>',
    '9': f'<path d="{SIX}" transform="rotate(180 7.5 10)"/>',
    '10': '<path d="M4 4V16"/><rect x="7" y="5" width="4" height="10" rx="1.8"/>',
    'J': '<path d="M5 5H9.4M8.4 5V12.6C8.4 14.2 7.5 15 6.2 15C4.9 15 4 14.2 4 12.8"/>',
    'Q': '<rect x="4" y="5" width="5" height="10" rx="2.2"/><path d="M7 12.6L9.8 16"/>',
    'K': '<path d="M3.5 4V16M2 5H5.6M7.4 5H11M2 15H5.6M7.4 15H11M9 5.2L4.2 10.4M5.8 8.8L9.4 14.8"/>',
}

L, M, R = 15, 28, 41
MID = 40.5  # centered row: (96 - 15) / 2
LAYOUT = {
    'A': [(M, MID, 0)], '2': [(M, 10, 1)], '3': [(M, 10, 1), (M, MID, 0)],
    '4': [(L, 10, 1), (R, 10, 1)],
}
LAYOUT['5'] = LAYOUT['4'] + [(M, MID, 0)]
LAYOUT['6'] = LAYOUT['4'] + [(L, MID, 0), (R, MID, 0)]
LAYOUT['7'] = LAYOUT['6'] + [(M, 25, 0)]
LAYOUT['8'] = LAYOUT['6'] + [(M, 25, 1)]
LAYOUT['9'] = [(L, 10, 1), (R, 10, 1), (L, 30, 1), (R, 30, 1), (M, MID, 0)]
LAYOUT['10'] = [(L, 10, 1), (R, 10, 1), (L, 30, 1), (R, 30, 1), (M, 20, 1)]

# Byron inner court frame in his 167.087 x 242.667 user units.
ART_X, ART_Y, ART_W, ART_H = 23.51, 23.47, 120.02, 195.17


def classify(hexc):
    r, g, b = (int(hexc[i:i + 2], 16) / 255 for i in (1, 3, 5))
    h, l, s = colorsys.rgb_to_hls(r, g, b)
    if l > 0.85:
        return 'white'
    if l < 0.18 or (s < 0.25 and l < 0.5):
        return 'black'
    if s < 0.25:
        return 'white'
    h *= 360
    if h < 20 or h >= 330:
        return 'red'
    if h < 160:
        return 'yellow'
    return 'blue'


def court_art(rank, suit):
    src = (BYRON / f'{rank}{suit}.svg').read_text()
    body = src[src.index('>', src.index('<svg')) + 1:src.rindex('</svg>')]
    body = re.sub(r'<metadata.*?</metadata>', '', body, flags=re.S)
    body = re.sub(r'<sodipodi:namedview.*?(/>|</sodipodi:namedview>)', '', body, flags=re.S)
    body = re.sub(r'\s(?:inkscape|sodipodi):[\w-]+="[^"]*"', '', body)
    body = re.sub(r'\sd="[^"]*"', lambda m: re.sub(r'(\d+\.\d)\d+', r'\1', m.group(0)), body)
    body = re.sub(r'#[0-9a-fA-F]{6}\b', lambda m: V['pal'][classify(m.group(0))], body)
    s = 45 / ART_W
    tx, ty = 13 - ART_X * s, 12 - ART_Y * s - (ART_H * s - 72) / 2
    # XP's jacks and queens face the other way (suit pip top-right in the frame).
    mirror = 'translate(71 0) scale(-1 1) ' if rank in 'JQ' else ''
    # Qt SVG ignores clipPath, so instead of clipping, paint the card white
    # everywhere outside the art box (card shape minus art box, even-odd).
    r = V['radius']
    mask = (f'M{r} 0H{71 - r}A{r} {r} 0 0 1 71 {r}V{96 - r}A{r} {r} 0 0 1 {71 - r} 96'
            f'H{r}A{r} {r} 0 0 1 0 {96 - r}V{r}A{r} {r} 0 0 1 {r} 0Z'
            f'M13 12H58V84H13Z')
    return (f'<g transform="{mirror}translate({tx:.4f} {ty:.4f}) scale({s:.6f})">{body}</g>'
            f'<path fill="#FFFFFF" fill-rule="evenodd" d="{mask}"/>')


def pip(suit, x, y, scale=1.0, flip=False):
    t = f'translate({x} {y}) scale({scale})'
    if flip:
        t = f'translate({x} {y + 15 * scale}) scale({scale} {-scale})'
    return f'<path transform="{t}" d="{PIP[suit]}"/>'


def corner(rank, suit, court):
    dy = -1 if court else 0
    g = (f'<g fill="none" stroke-width="{V["stroke"]}" stroke-linejoin="round">{GLYPH[rank]}</g>'
         f'<g stroke="none">{pip(suit, 7.5 - 4.8, 18.6 + dy, 0.64)}</g>')
    return g + f'<g transform="rotate(180 35.5 48)">{g}</g>'


def card(rank, suit):
    ink = V['red'] if suit in 'DH' else V['black']
    court = rank in 'JQK'
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" '
           f'viewBox="0 0 71 96" width="71" height="96">',
           f'<rect width="71" height="96" rx="{V["radius"]}" fill="#FFFFFF"/>']
    if court:
        out.append(court_art(rank, suit))
        out.append(f'<rect x="12.5" y="11.5" width="46" height="73" fill="none" '
                   f'stroke="{V["black"]}" stroke-width="1"/>')
    b = V['border']
    out.append(f'<rect x="{b / 2}" y="{b / 2}" width="{71 - b}" height="{96 - b}" '
               f'rx="{V["radius"] - b / 2}" fill="none" stroke="{V["black"]}" stroke-width="{b}"/>')
    out.append(f'<g fill="{ink}" stroke="{ink}">{corner(rank, suit, court)}')
    if not court:
        out.append('<g stroke="none">')
        if rank == 'A' and suit == 'S':
            out.append(pip('S', 35.5 - 18, 48 - 18, 2.4))
        else:
            for x, y, pair in LAYOUT[rank]:
                out.append(pip(suit, x, y))
                if pair:
                    out.append(pip(suit, x, 96 - y - 15, flip=True))
        out.append('</g>')
    out.append('</g></svg>')
    return ''.join(out)


def main():
    global V
    for name, style in VARIANTS.items():
        V = style
        out = OUT / name
        out.mkdir(parents=True, exist_ok=True)
        for r in ['A', '2', '3', '4', '5', '6', '7', '8', '9', '10', 'J', 'Q', 'K']:
            for su in 'CDHS':
                (out / f'{r}{su}.svg').write_text(card(r, su))
        print(f'wrote 52 faces to {out}')


if __name__ == '__main__':
    main()
