#include "ucsi_ppm.h"
#include "ucsi_ppm_i.h"

#include <string.h>

// --- CCI helpers -----------------------------------------------------------

static void cci_store(UcsiPpm* ppm, uint32_t cci) {
    ppm->regfile[UCSI_PPM_OFFSET_CCI + 0] = (uint8_t)(cci & 0xFFu);
    ppm->regfile[UCSI_PPM_OFFSET_CCI + 1] = (uint8_t)((cci >> 8) & 0xFFu);
    ppm->regfile[UCSI_PPM_OFFSET_CCI + 2] = (uint8_t)((cci >> 16) & 0xFFu);
    ppm->regfile[UCSI_PPM_OFFSET_CCI + 3] = (uint8_t)((cci >> 24) & 0xFFu);
}

static uint32_t cci_load(const UcsiPpm* ppm) {
    return ((uint32_t)ppm->regfile[UCSI_PPM_OFFSET_CCI + 0]) |
           ((uint32_t)ppm->regfile[UCSI_PPM_OFFSET_CCI + 1] << 8) |
           ((uint32_t)ppm->regfile[UCSI_PPM_OFFSET_CCI + 2] << 16) |
           ((uint32_t)ppm->regfile[UCSI_PPM_OFFSET_CCI + 3] << 24);
}

static uint32_t cci_with_data_length(uint32_t flags, uint8_t data_length) {
    return flags | ((uint32_t)data_length << UCSI_PPM_CCI_DATA_LENGTH_SHIFT);
}

// --- bmAttributes / bmOptionalFeatures packing -----------------------------
// commands.md §2.6 Table 6-14 / §1.6 Table 6-88.

#define BMATTR_DISABLED_STATE   (1u << 0)
#define BMATTR_BATTERY_CHARGING (1u << 1)
#define BMATTR_USB_PD           (1u << 2)
#define BMATTR_TYPEC_CURRENT    (1u << 6)
#define BMATTR_POWER_AC         (1u << 8)
#define BMATTR_POWER_OTHER      (1u << 10)
#define BMATTR_POWER_VBUS       (1u << 14)

#define BMOPT_SET_CCOM              (1u << 0)
#define BMOPT_SET_POWER_LEVEL       (1u << 1) // always 1 per spec
#define BMOPT_ALT_MODE_DETAILS      (1u << 2)
#define BMOPT_ALT_MODE_OVERRIDE     (1u << 3)
#define BMOPT_PDO_DETAILS           (1u << 4)
#define BMOPT_CABLE_DETAILS         (1u << 5)
#define BMOPT_EXTERNAL_SUPPLY_NOTIF (1u << 6)
#define BMOPT_PD_RESET_NOTIF        (1u << 7)
#define BMOPT_GET_PD_MESSAGE        (1u << 8)
#define BMOPT_GET_ATTENTION_VDO     (1u << 9)
#define BMOPT_FW_UPDATE_REQUEST     (1u << 10)
#define BMOPT_NEGOTIATED_PL_NOTIF   (1u << 11)
#define BMOPT_SECURITY_REQUEST      (1u << 12)
#define BMOPT_SET_RETIMER_MODE      (1u << 13)
#define BMOPT_CHUNKING              (1u << 14)

static uint32_t pack_bm_attributes(const UcsiPpmConfig* c) {
    uint32_t v = 0;
    if(c->supports_disabled_state) v |= BMATTR_DISABLED_STATE;
    if(c->supports_battery_charging) v |= BMATTR_BATTERY_CHARGING;
    if(c->supports_usb_pd) v |= BMATTR_USB_PD;
    if(c->supports_typec_current) v |= BMATTR_TYPEC_CURRENT;
    if(c->power_source_ac) v |= BMATTR_POWER_AC;
    if(c->power_source_other) v |= BMATTR_POWER_OTHER;
    if(c->power_source_vbus) v |= BMATTR_POWER_VBUS;
    return v;
}

