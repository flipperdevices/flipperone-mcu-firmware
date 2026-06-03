#include "ucsi_ppm.h"
#include "ucsi_ppm_i.h"
#include "ucsi_ppm_pe.h"
#include "ucsi_ppm_tc.h"

#include <string.h>

// --- Spec-derived constants ------------------------------------------------

// MESSAGE_IN response sizes per command (commands.md §2.x "CCI отличия").
#define DATA_LEN_GET_CAPABILITY           16u
#define DATA_LEN_GET_CONNECTOR_CAPABILITY 4u
#define DATA_LEN_GET_CONNECTOR_STATUS     19u
#define DATA_LEN_GET_ERROR_STATUS         16u

// pd-scope.md §2: a single PDO is 32 bits = 4 bytes; SPR allows up to 7 PDOs.
#define PDO_BYTES         4u
#define SPR_MAX_PDO_COUNT 7u

// GET_ERROR_STATUS layout (commands.md §2.18).
#define ERROR_STATUS_OFFSET_INFO           0u // 2 bytes
#define ERROR_STATUS_OFFSET_VENDOR         2u
#define ERROR_STATUS_VENDOR_RESERVED_BYTES 14u

// GET_CAPABILITY response field offsets (commands.md §2.6).
#define GET_CAP_OFF_BM_ATTRIBUTES   0u // 4 bytes
#define GET_CAP_OFF_NUM_CONNECTORS  4u // 7 bits + 1 reserved
#define GET_CAP_OFF_BM_OPTIONAL     5u // 3 bytes (24 bits)
#define GET_CAP_OFF_NUM_ALT_MODES   8u // 1 byte
#define GET_CAP_OFF_RESERVED        9u // 1 byte
#define GET_CAP_OFF_BCD_BC          10u // 2 bytes
#define GET_CAP_OFF_BCD_PD          12u // 2 bytes
#define GET_CAP_OFF_BCD_TYPEC       14u // 2 bytes
#define GET_CAP_NUM_CONNECTORS_MASK 0x7Fu // 7-bit field

// GET_CONNECTOR_CAPABILITY response bit positions (commands.md §2.7).
#define CONN_CAP_OP_MODE_RP_ONLY (1u << 0)
#define CONN_CAP_OP_MODE_RD_ONLY (1u << 1)
#define CONN_CAP_OP_MODE_DRP     (1u << 2)
#define CONN_CAP_OP_MODE_USB2    (1u << 5)
#define CONN_CAP_OP_MODE_USB3    (1u << 6)
#define CONN_CAP_PROVIDER        (1u << 8)
#define CONN_CAP_CONSUMER        (1u << 9)
#define CONN_CAP_SWAP_TO_DFP     (1u << 10)
#define CONN_CAP_SWAP_TO_UFP     (1u << 11)
#define CONN_CAP_SWAP_TO_SRC     (1u << 12)
#define CONN_CAP_SWAP_TO_SNK     (1u << 13)

// SET_CCOM CC Operation Mode bits (commands.md §2.8).
#define SET_CCOM_RP_ONLY  (1u << 0)
#define SET_CCOM_RD_ONLY  (1u << 1)
#define SET_CCOM_DRP      (1u << 2)
#define SET_CCOM_DISABLED (1u << 3)

// SET_UOR / SET_PDR role bits (commands.md §2.9 / §2.10) — same layout,
// reused. Bits 0/1 are "initiate swap" and are mutually exclusive in SET_UOR.
#define ROLE_INITIATE_PRIMARY   (1u << 0) // swap to DFP (SET_UOR) / Source (SET_PDR)
#define ROLE_INITIATE_SECONDARY (1u << 1) // swap to UFP / Sink
#define ROLE_ACCEPT_SWAPS       (1u << 2)
#define ROLE_INITIATE_MASK      (ROLE_INITIATE_PRIMARY | ROLE_INITIATE_SECONDARY)

// CONTROL parameter bit offsets within the 64-bit CONTROL register
// (commands.md §2.x — first column "Offset" in each command's CONTROL table).
#define BIT_SET_CCOM_CC_MODE   23u
#define WIDTH_SET_CCOM_CC_MODE 4u

#define BIT_ROLE   23u // shared by SET_UOR / SET_PDR
#define WIDTH_ROLE 3u

