#pragma once

#include "ucsi_ppm.h"

#ifdef __cplusplus
extern "C" {
#endif

// UCSI 3.0 Table 4-1: register file layout. Total size 528 bytes.
#define UCSI_PPM_REGFILE_SIZE 528u

#define UCSI_PPM_OFFSET_VERSION     0u
#define UCSI_PPM_SIZE_VERSION       3u
#define UCSI_PPM_OFFSET_RESERVED1   3u
#define UCSI_PPM_SIZE_RESERVED1     1u
#define UCSI_PPM_OFFSET_CCI         4u
#define UCSI_PPM_SIZE_CCI           4u
#define UCSI_PPM_OFFSET_CONTROL     8u
#define UCSI_PPM_SIZE_CONTROL       8u
#define UCSI_PPM_OFFSET_MESSAGE_IN  16u
#define UCSI_PPM_SIZE_MESSAGE_IN    255u
#define UCSI_PPM_OFFSET_RESERVED2   271u
#define UCSI_PPM_SIZE_RESERVED2     1u
#define UCSI_PPM_OFFSET_MESSAGE_OUT 272u
#define UCSI_PPM_SIZE_MESSAGE_OUT   255u
#define UCSI_PPM_OFFSET_RESERVED3   527u
#define UCSI_PPM_SIZE_RESERVED3     1u

// CONTROL[0] is the Command opcode byte. A non-zero write to this byte
// triggers command processing (architecture.md §2, api.md §6).
#define UCSI_PPM_OFFSET_CONTROL_COMMAND UCSI_PPM_OFFSET_CONTROL

// UCSI command opcodes (commands.md §1.4, Table 6-87 / Appendix A.1).
#define UCSI_PPM_OPCODE_PPM_RESET                0x01u
#define UCSI_PPM_OPCODE_CANCEL                   0x02u
#define UCSI_PPM_OPCODE_CONNECTOR_RESET          0x03u
#define UCSI_PPM_OPCODE_ACK_CC_CI                0x04u
#define UCSI_PPM_OPCODE_SET_NOTIFICATION_ENABLE  0x05u
#define UCSI_PPM_OPCODE_GET_CAPABILITY           0x06u
#define UCSI_PPM_OPCODE_GET_CONNECTOR_CAPABILITY 0x07u
#define UCSI_PPM_OPCODE_SET_CCOM                 0x08u
#define UCSI_PPM_OPCODE_SET_UOR                  0x09u
#define UCSI_PPM_OPCODE_SET_PDR                  0x0Bu
#define UCSI_PPM_OPCODE_GET_PDOS                 0x10u
#define UCSI_PPM_OPCODE_GET_CABLE_PROPERTY       0x11u
#define UCSI_PPM_OPCODE_GET_CONNECTOR_STATUS     0x12u
#define UCSI_PPM_OPCODE_GET_ERROR_STATUS         0x13u
#define UCSI_PPM_OPCODE_SET_POWER_LEVEL          0x14u
#define UCSI_PPM_OPCODE_MAX                      0x22u

// CCI bits (commands.md §1.1, Table 4-3).
#define UCSI_PPM_CCI_END_OF_MESSAGE         (1u << 0)
#define UCSI_PPM_CCI_CONNECTOR_CHANGE_SHIFT 1u
#define UCSI_PPM_CCI_CONNECTOR_CHANGE_MASK  (0x7Fu << 1)
#define UCSI_PPM_CCI_DATA_LENGTH_SHIFT      8u
#define UCSI_PPM_CCI_DATA_LENGTH_MASK       (0xFFu << 8)
#define UCSI_PPM_CCI_VENDOR_DEFINED         (1u << 16)
#define UCSI_PPM_CCI_SECURITY_REQUEST       (1u << 23)
#define UCSI_PPM_CCI_FW_UPDATE_REQUEST      (1u << 24)
#define UCSI_PPM_CCI_NOT_SUPPORTED          (1u << 25)
#define UCSI_PPM_CCI_CANCEL_COMPLETED       (1u << 26)
#define UCSI_PPM_CCI_RESET_COMPLETED        (1u << 27)
#define UCSI_PPM_CCI_BUSY                   (1u << 28)
#define UCSI_PPM_CCI_ACK_COMMAND            (1u << 29)
#define UCSI_PPM_CCI_ERROR                  (1u << 30)
#define UCSI_PPM_CCI_COMMAND_COMPLETED      (1u << 31)

// ACK_CC_CI bits in CONTROL (commands.md §2.4).
// CONTROL bit 16 = byte 2 bit 0.
#define UCSI_PPM_ACK_CC_CI_BYTE                       2u
#define UCSI_PPM_ACK_CC_CI_CONNECTOR_CHANGE_ACK       (1u << 0)
#define UCSI_PPM_ACK_CC_CI_COMMAND_COMPLETED_ACK      (1u << 1)

// Error Information bits (commands.md §2.18 GET_ERROR_STATUS).
#define UCSI_PPM_ERR_UNRECOGNIZED_COMMAND       (1u << 0)
#define UCSI_PPM_ERR_NONEXISTENT_CONNECTOR      (1u << 1)
#define UCSI_PPM_ERR_INVALID_CMD_PARAMS         (1u << 2)
#define UCSI_PPM_ERR_INCOMPATIBLE_PARTNER       (1u << 3)
#define UCSI_PPM_ERR_CC_COMMUNICATION           (1u << 4)
#define UCSI_PPM_ERR_DEAD_BATTERY               (1u << 5)
#define UCSI_PPM_ERR_CONTRACT_NEGOTIATION       (1u << 6)
#define UCSI_PPM_ERR_OVERCURRENT                (1u << 7)
#define UCSI_PPM_ERR_UNDEFINED                  (1u << 8)
#define UCSI_PPM_ERR_PARTNER_REJECTED_SWAP      (1u << 9)
#define UCSI_PPM_ERR_HARD_RESET                 (1u << 10)
#define UCSI_PPM_ERR_PPM_POLICY_CONFLICT        (1u << 11)
#define UCSI_PPM_ERR_SWAP_REJECTED              (1u << 12)
#define UCSI_PPM_ERR_REVERSE_CURRENT_PROTECTION (1u << 13)
#define UCSI_PPM_ERR_SET_SINK_PATH_REJECTED     (1u << 14)

// L2 command state machine (architecture.md §3.3).
// Busy/Processing are reserved for later; in v1 all handlers are synchronous.
typedef enum {
    UcsiPpmCmdStateIdle = 0,
    UcsiPpmCmdStateWaitForAck,
} UcsiPpmCmdState;

typedef enum {
    UcsiPpmLifecycleAllocated,
    UcsiPpmLifecycleInitialized,
} UcsiPpmLifecycle;

struct UcsiPpm {
    UcsiPpmLifecycle lifecycle;
    UcsiPpmConfig config;
    uint8_t regfile[UCSI_PPM_REGFILE_SIZE];

    // L2 command state.
    UcsiPpmCmdState cmd_state;
    // 17-bit notification mask set via SET_NOTIFICATION_ENABLE
    // (commands.md §2.5). Defaults to 0 — all notifications off.
    uint32_t notification_mask;

    // Runtime CC mode after SET_CCOM (commands.md §2.8). Resets to
    // config.initial_cc_operation_mode on init / PPM_RESET.
    UcsiPpmCcOperationMode current_cc_operation_mode;

    // Policy flags for swap requests from the partner (api.md §4, defaults
    // section). Both default to true after init / PPM_RESET; updated by
    // SET_UOR bit 2 / SET_PDR bit 2.
    bool accept_dr_swap;
    bool accept_pr_swap;

    // Accumulated Error Information bitmap (commands.md §2.18). Set on
    // command failures; cleared on PPM_RESET or ACK_CC_CI that confirmed
    // a CCI with Error Indicator (architecture.md §9).
    uint16_t error_info;

    // Atomic flags set from ISR-context callers (notify_*); consumed in tick.
    // api.md §8: v1 uses a simple volatile uint32_t with the single-writer-
    // from-ISR / single-reader-from-task pattern (safe on Cortex-M without
    // explicit barriers). Swap to FuriEventFlag if SMP is added.
    volatile uint32_t pending_flags;

    // L3 Type-C state machine (ucsi_ppm_tc.c). Defined inline to avoid an
    // extra forward declaration / pointer chase.
    //   tc_state                 — current Type-C SM state (UcsiPpmTcState).
    //   tc_orientation           — active CC pin once TOGGLE has settled.
    //   tc_role_is_src           — true after TOGSS settled SRC; false after SNK.
    //   tc_attach_wait_start_ms  — time_ms() at AttachWait entry; basis for
    //                              the CCDebounce timer.
    //   tc_vbus_seen             — true after VBUS_OK observed in AttachWait;
    //                              gate for the AttachWait → Attached commit.
    // We keep the values as raw ints/bool here to avoid pulling more types
    // into this header; the actual UcsiPpmTcState enum lives in ucsi_ppm_tc.h.
    int tc_state;
    int tc_orientation;
    bool tc_role_is_src;
    uint32_t tc_attach_wait_start_ms;
    bool tc_vbus_seen;

    // L3 PRL (Protocol Layer) state, ucsi_ppm_prl.c.
    //   prl_next_tx_msg_id      — MessageID stamped into the next outgoing
    //                             header (PD R3.0 §6.2.1.1.4, 3-bit field).
    //   prl_last_rx_msg_id      — MessageID of the last accepted SOP message,
    //                             used for duplicate detection (PD §6.8.1).
    //   prl_last_rx_valid       — gate for last_rx_msg_id: only meaningful
    //                             after the first non-dup SOP delivery.
    //   prl_messages_delivered  — running count of non-duplicate messages
    //                             passed up to PE (introspection / tests;
    //                             real PE would just consume them).
    uint8_t prl_next_tx_msg_id;
    uint8_t prl_last_rx_msg_id;
    bool prl_last_rx_valid;
    uint32_t prl_messages_delivered;

    // L3 PE (Policy Engine) — ucsi_ppm_pe.c.
    //   pe_state                 — current PE state (UcsiPpmPeState).
    //   pe_timer_start_ms        — start of the currently-armed PE timer.
    //   pe_received_pdos[]       — most recent Source_Capabilities payload.
    //   pe_received_pdo_count    — number of PDOs in pe_received_pdos.
    //   pe_requested_pdo_index   — 1-based PDO position we last Request-ed.
    //   pe_negotiated_voltage_mv — committed contract voltage after PS_RDY.
    //   pe_negotiated_current_ma — committed contract current after PS_RDY.
    int pe_state;
    uint32_t pe_timer_start_ms;
    uint32_t pe_received_pdos[UCSI_PPM_MAX_PDOS];
    uint8_t pe_received_pdo_count;
    uint8_t pe_requested_pdo_index;
    uint16_t pe_negotiated_voltage_mv;
    uint16_t pe_negotiated_current_ma;
    // Source-side: counts retransmissions of Source_Capabilities while waiting
    // for partner's Request. Bounded by nCapsCount (PD R3.0 §7.10.4) — exceeding
    // it means partner is Type-C only / non-PD-capable.
    uint8_t pe_caps_counter;
    // Current PD RDO held by the explicit contract — sink side stores the
    // RDO we sent; source side stores partner's RDO. Surfaced through
    // GET_CONNECTOR_STATUS (commands.md §2.17 bits 32..63).
    uint32_t pe_current_rdo;
    // Bounded retry count for self-initiated Hard Resets. PD R3.0 caps it at
    // nHardResetCount = 2 (Table 7.12). Reset on entry to *Ready.
    uint8_t pe_hard_reset_counter;
    // Current data role. Initialised from tc_role_is_src at attach (sink=UFP,
    // source=DFP) and updated independently by DR_Swap (PD R3.0 §6.3.10).
    // Drives the Data Role bit in outgoing PD headers and Partner Type
    // reported via GET_CONNECTOR_STATUS.
    bool pe_data_role_is_dfp;

    // Accumulated Connector Status Change bitmap (commands.md §2.17 / Table
    // 6-44). PE / TC layers OR new bits in via ucsi_ppm_notify_connector_change;
    // GET_CONNECTOR_STATUS reads + clears it. Not gated by notification mask
    // — the mask only filters the OPM-facing alert (architecture.md §4.3).
    uint16_t connector_status_change;
};

// Connector Status Change bit positions (Table 6-44 in commands.md §2.17).
#define UCSI_PPM_CSC_EXTERNAL_SUPPLY_CHANGE  (1u << 1)
#define UCSI_PPM_CSC_POWER_OP_MODE_CHANGE    (1u << 2)
#define UCSI_PPM_CSC_ATTENTION               (1u << 3)
#define UCSI_PPM_CSC_SUPPORTED_PROVIDER_CAPS (1u << 5)
#define UCSI_PPM_CSC_NEGOTIATED_PL_CHANGE    (1u << 6)
#define UCSI_PPM_CSC_PD_RESET_COMPLETE       (1u << 7)
#define UCSI_PPM_CSC_SUPPORTED_CAM_CHANGE    (1u << 8)
#define UCSI_PPM_CSC_BATTERY_CHARGING_STATUS (1u << 9)
#define UCSI_PPM_CSC_PARTNER_CHANGED         (1u << 11)
#define UCSI_PPM_CSC_POWER_DIRECTION_CHANGED (1u << 12)
#define UCSI_PPM_CSC_SINK_PATH_STATUS_CHANGE (1u << 13)
#define UCSI_PPM_CSC_CONNECT_CHANGE          (1u << 14)
#define UCSI_PPM_CSC_ERROR                   (1u << 15)

// L3 → L2 event hook. PE/TC call this when they cross a state boundary OPM
// might care about. Accumulates `change_bits` in the change bitmap, stamps
// CCI.Connector Change Indicator (=our port number), and raises the alert
// callback IF any of the raised CSC bits intersect notification_mask
// (architecture.md §4.3 — mask gates alert only; bitmap/CCI persist). Safe
// to call multiple times in a single tick — bits OR together and the alert
// fires once per call when gated through.
void ucsi_ppm_notify_connector_change(UcsiPpm* ppm, uint16_t change_bits);

// pending_flags bits.
#define UCSI_PPM_PENDING_PHY_IRQ           (1u << 0)
#define UCSI_PPM_PENDING_POWER_SUPPLY_RDY  (1u << 1)

// L2 entry point: invoked by L1 (register_write) when a non-zero CONTROL[0]
// is detected. Reads the opcode + payload from the regfile, dispatches to
// the handler, updates CCI / MESSAGE_IN, transitions cmd_state, and calls
// the alert callback if CCI ends up non-zero. Defined in ucsi_ppm_cmd.c.
void ucsi_ppm_cmd_dispatch(UcsiPpm* ppm);

// Reset L2 command state to defaults (cmd_state = Idle, notification_mask = 0).
// Called from ucsi_ppm_init / ucsi_ppm_reset. Defined in ucsi_ppm_cmd.c.
void ucsi_ppm_cmd_reset_state(UcsiPpm* ppm);

#ifdef __cplusplus
}
#endif