static uint32_t pack_bm_optional_features(const UcsiPpmConfig* c) {
    uint32_t v = BMOPT_SET_POWER_LEVEL; // always set
    if(c->supports_set_ccom) v |= BMOPT_SET_CCOM;
    if(c->supports_alt_mode_details) v |= BMOPT_ALT_MODE_DETAILS;
    if(c->supports_alt_mode_override) v |= BMOPT_ALT_MODE_OVERRIDE;
    if(c->supports_pdo_details) v |= BMOPT_PDO_DETAILS;
    if(c->supports_cable_details) v |= BMOPT_CABLE_DETAILS;
    if(c->supports_external_supply_notif) v |= BMOPT_EXTERNAL_SUPPLY_NOTIF;
    if(c->supports_pd_reset_notif) v |= BMOPT_PD_RESET_NOTIF;
    if(c->supports_get_pd_message) v |= BMOPT_GET_PD_MESSAGE;
    if(c->supports_get_attention_vdo) v |= BMOPT_GET_ATTENTION_VDO;
    if(c->supports_fw_update_request) v |= BMOPT_FW_UPDATE_REQUEST;
    if(c->supports_negotiated_pl_notif) v |= BMOPT_NEGOTIATED_PL_NOTIF;
    if(c->supports_security_request) v |= BMOPT_SECURITY_REQUEST;
    if(c->supports_set_retimer_mode) v |= BMOPT_SET_RETIMER_MODE;
    if(c->supports_chunking) v |= BMOPT_CHUNKING;
    return v;
}

// --- handlers --------------------------------------------------------------
// Each handler reads CONTROL bytes from regfile, writes any payload to
// MESSAGE_IN, and returns the CCI bits the dispatcher should write back.

static uint32_t handle_ppm_reset(UcsiPpm* ppm) {
    // architecture.md §3.3: PPM_RESET is always valid; CCI shows Reset Completed,
    // and per commands.md §1.1 all other CCI bits MUST be 0. The bit clears on
    // the next command (not via ACK_CC_CI).
    ucsi_ppm_cmd_reset_state(ppm);
    memset(&ppm->regfile[UCSI_PPM_OFFSET_MESSAGE_IN], 0, UCSI_PPM_SIZE_MESSAGE_IN);
    memset(&ppm->regfile[UCSI_PPM_OFFSET_MESSAGE_OUT], 0, UCSI_PPM_SIZE_MESSAGE_OUT);
    return UCSI_PPM_CCI_RESET_COMPLETED;
}

static uint32_t handle_get_capability(UcsiPpm* ppm) {
    // commands.md §2.6 — 16 bytes of layout.
    const UcsiPpmConfig* c = &ppm->config;
    uint8_t* msg = &ppm->regfile[UCSI_PPM_OFFSET_MESSAGE_IN];

    const uint32_t bm_attr = pack_bm_attributes(c);
    msg[0] = (uint8_t)(bm_attr & 0xFFu);
    msg[1] = (uint8_t)((bm_attr >> 8) & 0xFFu);
    msg[2] = (uint8_t)((bm_attr >> 16) & 0xFFu);
    msg[3] = (uint8_t)((bm_attr >> 24) & 0xFFu);

    // bNumConnectors (7 bits) + reserved (1 bit).
    msg[4] = (uint8_t)(UCSI_PPM_NUM_CONNECTORS & 0x7Fu);

    const uint32_t bm_opt = pack_bm_optional_features(c);
    msg[5] = (uint8_t)(bm_opt & 0xFFu);
    msg[6] = (uint8_t)((bm_opt >> 8) & 0xFFu);
    msg[7] = (uint8_t)((bm_opt >> 16) & 0xFFu);

    msg[8] = (uint8_t)UCSI_PPM_NUM_ALT_MODES;
    msg[9] = 0u; // reserved

    msg[10] = (uint8_t)(UCSI_PPM_VERSION_BC & 0xFFu);
    msg[11] = (uint8_t)((UCSI_PPM_VERSION_BC >> 8) & 0xFFu);
    msg[12] = (uint8_t)(UCSI_PPM_VERSION_PD & 0xFFu);
    msg[13] = (uint8_t)((UCSI_PPM_VERSION_PD >> 8) & 0xFFu);
    msg[14] = (uint8_t)(UCSI_PPM_VERSION_TYPEC & 0xFFu);
    msg[15] = (uint8_t)((UCSI_PPM_VERSION_TYPEC >> 8) & 0xFFu);

    return cci_with_data_length(UCSI_PPM_CCI_COMMAND_COMPLETED, 16u);
}

