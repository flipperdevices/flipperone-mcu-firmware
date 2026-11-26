# Display

How Linux sends frames to the MCU over SPI, and how the screen passes between Linux and the MCU's own UI.

> [!NOTE]
> Not written yet. The sections follow [the contract template](README.md#writing-a-contract-document).

- **Counterpart:** the Linux display driver.
- **Source of truth:** [`lib/drivers/spi_get_frame`](../../lib/drivers/spi_get_frame) and [`lib/drivers/pio_get_frame`](../../lib/drivers/pio_get_frame); the frame request interrupt in [`i2c_registers_map.h`](../../applications/services/i2c_intercom/i2c_registers_map.h).

Planned contents:

- The SPI link: mode, clock, chip select
- The frame: size, pixel format, buffering
- When the MCU shows the Linux screen, and when its own UI
- The frame request after the MCU takes the screen back
