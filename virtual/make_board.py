#!/usr/bin/env python3
"""Render the front of the keychain board, one file per mask colour.

The silkscreen, pads and board outline are plotted straight from the
gerbers in hardware/fabrication, and the part outlines come from the
board file, so the virtual mode shows the same board as the fabrication
output. Re-run it after re-plotting:

    python3 virtual/make_board.py

Only the geometry the game touches (digits, LEDs, buttons, switch) is
left out here so index.html can draw those itself.
"""

import math
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FAB = ROOT / "hardware" / "fabrication"
PCB = ROOT / "hardware" / "blackjack-keychain.kicad_pcb"
OUT = ROOT / "virtual"

SILK = "#e9e3d1"
GOLD = "#c9a25d"
HOLE = "#0a0f13"
EDGE = "#0a0f13"

# solder mask: lightest tone, body tone, darkest tone
MASKS = {
    "blue": ("#20588c", "#15406a", "#0b2743"),
    "red": ("#a22a2e", "#7a1c21", "#480f13"),
}

LED_COLORS = {
    "pair": "#ffc247",
    "soft": "#4fd6ff",
    "win": "#5ce07a",
    "push": "#ffd166",
    "lose": "#ff5a4d",
    "right": "#5ce07a",
    "wrong": "#ff5a4d",
}


def mm(value, places=3):
    text = f"{value:.{places}f}".rstrip("0").rstrip(".")
    return "0" if text in ("", "-0") else text


# --------------------------------------------------------------------------
# gerber
# --------------------------------------------------------------------------

TOKEN = re.compile(r"%([^%]*)%|([^%*]+)\*", re.S)


def _tokens(text):
    for match in TOKEN.finditer(text):
        if match.group(1) is not None:
            yield True, match.group(1)
        else:
            yield False, match.group(2).strip()


def _rotate(points, degrees):
    a = math.radians(degrees)
    ca, sa = math.cos(a), math.sin(a)
    return [(x * ca - y * sa, x * sa + y * ca) for x, y in points]


def _value(expr, params):
    expr = re.sub(r"\$(\d+)", lambda m: repr(params[int(m.group(1)) - 1]), expr)
    if not re.fullmatch(r"[-\d.+*/() ]+", expr):
        return 0.0
    return float(eval(expr, {"__builtins__": {}}, {}))  # arithmetic only


def _arc(x0, y0, x1, y1, i, j, clockwise):
    """Flatten a gerber arc (I/J centre offsets) into points, gerber axes."""
    cx, cy = x0 + i, y0 + j
    a0 = math.atan2(y0 - cy, x0 - cx)
    a1 = math.atan2(y1 - cy, x1 - cx)
    sweep = a1 - a0
    if clockwise:
        while sweep >= 0:
            sweep -= 2 * math.pi
    else:
        while sweep <= 0:
            sweep += 2 * math.pi
    radius = math.hypot(x0 - cx, y0 - cy)
    steps = max(2, int(abs(sweep) / (math.pi / 32)) + 1)
    return [
        (cx + radius * math.cos(a0 + sweep * k / steps),
         cy + radius * math.sin(a0 + sweep * k / steps))
        for k in range(1, steps + 1)
    ]


