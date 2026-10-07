# FPGA activity stretcher

Optional. PERCH does not require a bitstream.

The SLG47910 cannot parse 802.11. This module only stretches a strobe from the MCU so the FPGA user LED (FPGA GPIO16) stays visible.

Suggested mapping, confirmed against the board schematic before flashing:

| Signal | Side |
|---|---|
| `activity` | MCU GPIO reserved for the link, into the FPGA |
| `led` | FPGA GPIO16, active high |
| `clk` | whatever clock Go Configure assigns; retune `STRETCH` |

Hold the FPGA in reset if this bitstream is not loaded. The firmware already does that by not enabling GPIO8/GPIO9.