#define BIT_NOTIFICATION_MASK   16u
#define WIDTH_NOTIFICATION_MASK 17u

#define BIT_GET_PDOS_PARTNER         23u
#define WIDTH_GET_PDOS_PARTNER       1u
#define BIT_GET_PDOS_OFFSET          24u
#define WIDTH_GET_PDOS_OFFSET        8u
#define BIT_GET_PDOS_NUM_MINUS_ONE   32u
#define WIDTH_GET_PDOS_NUM_MINUS_ONE 2u
#define BIT_GET_PDOS_SOURCE          34u
#define WIDTH_GET_PDOS_SOURCE        1u

// SET_POWER_LEVEL parameters (commands.md §2.19).
#define BIT_SET_POWER_LEVEL_SRC_OR_SINK    23u
#define BIT_SET_POWER_LEVEL_OP_CURRENT     36u
#define WIDTH_SET_POWER_LEVEL_OP_CURRENT   7u
#define SET_POWER_LEVEL_OP_CURRENT_STEP_MA 50u

// --- byte / field helpers --------------------------------------------------

// Writes a little-endian uint16_t into dst[0..1].
static void write_le16(uint8_t* dst, uint16_t v) {
    dst[0] = (uint8_t)(v & 0xFFu);
    dst[1] = (uint8_t)((v >> 8) & 0xFFu);
}

// Writes a little-endian uint32_t into dst[0..3].
static void write_le32(uint8_t* dst, uint32_t v) {
    dst[0] = (uint8_t)(v & 0xFFu);
    dst[1] = (uint8_t)((v >> 8) & 0xFFu);
    dst[2] = (uint8_t)((v >> 16) & 0xFFu);
    dst[3] = (uint8_t)((v >> 24) & 0xFFu);
}

// Reads a little-endian uint32_t from src[0..3].
static uint32_t read_le32(const uint8_t* src) {
    return ((uint32_t)src[0]) | ((uint32_t)src[1] << 8) | ((uint32_t)src[2] << 16) | ((uint32_t)src[3] << 24);
}

// Extracts a `width`-bit field from CONTROL starting at bit `bit_offset`.
// UCSI command parameters (commands.md §2.x) are specified by their bit
// offset within the 64-bit CONTROL register; fields routinely straddle byte
// boundaries. The helper isolates that detail.
static uint32_t control_get_field(const UcsiPpm* ppm, uint8_t bit_offset, uint8_t width) {
    const uint8_t* ctrl = &ppm->regfile[UCSI_PPM_OFFSET_CONTROL];
    uint32_t v = 0;
    for(uint8_t i = 0; i < width; ++i) {
        const uint8_t bit = (uint8_t)(bit_offset + i);
        v |= (uint32_t)((ctrl[bit / 8u] >> (bit % 8u)) & 1u) << i;
    }
    return v;
}

// --- CCI helpers -----------------------------------------------------------

static void cci_store(UcsiPpm* ppm, uint32_t cci) {
    write_le32(&ppm->regfile[UCSI_PPM_OFFSET_CCI], cci);
}

static uint32_t cci_load(const UcsiPpm* ppm) {
    return read_le32(&ppm->regfile[UCSI_PPM_OFFSET_CCI]);
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

    write_le32(&msg[GET_CAP_OFF_BM_ATTRIBUTES], pack_bm_attributes(c));
    msg[GET_CAP_OFF_NUM_CONNECTORS] = (uint8_t)(UCSI_PPM_NUM_CONNECTORS & GET_CAP_NUM_CONNECTORS_MASK);

    // bmOptionalFeatures is a 24-bit field; write 16 bits + 1 byte.
    const uint32_t bm_opt = pack_bm_optional_features(c);
    write_le16(&msg[GET_CAP_OFF_BM_OPTIONAL], (uint16_t)(bm_opt & 0xFFFFu));
    msg[GET_CAP_OFF_BM_OPTIONAL + 2u] = (uint8_t)((bm_opt >> 16) & 0xFFu);

    msg[GET_CAP_OFF_NUM_ALT_MODES] = (uint8_t)UCSI_PPM_NUM_ALT_MODES;
    msg[GET_CAP_OFF_RESERVED] = 0u;

    write_le16(&msg[GET_CAP_OFF_BCD_BC], UCSI_PPM_VERSION_BC);
    write_le16(&msg[GET_CAP_OFF_BCD_PD], UCSI_PPM_VERSION_PD);
    write_le16(&msg[GET_CAP_OFF_BCD_TYPEC], UCSI_PPM_VERSION_TYPEC);

    return cci_with_data_length(UCSI_PPM_CCI_COMMAND_COMPLETED, DATA_LEN_GET_CAPABILITY);
}