static uint32_t handle_get_connector_capability(UcsiPpm* ppm) {
    // commands.md §2.7 — 4 bytes (32 bits) of layout.
    const UcsiPpmConfig* c = &ppm->config;
    uint32_t cap = 0;

    // Operation Mode (bits 0..7).
    switch(c->initial_cc_operation_mode) {
    case UcsiPpmCcModeRpOnly:
        cap |= (1u << 0);
        break;
    case UcsiPpmCcModeRdOnly:
        cap |= (1u << 1);
        break;
    case UcsiPpmCcModeDrp:
        cap |= (1u << 2);
        break;
    case UcsiPpmCcModeDisabled:
        break; // no Rp/Rd/DRP bit
    }
    if(c->connector_usb2_capable) cap |= (1u << 5);
    if(c->connector_usb3_capable) cap |= (1u << 6);

    // Provider/Consumer + swap flags (bits 8..13).
    const bool drp = (c->initial_cc_operation_mode == UcsiPpmCcModeDrp);
    const bool rp = drp || (c->initial_cc_operation_mode == UcsiPpmCcModeRpOnly);
    const bool rd = drp || (c->initial_cc_operation_mode == UcsiPpmCcModeRdOnly);
    if(rp) cap |= (1u << 8); // Provider
    if(rd) cap |= (1u << 9); // Consumer
    if(drp) cap |= (1u << 10); // Swap to DFP
    if(drp) cap |= (1u << 11); // Swap to UFP
    if(drp) cap |= (1u << 12); // Swap to SRC
    if(drp) cap |= (1u << 13); // Swap to SNK

    // Extended Operation Mode (bits 14..21), Misc (22..25), RCP (26),
    // Partner PD Revision (27..28), Reserved (29..31) — all zero in v1
    // (no USB4/EPR, no partner connected at GET_CONNECTOR_CAPABILITY time).

    uint8_t* msg = &ppm->regfile[UCSI_PPM_OFFSET_MESSAGE_IN];
    msg[0] = (uint8_t)(cap & 0xFFu);
    msg[1] = (uint8_t)((cap >> 8) & 0xFFu);
    msg[2] = (uint8_t)((cap >> 16) & 0xFFu);
    msg[3] = (uint8_t)((cap >> 24) & 0xFFu);

    return cci_with_data_length(UCSI_PPM_CCI_COMMAND_COMPLETED, 4u);
}

static uint32_t handle_set_notification_enable(UcsiPpm* ppm) {
    // commands.md §2.5: 17-bit Notification Enable bitmap starting at CONTROL bit 16.
    // CONTROL bit 16 == byte 2 bit 0; the field spans byte 2 (8 bits) +
    // byte 3 (8 bits) + byte 4 bit 0 (1 bit) = 17 bits.
    const uint8_t* ctrl = &ppm->regfile[UCSI_PPM_OFFSET_CONTROL];
    uint32_t mask = (uint32_t)ctrl[2];
    mask |= ((uint32_t)ctrl[3]) << 8;
    mask |= ((uint32_t)(ctrl[4] & 0x01u)) << 16;
    ppm->notification_mask = mask;
    return UCSI_PPM_CCI_COMMAND_COMPLETED;
}

static uint32_t handle_ack_cc_ci(UcsiPpm* ppm) {
    // commands.md §2.4: ACK_CC_CI clears the acknowledged indicators in CCI.
    // We honour both Command Completed Ack (bit 1) and Connector Change Ack
    // (bit 0). Anything else stays as-is.
    const uint8_t ack = ppm->regfile[UCSI_PPM_OFFSET_CONTROL + UCSI_PPM_ACK_CC_CI_BYTE];
    uint32_t cci = cci_load(ppm);

    if(ack & UCSI_PPM_ACK_CC_CI_COMMAND_COMPLETED_ACK) {
        // architecture.md §9: an ACK that confirmed a failed command clears
        // the accumulated Error Information bitmap.
        if(cci & UCSI_PPM_CCI_ERROR) {
            ppm->error_info = 0u;
        }
        cci &= ~(UCSI_PPM_CCI_COMMAND_COMPLETED | UCSI_PPM_CCI_DATA_LENGTH_MASK |
                 UCSI_PPM_CCI_NOT_SUPPORTED | UCSI_PPM_CCI_ERROR |
                 UCSI_PPM_CCI_CANCEL_COMPLETED | UCSI_PPM_CCI_BUSY);
    }
    if(ack & UCSI_PPM_ACK_CC_CI_CONNECTOR_CHANGE_ACK) {
        cci &= ~UCSI_PPM_CCI_CONNECTOR_CHANGE_MASK;
    }
    return cci;
}

