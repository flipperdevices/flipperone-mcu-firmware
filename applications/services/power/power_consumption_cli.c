#include "power_cli.h"

#include <cli/args.h>
#include <cli/cli_ansi.h>
#include <cli/cli_command.h>
#include <furi_hal_i2c.h>
#include <furi_hal_i2c_config.h>
#include <drivers/ina4230/ina4230.h>

bool power_consumption_cli(PipeSide* pipe, FuriString* args) {
    UNUSED(pipe);
    UNUSED(args);

    Ina4230* ina4230_add[5] = {0};

    ina4230_add[0] = ina4230_init(&furi_hal_i2c_handle_main, 0x40);
    if(ina4230_add[0]) {
        ina4230_set_config_channel(ina4230_add[0], 0, "VDD_0V75_S3        ", 0.050f, 0.5f);
        ina4230_set_config_channel(ina4230_add[0], 1, "VCC3V3_CONTROL     ", 0.010f, 6.0f);
        ina4230_set_config_channel(ina4230_add[0], 2, "VDD0V85_DDR_S0     ", 0.020f, 3.0f);
        ina4230_set_config_channel(ina4230_add[0], 3, "VCC_3V3_S3         ", 0.010f, 5.0f);
    }

    ina4230_add[1] = ina4230_init(&furi_hal_i2c_handle_main, 0x41);
    if(ina4230_add[1]) {
        ina4230_set_config_channel(ina4230_add[1], 0, "VDDQ0V51_DDR_S0    ", 0.020f, 3.0f);
        ina4230_set_config_channel(ina4230_add[1], 1, "VDD0V75_NPU_S0     ", 0.010f, 5.0f);
        ina4230_set_config_channel(ina4230_add[1], 2, "VDD0V75_GPU_S0     ", 0.020f, 3.0f);
        ina4230_set_config_channel(ina4230_add[1], 3, "VDD0V75_LOGIC_S0   ", 0.020f, 3.0f);
    }

    ina4230_add[2] = ina4230_init(&furi_hal_i2c_handle_main, 0x46);
    if(ina4230_add[2]) {
        ina4230_set_config_channel(ina4230_add[2], 0, "VCCA_3V3_S0        ", 0.020f, 0.5f);
        ina4230_set_config_channel(ina4230_add[2], 1, "VCCIO3V3/1V8_SD_S0 ", 0.050f, 0.3f);
        ina4230_set_config_channel(ina4230_add[2], 2, "VDD2_1V05_DDR_S3   ", 0.020f, 2.5f);
        ina4230_set_config_channel(ina4230_add[2], 3, "VCC_1V8_S3         ", 0.020f, 3.0f);
    }

    ina4230_add[3] = ina4230_init(&furi_hal_i2c_handle_main, 0x43);
    if(ina4230_add[3]) {
        ina4230_set_config_channel(ina4230_add[3], 0, "VDD0V75_CPU_BIG_S0 ", 0.010f, 6.5f);
        ina4230_set_config_channel(ina4230_add[3], 1, "VDDA_1V2_S0        ", 0.050f, 0.3f);
        ina4230_set_config_channel(ina4230_add[3], 2, "VCCA_1V8_S0        ", 0.020f, 0.5f);
        ina4230_set_config_channel(ina4230_add[3], 3, "VDD0V75_CPU_LIT_S0 ", 0.010f, 5.0f);
    }

    ina4230_add[4] = ina4230_init(&furi_hal_i2c_handle_main, 0x44);
    if(ina4230_add[4]) {
        ina4230_set_config_channel(ina4230_add[4], 0, "VDDA_0V75_S0       ", 0.050f, 0.3f);
        ina4230_set_config_channel(ina4230_add[4], 1, "VDDA_0V85_S0       ", 0.020f, 0.5f);
        ina4230_set_config_channel(ina4230_add[4], 2, "VDDA0V75_HDMI_S0   ", 0.020f, 0.5f);
        ina4230_set_config_channel(ina4230_add[4], 3, "VDDA0V85_DDR_PLL_S0", 0.050f, 0.3f);
    }
    printf(ANSI_ERASE_DISPLAY(ANSI_ERASE_ENTIRE)); // Clear display
    while(!cli_is_pipe_broken_or_is_etx_next_char(pipe)) {
        printf(ANSI_CURSOR_POS("1", "1"));
        for(uint32_t ina = 0; ina < 5; ina++) {
            printf("INA4230 %ld\r\n", ina);
            if(ina4230_add[ina] == NULL) {
                printf("Failed to initialize INA4230 %ld\r\n", ina);
                continue;
            }
            for(uint32_t channel = 0; channel < 4; channel++) {
                float bus_voltage = ina4230_get_bus_voltage_v(ina4230_add[ina], channel);
                float shunt_voltage = ina4230_get_shunt_voltage_mv(ina4230_add[ina], channel);
                float current = ina4230_get_current_a(ina4230_add[ina], channel);
                float power = ina4230_get_power_w(ina4230_add[ina], channel);
                const char* name = ina4230_get_channel_name(ina4230_add[ina], channel);
                printf(
                    "Channel %ld (%s): \tBus Voltage: %.3f V, \tShunt Voltage: %.3f mV, \tCurrent: %.3f A, \tPower: %.3f W\r\n",
                    channel,
                    name,
                    bus_voltage,
                    shunt_voltage,
                    current,
                    power);
            }
        }
        furi_delay_ms(500);
    }
    return true;
}