static uint32_t handle_get_connector_capability(UcsiPpm* ppm) {
    // commands.md §2.7 — 4 bytes (32 bits) of layout.
    const UcsiPpmConfig* c = &ppm->config;
    uint32_t cap = 0;

    // Operation Mode (bits 0..7).
    switch(c->initial_cc_operation_mode) {
    case UcsiPpmCcModeRpOnly:
        cap |= CONN_CAP_OP_MODE_RP_ONLY;
        break;
    case UcsiPpmCcModeRdOnly:
        cap |= CONN_CAP_OP_MODE_RD_ONLY;
        break;
    case UcsiPpmCcModeDrp:
        cap |= CONN_CAP_OP_MODE_DRP;
        break;
    case UcsiPpmCcModeDisabled:
        break; // no Rp/Rd/DRP bit
    }
    if(c->connector_usb2_capable) cap |= CONN_CAP_OP_MODE_USB2;
    if(c->connector_usb3_capable) cap |= CONN_CAP_OP_MODE_USB3;

    // Provider/Consumer + swap flags (bits 8..13).
    const bool drp = (c->initial_cc_operation_mode == UcsiPpmCcModeDrp);
    const bool rp = drp || (c->initial_cc_operation_mode == UcsiPpmCcModeRpOnly);
    const bool rd = drp || (c->initial_cc_operation_mode == UcsiPpmCcModeRdOnly);
    if(rp) cap |= CONN_CAP_PROVIDER;
    if(rd) cap |= CONN_CAP_CONSUMER;
    if(drp) cap |= CONN_CAP_SWAP_TO_DFP | CONN_CAP_SWAP_TO_UFP | CONN_CAP_SWAP_TO_SRC | CONN_CAP_SWAP_TO_SNK;

    // Extended Operation Mode (bits 14..21), Misc (22..25), RCP (26),
    // Partner PD Revision (27..28), Reserved (29..31) — all zero in v1
    // (no USB4/EPR, no partner connected at GET_CONNECTOR_CAPABILITY time).

    write_le32(&ppm->regfile[UCSI_PPM_OFFSET_MESSAGE_IN], cap);
    return cci_with_data_length(UCSI_PPM_CCI_COMMAND_COMPLETED, DATA_LEN_GET_CONNECTOR_CAPABILITY);
}

static uint32_t handle_set_notification_enable(UcsiPpm* ppm) {
    // commands.md §2.5: 17-bit Notification Enable bitmap at CONTROL bit 16.
    ppm->notification_mask = control_get_field(ppm, BIT_NOTIFICATION_MASK, WIDTH_NOTIFICATION_MASK);
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
        cci &=
            ~(UCSI_PPM_CCI_COMMAND_COMPLETED | UCSI_PPM_CCI_DATA_LENGTH_MASK | UCSI_PPM_CCI_NOT_SUPPORTED | UCSI_PPM_CCI_ERROR | UCSI_PPM_CCI_CANCEL_COMPLETED |
              UCSI_PPM_CCI_BUSY);
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
    // commands.md §2.8.
    const uint8_t cc_mode_bits = (uint8_t)control_get_field(ppm, BIT_SET_CCOM_CC_MODE, WIDTH_SET_CCOM_CC_MODE);

    if(cc_mode_bits == 0) {
        return fail_with_error(ppm, UCSI_PPM_ERR_INVALID_CMD_PARAMS);
    }

    // OPM provides a "subset of acceptable modes"; PPM picks the most capable.
    UcsiPpmCcOperationMode chosen;
    if(cc_mode_bits & SET_CCOM_DRP) {
        chosen = UcsiPpmCcModeDrp;
    } else if(cc_mode_bits & SET_CCOM_RP_ONLY) {
        chosen = UcsiPpmCcModeRpOnly;
    } else if(cc_mode_bits & SET_CCOM_RD_ONLY) {
        chosen = UcsiPpmCcModeRdOnly;
    } else { // SET_CCOM_DISABLED
        if(!ppm->config.supports_disabled_state) {
            return fail_with_error(ppm, UCSI_PPM_ERR_INVALID_CMD_PARAMS);
        }
        chosen = UcsiPpmCcModeDisabled;
    }
    ppm->current_cc_operation_mode = chosen;
    return UCSI_PPM_CCI_COMMAND_COMPLETED;
}

