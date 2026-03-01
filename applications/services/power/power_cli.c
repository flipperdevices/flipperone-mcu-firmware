#include "power_cli.h"

#include <args.h>
#include <power/power.h>

static char* power_cli_get_charger_status1_vbus_str(uint8_t stat) {
    switch(stat) {
    case Bq25792ChargerStatus1VbusNoInput:
        return "No Input";
    case Bq25792ChargerStatus1VbusSdp:
        return "USB SDP (500mA)";
    case Bq25792ChargerStatus1VbusCdp:
        return "USB CDP (1.5A)";
    case Bq25792ChargerStatus1VbusDcp:
        return "USB DCP (3.25A)";
    case Bq25792ChargerStatus1VbusHVDCP:
        return "DCP (HVDCP) (1.5A)";
    case Bq25792ChargerStatus1VbusUnknown:
        return "Unknown adaptor (3A)";
    case Bq25792ChargerStatus1VbusNonStandard:
        return "Non-Standard Adapter (1A/2A/2.1A/2.4A)";
    case Bq25792ChargerStatus1VbusOtg:
        return "In OTG mode";
    case Bq25792ChargerStatus1VbusNotQualified:
        return "Not qualified adaptor";
    case Bq25792ChargerStatus1VbusVbus:
        return "Device directly powered from VBUS";
    default:
        return "Unknown";
    }
}

static char* power_cli_get_status1_charger_str(uint8_t stat) {
    switch(stat) {
    case Bq25792ChargerStatus1ChargeNot:
        return "Not Charging";
    case Bq25792ChargerStatus1ChargeTrickle:
        return "Trickle Charge";
    case Bq25792ChargerStatus1ChargePre:
        return "Pre-charge";
    case Bq25792ChargerStatus1ChargeFast:
        return "Fast charge (CC mode)";
    case Bq25792ChargerStatus1ChargeTaper:
        return "Taper Charge (CV mode)";
    case Bq25792ChargerStatus1ChargeTopOff:
        return "Top-off Timer Active Charging";
    case Bq25792ChargerStatus1ChargeTermination:
        return "Charge Termination Done";
    default:
        return "Unknown";
    }
}

static char* power_cli_get_status2_ico_str(uint8_t stat) {
    switch(stat) {
    case Bq25792ChargerStatus2IcoDisabled:
        return "ICO disabled";
    case Bq25792ChargerStatus2IcoOptimization:
        return "ICO optimization in progress";
    case Bq25792ChargerStatus2IcoMaximum:
        return "Maximum input current detected";
    default:
        return "Unknown";
    }
}

static void power_cli_print_ina219(Power* power) {
    float bus_v = power_ina219_get_voltage_v(power);
    float current_a = power_ina219_get_current_a(power);
    float power_w = power_ina219_get_power_w(power);
    float shunt_mv = power_ina219_get_shunt_voltage_mv(power);

    printf(
        "INA219:\r\n"
        "  VSYS:  %.3fV\r\n"
        "  ISYS:  %.2fmA\r\n"
        "  Shunt: %.4fmV\r\n"
        "  Power: %.2fW\r\n\r\n",
        bus_v,
        current_a * 1000.0f,
        shunt_mv,
        power_w);
}

