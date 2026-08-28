# A Digital Prism-Based Refractometer

Firmware, CAD, raw data and analysis scripts for a low-cost automated
refractometer, built for the **Singapore Science Mentorship Programme 2026**
(Team 26ACS01).

The instrument measures a liquid's refractive index by finding the **critical
angle** at a prism–liquid interface. A stepper motor sweeps the prism through a
range of angles while a light-dependent resistor records how much light reaches
it; the critical angle is recovered from the steep edge of that signal, and
converted to refractive index on board.

**Team** — Isaac Yong Tze Hsi · Ng Yi-Shen · Shah Prish Vaishal \
**Mentor** — Mr Li Zhen, CRADLE, Science Centre Singapore\
**Teacher advisors** — Mdm Ali Basheera Banu, Mr Alvin Liew Shao Chuan\
**School** — Anglo-Chinese School (Independent), Singapore

## Headline results

| | |
|---|---|
| Mean deviation from reference values, five liquids | 0.18 % |
| Repeatability (standard error of five repeats, in *n*) | ±0.0005 |
| Total build cost | S$291 |
| Range | *n* = 1.000 – 1.498 (upper limit set by the prism's geometry and refractive index) |

## What is in this repository

| Folder | Contents |
|---|---|
| `firmware/` | The Arduino sketch that runs the instrument |
| `hardware/` | 3D-printable parts for the optical head and stage, as well as circuit schematic and total build cost |
| `data/` | Raw measurements, exactly as recorded |
| `analysis/` | Python scripts that turn the data into the paper's figures |
| `docs/` | The research paper and conference poster |

## Reproducing the figures

```bash
pip install numpy matplotlib openpyxl
cd analysis
python3 intensity_profile.py     # the sweep data, and the edge fit
python3 calibration.py           # measured vs accepted refractive index
python3 circuit_schematic.py     # wiring diagram, drawn from the pin map
```

## The main finding

Our first analysis took the **brightest point** of the light curve as the
critical angle. Checked against liquids of known refractive index, that was
wrong by up to 0.025.

The critical angle actually sits on the **steep rising edge** of the curve, at
the half-height point. Read there, the result is correct to 0.003 — about eight
times better. Where the brightest point falls depends on the beam width and the
sensor's position, so it shifts when the stage sweeps the other way; the steep
edge does not. `analysis/intensity_profile.py` shows both, for both sweep
directions.