static uint32_t handle_set_uor(UcsiPpm* ppm) {
    // commands.md §2.9.
    const uint8_t role = (uint8_t)control_get_field(ppm, BIT_ROLE, WIDTH_ROLE);

    // Initiate-DFP and Initiate-UFP are mutually exclusive (spec).
    if((role & ROLE_INITIATE_MASK) == ROLE_INITIATE_MASK) {
        return fail_with_error(ppm, UCSI_PPM_ERR_INVALID_CMD_PARAMS);
    }

    // accept-swap policy flag is always stored regardless of contract state.
    ppm->accept_dr_swap = (role & ROLE_ACCEPT_SWAPS) != 0u;

    if((role & ROLE_INITIATE_MASK) != 0u) {
        // Initiate a DR_Swap toward the requested role. Requires an active
        // PD contract — otherwise there's nothing to swap.
        const bool to_dfp = (role & ROLE_INITIATE_PRIMARY) != 0u;
        const UcsiPpmStatus s = ucsi_ppm_pe_request_dr_swap(ppm, to_dfp);
        if(s != UcsiPpmStatusOk) {
            return fail_with_error(ppm, UCSI_PPM_ERR_PPM_POLICY_CONFLICT);
        }
    }
    return UCSI_PPM_CCI_COMMAND_COMPLETED;
}

static uint32_t handle_set_pdr(UcsiPpm* ppm) {
    // commands.md §2.10.
    const uint8_t role = (uint8_t)control_get_field(ppm, BIT_ROLE, WIDTH_ROLE);

    if(role == 0u) {
        // Spec: "Все 0 — нелегально".
        return fail_with_error(ppm, UCSI_PPM_ERR_INVALID_CMD_PARAMS);
    }

    ppm->accept_pr_swap = (role & ROLE_ACCEPT_SWAPS) != 0u;

    // ROLE_INITIATE_PRIMARY = "swap to Source" — kick off PR_Swap snk→src
    // via PE. Only the snk→src direction is wired in v1; src→snk would
    // require us to drop our own VBUS first (more careful PSU teardown).
    if((role & ROLE_INITIATE_PRIMARY) != 0u) {
        const UcsiPpmStatus s = ucsi_ppm_pe_request_pr_swap_to_source(ppm);
        if(s != UcsiPpmStatusOk) {
            return fail_with_error(ppm, UCSI_PPM_ERR_PPM_POLICY_CONFLICT);
        }
    }
    return UCSI_PPM_CCI_COMMAND_COMPLETED;
}