static void power_cli_print_bq25792(Power* power) {
    int16_t ibus_ma = 0;
    int16_t ibat_ma = 0;
    uint16_t vbus_mv = 0;
    uint16_t vbat_mv = 0;
    uint16_t vsys_mv = 0;
    float charger_temp = 0;
    float battery_temp = 0;
    uint16_t input_current_limit_ma = 0;
    uint16_t charge_voltage_limit_mv = 0;
    uint16_t charge_current_limit_ma = 0;

    power_bq25792_get_ibus_ma(power, &ibus_ma);
    power_bq25792_get_ibat_ma(power, &ibat_ma);
    power_bq25792_get_vbus_mv(power, &vbus_mv);
    power_bq25792_get_vbat_mv(power, &vbat_mv);
    power_bq25792_get_vsys_mv(power, &vsys_mv);
    power_bq25792_get_charger_temperature(power, &charger_temp);
    power_bq25792_get_temperature_battery_celsius(power, &battery_temp);
    power_bq25792_get_input_current_limit_ma(power, &input_current_limit_ma);
    power_bq25792_get_charge_voltage_limit_ma(power, &charge_voltage_limit_mv);
    power_bq25792_get_charge_current_limit_ma(power, &charge_current_limit_ma);

    printf(
        "BQ25792:\r\n"
        "  VSYS:    %.3fV\r\n"
        "  VBUS:    %.3fV\r\n"
        "  IBUS:    %dmA\r\n"
        "  VBAT:    %.3fV\r\n"
        "  IBAT:    %dmA\r\n"
        "  ChgTemp: %.2fC\r\n"
        "  BatTemp: %.2fC\r\n"
        "  IINDPM:  %dmA\r\n"
        "  VCHRG:   %dmV\r\n"
        "  ICHRG:   %dmA\r\n\r\n",
        (float_t)vsys_mv / 1000.0f,
        (float_t)vbus_mv / 1000.0f,
        ibus_ma,
        (float_t)vbat_mv / 1000.0f,
        ibat_ma,
        charger_temp,
        battery_temp,
        input_current_limit_ma,
        charge_voltage_limit_mv,
        charge_current_limit_ma);
}

static void power_cli_print_charger_status(Power* power, FuriString* arena) {
    Bq25792ChargerStatusReg s = {0};
    power_bq25792_get_charger_status(power, &s);
    furi_string_set(arena, "");

    printf("  Status0: 0x%02X", s.data[0]);
    if(s.stat0.vbus_present_stat) furi_string_cat_printf(arena, " VBUS_PRESENT");
    if(s.stat0.ac1_present_stat) furi_string_cat_printf(arena, " AC1_PRESENT");
    if(s.stat0.ac2_present_stat) furi_string_cat_printf(arena, " AC2_PRESENT");
    if(s.stat0.pg_stat) furi_string_cat_printf(arena, " PG");
    if(s.stat0.poorsrc_stat) furi_string_cat_printf(arena, " POORSRC");
    if(s.stat0.wd_stat) furi_string_cat_printf(arena, " WD");
    if(s.stat0.vindpm_stat) furi_string_cat_printf(arena, " VINDPM");
    if(s.stat0.iindpm_stat) furi_string_cat_printf(arena, " IINDPM");
    if(furi_string_size(arena) == 0) furi_string_set(arena, " ---");
    printf("%s\r\n", furi_string_get_cstr(arena));

    // VBUS_STAT and CHG_STAT are enums — always show
    printf(
        "  Status1: 0x%02X VBUS: \"%s\" CHG: \"%s\"",
        s.data[1],
        power_cli_get_charger_status1_vbus_str(s.stat1.vbus_stat),
        power_cli_get_status1_charger_str(s.stat1.chg_stat));
    if(s.stat1.bc12_done_stat) printf(" BC1.2_DONE");
    printf("\r\n");

    // ICO_STAT is an enum — always show
    printf("  Status2: 0x%02X ICO: \"%s\"", s.data[2], power_cli_get_status2_ico_str(s.stat2.ico_stat));
    if(s.stat2.vbat_present_stat) printf(" VBAT_PRESENT");
    if(s.stat2.dpdm_stat) printf(" DPDM");
    if(s.stat2.treg_stat) printf(" TREG");
    printf("\r\n");

    printf("  Status3: 0x%02X", s.data[3]);
    furi_string_set(arena, "");
    if(s.stat3.prechg_tmr_stat) furi_string_cat_printf(arena, " PRECHG_TMR");
    if(s.stat3.trichg_tmr_stat) furi_string_cat_printf(arena, " TRICHG_TMR");
    if(s.stat3.chg_tmr_stat) furi_string_cat_printf(arena, " CHG_TMR");
    if(s.stat3.vsys_stat) furi_string_cat_printf(arena, " VSYS");
    if(s.stat3.adc_done_stat) furi_string_cat_printf(arena, " ADC_DONE");
    if(s.stat3.acrb1_stat) furi_string_cat_printf(arena, " ACRB1");
    if(s.stat3.acrb2_stat) furi_string_cat_printf(arena, " ACRB2");
    if(furi_string_size(arena) == 0) furi_string_set(arena, " ---");
    printf("%s\r\n", furi_string_get_cstr(arena));

    printf("  Status4: 0x%02X", s.data[4]);
    furi_string_set(arena, "");
    if(s.stat4.ts_hot_stat) furi_string_cat_printf(arena, " TS_HOT");
    if(s.stat4.ts_warm_stat) furi_string_cat_printf(arena, " TS_WARM");
    if(s.stat4.ts_cool_stat) furi_string_cat_printf(arena, " TS_COOL");
    if(s.stat4.ts_cold_stat) furi_string_cat_printf(arena, " TS_COLD");
    if(s.stat4.vbatotg_low_stat) furi_string_cat_printf(arena, " VBATOTG_LOW");
    if(furi_string_size(arena) == 0) furi_string_set(arena, " ---");
    printf("%s\r\n\r\n", furi_string_get_cstr(arena));
}

