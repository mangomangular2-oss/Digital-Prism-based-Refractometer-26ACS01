#!/usr/bin/env python3
"""Intensity profile across the critical angle, for both sweep directions.

Reads data/intensity_profiles.xlsx and writes intensity_profile.png.
Finds the steep edge of each curve, fits a straight line to its 20-80 %% portion
and solves for the 50 %% crossing - the point taken as the critical angle."""

import math
import numpy as np
import openpyxl
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

N_PRISM, APEX, DPS = 1.500, 45.0, 360.0 / 4076.0
BLUE, OCHRE, INK, MUTED, GRID = "#2E5FA3", "#C1720B", "#22262B", "#6B7280", "#EDEFF2"

wb = openpyxl.load_workbook("../data/intensity_profiles.xlsx", data_only=True)
rows = list(wb.active.iter_rows(values_only=True))
hdr = list(rows[0])
D = np.array([[float(x) for x in r] for r in rows[1:]])
ANG = D[:, 1]


def smooth(y, h=8):
    n = len(y)
    return np.array([y[max(0, i - h):min(n, i + h + 1)].mean() for i in range(n)])


def n_of(a):
    return N_PRISM * math.sin(math.radians(
        APEX + math.degrees(math.asin(math.sin(math.radians(a)) / N_PRISM))))


def a_of(n):
    th = math.degrees(math.asin(n / N_PRISM))
    return math.degrees(math.asin(N_PRISM * math.sin(math.radians(th - APEX))))


def flank(sm, side):
    b, pk = sm.min(), sm.max()
    bi = int(sm.argmax())
    c = pk - b
    lo, hi, thr = b + 0.2 * c, b + 0.8 * c, b + 0.5 * c
    i = np.arange(len(sm))
    m = (sm >= lo) & (sm <= hi) & ((i < bi) if side == "rise" else (i > bi))
    s, q = np.polyfit(i[m], sm[m], 1)
    return (thr - q) / s, s, thr, i[m], s, q


def steepest(sm):
    r, f = flank(sm, "rise"), flank(sm, "fall")
    return r if abs(r[1]) > abs(f[1]) else f


PANELS = [
    ("Clockwise sweep", [("A_water_counts", "Deionised water", 1.3330, BLUE),
                         ("A_olive_oil_counts", "Olive oil", 1.4670, OCHRE)]),
    ("Anticlockwise sweep", [("B_water_counts", "Deionised water", 1.3330, BLUE),
                             ("B_olive_oil_counts", "Olive oil", 1.4670, OCHRE)]),
]

plt.rcParams.update({
    "font.family": "Liberation Serif", "font.size": 9,
    "axes.edgecolor": "#B9BEC5", "axes.labelcolor": INK,
    "xtick.color": MUTED, "ytick.color": MUTED, "text.color": INK,
})

fig, axes = plt.subplots(2, 1, figsize=(6.3, 5.6), dpi=300, sharex=True)
res = []

for k, (ax, (title, items)) in enumerate(zip(axes, PANELS)):
    for col, label, n_acc, colour in items:
        y = D[:, hdr.index(col)]
        sm = smooth(y)
        xc, slope, thr, band, s, q = steepest(sm)
        ang_c = 15.0 + xc * DPS
        res.append((title, label, ang_c, n_of(ang_c), n_acc))

        ax.plot(ANG, y, lw=0.6, color=colour, alpha=0.30, zorder=2)
        ax.plot(ANG, sm, lw=1.8, color=colour, label=label, zorder=3,
                solid_capstyle="round")

        xf = np.linspace(band.min() - 6, band.max() + 6, 20)
        ax.plot(15.0 + xf * DPS, s * xf + q, lw=1.0, ls=(0, (4.5, 2.5)),
                color=colour, alpha=0.9, zorder=4)

        ax.plot([ang_c], [thr], "o", ms=9.5, mfc="none", mec="white", mew=2.2,
                zorder=5)
        ax.plot([ang_c], [thr], "o", ms=6.5, mfc="white", mec=colour, mew=1.9,
                zorder=6)
        ax.annotate(f"{ang_c:.2f}°", xy=(ang_c, thr), xytext=(0, -15),
                    textcoords="offset points", ha="center", fontsize=8,
                    color=colour)

    ax.set_ylim(80, 1035)
    ax.set_xlim(15, 70)
    ax.grid(True, color=GRID, lw=0.7)
    ax.set_axisbelow(True)
    for sp in ("top", "right"):
        ax.spines[sp].set_visible(False)
    ax.set_ylabel("Detected intensity / ADC counts")
    ax.text(0.011, 0.95, f"({'ab'[k]})  {title}", transform=ax.transAxes,
            fontsize=9.5, fontweight="bold", va="top")

axes[0].legend(frameon=False, fontsize=9, loc="upper center",
               handlelength=1.7, borderaxespad=0.2)
axes[1].set_xlabel("Angle of incidence at the entry face / °")

fig.tight_layout(pad=0.4, h_pad=0.8)
fig.savefig("intensity_profile.png", dpi=300,
            facecolor="white")

print(f"{'sweep':22s} {'liquid':17s} {'edge/°':>8s} {'n_meas':>9s} "
      f"{'n_acc':>8s} {'error':>9s}")
for t, l, a, nm, na in res:
    print(f"{t:22s} {l:17s} {a:8.3f} {nm:9.5f} {na:8.4f} {nm - na:+9.5f}")
