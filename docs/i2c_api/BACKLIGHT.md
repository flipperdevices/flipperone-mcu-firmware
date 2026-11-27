# Backlight

The backlight registers of the I2C intercom: display brightness, the inactivity timeout, and the control word that wakes the backlight or keeps it on.

## Map

| Address | Register | Access | After reset |
|---|---|---|---|
| `0x0350` | Backlight level, `0..255` | R/W | From flash, `26` (10%) if never written |
| `0x0352` | Backlight timeout, in 100 ms units | R/W | From flash, `100` (10 s) if never written |
| `0x0354` | Backlight control | R/W | `0` |

Reading the level or the timeout returns the value in use, also when it was changed on the device itself, from the MCU settings menu. The control register is a command: a read returns the last word written to it.

## Level

`0` is off, `255` is the maximum; only the low byte of the register is used. The value is written to flash on every change, so it survives a reset.

Writing a level that differs from the one in use applies it at once: the display comes on at the new level and the timeout is restarted. A write of `0` turns the backlight off.

## Timeout

The time after which the backlight goes off when nothing happens, in units of 100 ms: `10` is one second, `600` is a minute, the largest value `65535` is about 109 minutes.

- The value is written to flash on every change, so it survives a reset.
- Writing it restarts the timer with the new value, unless the override bit is set.
- `0` disables the automatic turn-off: a backlight that is on stays on. It does not turn the backlight on by itself.

The firmware also restarts the timer on its own input events: any key press or touchpad touch.

## Control

| Bit | Name | Effect |
|---|---|---|
| `0` | Override | Keep the backlight on and ignore the timeout. Setting it turns the backlight on |
| `1` | Ping | Wake the backlight: turn it on at the stored level and restart the timeout |

## Example

Half brightness, a 30 second timeout, then a wake, with the `i2c-dev` tool `i2ctransfer` on bus 0:

```sh
# level = 128
i2ctransfer 0 w4@0x69 0x03 0x50 0x80 0x00 -y

# timeout = 300, in 100 ms units
i2ctransfer 0 w4@0x69 0x03 0x52 0x2c 0x01 -y

# ping
i2ctransfer 0 w4@0x69 0x03 0x54 0x02 0x00 -y
```