class Gerber:
    """Just enough RS-274X for KiCad plots: round apertures, aperture
    macros, linear draws, arcs and G36 regions."""

    def __init__(self, path):
        self.macros = {}
        self.apertures = {}
        self.flashes = []
        self.strokes = []
        self.regions = []
        self.x = self.y = 0.0
        self.interp = 1
        self.region = False
        self.contour = []
        self.run = []
        self.ap = None
        for extended, chunk in _tokens(Path(path).read_text()):
            if extended:
                self._extended(chunk)
            else:
                self._command(chunk)
        self._flush()

    def _extended(self, block):
        lines = [line.strip() for line in block.split("*") if line.strip()]
        if not lines:
            return
        if lines[0].startswith("AM"):
            self.macros[lines[0][2:]] = lines[1:]
            return
        for line in lines:
            match = re.match(r"ADD(\d+)([A-Za-z_]\w*)(?:,(.*))?$", line)
            if match:
                code = int(match.group(1))
                params = [float(v) for v in match.group(3).split("X")] if match.group(3) else []
                self.apertures[code] = (match.group(2), params)

    def _command(self, cmd):
        if not cmd or cmd.startswith(("G04", "M02")):
            return

        for code in re.findall(r"G(\d+)", cmd):
            code = int(code)
            if code in (1, 2, 3):
                self.interp = code
            elif code == 36:
                self.region, self.contour = True, []
            elif code == 37:
                if len(self.contour) > 2:
                    self.regions.append(self.contour)
                self.region, self.contour = False, []

        match = re.fullmatch(r"D(\d+)", cmd)
        if match and int(match.group(1)) >= 10:
            self._flush()
            self.ap = int(match.group(1))
            return

        words = dict(re.findall(r"([XYIJ])(-?\d+)", cmd))
        op = re.search(r"D0?([123])$", cmd)
        if not op:
            return
        op = int(op.group(1))

        nx = int(words["X"]) / 1e6 if "X" in words else self.x
        ny = int(words["Y"]) / 1e6 if "Y" in words else self.y

        if op == 3:
            self.flashes.append((nx, ny, self.ap))
        elif op == 2:
            # start a new run here; a move onto the current point continues
            # the run, which keeps split arcs (the keychain hole) together
            if not self.run or self.run[-1] != (nx, ny):
                self._flush()
                self.run = [(nx, ny)]
        elif op == 1:
            if self.interp == 1:
                points = [(nx, ny)]
            else:
                i = int(words.get("I", 0)) / 1e6
                j = int(words.get("J", 0)) / 1e6
                points = _arc(self.x, self.y, nx, ny, i, j, self.interp == 2)
            if self.region:
                self.contour.extend(points)
            else:
                self.run.extend(points)

        self.x, self.y = nx, ny

    def _flush(self):
        if len(self.run) > 1:
            self.strokes.append((self.run, self.width()))
        self.run = []

    def width(self):
        aperture = self.apertures.get(self.ap)
        if aperture and aperture[0] == "C" and aperture[1]:
            return aperture[1][0]
        return 0.12


def _primitive(code, v):
    """One aperture-macro primitive, in board coordinates about the origin."""
    if code == 1 and len(v) >= 4:                      # circle
        rotation = v[4] if len(v) > 4 else 0
        (cx, cy), = _rotate([(v[2], -v[3])], rotation)
        return f'<circle cx="{mm(cx)}" cy="{mm(cy)}" r="{mm(v[1] / 2)}"/>'

    if code == 4 and len(v) >= 2:                      # outline
        n = int(v[1])
        if len(v) < 2 + 2 * n:
            return ""
        points = [(v[2 + 2 * k], -v[3 + 2 * k]) for k in range(n)]
        # rotation is the last parameter: KiCad repeats the first vertex
        # between the vertices and the angle (the chamfered ISP pad)
        rotation = v[-1] if len(v) > 2 + 2 * n else 0
        points = _rotate(points, rotation)
        return '<polygon points="' + " ".join(f"{mm(x)},{mm(y)}" for x, y in points) + '"/>'

    if code == 5 and len(v) >= 5:                      # regular polygon
        n = int(v[1])
        rotation = v[5] if len(v) > 5 else 0
        points = [
            (v[2] + v[4] / 2 * math.cos(2 * math.pi * k / n),
             v[3] + v[4] / 2 * math.sin(2 * math.pi * k / n))
            for k in range(n)
        ]
        points = _rotate([(x, -y) for x, y in points], rotation)
        return '<polygon points="' + " ".join(f"{mm(x)},{mm(y)}" for x, y in points) + '"/>'

    if code == 20 and len(v) >= 6:                     # vector line
        rotation = v[6] if len(v) > 6 else 0
        (x0, y0), (x1, y1) = _rotate([(v[2], -v[3]), (v[4], -v[5])], rotation)
        return (f'<line x1="{mm(x0)}" y1="{mm(y0)}" x2="{mm(x1)}" y2="{mm(y1)}" '
                f'stroke-width="{mm(v[1])}" stroke-linecap="round"/>')

    if code == 21 and len(v) >= 4:                     # centre line
        rotation = v[5] if len(v) > 5 else 0
        w, h = v[1], v[2]
        points = _rotate([(v[3] - w / 2, -(v[4] - h / 2)), (v[3] + w / 2, -(v[4] - h / 2)),
                          (v[3] + w / 2, -(v[4] + h / 2)), (v[3] - w / 2, -(v[4] + h / 2))],
                         rotation)
        return '<polygon points="' + " ".join(f"{mm(x)},{mm(y)}" for x, y in points) + '"/>'

    return ""


