#!/usr/bin/env python3
"""Figure 3 — circuit schematic, generated to match refractometer.ino exactly.

Pin map taken verbatim from the sketch header:
  D2  measure button (INPUT_PULLUP)      D8-D11  stepper coils -> ULN2003
  D3  mode rocker    (INPUT_PULLUP)      D12,D13 keypad cols 1,2
  D4-D7 keypad rows                      A0 LDR   A1 keypad col 3
  A4,A5 I2C to LCD                       A2,A3 unused
"""

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, Circle, FancyBboxPatch

INK, GREY = "#111111", "#5F6670"
FS, FL, FT = 8.4, 9.6, 7.4
LW = 1.15
MONO = "DejaVu Sans Mono"

fig, ax = plt.subplots(figsize=(11.4, 7.4), dpi=300)
ax.set_xlim(0, 122); ax.set_ylim(0, 78); ax.axis("off"); ax.set_aspect("equal")
fig.patch.set_facecolor("white")

RAIL5, RAILG = 72.0, 6.0


def box(x, y, w, h, title, sub=None, fs=FL):
    ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0,rounding_size=0.8",
                                fc="white", ec=INK, lw=LW * 1.3, zorder=3))
    ty = y + h / 2 + (1.4 if sub else 0)
    ax.text(x + w / 2, ty, title, ha="center", va="center", fontsize=fs,
            fontweight="bold", color=INK, zorder=4)
    if sub:
        ax.text(x + w / 2, ty - 2.9, sub, ha="center", va="center",
                fontsize=FT, color=GREY, zorder=4)


def wire(pts, lw=LW, color=INK, ls="-", z=2):
    ax.plot([p[0] for p in pts], [p[1] for p in pts], lw=lw, color=color, ls=ls,
            solid_capstyle="round", solid_joinstyle="round", zorder=z)


def dot(x, y, r=0.45):
    ax.add_patch(Circle((x, y), r, fc=INK, ec="none", zorder=5))


def resistor(x, y, length, n=6, amp=1.6):
    lead = length * 0.20
    body = length - 2 * lead
    pts = [(x, y), (x, y + lead)]
    step = body / (n * 2)
    cy = y + lead
    for i in range(n * 2):
        cy += step
        pts.append((x + (amp if i % 2 == 0 else -amp), cy))
    pts += [(x, y + lead + body), (x, y + length)]
    wire(pts)


def gnd_stub(x, y_from):
    wire([(x, y_from), (x, RAILG)])
    dot(x, RAILG)


def pin(x, y, name, side):
    d = 2.6
    if side == "right":                      # pin on the LEFT edge of the MCU
        wire([(x - d, y), (x, y)])
        ax.text(x + 1.0, y, name, ha="left", va="center", fontsize=FT,
                color=INK, fontfamily=MONO, zorder=6)
    else:
        wire([(x, y), (x + d, y)])
        ax.text(x - 1.0, y, name, ha="right", va="center", fontsize=FT,
                color=INK, fontfamily=MONO, zorder=6)


# =============================================================== rails
wire([(6, RAIL5), (118, RAIL5)], lw=LW * 1.35)
ax.text(5.0, RAIL5, "+5 V", ha="right", va="center", fontsize=FL, color=INK)
wire([(6, RAILG), (118, RAILG)], lw=LW * 1.35)
for i, f in enumerate((3.4, 2.2, 1.0)):
    wire([(6 - f / 2, RAILG - 1.5 - i * 0.95), (6 + f / 2, RAILG - 1.5 - i * 0.95)])
wire([(6, RAILG), (6, RAILG - 1.5)])

# ================================================================= MCU
AX, AY, AW, AH = 40.0, 15.0, 24.0, 53.0
box(AX, AY, AW, AH, "Arduino Uno", "ATmega328P", fs=10.8)
L, R = AX, AX + AW

