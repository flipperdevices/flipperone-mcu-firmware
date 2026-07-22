#include "power_show_cli.h"

#include <cli/args.h>
#include <cli/cli_ansi.h>
#include <cli/cli_command.h>

#include <power/power.h>

static char* power_show_cli_get_charger_status1_vbus_str(uint8_t stat) {
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

static char* power_show_cli_get_status1_charger_str(uint8_t stat) {
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

static char* power_show_cli_get_status2_ico_str(uint8_t stat) {
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

static void power_show_cli_print_ina219(Power* power) {
    float bus_v = power_ina219_get_voltage_v(power);
    float current_a = power_ina219_get_current_a(power);
    float power_w = power_ina219_get_power_w(power);
    float shunt_mv = power_ina219_get_shunt_voltage_mv(power);

    // clang-format off
    printf(
        ANSI_ERASE_LINE(ANSI_ERASE_ENTIRE) "INA219:\r\n"
        ANSI_ERASE_LINE(ANSI_ERASE_ENTIRE) "  VSYS:  %.3fV\r\n"
        ANSI_ERASE_LINE(ANSI_ERASE_ENTIRE) "  ISYS:  %.2fmA\r\n"
        ANSI_ERASE_LINE(ANSI_ERASE_ENTIRE) "  Shunt: %.4fmV\r\n"
        ANSI_ERASE_LINE(ANSI_ERASE_ENTIRE) "  Power: %.2fW\r\n\r\n",
        bus_v,
        current_a * 1000.0f,
        shunt_mv,
        power_w);
    // clang-format on
}

static void power_show_cli_print_bq25792(Power* power) {
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
    uint16_t ico_current_limit_ma = 0;

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
    power_bq25792_get_ico_current_limit_ma(power, &ico_current_limit_ma);

    // clang-format off
    printf(
        ANSI_ERASE_LINE(ANSI_ERASE_ENTIRE) "BQ25792:\r\n" 
        ANSI_ERASE_LINE(ANSI_ERASE_ENTIRE) "  VSYS:    %.3fV\r\n" 
        ANSI_ERASE_LINE(ANSI_ERASE_ENTIRE) "  VBUS:    %.3fV\r\n" 
        ANSI_ERASE_LINE(ANSI_ERASE_ENTIRE) "  IBUS:    %dmA\r\n" 
        ANSI_ERASE_LINE(ANSI_ERASE_ENTIRE) "  VBAT:    %.3fV\r\n" 
        ANSI_ERASE_LINE(ANSI_ERASE_ENTIRE) "  IBAT:    %dmA\r\n" 
        ANSI_ERASE_LINE(ANSI_ERASE_ENTIRE) "  ChgTemp: %.2fC\r\n"
        ANSI_ERASE_LINE(ANSI_ERASE_ENTIRE) "  BatTemp: %.2fC\r\n"
        ANSI_ERASE_LINE(ANSI_ERASE_ENTIRE) "  IINDPM:  %dmA\r\n"
        ANSI_ERASE_LINE(ANSI_ERASE_ENTIRE) "  VREG:    %dmV\r\n"
        ANSI_ERASE_LINE(ANSI_ERASE_ENTIRE) "  ICHG:    %dmV\r\n"
        ANSI_ERASE_LINE(ANSI_ERASE_ENTIRE) "  ICO:     %dmA\r\n\r\n",
        (float_t)vsys_mv / 1000.0f,
        (float_t)vbus_mv / 1000.0f,
        ibus_ma,
        (float_t)vbat_mv / 1000.0f,
        ibat_ma,
        charger_temp,
        battery_temp,
        input_current_limit_ma,
        charge_voltage_limit_mv,
        charge_current_limit_ma,
        ico_current_limit_ma);
    // clang-format on
}

static void power_show_cli_print_bq28z620(Power* power) {
    uint16_t time_to_empty_min = 0;
    float temperature_c = 0;
    float voltage_v = 0;
    int16_t current_ma = 0;
    uint16_t remaining_capacity_mah = 0;
    uint16_t full_charge_capacity_mah = 0;
    int16_t average_current_ma = 0;
    uint16_t average_time_to_empty_min = 0;
    uint16_t average_time_to_full_min = 0;
    int16_t standby_current_ma = 0;
    uint16_t standby_time_to_empty_min = 0;
    int16_t max_load_current_ma = 0;
    uint16_t max_load_time_to_empty_min = 0;
    int16_t average_power_mw = 0;
    float internal_temperature_c = 0;
    uint16_t cycle_count = 0;
    uint8_t relative_state_of_charge_percent = 0;
    uint8_t state_of_health_percent = 0;
    float charging_voltage_v = 0;
    int16_t charging_current_ma = 0;
    uint16_t design_capacity_mah = 0;

    power_bq28z620_get_time_to_empty(power, &time_to_empty_min);
    power_bq28z620_get_temperature(power, &temperature_c);
    power_bq28z620_get_voltage(power, &voltage_v);
    power_bq28z620_get_current(power, &current_ma);
    power_bq28z620_get_remaining_capacity(power, &remaining_capacity_mah);
    power_bq28z620_get_full_charge_capacity(power, &full_charge_capacity_mah);
    power_bq28z620_get_average_current(power, &average_current_ma);
    power_bq28z620_get_average_time_to_empty(power, &average_time_to_empty_min);
    power_bq28z620_get_average_time_to_full(power, &average_time_to_full_min);
    power_bq28z620_get_standby_current(power, &standby_current_ma);
    power_bq28z620_get_standby_time_to_empty(power, &standby_time_to_empty_min);
    power_bq28z620_get_max_load_current(power, &max_load_current_ma);
    power_bq28z620_get_max_load_time_to_empty(power, &max_load_time_to_empty_min);
    power_bq28z620_get_average_power(power, &average_power_mw);
    power_bq28z620_get_internal_temperature(power, &internal_temperature_c);
    power_bq28z620_get_cycle_count(power, &cycle_count);
    power_bq28z620_get_relative_state_of_charge(power, &relative_state_of_charge_percent);
    power_bq28z620_get_state_of_health(power, &state_of_health_percent);
    power_bq28z620_get_charging_voltage(power, &charging_voltage_v);
    power_bq28z620_get_charging_current(power, &charging_current_ma);
    power_bq28z620_get_design_capacity(power, &design_capacity_mah);

    // clang-format off
    printf(
        ANSI_ERASE_LINE(ANSI_ERASE_ENTIRE) "BQ28Z620:\r\n"
        ANSI_ERASE_LINE(ANSI_ERASE_ENTIRE) "  Voltage:            %.3fV\r\n"
        ANSI_ERASE_LINE(ANSI_ERASE_ENTIRE) "  Current:            %dmA\r\n"
        ANSI_ERASE_LINE(ANSI_ERASE_ENTIRE) "  Temp:               %.2fC\r\n"
        ANSI_ERASE_LINE(ANSI_ERASE_ENTIRE) "  IntTemp:            %.2fC\r\n"
        ANSI_ERASE_LINE(ANSI_ERASE_ENTIRE) "  RemCap:             %dmAh\r\n"
        ANSI_ERASE_LINE(ANSI_ERASE_ENTIRE) "  FullCap:            %dmAh\r\n"
        ANSI_ERASE_LINE(ANSI_ERASE_ENTIRE) "  AvgCurr:            %dmA\r\n"
        ANSI_ERASE_LINE(ANSI_ERASE_ENTIRE) "  AvgPwr:             %dmW\r\n"
        ANSI_ERASE_LINE(ANSI_ERASE_ENTIRE) "  CycleCt:            %d\r\n"
        ANSI_ERASE_LINE(ANSI_ERASE_ENTIRE) "  SoC:                %d%%\r\n"
        ANSI_ERASE_LINE(ANSI_ERASE_ENTIRE) "  SoH:                %d%%\r\n"
        ANSI_ERASE_LINE(ANSI_ERASE_ENTIRE) "  ChgVolt:            %.3fV\r\n"
        ANSI_ERASE_LINE(ANSI_ERASE_ENTIRE) "  ChgCurr:            %dmA\r\n"
        ANSI_ERASE_LINE(ANSI_ERASE_ENTIRE) "  DesignCap:          %dmAh\r\n"
        ANSI_ERASE_LINE(ANSI_ERASE_ENTIRE) "  TimeToEmpty:        %d min\r\n"
        ANSI_ERASE_LINE(ANSI_ERASE_ENTIRE) "  AvgTimeToEmpty:     %d min\r\n"
        ANSI_ERASE_LINE(ANSI_ERASE_ENTIRE) "  TimeToFull:         %d min\r\n"
        ANSI_ERASE_LINE(ANSI_ERASE_ENTIRE) "  StandbyCurr:        %dmA\r\n"
        ANSI_ERASE_LINE(ANSI_ERASE_ENTIRE) "  StandbyTimeToEmpty: %d min\r\n"
        ANSI_ERASE_LINE(ANSI_ERASE_ENTIRE) "  MaxLoadCurr:        %dmA\r\n"
        ANSI_ERASE_LINE(ANSI_ERASE_ENTIRE) "  MaxLoadTimeToEmpty: %d min\r\n",
        voltage_v,
        current_ma,
        temperature_c,
        internal_temperature_c,
        remaining_capacity_mah,
        full_charge_capacity_mah,
        average_current_ma,
        average_power_mw,
        cycle_count,
        relative_state_of_charge_percent,
        state_of_health_percent,
        charging_voltage_v,
        charging_current_ma,
        design_capacity_mah,
        time_to_empty_min,
        average_time_to_empty_min,
        average_time_to_full_min,
        standby_current_ma,
        standby_time_to_empty_min,
        max_load_current_ma,
        max_load_time_to_empty_min);
    // clang-format on
}

static void power_show_cli_print_charger_status(Power* power, FuriString* arena) {
    Bq25792ChargerStatusReg s = {0};
    power_bq25792_get_charger_status(power, &s);
    furi_string_set(arena, "");

    printf(ANSI_ERASE_LINE(ANSI_ERASE_ENTIRE) "  Status0: 0x%02X", s.data[0]);
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
        ANSI_ERASE_LINE(ANSI_ERASE_ENTIRE) "  Status1: 0x%02X VBUS: \"%s\" CHG: \"%s\"",
        s.data[1],
        power_show_cli_get_charger_status1_vbus_str(s.stat1.vbus_stat),
        power_show_cli_get_status1_charger_str(s.stat1.chg_stat));
    if(s.stat1.bc12_done_stat) printf(" BC1.2_DONE");
    printf("\r\n");

    // ICO_STAT is an enum — always show
    printf(ANSI_ERASE_LINE(ANSI_ERASE_ENTIRE) "  Status2: 0x%02X ICO: \"%s\"", s.data[2], power_show_cli_get_status2_ico_str(s.stat2.ico_stat));
    if(s.stat2.vbat_present_stat) printf(" VBAT_PRESENT");
    if(s.stat2.dpdm_stat) printf(" DPDM");
    if(s.stat2.treg_stat) printf(" TREG");
    printf("\r\n");

    printf(ANSI_ERASE_LINE(ANSI_ERASE_ENTIRE) "  Status3: 0x%02X", s.data[3]);
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

    printf(ANSI_ERASE_LINE(ANSI_ERASE_ENTIRE) "  Status4: 0x%02X", s.data[4]);
    furi_string_set(arena, "");
    if(s.stat4.ts_hot_stat) furi_string_cat_printf(arena, " TS_HOT");
    if(s.stat4.ts_warm_stat) furi_string_cat_printf(arena, " TS_WARM");
    if(s.stat4.ts_cool_stat) furi_string_cat_printf(arena, " TS_COOL");
    if(s.stat4.ts_cold_stat) furi_string_cat_printf(arena, " TS_COLD");
    if(s.stat4.vbatotg_low_stat) furi_string_cat_printf(arena, " VBATOTG_LOW");
    if(furi_string_size(arena) == 0) furi_string_set(arena, " ---");
    printf("%s\r\n\r\n", furi_string_get_cstr(arena));
}

static void power_show_cli_print_charger_faults(Power* power, FuriString* arena) {
    Bq25792FaultStatusReg f = {0};
    power_bq25792_get_charger_fault(power, &f);

    printf(ANSI_ERASE_LINE(ANSI_ERASE_ENTIRE) "  Fault0:  0x%02X", f.data[0]);
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

    printf(ANSI_ERASE_LINE(ANSI_ERASE_ENTIRE) "  Fault1:  0x%02X", f.data[1]);
    furi_string_set(arena, "");
    if(f.fault1.tshut_stat) furi_string_cat_printf(arena, " TSHUT");
    if(f.fault1.otg_uvp_stat) furi_string_cat_printf(arena, " OTG_UVP");
    if(f.fault1.otg_ovp_stat) furi_string_cat_printf(arena, " OTG_OVP");
    if(f.fault1.vsys_ovp_stat) furi_string_cat_printf(arena, " VSYS_OVP");
    if(f.fault1.vsys_short_stat) furi_string_cat_printf(arena, " VSYS_SHORT");
    if(furi_string_size(arena) == 0) furi_string_set(arena, " ---");
    printf("%s\r\n\r\n", furi_string_get_cstr(arena));
}

static void power_show_cli_print_bq28z620_control_status(Power* power, FuriString* arena) {
    Bq28z620StdCmdControlStatusRegBits s = {0};
    power_bq28z620_get_control_status(power, &s);

    printf(ANSI_ERASE_LINE(ANSI_ERASE_ENTIRE) "  Control Status: 0x%04X", *(uint16_t*)&s);

    furi_string_set(arena, "");
    if(s.qmax) furi_string_cat_printf(arena, " QMAX");
    if(s.vok) furi_string_cat_printf(arena, " VOK");
    if(s.r_dis) furi_string_cat_printf(arena, " R_DIS");
    if(s.ldmd) furi_string_cat_printf(arena, " LDMD");
    if(s.checksum_valid) furi_string_cat_printf(arena, " CHECKSUM_VALID");
    if(s.authcalm) furi_string_cat_printf(arena, " AUTHCALM");
    if(s.sec == 0b00) {
        furi_string_cat_printf(arena, " SEC: Full_Access");
    } else if(s.sec == 0b01) {
        furi_string_cat_printf(arena, " SEC: Reserved");
    } else if(s.sec == 0b10) {
        furi_string_cat_printf(arena, " SEC: Unsealed");
    } else if(s.sec == 0b11) {
        furi_string_cat_printf(arena, " SEC: Sealed");
    } else {
        furi_string_cat_printf(arena, " SEC: Unknown(%02b)", s.sec);
    }
    if(furi_string_size(arena) == 0) furi_string_set(arena, " ---");
    printf("%s\r\n", furi_string_get_cstr(arena));
}

static void power_show_cli_print_bq28z620_battery_status(Power* power, FuriString* arena) {
    Bq28z620StdCmdBatteryStatusRegBits s = {0};
    power_bq28z620_get_battery_status(power, &s);

    printf(ANSI_ERASE_LINE(ANSI_ERASE_ENTIRE) "  Battery Status: 0x%04X", *(uint16_t*)&s);

    furi_string_set(arena, "");
    if(s.error_code == 0x00) {
        furi_string_cat_printf(arena, " ERROR_CODE: OK");
    } else if(s.error_code == 0x1) {
        furi_string_cat_printf(arena, " ERROR_CODE: Busy");
    } else if(s.error_code == 0x2) {
        furi_string_cat_printf(arena, " ERROR_CODE: Reserved Command");
    } else if(s.error_code == 0x3) {
        furi_string_cat_printf(arena, " ERROR_CODE: Unsupported Command");
    } else if(s.error_code == 0x4) {
        furi_string_cat_printf(arena, " ERROR_CODE: AccessDenied");
    } else if(s.error_code == 0x5) {
        furi_string_cat_printf(arena, " ERROR_CODE: Overflow/Underflow");
    } else if(s.error_code == 0x6) {
        furi_string_cat_printf(arena, " ERROR_CODE: BadSize");
    } else if(s.error_code == 0x7) {
        furi_string_cat_printf(arena, " ERROR_CODE: UnknownError");
    } else {
        furi_string_cat_printf(arena, " ERROR_CODE: Unknown(%d)", s.error_code);
    }
    if(s.fully_discharged) furi_string_cat_printf(arena, " FULLY_DISCHARGED");
    if(s.fully_charged) furi_string_cat_printf(arena, " FULLY_CHARGED");
    if(s.discharging) furi_string_cat_printf(arena, " DISCHARGING");
    if(s.initialization) furi_string_cat_printf(arena, " INITIALIZATION");
    if(s.remaining_time_alarm) furi_string_cat_printf(arena, " REMAINING_TIME_ALARM");
    if(s.remaining_capacity_alarm) furi_string_cat_printf(arena, " REMAINING_CAPACITY_ALARM");
    if(s.terminate_discharge_alarm) furi_string_cat_printf(arena, " TERMINATE_DISCHARGE_ALARM");
    if(s.overtemperature_alarm) furi_string_cat_printf(arena, " OVERTEMPERATURE_ALARM");
    if(s.terminate_charge_alarm) furi_string_cat_printf(arena, " TERMINATE_CHARGE_ALARM");
    if(s.overcharged_alarm) furi_string_cat_printf(arena, " OVERCHARGED_ALARM");
    if(furi_string_size(arena) == 0) furi_string_set(arena, " ---");
    printf("%s\r\n", furi_string_get_cstr(arena));
}

bool power_show_cli(PipeSide* pipe, FuriString* args) {
    UNUSED(pipe);
    UNUSED(args);
    Power* power = furi_record_open(RECORD_POWER);
    FuriString* arena = furi_string_alloc();
    printf(ANSI_ERASE_DISPLAY(ANSI_ERASE_ENTIRE)); // Clear display
    while(!cli_is_pipe_broken_or_is_etx_next_char(pipe)) {
        printf(ANSI_CURSOR_POS("1", "1")); // Return to 0, but don't clear (faster and less flickery)
        power_show_cli_print_ina219(power);
        power_show_cli_print_bq25792(power);
        power_show_cli_print_charger_status(power, arena);
        power_show_cli_print_charger_faults(power, arena);

        power_show_cli_print_bq28z620(power);
        power_show_cli_print_bq28z620_control_status(power, arena);
        power_show_cli_print_bq28z620_battery_status(power, arena);
        furi_delay_ms(500);
    }

    furi_string_free(arena);
    furi_record_close(RECORD_POWER);
    return true;
}
