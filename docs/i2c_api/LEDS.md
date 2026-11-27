# LEDs

The LED registers of the I2C intercom: group brightness, the color and animation pattern for each network LED.

## Map

| Address | Register | Access | After reset |
|---|---|---|---|
| `0x0300` | Link group brightness | R/W | From flash, `255` if never written |
| `0x0302` | Power group brightness | R/W | From flash, `255` if never written |
| `0x0304` | Wattmeter group brightness | R/W | From flash, `255` if never written |
| `0x0310` | Link1 color, RGB565 | R/W | `0` |
| `0x0312` | Link2 color, RGB565 | R/W | `0` |
| `0x0314` | Link3 color, RGB565 | R/W | `0` |
| `0x0316` | Link4 color, RGB565 | R/W | `0` |
| `0x0320` | Link1 pattern ID | R/W | `0` |
| `0x0322` | Link2 pattern ID | R/W | `0` |
| `0x0324` | Link3 pattern ID | R/W | `0` |
| `0x0326` | Link4 pattern ID | R/W | `0` |

The link LEDs are the four indicators at the network connector:

| Register | LED | `FuriBspLedType` |
|---|---|---|
| Link1 | LINK | `FuriBspLedTypeNet` |
| Link2 | WI-FI | `FuriBspLedTypeWiFi` |
| Link3 | ETH1 | `FuriBspLedTypeEth2` |
| Link4 | ETH0 | `FuriBspLedTypeEth1` |

## Brightness

`0` is off, `255` is the maximum. The value scales everything its group shows, a
running pattern included, and is written to flash on every change, so it survives a
reset.

| Group | Covers |
|---|---|
| Link | The four network LEDs |
| Power | The power button LED |
| Wattmeter | The battery outline, its four power segments and charging indicator, four USB power segments |

## Color

A color is RGB565, high byte first:

| Bits | Meaning |
|---|---|
| `15:11` | Red, 5 bits |
| `10:5` | Green, 6 bits |
| `4:0` | Blue, 5 bits |

For example, red is `0xF800`, green `0x07E0`, blue `0x001F`, white
`0xFFFF`.

The color is also the base color of the LED's pattern: `fade` scales it, `blink`
turns it on and off, and only `rainbow gradient` ignores it.

## Patterns

Every link LED has its own pattern and runs it on its own timer, independently of the
other three.

| ID | Pattern | Step | Behavior |
|---|---|---|---|
| `0` | Static color | — | Shows the color register value. The default |
| `1` | Rainbow gradient | `16 ms` | Cycles the hue, a full circle in ~4 s. Fixed value and saturation, ignores the color |
| `2` | Fade | `10 ms` | Scales the color `1 → 255 → 0` over 256 steps, ~2.6 s per cycle |
| `3` | Blink | `500 ms` | Color for one step, off for the next, endlessly |

- Writing an ID starts that pattern from its first step, even if it is already
  running.
- Writing `0` stops the animation and shows the static color again.
- A color written while a pattern runs does not restart it: the new color is used
  from the next step, which for `blink` can take up to 500 ms.
- An ID above `3` is ignored, the previous pattern keeps running.
- The device's "all off" action stops every running pattern, along with the colors.

## Example

Red on LINK, then blinking blue on WI-FI, with the `i2c-dev` tool `i2ctransfer` on
bus 1:

```sh
# LINK LED color = 0xF800, static
i2ctransfer 0 w4@0x69 0x03 0x10 0x00 0xF8 -y
i2ctransfer 0 w4@0x69 0x03 0x20 0x00 0x00 -y

# WI-FI LED color = 0x001F, pattern = 3
i2ctransfer 0 w4@0x69 0x03 0x12 0x1F 0x00 -y
i2ctransfer 0 w4@0x69 0x03 0x22 0x03 0x00 -y
    
# Read WI-FI LED pattern back
i2ctransfer 0 w2@0x69 0x03 0x22 r2 -y
```
