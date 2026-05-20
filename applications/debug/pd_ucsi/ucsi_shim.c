#include "ucsi_shim.h"

#include <string.h>

#include <furi.h>

#include "ucsi_ppm_i.h"

#define TAG "PdUcsiShim"

// --- private regfile helpers -----------------------------------------------

static uint32_t shim_read_cci(UcsiPpm* ppm) {
    uint8_t buf[4];
    ucsi_ppm_register_read(ppm, UCSI_PPM_OFFSET_CCI, 4, buf);
    return (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) | ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
}

// Spin-polls CCI for Command Completed up to UCSI_SHIM_CMD_TIMEOUT_MS. The
// dispatcher runs synchronously inside ucsi_ppm_register_write so the typical
// case completes by the time we get here — the poll is for swap/contract
// commands that flip the state machine before reporting completion.
static bool shim_wait_cc(UcsiPpm* ppm, uint32_t* cci_out) {
    const uint32_t t0 = furi_get_tick();
    while(furi_get_tick() - t0 < UCSI_SHIM_CMD_TIMEOUT_MS) {
        const uint32_t cci = shim_read_cci(ppm);
        if(cci & UCSI_PPM_CCI_COMMAND_COMPLETED) {
            *cci_out = cci;
            return true;
        }
        furi_delay_ms(1);
    }
    *cci_out = shim_read_cci(ppm);
    return false;
}

static void shim_ack_cc(UcsiPpm* ppm) {
    uint8_t ack_byte = UCSI_PPM_ACK_CC_CI_COMMAND_COMPLETED_ACK;
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL + 2u, 1, &ack_byte);
    uint8_t ack_op = UCSI_PPM_OPCODE_ACK_CC_CI;
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 1, &ack_op);
}

// Sets a single bit at bit_offset within the CONTROL window. Helper for the
// scattered fields in the UCSI command layout (commands.md §2.x).
static void shim_set_bit(uint8_t ctrl[8], uint16_t bit_offset, uint32_t value, uint8_t width) {
    for(uint8_t i = 0; i < width; ++i) {
        const uint16_t b = (uint16_t)(bit_offset + i);
        if((value >> i) & 1u) {
            ctrl[b / 8u] |= (uint8_t)(1u << (b % 8u));
        }
    }
}

// Common command issue path: writes CONTROL[1..7] (params) followed by the
// opcode byte (which triggers the dispatcher), waits for Command Completed,
// optionally copies MESSAGE_IN out, then ACKs. Returns false on timeout / Error.
static bool shim_issue(UcsiPpm* ppm, uint8_t opcode, const uint8_t ctrl[8], uint8_t* message_in_out, size_t message_in_len) {
    // Stage the parameter bytes first; the dispatcher fires when the opcode
    // hits CONTROL[0].
    if(ctrl) {
        ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL + 1u, 7, &ctrl[1]);
    }
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 1, &opcode);

    uint32_t cci = 0;
    const bool ok = shim_wait_cc(ppm, &cci);
    if(!ok) {
        FURI_LOG_E(TAG, "op=0x%02X cci timeout (last=0x%08lX)", opcode, (unsigned long)cci);
        shim_ack_cc(ppm);
        return false;
    }
    if(cci & UCSI_PPM_CCI_ERROR) {
        FURI_LOG_E(TAG, "op=0x%02X error (cci=0x%08lX)", opcode, (unsigned long)cci);
        shim_ack_cc(ppm);
        return false;
    }
    if(message_in_out && message_in_len > 0) {
        ucsi_ppm_register_read(ppm, UCSI_PPM_OFFSET_MESSAGE_IN, message_in_len, message_in_out);
    }
    shim_ack_cc(ppm);
    return true;
}

// --- public helpers --------------------------------------------------------

bool ucsi_shim_set_notification_enable(UcsiPpm* ppm, uint32_t mask) {
    uint8_t ctrl[8] = {0};
    // 17-bit mask field at CONTROL bit 16 (commands.md §2.5).
    shim_set_bit(ctrl, 16u, mask, 17u);
    return shim_issue(ppm, UCSI_PPM_OPCODE_SET_NOTIFICATION_ENABLE, ctrl, NULL, 0);
}

bool ucsi_shim_set_ccom(UcsiPpm* ppm, uint8_t cc_mode_bits) {
    uint8_t ctrl[8] = {0};
    // CC Operation Mode field at bit 23 (4 bits).
    shim_set_bit(ctrl, 23u, cc_mode_bits, 4u);
    return shim_issue(ppm, UCSI_PPM_OPCODE_SET_CCOM, ctrl, NULL, 0);
}

bool ucsi_shim_set_uor(UcsiPpm* ppm, uint8_t role_bits) {
    uint8_t ctrl[8] = {0};
    shim_set_bit(ctrl, 23u, role_bits, 3u);
    return shim_issue(ppm, UCSI_PPM_OPCODE_SET_UOR, ctrl, NULL, 0);
}

bool ucsi_shim_set_pdr(UcsiPpm* ppm, uint8_t role_bits) {
    uint8_t ctrl[8] = {0};
    shim_set_bit(ctrl, 23u, role_bits, 3u);
    return shim_issue(ppm, UCSI_PPM_OPCODE_SET_PDR, ctrl, NULL, 0);
}