static uint32_t handle_get_pdos(UcsiPpm* ppm) {
    // commands.md §2.15.
    const bool partner_pdo = control_get_field(ppm, BIT_GET_PDOS_PARTNER, WIDTH_GET_PDOS_PARTNER) != 0u;
    const uint8_t pdo_offset = (uint8_t)control_get_field(ppm, BIT_GET_PDOS_OFFSET, WIDTH_GET_PDOS_OFFSET);
    const uint8_t num_pdos = (uint8_t)(control_get_field(ppm, BIT_GET_PDOS_NUM_MINUS_ONE,
                                                         WIDTH_GET_PDOS_NUM_MINUS_ONE) + 1u); // value+1, range 1..4
    const bool want_source = control_get_field(ppm, BIT_GET_PDOS_SOURCE, WIDTH_GET_PDOS_SOURCE) != 0u;
    // Source Capabilities Type (bits 35..36) and Range (bits 37..38) — ignored
    // in v1: we have only static config caps; partner PDOs aren't tracked yet.

    // Choose the PDO list. Partner-side returns the most recently received
    // Source_Capabilities (only the sink-side flow caches them in v1).
    UcsiPpmPdoList partner_list = {0};
    const UcsiPpmPdoList* list;
    if(partner_pdo) {
        if(!want_source || ppm->pe_received_pdo_count == 0u) {
            // We never request partner's Sink_Capabilities, and an empty
            // received-PDO cache means partner is non-PD / not connected.
            return fail_with_error(ppm, UCSI_PPM_ERR_CC_COMMUNICATION);
        }
        partner_list.count = ppm->pe_received_pdo_count;
        for(uint8_t i = 0; i < ppm->pe_received_pdo_count; ++i) {
            partner_list.pdos[i] = ppm->pe_received_pdos[i];
        }
        list = &partner_list;
    } else {
        list = want_source ? &ppm->config.source_caps : &ppm->config.sink_caps;
    }
    if((uint32_t)pdo_offset + (uint32_t)num_pdos > SPR_MAX_PDO_COUNT) {
        return fail_with_error(ppm, UCSI_PPM_ERR_INVALID_CMD_PARAMS);
    }

    uint8_t* msg = &ppm->regfile[UCSI_PPM_OFFSET_MESSAGE_IN];
    for(uint8_t i = 0; i < num_pdos; ++i) {
        const uint8_t list_index = (uint8_t)(pdo_offset + i);
        UcsiPpmPdo pdo = 0u;
        if(list_index < list->count) {
            pdo = list->pdos[list_index];
        }
        // Past the end of the list we emit zero PDOs (commands.md §2.15 doesn't
        // error here — it just trims; the response carries Data Length =
        // PDO_BYTES * num_pdos regardless).
        write_le32(&msg[(size_t)i * PDO_BYTES], pdo);
    }
    return cci_with_data_length(UCSI_PPM_CCI_COMMAND_COMPLETED, (uint8_t)(num_pdos * PDO_BYTES));
}

// Writes a `width`-bit field at bit offset `bit_offset` into `buf`, LSB first.
// Symmetric counterpart to control_get_field; handles fields that straddle
// byte boundaries (GET_CONNECTOR_STATUS layout has plenty of those).
static void write_field(uint8_t* buf, uint32_t bit_offset, uint32_t width, uint64_t value) {
    for(uint32_t i = 0; i < width; ++i) {
        const uint32_t bit = bit_offset + i;
        const uint32_t byte_idx = bit / 8u;
        const uint8_t bit_mask = (uint8_t)(1u << (bit % 8u));
        if((value >> i) & 1u) {
            buf[byte_idx] |= bit_mask;
        } else {
            buf[byte_idx] &= (uint8_t)~bit_mask;
        }
    }
}

// commands.md §2.17 Power Operation Mode values (Table 6-43).
#define CONNSTAT_POM_USB_DEFAULT 1u
#define CONNSTAT_POM_PD          3u

// Bit offsets within the GET_CONNECTOR_STATUS payload (commands.md §2.17).
#define CONNSTAT_OFF_CHANGE_BITMAP    0u
#define CONNSTAT_OFF_POWER_OP_MODE    16u
#define CONNSTAT_OFF_CONNECT_STATUS   19u
#define CONNSTAT_OFF_POWER_DIRECTION  20u
#define CONNSTAT_OFF_PARTNER_FLAGS    21u
#define CONNSTAT_OFF_PARTNER_TYPE     29u
#define CONNSTAT_OFF_RDO              32u
#define CONNSTAT_OFF_BCD_PD           70u
#define CONNSTAT_OFF_ORIENTATION      86u
#define CONNSTAT_OFF_SINK_PATH_STATUS 87u

