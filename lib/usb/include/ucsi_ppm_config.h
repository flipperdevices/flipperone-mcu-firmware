#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "ucsi_ppm_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UCSI_PPM_MAX_PDOS 7

typedef uint32_t UcsiPpmPdo;

typedef struct {
    UcsiPpmPdo pdos[UCSI_PPM_MAX_PDOS];
    uint8_t count;
} UcsiPpmPdoList;

UcsiPpmPdo ucsi_ppm_pdo_fixed_source(
    uint16_t voltage_mv,
    uint16_t max_current_ma,
    bool dual_role_power,
    bool unconstrained_power,
    bool usb_comms,
    bool dual_role_data);

UcsiPpmPdo ucsi_ppm_pdo_fixed_sink(
    uint16_t voltage_mv,
    uint16_t max_current_ma,
    bool dual_role_power,
    bool higher_capability,
    bool unconstrained_power,
    bool usb_comms,
    bool dual_role_data);

typedef enum {
    UcsiPpmCcModeRpOnly,
    UcsiPpmCcModeRdOnly,
    UcsiPpmCcModeDrp,
    UcsiPpmCcModeDisabled,
} UcsiPpmCcOperationMode;

typedef enum {
    UcsiPpmDrpFirstSrc,
    UcsiPpmDrpFirstSnk,
} UcsiPpmDrpFirstRole;

typedef enum {
    UcsiPpmRpCurrentUsbDefault,
    UcsiPpmRpCurrent1A5,
    UcsiPpmRpCurrent3A,
} UcsiPpmRpCurrent;

typedef struct {
    void* hal_ctx;

    UcsiPpmTimeMsFn time_ms;
    UcsiPpmAlertFn alert;
    UcsiPpmI2cReadFn i2c_read;
    UcsiPpmI2cWriteFn i2c_write;
    UcsiPpmI2cWriteReadFn i2c_write_read; // optional; see ucsi_ppm_hal.h
    UcsiPpmGpioWriteFn gpio_write_vbus_source;
    UcsiPpmPowerSupplySetFn power_supply_set;
    UcsiPpmHasAltPowerFn has_alt_power;

    UcsiPpmGpioReadFn gpio_read_fusb302_int;
    UcsiPpmGpioWriteFn gpio_write_vbus_discharge;
    UcsiPpmLogFn log;

    uint8_t fusb302_i2c_addr;

    UcsiPpmCcOperationMode initial_cc_operation_mode;
    UcsiPpmDrpFirstRole drp_advertise_first;
    UcsiPpmRpCurrent source_rp_current;

    UcsiPpmPdoList source_caps;
    UcsiPpmPdoList sink_caps;

    bool supports_disabled_state;
    bool supports_battery_charging;
    bool supports_usb_pd;
    bool supports_typec_current;
    bool power_source_ac;
    bool power_source_other;
    bool power_source_vbus;

    bool supports_set_ccom;
    bool supports_alt_mode_details;
    bool supports_alt_mode_override;
    bool supports_pdo_details;
    bool supports_cable_details;
    bool supports_external_supply_notif;
    bool supports_pd_reset_notif;
    bool supports_get_pd_message;
    bool supports_get_attention_vdo;
    bool supports_fw_update_request;
    bool supports_negotiated_pl_notif;
    bool supports_security_request;
    bool supports_set_retimer_mode;
    bool supports_chunking;

    bool connector_usb2_capable;
    bool connector_usb3_capable;
} UcsiPpmConfig;

#ifdef __cplusplus
}
#endif
