# FPGA activity stretcher

Optional. PERCH runs without a bitstream. The radio path does not enter the fabric.

`activity_stretcher.v` holds FPGA GPIO16 high for `STRETCH` clocks after a rising edge on `activity`. That is the whole contract. 1120 LUTs is more than this needs. Do not grow it into a parser.

## Pin contract

| Signal | Connect |
|---|---|
| `activity` | ESP32 GPIO14, only if `PERCH_FPGA_STROBE` is 1 in `config.h` |
| `led` | FPGA GPIO16, the FPGA user LED, active high |
| `clk` | the clock Go Configure assigns; retune `STRETCH` |

GPIO14 is a header pin. It is not the FPGA config bus. The config bus is GPIO8 PWR, GPIO9 EN, GPIO10 SS, GPIO11 MOSI, GPIO12 SCLK, GPIO13 MISO. Leave those alone.

## Go Configure

This tree does not contain a bitstream. Go Configure is the Renesas desktop tool, and it is not run from here.

1. New design for the SLG47910.
2. Paste `activity_stretcher.v`, or redraw the same edge-stretch in the schematic editor.
3. Lock `led` to FPGA GPIO16.
4. Lock `activity` to the FPGA pin you wired to ESP32 GPIO14. Confirm that pin on the Shrike-fi schematic before flashing.
5. Export the bitstream and load it with ShrikeFlash.
6. Set `PERCH_FPGA_STROBE` to 1 and rebuild the sketch.

Until that bitstream is loaded, only the MCU LED on GPIO21 moves. With the FPGA held in reset, GPIO14 pulses into an unconfigured pin and the FPGA LED stays off.