static uint32_t handle_set_power_level(UcsiPpm* ppm) {
    // commands.md §2.19. v1 supports sink-direction renegotiation only;
    // source-direction limits and AVS/PPS fields are accepted but no-op'd
    // beyond validation (Type-C current adjustment will land with PE-5).
    const bool is_source = control_get_field(ppm, BIT_SET_POWER_LEVEL_SRC_OR_SINK, 1u) != 0u;
    const uint8_t op_units = (uint8_t)control_get_field(ppm, BIT_SET_POWER_LEVEL_OP_CURRENT, WIDTH_SET_POWER_LEVEL_OP_CURRENT);
    const uint16_t op_current_ma = (uint16_t)(op_units * SET_POWER_LEVEL_OP_CURRENT_STEP_MA);

    if(is_source) {
        // Source-side power-level adjustment lives in PE-5 alongside re-
        // advertising Source_Capabilities. For now reject so OPM knows it
        // didn't land — better than silently completing a no-op.
        return fail_with_error(ppm, UCSI_PPM_ERR_INVALID_CMD_PARAMS);
    }
    if(ppm->pe_state != (int)UcsiPpmPeSnkReady) {
        // No active sink contract to renegotiate.
        return fail_with_error(ppm, UCSI_PPM_ERR_INVALID_CMD_PARAMS);
    }
    if(op_current_ma == 0u) {
        // 0 = "PPM-decides" per spec — re-request the advertised max.
        // Fixed-Supply PDO bits 9:0 = Maximum Current in 10 mA units.
        const uint32_t pdo = ppm->pe_received_pdos[ppm->pe_requested_pdo_index - 1u];
        const uint16_t max_ma = (uint16_t)((pdo & 0x3FFu) * 10u);
        if(ucsi_ppm_pe_request_renegotiate(ppm, max_ma) != UcsiPpmStatusOk) {
            return fail_with_error(ppm, UCSI_PPM_ERR_PPM_POLICY_CONFLICT);
        }
    } else {
        if(ucsi_ppm_pe_request_renegotiate(ppm, op_current_ma) != UcsiPpmStatusOk) {
            return fail_with_error(ppm, UCSI_PPM_ERR_PPM_POLICY_CONFLICT);
        }
    }
    return UCSI_PPM_CCI_COMMAND_COMPLETED;
}

static uint32_t handle_get_connector_status(UcsiPpm* ppm) {
    // 19 bytes (152 bits) of packed layout per commands.md §2.17.
    uint8_t* msg = &ppm->regfile[UCSI_PPM_OFFSET_MESSAGE_IN];
    memset(msg, 0, DATA_LEN_GET_CONNECTOR_STATUS);

    // Connector Status Change bitmap — return current value and reset
    // (spec: "Сбрасывается при чтении GET_CONNECTOR_STATUS").
    write_field(msg, CONNSTAT_OFF_CHANGE_BITMAP, 16u, ppm->connector_status_change);
    ppm->connector_status_change = 0u;

    const bool is_attached = ppm->tc_state == (int)UcsiPpmTcStateAttachedSrc || ppm->tc_state == (int)UcsiPpmTcStateAttachedSnk;
    const bool is_pd = ppm->pe_state == (int)UcsiPpmPeSnkReady || ppm->pe_state == (int)UcsiPpmPeSrcReady;

    if(is_attached) {
        const uint8_t pom = is_pd ? CONNSTAT_POM_PD : CONNSTAT_POM_USB_DEFAULT;
        write_field(msg, CONNSTAT_OFF_POWER_OP_MODE, 3u, pom);
        write_field(msg, CONNSTAT_OFF_CONNECT_STATUS, 1u, 1u);
        write_field(msg, CONNSTAT_OFF_POWER_DIRECTION, 1u, ppm->tc_role_is_src ? 1u : 0u);

        // Partner Type (Table 6-43): 1 = DFP attached, 2 = UFP attached.
        // We're DFP when pe_data_role_is_dfp (modified independently by
        // DR_Swap from the power role) — partner is the opposite.
        const uint8_t partner_type = ppm->pe_data_role_is_dfp ? 2u : 1u;
        write_field(msg, CONNSTAT_OFF_PARTNER_TYPE, 3u, partner_type);
        // Partner Flags bit 0 = USB capability; we assume true once attached.
        write_field(msg, CONNSTAT_OFF_PARTNER_FLAGS, 1u, 1u);

        // Orientation: 0 = direct (CC1), 1 = flipped (CC2).
        if(ppm->tc_orientation == (int)UcsiPpmPhyCc2) {
            write_field(msg, CONNSTAT_OFF_ORIENTATION, 1u, 1u);
        }
        // Sink Path Status — 1 when consuming (sink-side attached).
        if(!ppm->tc_role_is_src) {
            write_field(msg, CONNSTAT_OFF_SINK_PATH_STATUS, 1u, 1u);
        }
    }

    if(is_pd) {
        write_field(msg, CONNSTAT_OFF_RDO, 32u, ppm->pe_current_rdo);
        write_field(msg, CONNSTAT_OFF_BCD_PD, 16u, UCSI_PPM_VERSION_PD);
    }

    return cci_with_data_length(UCSI_PPM_CCI_COMMAND_COMPLETED, DATA_LEN_GET_CONNECTOR_STATUS);
}