static void power_cli_print_charger_faults(Power* power, FuriString* arena) {
    Bq25792FaultStatusReg f = {0};
    power_bq25792_get_charger_fault(power, &f);

    printf("  Fault0:  0x%02X", f.data[0]);
    furi_string_set(arena, "");
    if(f.fault0.vac1_ovp_stat) furi_string_cat_printf(arena, " VAC1_OVP");
    if(f.fault0.vac2_ovp_stat) furi_string_cat_printf(arena, " VAC2_OVP");
    if(f.fault0.conv_ocp_stat) furi_string_cat_printf(arena, " CONV_OCP");
    if(f.fault0.ibat_ocp_stat) furi_string_cat_printf(arena, " IBAT_OCP");
    if(f.fault0.ibus_ocp_stat) furi_string_cat_printf(arena, " IBUS_OCP");
    if(f.fault0.vbat_ovp_stat) furi_string_cat_printf(arena, " VBAT_OVP");
    if(f.fault0.vbus_ovp_stat) furi_string_cat_printf(arena, " VBUS_OVP");
    if(f.fault0.ibat_reg_stat) furi_string_cat_printf(arena, " IBAT_REG");
    if(furi_string_size(arena) == 0) furi_string_set(arena, " ---");
    printf("%s\r\n", furi_string_get_cstr(arena));

    printf("  Fault1:  0x%02X", f.data[1]);
    furi_string_set(arena, "");
    if(f.fault1.tshut_stat) furi_string_cat_printf(arena, " TSHUT");
    if(f.fault1.otg_uvp_stat) furi_string_cat_printf(arena, " OTG_UVP");
    if(f.fault1.otg_ovp_stat) furi_string_cat_printf(arena, " OTG_OVP");
    if(f.fault1.vsys_ovp_stat) furi_string_cat_printf(arena, " VSYS_OVP");
    if(f.fault1.vsys_short_stat) furi_string_cat_printf(arena, " VSYS_SHORT");
    if(furi_string_size(arena) == 0) furi_string_set(arena, " ---");
    printf("%s\r\n\r\n", furi_string_get_cstr(arena));
}