P_5V, P_A0, P_D2, P_D3, P_GND = 65.0, 55.0, 30.0, 23.0, 18.0
for yy, nm in ((P_5V, "5V"), (P_A0, "A0"), (P_D2, "D2"), (P_D3, "D3"), (P_GND, "GND")):
    pin(L, yy, nm, "right")

P_COIL = [65.0, 62.0, 59.0, 56.0]
P_SDA, P_SCL = 47.0, 44.0
P_ROW = [36.5, 34.0, 31.5, 29.0]
P_COL = [24.5, 22.0, 19.5]
for yy, nm in zip(P_COIL, ("D8", "D9", "D10", "D11")):
    pin(R, yy, nm, "left")
for yy, nm in ((P_SDA, "A4/SDA"), (P_SCL, "A5/SCL")):
    pin(R, yy, nm, "left")
for yy, nm in zip(P_ROW, ("D4", "D5", "D6", "D7")):
    pin(R, yy, nm, "left")
for yy, nm in zip(P_COL, ("D12", "D13", "A1")):
    pin(R, yy, nm, "left")

wire([(L - 2.6, P_5V), (33.0, P_5V), (33.0, RAIL5)]); dot(33.0, RAIL5)
gnd_stub(37.0, P_GND); wire([(L - 2.6, P_GND), (37.0, P_GND)])
ax.text(AX + AW / 2, AY - 2.2, "A2 and A3 unused", ha="center", va="top",
        fontsize=FT, color=GREY, style="italic")

# =============================================== LDR potential divider
LX = 15.0
wire([(LX, RAIL5), (LX, 65.0)]); dot(LX, RAIL5)
resistor(LX, 57.0, 8.0)
for dy in (-1.7, 1.7):
    ax.annotate("", xy=(LX - 3.0, 61.0 + dy), xytext=(LX - 6.6, 63.4 + dy),
                arrowprops=dict(arrowstyle="-|>", lw=0.95, color=INK,
                                mutation_scale=8), zorder=4)
ax.text(LX - 7.4, 62.4, "LDR", ha="right", va="center", fontsize=FS, color=INK)
ax.text(LX - 7.4, 59.8, "on rotating stage", ha="right", va="center",
        fontsize=FT, color=GREY, style="italic")
wire([(LX, 55.0), (LX, 57.0)])
dot(LX, P_A0)
wire([(LX, P_A0), (L - 2.6, P_A0)])
resistor(LX, 44.0, 8.0)
ax.text(LX + 3.4, 48.0, "10 kΩ", ha="left", va="center", fontsize=FS, color=INK)
wire([(LX, 44.0), (LX, RAILG)]); dot(LX, RAILG)

ax.add_patch(Rectangle((LX - 4.6, 66.0), 9.2, 3.2, fc="white", ec=GREY, lw=0.9,
                       ls=(0, (2.6, 2.0)), zorder=4))
ax.text(LX, 67.6, "slip ring", ha="center", va="center", fontsize=FT,
        color=GREY, zorder=5)

# ============================================= momentary + rocker input
def switch(x, ytop, name, note_y):
    wire([(x, ytop), (x, ytop - 2.6)])
    wire([(x - 2.4, ytop - 2.6), (x + 2.4, ytop - 2.6)])
    wire([(x - 1.6, ytop - 4.4), (x + 2.8, ytop - 5.6)])
    wire([(x, ytop - 4.4), (x, RAILG)]); dot(x, RAILG)
    ax.text(x, ytop + 2.2, name, ha="center", va="bottom", fontsize=FS, color=INK)

wire([(22.0, P_D2), (L - 2.6, P_D2)]); switch(22.0, P_D2, "MEASURE", 0)
wire([(32.0, P_D3), (L - 2.6, P_D3)]); switch(32.0, P_D3, "MODE", 0)
ax.text(27.0, 13.0, "internal pull-ups enabled; closed = LOW", ha="center",
        va="top", fontsize=FT, color=GREY, style="italic")