void ucsi_ppm_notify_connector_change(UcsiPpm* ppm, uint16_t change_bits) {
    // Accumulate — bitmap is part of port state and persists across the
    // notification mask filter (architecture.md §4.3).
    ppm->connector_status_change |= change_bits;

    // Stamp our connector number (1) into CCI.Connector Change Indicator,
    // preserving all other CCI bits (e.g., Command Completed in flight).
    uint32_t cci = cci_load(ppm);
    cci = (cci & ~UCSI_PPM_CCI_CONNECTOR_CHANGE_MASK) | (((uint32_t)UCSI_PPM_NUM_CONNECTORS & 0x7Fu) << UCSI_PPM_CCI_CONNECTOR_CHANGE_SHIFT);
    cci_store(ppm, cci);

    // Wake OPM only if at least one of the raised CSC bits is enabled in the
    // notification mask. UCSI Notification Enable (commands.md §2.5 Table 6-25)
    // bits 1..15 map 1:1 to the Connector Status Change bitmap bits (Table
    // 6-44); bit 0 is Command Completed and unrelated to this path.
    const uint32_t alert_gate = (uint32_t)change_bits & ppm->notification_mask;
    if(alert_gate != 0u && ppm->config.alert) {
        ppm->config.alert(ppm->config.hal_ctx);
    }
}

static uint32_t handle_get_error_status(UcsiPpm* ppm) {
    // commands.md §2.18. 16 bytes total: 2 bytes Error Information + 14 bytes
    // vendor-defined (0 in v1).
    uint8_t* msg = &ppm->regfile[UCSI_PPM_OFFSET_MESSAGE_IN];
    write_le16(&msg[ERROR_STATUS_OFFSET_INFO], ppm->error_info);
    memset(&msg[ERROR_STATUS_OFFSET_VENDOR], 0, ERROR_STATUS_VENDOR_RESERVED_BYTES);
    return cci_with_data_length(UCSI_PPM_CCI_COMMAND_COMPLETED, DATA_LEN_GET_ERROR_STATUS);
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

    // ACK_CC_CI is special: it consumes CCI bits rather than producing them,
    // and is allowed in *any* state. The Connector Change Acknowledge in
    // particular arrives without a preceding UCSI command (it acks an async
    // event raised by L3 via ucsi_ppm_notify_connector_change). No alert is
    // raised on the ACK path — CCI only goes down here.
    if(opcode == UCSI_PPM_OPCODE_ACK_CC_CI) {
        const uint32_t new_cci = handle_ack_cc_ci(ppm);
        cci_store(ppm, new_cci);
        // Drop back to Idle only when CCI has fully cleared *and* we were
        // waiting on a command response (otherwise we were already Idle).
        if(ppm->cmd_state == UcsiPpmCmdStateWaitForAck && new_cci == 0u) {
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
    case UCSI_PPM_OPCODE_SET_POWER_LEVEL:
        result_cci = handle_set_power_level(ppm);
        break;
    default:
        result_cci = handle_not_supported(ppm);
        break;
    }

    cci_store(ppm, result_cci);
    ppm->cmd_state = stays_idle ? UcsiPpmCmdStateIdle : UcsiPpmCmdStateWaitForAck;

    // OPM alert: gated by SET_NOTIFICATION_ENABLE bit 0 (Command Completed
    // Notification Enable, commands.md §2.5 Table 6-25). Reset Completed is
    // exempt — PPM_RESET clears notification_mask back to 0, so without
    // this carve-out OPM would never see the reset acknowledged.
    if(result_cci != 0u && ppm->config.alert) {
        const bool reset_completed = (result_cci & UCSI_PPM_CCI_RESET_COMPLETED) != 0u;
        const bool cmd_alert_enabled = (ppm->notification_mask & UCSI_PPM_NOTIF_CMD_COMPLETED) != 0u;
        if(reset_completed || cmd_alert_enabled) {
            ppm->config.alert(ppm->config.hal_ctx);
        }
    }
}