def _macro_shape(lines, params):
    parts = []
    for line in lines:
        fields = [field.strip() for field in line.split(",")]
        try:
            code = int(fields[0])
        except ValueError:
            continue
        parts.append(_primitive(code, [_value(field, params) for field in fields[1:]]))
    return "".join(parts)


def _aperture(gerber, code):
    name, params = gerber.apertures[code]
    if name == "C":
        return f'<circle r="{mm(params[0] / 2)}"/>'
    if name == "R":
        w, h = params[0], params[1]
        return f'<rect x="{mm(-w / 2)}" y="{mm(-h / 2)}" width="{mm(w)}" height="{mm(h)}"/>'
    if name == "O":
        w, h = params[0], params[1]
        return (f'<rect x="{mm(-w / 2)}" y="{mm(-h / 2)}" width="{mm(w)}" height="{mm(h)}" '
                f'rx="{mm(min(w, h) / 2)}"/>')
    return _macro_shape(gerber.macros.get(name, []), params)


# --------------------------------------------------------------------------
# board file: part outlines
# --------------------------------------------------------------------------

def _forms(text, names):
    """Balanced (fp_<name> ...) forms, since they nest arbitrarily."""
    found = []
    for match in re.finditer(r"\(fp_(?:" + "|".join(names) + r")\b", text):
        depth = 0
        for index in range(match.start(), len(text)):
            char = text[index]
            if char == "(":
                depth += 1
            elif char == ")":
                depth -= 1
                if depth == 0:
                    found.append(text[match.start():index + 1])
                    break
    return found


def _fab_box(block):
    xs, ys = [], []
    for form in _forms(block, ("rect", "line", "poly")):
        if "F.Fab" not in form:
            continue
        for x, y in re.findall(r"\((?:start|end|xy)\s+(-?[\d.]+)\s+(-?[\d.]+)\)", form):
            xs.append(float(x))
            ys.append(float(y))
    if not xs:
        return None
    return min(xs), min(ys), max(xs), max(ys)


def footprints():
    text = PCB.read_text()
    starts = [m.start() for m in re.finditer(r"^\t\(footprint ", text, re.M)]
    parts = []
    for index, start in enumerate(starts):
        end = starts[index + 1] if index + 1 < len(starts) else len(text)
        block = text[start:end]
        at = re.search(r"\(at (-?[\d.]+) (-?[\d.]+)(?: (-?[\d.]+))?\)", block)
        layer = re.search(r'\(layer "([^"]+)"\)', block)
        ref = re.search(r'\(property "Reference" "([^"]*)"', block)
        value = re.search(r'\(property "Value" "([^"]*)"', block)
        parts.append({
            "name": re.match(r'\t\(footprint "([^"]+)"', block).group(1),
            "x": float(at.group(1)),
            "y": float(at.group(2)),
            "rot": float(at.group(3) or 0),
            "layer": layer.group(1),
            "ref": ref.group(1) if ref else "",
            "value": value.group(1) if value else "",
            "box": _fab_box(block),
        })
    return parts


# --------------------------------------------------------------------------
# parts
# --------------------------------------------------------------------------

def _rect(x0, y0, x1, y1, rx=0.0, fill="#000", extra=""):
    return (f'<rect x="{mm(x0)}" y="{mm(y0)}" width="{mm(x1 - x0)}" height="{mm(y1 - y0)}" '
            f'rx="{mm(rx)}" fill="{fill}" {extra}/>')


def display(x0, y0, x1, y1):
    inset = 0.7
    return "".join([
        _rect(x0, y0, x1, y1, 0.5, "#0e1013"),
        _rect(x0 + inset, y0 + inset, x1 - inset, y1 - inset, 0.25, "#05080a"),
        _rect(x0 + 0.4, y0 + 0.12, x1 - 0.4, y0 + 0.3, 0.09, "#343a41", 'opacity="0.75"'),
    ])


