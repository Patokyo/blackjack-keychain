# Blackjack Keychain

A credit-card-sized blackjack strategy trainer. An ATmega328P deals a
six-deck shoe, you answer each decision with the on-board buttons, and
the firmware marks every answer correct or incorrect against basic
strategy before playing the hand out.

This is work in progress: the game logic and the serial log work, the
four-digit display and the LEDs are wired and mapped but not driven
yet.

## Repository layout

```
hardware/
  blackjack-keychain.kicad_pro    KiCad 10 project
  blackjack-keychain.kicad_sch    schematic, single sheet
  blackjack-keychain.kicad_pcb    board, 2 layer
  blackjack-keychain.net          netlist exported from the schematic
  fabrication/                    gerbers, drills, job file, fab zip
firmware/
  blackjack-keychain/
    blackjack-keychain.ino        the sketch
```

The KiCad project, its netlist and its outputs stay together under
`hardware/` so the project opens with everything it needs in one
place. `hardware/fabrication/` holds the last set of outputs sent to
a board house; regenerating them means re-plotting from KiCad, not
editing them by hand.

## Hardware

| | |
|---|---|
| Board | 50 x 70 mm, 2 layer, 1.6 mm, 3.5 mm corner radius |
| MCU | ATmega328P-AU, TQFP-32, 5 V from USB-C, internal 8 MHz RC, no crystal |
| Display | CC56-12EWA, 0.28" four digit, common cathode, one NPN low side switch per digit |
| Indicators | 7 LEDs, active high through 330R |
| Input | 5 tactile buttons on two resistor ladders, plus SW6 as a strap |
| Programming | 2x03 ISP pads |

The board is powered from the USB-C socket. There is no USB-serial
chip on it, so the ISP pads are the only way to program it.

Pin map, as used by the firmware:

| Function | Pin | Pad |
|---|---|---|
| Correct / wrong / lose / push LEDs | D2 / D1 / D3 / D4 | 32 / 31 / 1 / 2 |
| Soft / pair / win LEDs | A1 / A3 / PB6 | 24 / 26 / 7 |
| Buttons, hit/stand/double ladder | A4 | 27 |
| Buttons, split/bank ladder | A5 | 28 |
| Display segments a-g, dp | D12, D6, D8, D11, D13, D10, D7, D9 | 16, 10, 12, 15, 17, 14, 11, 13 |
| Display digit drives Q1-Q4 | PB7, A0, D5, A2 | 8, 23, 9, 25 |
| SW6 strap | D0 | 30 |
| Serial TX | D1 (shared with the wrong LED) | 31 |

Two consequences of that netlist are worth knowing before flashing:
serial transmit shares a pin with an LED, so that LED glows between
bytes and flickers during output, and serial receive is held at +5 V or
GND by SW6, so the port cannot be typed into on an assembled board.
The buttons are the only input.

## Firmware

Open `firmware/blackjack-keychain/blackjack-keychain.ino` in the
Arduino IDE. The folder name matches the sketch name, as the IDE
requires.

- Build with `F_CPU = 8000000L` — MiniCore's ATmega328 at "8 MHz
  internal", or a Pro Mini 3.3 V/8 MHz board definition.
- Low fuse `0xE2`, with CKDIV8 unprogrammed. A part straight from the
  factory divides the internal oscillator down to 1 MHz, which plays
  the game eight times too slowly and garbles the serial output.
- Serial monitor at 9600 baud.

Rules: six deck shoe reshuffled between hands, dealer stands on soft
17, double on any first two cards, double after split allowed, up to
four hands by splitting (no resplitting aces, split aces get one card),
two card 21 is a natural that beats a made 21. Start with $100, a flat
$10 a hand, and the game ends when $10 can no longer be covered.

Controls are `H` (hit), `S` (stand), `D` (double) and `P` (split),
from the buttons or typed into the serial monitor if SW6 is left off
the board. The bank button is wired but has no game action yet.

## Building the hardware

The schematic and board reference one project-specific footprint
library, `custom_footprint_library`, which provides the 7 segment
display and the ISP pads. It is not in this repository yet, so a fresh
clone will show those two footprints as missing until it is added or
re-pointed.

To regenerate the outputs in `hardware/fabrication/`, plot from KiCad
with the same layer set currently committed: F.Cu, B.Cu, F.Paste,
B.Paste, F.Silkscreen, B.Silkscreen, F.Mask, B.Mask, Edge.Cuts, plus
the PTH and NPTH drill files.