static uint32_t handle_not_supported(UcsiPpm* ppm) {
    (void)ppm;
    // Per Table 4-3 / commands.md §1.1: Not Supported is its own indicator,
    // distinct from Error. Both Command Completed and Not Supported are set.
    return UCSI_PPM_CCI_COMMAND_COMPLETED | UCSI_PPM_CCI_NOT_SUPPORTED;
}

// Records an error and returns the CCI that should be reported. The error
// bitmap is OR-accumulated and stays until ACK or PPM_RESET (architecture.md §9).
static uint32_t fail_with_error(UcsiPpm* ppm, uint16_t error_bits) {
    ppm->error_info |= error_bits;
    return UCSI_PPM_CCI_COMMAND_COMPLETED | UCSI_PPM_CCI_ERROR;
}

static uint32_t handle_set_ccom(UcsiPpm* ppm) {
    // commands.md §2.8. CC Operation Mode is a 4-bit bitmap at CONTROL bits 23..26.
    // bit 23 = byte 2 bit 7; bits 24..26 = byte 3 bits 0..2.
    const uint8_t* ctrl = &ppm->regfile[UCSI_PPM_OFFSET_CONTROL];
    const uint8_t cc_mode_bits = (uint8_t)(((ctrl[2] >> 7) & 0x01u) |
                                           ((ctrl[3] & 0x07u) << 1));

    if(cc_mode_bits == 0) {
        return fail_with_error(ppm, UCSI_PPM_ERR_INVALID_CMD_PARAMS);
    }

    // bit 0=Rp Only, 1=Rd Only, 2=DRP, 3=Disabled.
    // OPM provides a "subset of acceptable modes"; PPM picks the most capable.
    UcsiPpmCcOperationMode chosen;
    if(cc_mode_bits & (1u << 2)) {
        chosen = UcsiPpmCcModeDrp;
    } else if(cc_mode_bits & (1u << 0)) {
        chosen = UcsiPpmCcModeRpOnly;
    } else if(cc_mode_bits & (1u << 1)) {
        chosen = UcsiPpmCcModeRdOnly;
    } else { // bit 3
        if(!ppm->config.supports_disabled_state) {
            return fail_with_error(ppm, UCSI_PPM_ERR_INVALID_CMD_PARAMS);
        }
        chosen = UcsiPpmCcModeDisabled;
    }
    ppm->current_cc_operation_mode = chosen;
    return UCSI_PPM_CCI_COMMAND_COMPLETED;
}

static uint32_t handle_set_uor(UcsiPpm* ppm) {
    // commands.md §2.9. USB Operation Role is 3 bits at CONTROL bits 23..25.
    const uint8_t* ctrl = &ppm->regfile[UCSI_PPM_OFFSET_CONTROL];
    const uint8_t role = (uint8_t)(((ctrl[2] >> 7) & 0x01u) |
                                   ((ctrl[3] & 0x03u) << 1));

    // bit 0 (swap to DFP) and bit 1 (swap to UFP) are mutually exclusive.
    if((role & 0x03u) == 0x03u) {
        return fail_with_error(ppm, UCSI_PPM_ERR_INVALID_CMD_PARAMS);
    }

    // bit 2: accept-swap policy flag (always stored; takes effect once L3 is up).
    ppm->accept_dr_swap = (role & 0x04u) != 0u;

    // Initiate-swap bits (0/1) are a no-op in v1 (no PD partner). When L3
    // lands they'll kick off a DR_Swap; for now we just report success so
    // callers can configure policy before a connect.
    return UCSI_PPM_CCI_COMMAND_COMPLETED;
}

