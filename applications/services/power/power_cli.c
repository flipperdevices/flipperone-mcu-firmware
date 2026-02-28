#include "power_cli.h"

#include <args.h>
#include <power/power.h>

char* power_cli_get_charger_status1_vbus_str(uint8_t stat) {
    char* state = NULL;
    switch(stat) {
    case Bq25792ChargerStatus1VbusNoInput:
        state = "No Input";
        break;
    case Bq25792ChargerStatus1VbusSdp:
        state = "USB SDP (500mA)";
        break;
    case Bq25792ChargerStatus1VbusCdp:
        state = "USB CDP (1.5A)";
        break;
    case Bq25792ChargerStatus1VbusDcp:
        state = "USB DCP (3.25A)";
        break;
    case Bq25792ChargerStatus1VbusHVDCP:
        state = "DCP (HVDCP) (1.5A)";
        break;
    case Bq25792ChargerStatus1VbusUnknown:
        state = "Unknown adaptor (3A)";
        break;
    case Bq25792ChargerStatus1VbusNonStandard:
        state = "Non-Standard Adapter (1A/2A/2.1A/2.4A)";
        break;
    case Bq25792ChargerStatus1VbusOtg:
        state = "In OTG mode";
        break;
    case Bq25792ChargerStatus1VbusNotQualified:
        state = "Not qualified adaptor";
        break;
    case Bq25792ChargerStatus1VbusVbus:
        state = "Device directly powered from VBUS";
        break;
    default:
        state = "Unknown";
        break;
    }

    return state;
}

char* power_cli_get_status1_charger_str(uint8_t stat) {
    char* state = NULL;
    switch(stat) {
    case Bq25792ChargerStatus1ChargeNot:
        state = "Not Charging";
        break;
    case Bq25792ChargerStatus1ChargeTrickle:
        state = "Trickle Charge";
        break;
    case Bq25792ChargerStatus1ChargePre:
        state = "Pre-charge";
        break;
    case Bq25792ChargerStatus1ChargeFast:
        state = "Fast charge (CC mode)";
        break;
    case Bq25792ChargerStatus1ChargeTaper:
        state = "Taper Charge (CV mode)";
        break;
    case Bq25792ChargerStatus1ChargeTopOff:
        state = "Top-off Timer Active Charging";
        break;
    case Bq25792ChargerStatus1ChargeTermination:
        state = "Charge Termination Done";
        break;
    default:
        state = "Unknown";
        break;
    }

    return state;
}