bool ucsi_shim_set_power_level_sink(UcsiPpm* ppm, uint16_t op_current_ma) {
    uint8_t ctrl[8] = {0};
    // SrcOrSink bit 23 = 0 (sink); OperatingCurrent bit 36, 7 bits, 50 mA units.
    const uint8_t units = (uint8_t)((op_current_ma + 25u) / 50u);
    shim_set_bit(ctrl, 36u, units, 7u);
    return shim_issue(ppm, UCSI_PPM_OPCODE_SET_POWER_LEVEL, ctrl, NULL, 0);
}

bool ucsi_shim_get_capability(UcsiPpm* ppm, uint8_t out[16]) {
    return shim_issue(ppm, UCSI_PPM_OPCODE_GET_CAPABILITY, NULL, out, 16);
}

bool ucsi_shim_get_connector_capability(UcsiPpm* ppm, uint8_t out[4]) {
    return shim_issue(ppm, UCSI_PPM_OPCODE_GET_CONNECTOR_CAPABILITY, NULL, out, 4);
}

bool ucsi_shim_get_connector_status(UcsiPpm* ppm, uint8_t out[19]) {
    return shim_issue(ppm, UCSI_PPM_OPCODE_GET_CONNECTOR_STATUS, NULL, out, 19);
}

bool ucsi_shim_get_error_status(UcsiPpm* ppm, uint8_t out[16]) {
    return shim_issue(ppm, UCSI_PPM_OPCODE_GET_ERROR_STATUS, NULL, out, 16);
}

uint8_t ucsi_shim_get_pdos(UcsiPpm* ppm, bool partner, uint8_t pdo_offset, uint8_t num, bool source, uint32_t out_pdos[4]) {
    if(num == 0 || num > 4u) return 0u;
    uint8_t ctrl[8] = {0};
    // Partner PDO bit 23, PDO Offset bits 24..31, NumPdos-1 bits 32..33,
    // Source/Sink bit 34 (commands.md §2.15).
    shim_set_bit(ctrl, 23u, partner ? 1u : 0u, 1u);
    shim_set_bit(ctrl, 24u, pdo_offset, 8u);
    shim_set_bit(ctrl, 32u, (uint32_t)(num - 1u), 2u);
    shim_set_bit(ctrl, 34u, source ? 1u : 0u, 1u);

    uint8_t msg[16] = {0};
    if(!shim_issue(ppm, UCSI_PPM_OPCODE_GET_PDOS, ctrl, msg, sizeof(msg))) return 0u;

    for(uint8_t i = 0; i < num; ++i) {
        const uint8_t* p = &msg[i * 4u];
        out_pdos[i] = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    }
    return num;
}

// --- status pretty-print ----------------------------------------------------

static uint64_t read_packed(const uint8_t* buf, uint32_t bit_offset, uint32_t width) {
    uint64_t v = 0;
    for(uint32_t i = 0; i < width; ++i) {
        const uint32_t b = bit_offset + i;
        const uint64_t bit = (buf[b / 8u] >> (b % 8u)) & 1u;
        v |= bit << i;
    }
    return v;
}

static void log_status_from_resp(UcsiPpm* ppm, const uint8_t resp[19]) {
    const uint16_t csc_bitmap = (uint16_t)read_packed(resp, 0u, 16u);
    const uint8_t pom = (uint8_t)read_packed(resp, 16u, 3u);
    const uint8_t connect = (uint8_t)read_packed(resp, 19u, 1u);
    const uint8_t power_dir = (uint8_t)read_packed(resp, 20u, 1u);
    const uint8_t partner_type = (uint8_t)read_packed(resp, 29u, 3u);
    const uint32_t rdo = (uint32_t)read_packed(resp, 32u, 32u);

    FURI_LOG_I(
        TAG,
        "conn=%u dir=%s pom=%u partner=%s rdo=0x%08lX csc=0x%04X pe=%d rx=%lu tx_id=%u",
        connect,
        power_dir ? "src" : "snk",
        pom,
        partner_type == 1u ? "DFP" : (partner_type == 2u ? "UFP" : "?"),
        (unsigned long)rdo,
        csc_bitmap,
        ppm->pe_state,
        (unsigned long)ppm->prl_messages_delivered,
        (unsigned)ppm->prl_next_tx_msg_id);

    UcsiPpmContractInfo c = {0};
    if(ucsi_ppm_get_contract(ppm, &c) == UcsiPpmStatusOk && c.contract_in_place) {
        FURI_LOG_I(TAG, "contract: %u mV, %u mA, src=%d dfp=%d", c.voltage_mv, c.current_ma, c.is_source, c.is_dfp);
    }
}

void ucsi_shim_log_status(UcsiPpm* ppm) {
    uint8_t resp[19] = {0};
    if(!ucsi_shim_get_connector_status(ppm, resp)) {
        FURI_LOG_W(TAG, "GET_CONNECTOR_STATUS failed");
        return;
    }
    log_status_from_resp(ppm, resp);
}

bool ucsi_shim_log_status_if_changed(UcsiPpm* ppm) {
    uint8_t resp[19] = {0};
    if(!ucsi_shim_get_connector_status(ppm, resp)) {
        FURI_LOG_W(TAG, "GET_CONNECTOR_STATUS failed");
        return false;
    }
    const uint16_t csc_bitmap = (uint16_t)(resp[0] | ((uint16_t)resp[1] << 8));
    if(csc_bitmap == 0u) return false;
    log_status_from_resp(ppm, resp);
    return true;
}
