#include "ucsi_ppm_config.h"

// Field masks for Fixed Supply PDO (PD R3.0 §6.4.1, Table 6-9 / Table 6-14).
// Voltage field: 10 bits, in 50 mV units -> max 1023 * 50 = 51150 mV.
// Current field: 10 bits, in 10 mA units -> max 1023 * 10 = 10230 mA.
// Out-of-range values are silently truncated by the mask; validation is
// done by ucsi_ppm_init (see plan/api.md §5.1).
#define PDO_FIXED_FIELD_MASK      0x3FFu
#define PDO_FIXED_VOLTAGE_SHIFT   10
#define PDO_FIXED_CURRENT_SHIFT   0
#define PDO_FIXED_VOLTAGE_UNIT_MV 50
#define PDO_FIXED_CURRENT_UNIT_MA 10

// PDO type (bits 31:30): 00b = Fixed Supply, in both source/sink tables.

// Source Fixed Supply PDO #1 flag bits (PD R3.0 Table 6-9).
// Bits 28 (USB Suspend), 24 (Unchunked Ext Msgs), 23 (EPR Capable),
// 21:20 (Peak Current) are fixed to zero per pd-scope.md §2.1.
#define PDO_FIXED_SRC_DRP_BIT           (1u << 29)
#define PDO_FIXED_SRC_UNCONSTRAINED_BIT (1u << 27)
#define PDO_FIXED_SRC_USB_COMMS_BIT     (1u << 26)
#define PDO_FIXED_SRC_DR_DATA_BIT       (1u << 25)

// Sink Fixed Supply PDO #1 flag bits (PD R3.0 Table 6-14).
// Bits 24:23 (FRS required current) are fixed to zero — FRS is out of
// scope (pd-scope.md §8). Bit 28 differs from source: here it means
// Higher Capability, in source it means USB Suspend Supported.
#define PDO_FIXED_SNK_DRP_BIT           (1u << 29)
#define PDO_FIXED_SNK_HIGHER_CAP_BIT    (1u << 28)
#define PDO_FIXED_SNK_UNCONSTRAINED_BIT (1u << 27)
#define PDO_FIXED_SNK_USB_COMMS_BIT     (1u << 26)
#define PDO_FIXED_SNK_DR_DATA_BIT       (1u << 25)

UcsiPpmPdo
    ucsi_ppm_pdo_fixed_source(uint16_t voltage_mv, uint16_t max_current_ma, bool dual_role_power, bool unconstrained_power, bool usb_comms, bool dual_role_data) {
    UcsiPpmPdo pdo = 0;
    if(dual_role_power) pdo |= PDO_FIXED_SRC_DRP_BIT;
    if(unconstrained_power) pdo |= PDO_FIXED_SRC_UNCONSTRAINED_BIT;
    if(usb_comms) pdo |= PDO_FIXED_SRC_USB_COMMS_BIT;
    if(dual_role_data) pdo |= PDO_FIXED_SRC_DR_DATA_BIT;
    pdo |= ((UcsiPpmPdo)(voltage_mv / PDO_FIXED_VOLTAGE_UNIT_MV) & PDO_FIXED_FIELD_MASK) << PDO_FIXED_VOLTAGE_SHIFT;
    pdo |= ((UcsiPpmPdo)(max_current_ma / PDO_FIXED_CURRENT_UNIT_MA) & PDO_FIXED_FIELD_MASK) << PDO_FIXED_CURRENT_SHIFT;
    return pdo;
}

UcsiPpmPdo ucsi_ppm_pdo_fixed_sink(
    uint16_t voltage_mv,
    uint16_t max_current_ma,
    bool dual_role_power,
    bool higher_capability,
    bool unconstrained_power,
    bool usb_comms,
    bool dual_role_data) {
    UcsiPpmPdo pdo = 0;
    if(dual_role_power) pdo |= PDO_FIXED_SNK_DRP_BIT;
    if(higher_capability) pdo |= PDO_FIXED_SNK_HIGHER_CAP_BIT;
    if(unconstrained_power) pdo |= PDO_FIXED_SNK_UNCONSTRAINED_BIT;
    if(usb_comms) pdo |= PDO_FIXED_SNK_USB_COMMS_BIT;
    if(dual_role_data) pdo |= PDO_FIXED_SNK_DR_DATA_BIT;
    pdo |= ((UcsiPpmPdo)(voltage_mv / PDO_FIXED_VOLTAGE_UNIT_MV) & PDO_FIXED_FIELD_MASK) << PDO_FIXED_VOLTAGE_SHIFT;
    pdo |= ((UcsiPpmPdo)(max_current_ma / PDO_FIXED_CURRENT_UNIT_MA) & PDO_FIXED_FIELD_MASK) << PDO_FIXED_CURRENT_SHIFT;
    return pdo;
}
