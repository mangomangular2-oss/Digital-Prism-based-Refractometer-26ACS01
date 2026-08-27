# Firmware

`refractometer.ino` — the sketch as flashed to the instrument. Arduino Uno.

## Libraries

Install through the Arduino IDE's Library Manager:

- **LiquidCrystal I2C** by Frank de Brabander
- **Keypad** by Mark Stanley / Alexander Brevig

## Pin map

| Pin | Use |
|---|---|
| D2 | MEASURE button (`INPUT_PULLUP`, pressed = LOW) |
| D3 | Mode rocker (`INPUT_PULLUP`, LOW = concentration mode) |
| D4–D7 | Keypad rows |
| D8–D11 | Stepper coils, via ULN2003 driver board |
| D12, D13 | Keypad columns 0, 1 |
| A0 | LDR (light-dependent resistor), in a divider with 10 kΩ |
| A1 | Keypad column 2 |
| A2 | Free |
| A3 | Free — reserved for a DS18B20 temperature sensor |
| A4, A5 | I²C to the LCD backpack (address 0x27) |

The 589 nm laser is powered from the 5 V rail and is not switched by the
microcontroller.

## Two modes

- **Rocker HIGH** — a single refractive-index measurement.
- **Rocker LOW** — calibration against standards of known concentration,
  then concentration readings for that solute.

## How the reading is taken

`findEdgeIndex()` is the core of it. After a sweep, it takes the samples lying
between 20 % and 80 % of the way from the dark level to the bright level,
least-squares fits a straight line through them, and solves that line for the
50 % crossing. That crossing is the critical angle, in units of motor steps.

One motor step is 0.0883°: the 28BYJ-48's real reduction is 63.68:1, not the
64:1 usually quoted, giving 4076 half-steps per revolution.