static uint32_t handle_set_pdr(UcsiPpm* ppm) {
    // commands.md §2.10. Power Direction Role: 3 bits at CONTROL bits 23..25.
    const uint8_t* ctrl = &ppm->regfile[UCSI_PPM_OFFSET_CONTROL];
    const uint8_t role = (uint8_t)(((ctrl[2] >> 7) & 0x01u) |
                                   ((ctrl[3] & 0x03u) << 1));

    if(role == 0u) {
        // Spec: "Все 0 — нелегально".
        return fail_with_error(ppm, UCSI_PPM_ERR_INVALID_CMD_PARAMS);
    }

    ppm->accept_pr_swap = (role & 0x04u) != 0u;
    return UCSI_PPM_CCI_COMMAND_COMPLETED;
}

static uint32_t handle_get_pdos(UcsiPpm* ppm) {
    // commands.md §2.15. Parameters span CONTROL bits 16..38.
    const uint8_t* ctrl = &ppm->regfile[UCSI_PPM_OFFSET_CONTROL];
    const bool partner_pdo = ((ctrl[2] >> 7) & 0x01u) != 0u;
    const uint8_t pdo_offset = ctrl[3];
    const uint8_t num_pdos = (uint8_t)((ctrl[4] & 0x03u) + 1u); // value+1, range 1..4
    const bool want_source = ((ctrl[4] >> 2) & 0x01u) != 0u;
    // Source Capabilities Type (bits 35..36) and Range (bits 37..38) — ignored
    // in v1: we have only static config caps; partner PDOs aren't tracked yet.

    if(partner_pdo) {
        // No partner in v1 — commands.md §2.15 says "Partner PDO=1, нет
        // партнёра вообще → CC Communication Error".
        return fail_with_error(ppm, UCSI_PPM_ERR_CC_COMMUNICATION);
    }

    const UcsiPpmPdoList* list = want_source ? &ppm->config.source_caps :
                                               &ppm->config.sink_caps;
    // SPR-only in v1 → max offset+count is 7 (pd-scope.md).
    if((uint32_t)pdo_offset + (uint32_t)num_pdos > 7u) {
        return fail_with_error(ppm, UCSI_PPM_ERR_INVALID_CMD_PARAMS);
    }

    uint8_t emitted = 0u;
    uint8_t* msg = &ppm->regfile[UCSI_PPM_OFFSET_MESSAGE_IN];
    for(uint8_t i = 0; i < num_pdos; ++i) {
        const uint8_t list_index = (uint8_t)(pdo_offset + i);
        UcsiPpmPdo pdo = 0u;
        if(list_index < list->count) {
            pdo = list->pdos[list_index];
        }
        // Past the end of the list we emit zero PDOs (commands.md §2.15
        // doesn't error here — it just trims; the response carries
        // Data Length = 4 * num_pdos regardless).
        msg[i * 4 + 0] = (uint8_t)(pdo & 0xFFu);
        msg[i * 4 + 1] = (uint8_t)((pdo >> 8) & 0xFFu);
        msg[i * 4 + 2] = (uint8_t)((pdo >> 16) & 0xFFu);
        msg[i * 4 + 3] = (uint8_t)((pdo >> 24) & 0xFFu);
        emitted++;
    }
    return cci_with_data_length(UCSI_PPM_CCI_COMMAND_COMPLETED,
                                (uint8_t)(emitted * 4u));
}

static uint32_t handle_get_connector_status(UcsiPpm* ppm) {
    // commands.md §2.17. 19 bytes (152 bits). In v1 (no L3/L4) the connector
    // is always Unattached — every status bit is its default-zero value, and
    // there are no Connector Status Change events to report.
    memset(&ppm->regfile[UCSI_PPM_OFFSET_MESSAGE_IN], 0, 19u);
    return cci_with_data_length(UCSI_PPM_CCI_COMMAND_COMPLETED, 19u);
}

