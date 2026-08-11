# Flipper One MCU, target f100.

fw_target_sources(
    furi_hal/*.c*
    furi_bsp/*.c*
    src/*.c*
)

fw_target_includes(
    src
    furi_hal
    furi_bsp
    config
)

fw_sources(
    lib/corelibs/lib/furi/*.c*
    lib/corelibs/lib/version/*.c*
    lib/toolbox/*.c*
    lib/tusb/*.c*
    lib/drivers/bq2579x/*.c*
    lib/drivers/bq28z620/*.c*
    lib/drivers/display/*.c*
    lib/drivers/drv2605l/*.c*
    lib/drivers/fusb302/*.c*
    lib/drivers/hd3ss3220/*.c*
    lib/drivers/headphones/*.c*
    lib/drivers/i2c_slave/*.c*
    lib/drivers/ina219/*.c*
    lib/drivers/ina4230/*.c*
    lib/drivers/iqs7211e/*.c*
    lib/drivers/pcal6416/*.c*
    lib/drivers/spi_get_frame/*.c*
    lib/drivers/tca6416a/*.c*
    lib/drivers/tps62868x/*.c*
    applications/*.c
)

fw_includes(
    applications
    applications/services
    lib/toolbox
    lib/corelibs/lib/furi
    lib/corelibs/lib/version
    lib/tusb
    lib/corelibs/lib
    lib/freertos
    lib/corelibs/lib/mlib
    lib
)