char* power_cli_get_status2_ico_str(uint8_t stat) {
    char* state = NULL;
    switch(stat) {
    case Bq25792ChargerStatus2IcoDisabled:
        state = "ICO disabled";
        break;
    case Bq25792ChargerStatus2IcoOptimization:
        state = "ICO optimization in progress";
        break;
    case Bq25792ChargerStatus2IcoMaximum:
        state = "Maximum input current detected";
        break;
    default:
        state = "Unknown";
        break;
    }

    return state;
}

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
        printf(
            "Ina219\t\t\t\t\t| VSYS: %.3f V | ISYS: %.2f mA | Shunt Voltage: %.4f mV |  Power: %.2f W\r\n\r\n", bus_v, current_a * 1000.0f, shunt_mv, power_w);

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
            "Bq25792\t\t\t\t\t| VSYS: %.3f V | VBUS: %.3f V | IBUS: %d mA | VBAT: %.3f V | IBAT: %d mA \r\n\t\t\t\t\t| Charger Temp: %.2f C | Battery Temp: %.2f C | Input Current Limit: %d mA | Charge Voltage Limit: %d mV | Charge Current Limit: %d mA\r\n\r\n",
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
            "Charger Status0: \t%08b \t| VBUS_PRESENT_STAT: %d | AC1_PRESENT_STAT: %d | AC2_PRESENT_STAT: %d | PG_STAT : %d | POORSRC_STAT : %d| WD_STAT : %d| VINDPM_STAT: %d | IINDPM_STAT: %d\r\n"
            "Charger Status1: \t%08b \t| BC1.2_DONE_STAT: %d | VBUS_STAT: %s | CHG_STAT: %s\r\n"
            "Charger Status2: \t%08b \t| VBAT_PRESENT_STAT: %d | DPDM_STAT: %d | TREG_STAT: %d | ICO_STAT: %s\r\n"
            "Charger Status3: \t%08b \t| PRECHG_TMR_STAT: %d | TRICHG_TMR_STAT: %d | CHG_TMR_STAT: %d | VSYS_STAT: %d | ADC_DONE_STAT: %d | ACRB1_STAT: %d | ACRB2_STAT: %d\r\n"
            "Charger Status4: \t%08b \t| TS_HOT_STAT: %d | TS_WARM_STAT: %d | TS_COOL_STAT: %d | TS_COLD_STAT: %d | VBATOTG_LOW_STAT: %d\r\n",
            get_charger_status.data[0],
            get_charger_status.stat0.vbus_present_stat,
            get_charger_status.stat0.ac1_present_stat,
            get_charger_status.stat0.ac2_present_stat,
            get_charger_status.stat0.pg_stat,
            get_charger_status.stat0.poorsrc_stat,
            get_charger_status.stat0.wd_stat,
            get_charger_status.stat0.vindpm_stat,
            get_charger_status.stat0.iindpm_stat,
            get_charger_status.data[1],
            get_charger_status.stat1.bc12_done_stat,
            power_cli_get_charger_status1_vbus_str(get_charger_status.stat1.vbus_stat),
            power_cli_get_status1_charger_str(get_charger_status.stat1.chg_stat),
            get_charger_status.data[2],
            get_charger_status.stat2.vbat_present_stat,
            get_charger_status.stat2.dpdm_stat,
            get_charger_status.stat2.treg_stat,
            power_cli_get_status2_ico_str(get_charger_status.stat2.ico_stat),
            get_charger_status.data[3],
            get_charger_status.stat3.prechg_tmr_stat,
            get_charger_status.stat3.trichg_tmr_stat,
            get_charger_status.stat3.chg_tmr_stat,
            get_charger_status.stat3.vsys_stat,
            get_charger_status.stat3.adc_done_stat,
            get_charger_status.stat3.acrb1_stat,
            get_charger_status.stat3.acrb2_stat,
            get_charger_status.data[4],
            get_charger_status.stat4.ts_hot_stat,
            get_charger_status.stat4.ts_warm_stat,
            get_charger_status.stat4.ts_cool_stat,
            get_charger_status.stat4.ts_cold_stat,
            get_charger_status.stat4.vbatotg_low_stat);
        printf(
            "Charger Fault0: \t%08b \t| VAC1_OVP_STAT: %d | VAC2_OVP_STAT: %d | CONV_OCP_STAT: %d | IBAT_OCP_STAT: %d | IBUS_OCP_STAT: %d | VBAT_OVP_STAT: %d | VBUS_OVP_STAT: %d | IBAT_REG_STAT: %d\r\n"
            "Charger Fault1: \t%08b \t| TSHUT_STAT: %d | OTG_UVP_STAT: %d | OTG_OVP_STAT: %d | VSYS_OVP_STAT: %d | VSYS_SHORT_STAT: %d\r\n",
            get_charger_fault.data[0],
            get_charger_fault.fault0.vac1_ovp_stat,
            get_charger_fault.fault0.vac2_ovp_stat,
            get_charger_fault.fault0.conv_ocp_stat,
            get_charger_fault.fault0.ibat_ocp_stat,
            get_charger_fault.fault0.ibus_ocp_stat,
            get_charger_fault.fault0.vbat_ovp_stat,
            get_charger_fault.fault0.vbus_ovp_stat,
            get_charger_fault.fault0.ibat_reg_stat,
            get_charger_fault.data[1],
            get_charger_fault.fault1.tshut_stat,
            get_charger_fault.fault1.otg_uvp_stat,
            get_charger_fault.fault1.otg_ovp_stat,
            get_charger_fault.fault1.vsys_ovp_stat,
            get_charger_fault.fault1.vsys_short_stat);
        printf(
            "Charger IRQ Flags0:\t%08b \t| VBUS_PRESENT_FLAG: %d | AC1_PRESENT_FLAG: %d | AC2_PRESENT_FLAG: %d | PG_FLAG: %d | POORSRC_FLAG: %d | WD_FLAG: %d | VINDPM_FLAG: %d | IINDPM_FLAG: %d\r\n"
            "Charger IRQ Flags1:\t%08b \t| BC1.2_DONE_FLAG: %d | VBAT_PRESENT_FLAG: %d | TREG_FLAG: %d | VBUS_FLAG: %d | ICO_FLAG: %d | CHG_FLAG: %d\r\n"
            "Charger IRQ Flags2:\t%08b \t| TOPOFF_TMR_FLAG: %d | PRECHG_TMR_FLAG: %d | TRICHG_TMR_FLAG: %d | CHG_TMR_FLAG: %d | VSYS_FLAG: %d | ADC_DONE_FLAG: %d | DPDM_DONE_FLAG: %d\r\n"
            "Charger IRQ Flags3:\t%08b \t| TS_HOT_FLAG: %d | TS_WARM_FLAG: %d | TS_COOL_FLAG: %d | TS_COLD_FLAG: %d | VBATOTG_LOW_FLAG: %d\r\n",
            get_charger_irq_flags.data[0],
            get_charger_irq_flags.flag0.vbus_present_flag,
            get_charger_irq_flags.flag0.ac1_present_flag,
            get_charger_irq_flags.flag0.ac2_present_flag,
            get_charger_irq_flags.flag0.pg_flag,
            get_charger_irq_flags.flag0.poorsrc_flag,
            get_charger_irq_flags.flag0.wd_flag,
            get_charger_irq_flags.flag0.vindpm_flag,
            get_charger_irq_flags.flag0.iindpm_flag,
            get_charger_irq_flags.data[1],
            get_charger_irq_flags.flag1.bc12_done_flag,
            get_charger_irq_flags.flag1.vbat_present_flag,
            get_charger_irq_flags.flag1.treg_flag,
            get_charger_irq_flags.flag1.vbus_flag,
            get_charger_irq_flags.flag1.ico_flag,
            get_charger_irq_flags.flag1.chg_flag,
            get_charger_irq_flags.data[2],
            get_charger_irq_flags.flag2.topoff_tmr_flag,
            get_charger_irq_flags.flag2.prechg_tmr_flag,
            get_charger_irq_flags.flag2.trichg_tmr_flag,
            get_charger_irq_flags.flag2.chg_tmr_flag,
            get_charger_irq_flags.flag2.vsys_flag,
            get_charger_irq_flags.flag2.adc_done_flag,
            get_charger_irq_flags.flag2.dpdm_done_flag,
            get_charger_irq_flags.data[3],
            get_charger_irq_flags.flag3.ts_hot_flag,
            get_charger_irq_flags.flag3.ts_warm_flag,
            get_charger_irq_flags.flag3.ts_cool_flag,
            get_charger_irq_flags.flag3.ts_cold_flag,
            get_charger_irq_flags.flag3.vbatotg_low_flag);

        furi_delay_ms(500);
    }
    furi_record_close(RECORD_POWER);
}