static uint32_t handle_get_error_status(UcsiPpm* ppm) {
    // commands.md §2.18. 16 bytes total: 2 bytes Error Information + 14 bytes
    // vendor-defined (0 in v1).
    uint8_t* msg = &ppm->regfile[UCSI_PPM_OFFSET_MESSAGE_IN];
    msg[0] = (uint8_t)(ppm->error_info & 0xFFu);
    msg[1] = (uint8_t)((ppm->error_info >> 8) & 0xFFu);
    memset(&msg[2], 0, 14u);
    return cci_with_data_length(UCSI_PPM_CCI_COMMAND_COMPLETED, 16u);
}

// --- entry points ----------------------------------------------------------

void ucsi_ppm_cmd_reset_state(UcsiPpm* ppm) {
    ppm->cmd_state = UcsiPpmCmdStateIdle;
    ppm->notification_mask = 0u;
    ppm->current_cc_operation_mode = ppm->config.initial_cc_operation_mode;
    // Defaults documented in api.md §4 ("Defaults после init / reset"):
    // commands.md §2.9 / §2.10 — accept swaps by default.
    ppm->accept_dr_swap = true;
    ppm->accept_pr_swap = true;
    ppm->error_info = 0u;
}

void ucsi_ppm_cmd_dispatch(UcsiPpm* ppm) {
    const uint8_t opcode = ppm->regfile[UCSI_PPM_OFFSET_CONTROL_COMMAND];
    if(opcode == 0u) return;

    // ACK_CC_CI is special: only valid in WaitForAck; it consumes CCI rather
    // than producing it; no alert is raised (CCI ends at 0).
    if(opcode == UCSI_PPM_OPCODE_ACK_CC_CI) {
        if(ppm->cmd_state != UcsiPpmCmdStateWaitForAck) {
            // Outside WaitForAck the command is meaningless; spec doesn't
            // require us to error explicitly. Silently ignore for v1.
            return;
        }
        const uint32_t new_cci = handle_ack_cc_ci(ppm);
        cci_store(ppm, new_cci);
        if(new_cci == 0u) {
            ppm->cmd_state = UcsiPpmCmdStateIdle;
        }
        return;
    }

    // For every other command, clear CCI before running the handler (the spec
    // says PPM clears stale indicators on next command — see commands.md §1.1
    // note about Reset Completed Indicator).
    cci_store(ppm, 0u);

    uint32_t result_cci;
    bool stays_idle = false;
    switch(opcode) {
    case UCSI_PPM_OPCODE_PPM_RESET:
        result_cci = handle_ppm_reset(ppm);
        // PPM_RESET completes back to Idle; OPM sees Reset Completed and
        // clears it implicitly by sending the next command.
        stays_idle = true;
        break;
    case UCSI_PPM_OPCODE_GET_CAPABILITY:
        result_cci = handle_get_capability(ppm);
        break;
    case UCSI_PPM_OPCODE_GET_CONNECTOR_CAPABILITY:
        result_cci = handle_get_connector_capability(ppm);
        break;
    case UCSI_PPM_OPCODE_SET_NOTIFICATION_ENABLE:
        result_cci = handle_set_notification_enable(ppm);
        break;
    case UCSI_PPM_OPCODE_SET_CCOM:
        result_cci = handle_set_ccom(ppm);
        break;
    case UCSI_PPM_OPCODE_SET_UOR:
        result_cci = handle_set_uor(ppm);
        break;
    case UCSI_PPM_OPCODE_SET_PDR:
        result_cci = handle_set_pdr(ppm);
        break;
    case UCSI_PPM_OPCODE_GET_PDOS:
        result_cci = handle_get_pdos(ppm);
        break;
    case UCSI_PPM_OPCODE_GET_CONNECTOR_STATUS:
        result_cci = handle_get_connector_status(ppm);
        break;
    case UCSI_PPM_OPCODE_GET_ERROR_STATUS:
        result_cci = handle_get_error_status(ppm);
        break;
    default:
        result_cci = handle_not_supported(ppm);
        break;
    }

    cci_store(ppm, result_cci);
    ppm->cmd_state = stays_idle ? UcsiPpmCmdStateIdle : UcsiPpmCmdStateWaitForAck;

    // Alert when CCI is non-zero — OPM has something to read.
    if(result_cci != 0u && ppm->config.alert) {
        ppm->config.alert(ppm->config.hal_ctx);
    }
}
