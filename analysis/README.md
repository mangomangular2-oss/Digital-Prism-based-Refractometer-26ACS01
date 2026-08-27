# Analysis

Scripts that regenerate the paper's figures from the raw data. Run them from
inside this folder — the data paths are relative.

```bash
pip install numpy matplotlib openpyxl
python3 intensity_profile.py
python3 calibration.py
python3 circuit_schematic.py
```

| Script | Output | What it does |
|---|---|---|
| `intensity_profile.py` | `intensity_profile.png` | Plots both sweep directions, fits the steep edge of each curve and marks the 50 % crossing taken as the critical angle |
| `calibration.py` | `calibration.png` | Measured against accepted refractive index, with the best-fit line and R² |
| `circuit_schematic.py` | `circuit_schematic.png` | Draws the wiring diagram directly from the firmware's pin map |

`intensity_profile.py` prints the recovered critical angle and refractive index
for each liquid and sweep direction, alongside the accepted value, so the
numbers in the paper can be checked against the raw data.
