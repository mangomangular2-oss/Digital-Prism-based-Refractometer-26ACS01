# Data

Raw measurements, exactly as recorded. Nothing here has been corrected or
calibrated.

## `intensity_profiles.xlsx`

624 rows, one per motor half-step, covering a full sweep from a stage angle of
15°. This is the data behind the intensity-profile figure.

| Column | Meaning |
|---|---|
| `step` | Motor half-step number from the start of the sweep |
| `stage_angle_deg` | Stage angle, degrees. `15 + step x 0.0883` |
| `theta_hypotenuse_deg` | The corresponding angle at the prism–liquid interface |
| `A_water_counts` | Light sensor reading, deionised water, clockwise sweep |
| `A_olive_oil_counts` | Light sensor reading, olive oil, clockwise sweep |
| `B_water_counts` | Deionised water, anticlockwise sweep |
| `B_olive_oil_counts` | Olive oil, anticlockwise sweep |

Readings are raw 10-bit ADC counts (0–1023) from the LDR divider on A0.

The two sweep directions are the evidence for the main finding: the steep edge
of the curve lands in almost the same place either way, while the peak does not.

## `measurements.xlsx`

The trial-by-trial workbook behind the paper's tables.

| Sheet | Contents |
|---|---|
| `How to use` | Notes on filling the workbook |
| `T1 raw readings` | Every trial for the five reference liquids |
| `T1 summary` | Means and standard errors, all by formula |
| `T2 sucrose` | Concentration-mode trials, sucrose standards |
| `T3 bill of materials` | Costs, from receipts |
| `Conditions` | Instrument and session conditions |
| `Precision rules` | How many decimal places each quantity is reported to, and why |