static void power_cli_print_charger_irq(Power* power, FuriString* arena) {
    Bq25792ChargerFlagReg fl = {0};
    power_bq25792_get_charger_irq_flags(power, &fl);

    printf("  IRQ0:    0x%02X", fl.data[0]);
    furi_string_set(arena, "");
    if(fl.flag0.vbus_present_flag) furi_string_cat_printf(arena, " VBUS_PRESENT");
    if(fl.flag0.ac1_present_flag) furi_string_cat_printf(arena, " AC1_PRESENT");
    if(fl.flag0.ac2_present_flag) furi_string_cat_printf(arena, " AC2_PRESENT");
    if(fl.flag0.pg_flag) furi_string_cat_printf(arena, " PG");
    if(fl.flag0.poorsrc_flag) furi_string_cat_printf(arena, " POORSRC");
    if(fl.flag0.wd_flag) furi_string_cat_printf(arena, " WD");
    if(fl.flag0.vindpm_flag) furi_string_cat_printf(arena, " VINDPM");
    if(fl.flag0.iindpm_flag) furi_string_cat_printf(arena, " IINDPM");
    if(furi_string_size(arena) == 0) furi_string_set(arena, " ---");
    printf("%s\r\n", furi_string_get_cstr(arena));

    printf("  IRQ1:    0x%02X", fl.data[1]);
    furi_string_set(arena, "");
    if(fl.flag1.bc12_done_flag) furi_string_cat_printf(arena, " BC1.2_DONE");
    if(fl.flag1.vbat_present_flag) furi_string_cat_printf(arena, " VBAT_PRESENT");
    if(fl.flag1.treg_flag) furi_string_cat_printf(arena, " TREG");
    if(fl.flag1.vbus_flag) furi_string_cat_printf(arena, " VBUS");
    if(fl.flag1.ico_flag) furi_string_cat_printf(arena, " ICO");
    if(fl.flag1.chg_flag) furi_string_cat_printf(arena, " CHG");
    if(furi_string_size(arena) == 0) furi_string_set(arena, " ---");
    printf("%s\r\n", furi_string_get_cstr(arena));

    printf("  IRQ2:    0x%02X", fl.data[2]);
    furi_string_set(arena, "");
    if(fl.flag2.topoff_tmr_flag) furi_string_cat_printf(arena, " TOPOFF_TMR");
    if(fl.flag2.prechg_tmr_flag) furi_string_cat_printf(arena, " PRECHG_TMR");
    if(fl.flag2.trichg_tmr_flag) furi_string_cat_printf(arena, " TRICHG_TMR");
    if(fl.flag2.chg_tmr_flag) furi_string_cat_printf(arena, " CHG_TMR");
    if(fl.flag2.vsys_flag) furi_string_cat_printf(arena, " VSYS");
    if(fl.flag2.adc_done_flag) furi_string_cat_printf(arena, " ADC_DONE");
    if(fl.flag2.dpdm_done_flag) furi_string_cat_printf(arena, " DPDM_DONE");
    if(furi_string_size(arena) == 0) furi_string_set(arena, " ---");
    printf("%s\r\n", furi_string_get_cstr(arena));

    printf("  IRQ3:    0x%02X", fl.data[3]);
    furi_string_set(arena, "");
    if(fl.flag3.ts_hot_flag) furi_string_cat_printf(arena, " TS_HOT");
    if(fl.flag3.ts_warm_flag) furi_string_cat_printf(arena, " TS_WARM");
    if(fl.flag3.ts_cool_flag) furi_string_cat_printf(arena, " TS_COOL");
    if(fl.flag3.ts_cold_flag) furi_string_cat_printf(arena, " TS_COLD");
    if(fl.flag3.vbatotg_low_flag) furi_string_cat_printf(arena, " VBATOTG_LOW");
    if(furi_string_size(arena) == 0) furi_string_set(arena, " ---");
    printf("%s\r\n", furi_string_get_cstr(arena));
}

void power_cli(Cli* cli, FuriString* args, void* context) {
    UNUSED(cli);
    UNUSED(args);
    UNUSED(context);
    Power* power = furi_record_open(RECORD_POWER);
    FuriString* arena = furi_string_alloc();

    while(!cli_cmd_interrupt_received(cli)) {
        printf("\e[2J\e[0;0f"); // Clear display and return to 0
        power_cli_print_ina219(power);
        power_cli_print_bq25792(power);
        power_cli_print_charger_status(power, arena);
        power_cli_print_charger_faults(power, arena);
        power_cli_print_charger_irq(power, arena);
        furi_delay_ms(500);
    }

    furi_string_free(arena);
    furi_record_close(RECORD_POWER);
}