def led(x0, y0, x1, y1, value):
    color = LED_COLORS.get(value, "#ffffff")
    return "".join([
        _rect(x0, y0, x1, y1, 0.25, "#d8d2c1"),
        _rect(x0 + 0.4, y0 + 0.4, x1 - 0.4, y1 - 0.4, 0.15, color, 'opacity="0.32"'),
        _rect(x0, y0, x1, y1, 0.25, "none", 'stroke="#00000050" stroke-width="0.07"'),
    ])


def slide_switch(x0, y0, x1, y1):
    return "".join([
        _rect(x0, y0, x1, y1, 0.25, "#15171a"),
        _rect(x0 + 0.35, y0 + 0.45, x1 - 0.35, y1 - 0.45, 0.2, "url(#plate)"),
        _rect(x0, y0, x1, y1, 0.25, "none", 'stroke="#00000075" stroke-width="0.07"'),
    ])


def usb_socket(x0, y0, x1, y1):
    return "".join([
        _rect(x0, y0, x1, y1, 0.7, "url(#metal)"),
        _rect(x0 + 0.4, y1 - 1.7, x1 - 0.4, y1 - 0.3, 0.35, "#0a0c0d"),
        _rect(x0, y0, x1, y1, 0.7, "none", 'stroke="#00000055" stroke-width="0.08"'),
    ])


def chip(x0, y0, x1, y1):
    return "".join([
        _rect(x0, y0, x1, y1, 0.15, "#15171a"),
        f'<circle cx="{mm(x0 + 0.95)}" cy="{mm(y0 + 0.95)}" r="0.3" fill="#3c434b"/>',
        _rect(x0, y0, x1, y1, 0.15, "none", 'stroke="#00000070" stroke-width="0.08"'),
    ])


def sot(x0, y0, x1, y1):
    return "".join([
        _rect(x0, y0, x1, y1, 0.12, "#191b1e"),
        _rect(x0, y0, x1, y1, 0.12, "none", 'stroke="#00000070" stroke-width="0.07"'),
    ])


def passive(x0, y0, x1, y1, ceramic):
    width = x1 - x0
    cap = min(0.42, width * 0.24)
    body = "#bda87c" if ceramic else "#1b1d20"
    return "".join([
        _rect(x0, y0, x1, y1, 0.1, body),
        _rect(x0, y0, x0 + cap, y1, 0.06, "#b6b2a6"),
        _rect(x1 - cap, y0, x1, y1, 0.06, "#b6b2a6"),
    ])


def part(entry):
    if entry["layer"] != "F.Cu" or entry["box"] is None:
        return ""
    name = entry["name"]
    x0, y0, x1, y1 = entry["box"]

    if "7_Segment" in name:
        body = display(x0, y0, x1, y1)
    elif "LED_1210" in name:
        body = led(x0, y0, x1, y1, entry["value"])
    elif "SW_Tactile" in name:
        return ""                       # drawn by the page, so it can be pressed
    elif "SW_SPDT" in name:
        body = slide_switch(x0, y0, x1, y1)
    elif "USB_C" in name:
        body = usb_socket(x0, y0, x1, y1)
    elif "TQFP" in name:
        body = chip(x0, y0, x1, y1)
    elif "SOT-23" in name:
        body = sot(x0, y0, x1, y1)
    elif "_0805_" in name:                    # resistors and capacitors
        body = passive(x0, y0, x1, y1, "C_0805" in name)
    else:
        return ""

    return (f'<g transform="translate({mm(entry["x"])} {mm(entry["y"])}) '
            f'rotate({mm(-entry["rot"], 1)})">{body}</g>')


# --------------------------------------------------------------------------
# drills
# --------------------------------------------------------------------------

def drills(path):
    holes = []
    tools = {}
    tool = None
    for line in Path(path).read_text().splitlines():
        line = line.strip()
        match = re.fullmatch(r"T(\d+)C([\d.]+)", line)
        if match:
            tools[int(match.group(1))] = float(match.group(2))
            continue
        match = re.fullmatch(r"T(\d+)", line)
        if match:
            tool = int(match.group(1))
            continue
        match = re.fullmatch(r"X(-?[\d.]+)Y(-?[\d.]+)", line)
        if match and tool in tools:
            holes.append((float(match.group(1)), -float(match.group(2)), tools[tool]))
    return holes


