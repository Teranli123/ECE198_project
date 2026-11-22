# Strap-on casing

OpenSCAD source: `forearm_case.scad`. Every size is a parameter at the top. Measure your parts and edit them there.

| Version | File | Body size (L x W x H) | Width with strap tabs |
|---|---|---|---|
| Breadboard, battery lying on it | `breadboard_box.stl`, `breadboard_lid.stl` | 160.5 x 60.6 x 35 mm | 76.6 mm |
| No breadboard | `no_breadboard_box.stl`, `no_breadboard_lid.stl` | 103.9 x 59.4 x 35 mm | 75.4 mm |

## Dimensions used
| Part | Size (mm) | Source |
|---|---|---|
| Arduino UNO R4 Minima | 68.85 x 53.34, standard Uno hole pattern | [Core Electronics spec](https://core-electronics.com.au/arduino-uno-r4-minima.html) |
| USB-C position | not published; 16 mm wide opening, Eddie checked it looks right | estimate |
| MAX30102 breakout | 21 x 16 (other sellers list 20 x 15) | [OpenKits wiki](https://openkits-wiki.easyeda.com/en/coloreasyduino/module/sensor/max30102-sensor.html), [Jaycar XC3740](https://jaycar.com.au/p/XC3740) |
| MAX30102 chip | 5.6 x 3.3 x 1.55 | [Maxim datasheet](https://www.mouser.com/datasheet/2/256/maximintegratedproducts_MAX30102%20DS-1179649.pdf) |
| GY-61 ADXL335 | 21 x 15 x 3 without header, 11 tall with header | [Core Electronics](https://core-electronics.com.au/adxl335-triple-axis-accelerometer-gy-61.html) |
| Breadboard | 84 x 56 x 10 | Eddie's measurement |
| 9V battery | 44.8 x 26.2 x 17 + 10 for the snap connector | Eddie's measurement |

## Layout
- **Uno** at the USB end, on 4 split snap pegs 6.5 mm tall. Press it down until it clicks.
- **Both sensors sit on the floor under the Uno.** The heart rate chip faces down through a 13 x 11 mm skin window. There's about 7 mm of room under the Uno, so **solder the wires flat to the sensor boards (no pin headers)**.
- **Breadboard** lies on the floor next to the Uno. Use its adhesive backing. Two low stops keep it off the Uno.
- **9V battery** lies across the far end of the breadboard, with the connector toward one wall. The lid has a frame and 4 pads that hold it down. Wires on the breadboard under the battery have to lie flat.
- **Headroom:** about 14 mm above the Uno headers (dupont wires fit, but may need a slight bend), and 4 mm above the battery.
- **LED (5.2 mm) and toggle switch (6.5 mm)** go through the lid over the middle of the Uno.
- **USB-C** opening in the end wall.
- **Lid:** 5 snap clips (2 per long side plus 1 in the USB end wall). Press a hook in through its window to open it.
- **Strap:** two pairs of slot tabs (32 x 4 mm) on the long sides, one pair near each end. Each 30 mm lanyard goes through a tab on one side, around the arm, and through the matching tab on the other side.

## Printing (PLA)
- Box open side up, lid plate down. No supports needed.
- 0.2 mm layers, 3 walls, 20% infill.
- Print the lid first to test the clips. Lower `hook` if they're too tight, raise it if they're too loose.

## Not included
The Dragino Yun shield stacked on the Uno does not fit under 35 mm.
