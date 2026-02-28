#include "power_cli.h"

#include <args.h>
#include <power/power.h>

void power_cli(Cli* cli, FuriString* args, void* context) {
    UNUSED(cli);
    UNUSED(context);
    Power* power = furi_record_open(RECORD_POWER);
    float bus_v = 0;
    float current_a = 0;
    float power_w = 0;
    float shunt_mv = 0;

    int16_t get_ibus_ma = 0;
    int16_t get_ibat_ma = 0;
    uint16_t get_vbus_mv = 0;
    uint16_t get_vbat_mv = 0;
    uint16_t get_vsys_mv = 0;
    float get_charger_temperature = 0;
    float get_temperature_battery_celsius = 0;
    uint16_t get_input_current_limit_ma = 0;
    uint16_t get_charge_voltage_limit_ma = 0;
    uint16_t get_charge_current_limit_ma = 0;
    Bq25792ChargerStatusReg get_charger_status = {0};
    Bq25792FaultStatusReg get_charger_fault = {0};
    Bq25792ChargerFlagReg get_charger_irq_flags = {0};

    while(!cli_cmd_interrupt_received(cli)) {
        printf("\e[2J\e[0;0f"); // Clear display and return to 0

        bus_v = power_ina219_get_voltage_v(power);
        current_a = power_ina219_get_current_a(power);
        power_w = power_ina219_get_power_w(power);
        shunt_mv = power_ina219_get_shunt_voltage_mv(power);
        printf("Ina219\t VSYS: %.3f V | ISYS: %.2f mA | Shunt Voltage: %.4f mV |  Power: %.2f W\n", bus_v, current_a * 1000.0f, shunt_mv, power_w);

        power_bq25792_get_ibus_ma(power, &get_ibus_ma);
        power_bq25792_get_ibat_ma(power, &get_ibat_ma);
        power_bq25792_get_vbus_mv(power, &get_vbus_mv);
        power_bq25792_get_vbat_mv(power, &get_vbat_mv);
        power_bq25792_get_vsys_mv(power, &get_vsys_mv);
        power_bq25792_get_charger_temperature(power, &get_charger_temperature);
        power_bq25792_get_temperature_battery_celsius(power, &get_temperature_battery_celsius);
        power_bq25792_get_input_current_limit_ma(power, &get_input_current_limit_ma);
        power_bq25792_get_charge_voltage_limit_ma(power, &get_charge_voltage_limit_ma);
        power_bq25792_get_charge_current_limit_ma(power, &get_charge_current_limit_ma);
        power_bq25792_get_charger_status(power, &get_charger_status);
        power_bq25792_get_charger_fault(power, &get_charger_fault);
        power_bq25792_get_charger_irq_flags(power, &get_charger_irq_flags);

        printf(
            "BQ25792\t VSYS: %.3f V | VBUS: %.3f V | IBUS: %d mA | VBAT: %.3f V | IBAT: %d mA \r\nCharger Temp: %.2f C | Battery Temp: %.2f C | Input Current Limit: %d mA | Charge Voltage Limit: %d mV | Charge Current Limit: %d mA\n",
            (float_t)get_vsys_mv / 1000.0f,
            (float_t)get_vbus_mv / 1000.0f,
            get_ibus_ma,
            (float_t)get_vbat_mv / 1000.0f,
            get_ibat_ma,
            get_charger_temperature,
            get_temperature_battery_celsius,
            get_input_current_limit_ma,
            get_charge_voltage_limit_ma,
            get_charge_current_limit_ma);

        printf(
            "Charger Status0: %08b\r\n"
            "Charger Status1: %08b\r\n"
            "Charger Status2: %08b\r\n"
            "Charger Status3: %08b\r\n"
            "Charger Status4: %08b\r\n",
            get_charger_status.data[0],
            get_charger_status.data[1],
            get_charger_status.data[2],
            get_charger_status.data[3],
            get_charger_status.data[4]);
        printf(
            "Charger Fault0: %08b\r\n"
            "Charger Fault1: %08b\r\n",
            get_charger_fault.data[0],
            get_charger_fault.data[1]);
        printf(
            "Charger IRQ Flags0: %08b\r\n"
            "Charger IRQ Flags1: %08b\r\n"
            "Charger IRQ Flags2: %08b\r\n"
            "Charger IRQ Flags3: %08b\r\n",
            get_charger_irq_flags.data[0],
            get_charger_irq_flags.data[1],
            get_charger_irq_flags.data[2],
            get_charger_irq_flags.data[3]);

        furi_delay_ms(500);
    }
    furi_record_close(RECORD_POWER);
}