# ====================================================== stepper channel
UX, UY, UW, UH = 74.0, 53.0, 15.0, 15.0
box(UX, UY, UW, UH, "ULN2003", "driver board")
ax.text(UX + UW / 2, UY + UH + 1.2, "unipolar, half-step drive", ha="center",
        va="bottom", fontsize=FT, color=GREY, style="italic")
for i, yy in enumerate(P_COIL):
    iy = UY + UH - 2.6 - i * 3.2
    wire([(R + 2.6, yy), (70.0 - i * 0.9, yy), (70.0 - i * 0.9, iy), (UX, iy)])
    ax.text(UX - 1.2, iy + 1.15, f"IN{i+1}", ha="right", va="bottom", fontsize=FT, color=GREY)
wire([(UX + UW / 2, UY + UH), (UX + UW / 2, RAIL5)]); dot(UX + UW / 2, RAIL5)
wire([(UX + UW / 2, UY), (UX + UW / 2, 51.0), (104.0, 51.0), (104.0, RAILG)])
dot(104.0, RAILG)

MCX, MCY = 99.5, UY + UH / 2
ax.add_patch(Circle((MCX, MCY), 6.0, fc="white", ec=INK, lw=LW * 1.3, zorder=3))
ax.text(MCX, MCY + 1.2, "M", ha="center", va="center", fontsize=12,
        fontweight="bold", color=INK, zorder=4)
ax.text(MCX, MCY - 2.6, "28BYJ-48", ha="center", va="center", fontsize=FT,
        color=GREY, zorder=4)
for i in range(5):
    yy = UY + 2.4 + i * (UH - 4.8) / 4
    wire([(UX + UW, yy), (MCX - 6.0 + 0.4, yy)])


# =============================================================== LCD
CX, CY, CW, CH = 74.0, 41.0, 26.0, 8.0
box(CX, CY, CW, CH, "16 × 2 LCD", "PCF8574 I²C backpack, address 0x27")
wire([(R + 2.6, P_SDA), (70.0, P_SDA), (70.0, CY + CH * 0.68), (CX, CY + CH * 0.68)])
wire([(R + 2.6, P_SCL), (68.6, P_SCL), (68.6, CY + CH * 0.30), (CX, CY + CH * 0.30)])


# ============================================================= keypad
KX, KY, KW, KH = 74.0, 16.0, 16.0, 21.5
box(KX, KY, KW, KH, "4 × 3 keypad", None)
for i, yy in enumerate(P_ROW):
    wire([(R + 2.6, yy), (KX, yy)])
    ax.text(KX - 1.2, yy + 0.9, f"R{i+1}", ha="right", va="bottom", fontsize=FT, color=GREY)
for i, yy in enumerate(P_COL):
    wire([(R + 2.6, yy), (KX, yy)])
    ax.text(KX - 1.2, yy + 0.9, f"C{i+1}", ha="right", va="bottom", fontsize=FT, color=GREY)
ax.text(KX + KW / 2, KY - 2.0, "no pull-up resistors — library\ndrives rows, reads columns",
        ha="center", va="top", fontsize=FT, color=GREY, style="italic")

# ============================================================= laser
box(103.0, 10.0, 17.0, 7.6, "589 nm laser", None, fs=FS)
wire([(111.5, RAIL5), (111.5, 17.6)]); dot(111.5, RAIL5)
wire([(111.5, 10.0), (111.5, RAILG)]); dot(111.5, RAILG)
ax.text(111.5, 7.6, "", ha="center")
ax.text(111.5, 8.2, "not switched by the microcontroller", ha="center",
        va="top", fontsize=FT, color=GREY, style="italic")

ax.text(5.0, RAIL5 - 3.2, "regulated\nDC supply", ha="right", va="top",
        fontsize=FT, color=GREY, style="italic")

fig.tight_layout(pad=0.2)
fig.savefig("circuit_schematic.png", dpi=300, facecolor="white")
print("wrote circuit_schematic.png")