# --------------------------------------------------------------------------
# build
# --------------------------------------------------------------------------

def _path(strokes, close=False):
    runs = []
    for points, _width in strokes:
        d = "M" + " L".join(f"{mm(x)} {mm(-y)}" for x, y in points)
        runs.append(d + (" Z" if close else ""))
    return " ".join(runs)


def build(finish):
    high, mid, low = MASKS[finish]
    edge = Gerber(FAB / "blackjack-keychain-Edge_Cuts.gbr")
    silk = Gerber(FAB / "blackjack-keychain-F_Silkscreen.gbr")
    mask = Gerber(FAB / "blackjack-keychain-F_Mask.gbr")

    outline = _path(edge.strokes, close=True)
    xs = [x for run, _w in edge.strokes for x, _y in run]
    ys = [-y for run, _w in edge.strokes for _x, y in run]
    left, top = min(xs), min(ys)
    width, height = max(xs) - left, max(ys) - top

    # silk: one path per stroke width, plus filled regions (logos, arrows)
    silk_paths = []
    by_width = {}
    for points, w in silk.strokes:
        by_width.setdefault(round(w, 3), []).append((points, w))
    for w in sorted(by_width):
        silk_paths.append(
            f'<path d="{_path(by_width[w])}" fill="none" stroke="{SILK}" '
            f'stroke-width="{mm(w)}" stroke-linecap="round" stroke-linejoin="round"/>')
    for contour in silk.regions:
        d = "M" + " L".join(f"{mm(x)} {mm(-y)}" for x, y in contour) + " Z"
        silk_paths.append(f'<path d="{d}" fill="{SILK}"/>')

    # mask openings are the exposed pads
    codes = sorted({code for _x, _y, code in mask.flashes if code})
    pads = "".join(
        f'<g id="pad{code}" fill="{GOLD}" stroke="{GOLD}" stroke-width="0">{_aperture(mask, code)}</g>'
        for code in codes)
    pad_uses = "".join(
        f'<use href="#pad{code}" x="{mm(x)}" y="{mm(-y)}"/>' for x, y, code in mask.flashes if code)

    holes = "".join(
        f'<circle cx="{mm(x)}" cy="{mm(y)}" r="{mm(d / 2)}" fill="{HOLE}"/>'
        for path in (FAB / "blackjack-keychain-PTH.drl", FAB / "blackjack-keychain-NPTH.drl")
        for x, y, d in drills(path))

    parts = "".join(part(entry) for entry in footprints())

    return f'''<svg xmlns="http://www.w3.org/2000/svg" viewBox="{mm(left)} {mm(top)} {mm(width)} {mm(height)}" width="500" height="700">
<!-- front of the blackjack keychain, generated by virtual/make_board.py -->
<defs>
<linearGradient id="board" x1="0.1" y1="0" x2="0.55" y2="1">
<stop offset="0" stop-color="{high}"/><stop offset="0.55" stop-color="{mid}"/><stop offset="1" stop-color="{low}"/>
</linearGradient>
<linearGradient id="metal" x1="0" y1="0" x2="0" y2="1">
<stop offset="0" stop-color="#efeee7"/><stop offset="0.45" stop-color="#c3c1b6"/><stop offset="1" stop-color="#8c8a80"/>
</linearGradient>
<linearGradient id="plate" x1="0" y1="0" x2="0" y2="1">
<stop offset="0" stop-color="#b6b3a8"/><stop offset="1" stop-color="#6d6b63"/>
</linearGradient>
<clipPath id="board-clip"><path d="{outline}" clip-rule="evenodd"/></clipPath>
{pads}
</defs>
<path d="{outline}" fill="url(#board)" fill-rule="evenodd"/>
<g clip-path="url(#board-clip)">
{pad_uses}
{holes}
{''.join(silk_paths)}
{parts}
</g>
<path d="{outline}" fill="none" stroke="{EDGE}" stroke-width="0.14"/>
</svg>
'''


if __name__ == "__main__":
    for finish in MASKS:
        path = OUT / f"board-{finish}.svg"
        board = build(finish)
        path.write_text(board)
        print(f"wrote {path.relative_to(ROOT)} ({len(board) / 1024:.0f} kB)")
