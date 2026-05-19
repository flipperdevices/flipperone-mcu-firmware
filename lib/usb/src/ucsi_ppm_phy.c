#include "ucsi_ppm_phy.h"

#include "drivers/fusb302/fusb302_reg.h"

// All POWER blocks on (bandgap, receiver/refs, measure, oscillator).
// plan/fusb302.md §6 — full power for PD communication.
#define PHY_POWER_ALL_ON 0x0Fu

// HOST_CUR field values (CONTROL0[3:2]).
#define HOST_CUR_NONE        0x0u
#define HOST_CUR_USB_DEFAULT 0x1u // 80 µA
#define HOST_CUR_1A5         0x2u // 180 µA
#define HOST_CUR_3A          0x3u // 330 µA

// CONTROL2.MODE field values.
#define CONTROL2_MODE_DRP 0x1u
#define CONTROL2_MODE_SNK 0x2u
#define CONTROL2_MODE_SRC 0x3u

// CONTROL2.TOG_SAVE_PWR: 40 ms tDIS between toggle cycles (datasheet).
#define CONTROL2_TOG_SAVE_PWR 0x1u

// SWITCHES1.SPEC_REV: PD revision used for auto-GoodCRC header construction.
// 00 = R1.0 (deprecated), 01 = R2.0, 10/11 = Do Not Use.
// In v1 we always advertise R3.0 in real PD messages; auto-GoodCRC is
// fine echoing the partner's rev, so we default to R2.0 here.
#define SWITCHES1_SPEC_REV_2_0 0x1u

// --- I²C helpers -----------------------------------------------------------

static UcsiPpmStatus phy_read_reg(UcsiPpm* ppm, uint8_t reg, uint8_t* out_value) {
    const uint8_t addr = ppm->config.fusb302_i2c_addr;
    UcsiPpmStatus s = ppm->config.i2c_write(ppm->config.hal_ctx, addr, &reg, 1);
    if(s != UcsiPpmStatusOk) return s;
    return ppm->config.i2c_read(ppm->config.hal_ctx, addr, out_value, 1);
}

static UcsiPpmStatus phy_write_reg(UcsiPpm* ppm, uint8_t reg, uint8_t value) {
    const uint8_t buf[2] = {reg, value};
    return ppm->config.i2c_write(ppm->config.hal_ctx, ppm->config.fusb302_i2c_addr, buf, 2);
}

// Reads three consecutive registers in one burst (auto-increment).
// Used for the interrupt-pump fast path.
static UcsiPpmStatus phy_read_regs(UcsiPpm* ppm, uint8_t reg, uint8_t* out, size_t n) {
    const uint8_t addr = ppm->config.fusb302_i2c_addr;
    UcsiPpmStatus s = ppm->config.i2c_write(ppm->config.hal_ctx, addr, &reg, 1);
    if(s != UcsiPpmStatusOk) return s;
    return ppm->config.i2c_read(ppm->config.hal_ctx, addr, out, n);
}

// Read-modify-write a single register. Useful for sticky bits we don't
// want to clobber. Returns InvalidArg if shift/width are off.
static UcsiPpmStatus phy_rmw_reg(UcsiPpm* ppm, uint8_t reg, uint8_t mask, uint8_t value) {
    uint8_t cur;
    UcsiPpmStatus s = phy_read_reg(ppm, reg, &cur);
    if(s != UcsiPpmStatusOk) return s;
    const uint8_t next = (uint8_t)((cur & ~mask) | (value & mask));
    return phy_write_reg(ppm, reg, next);
}

// --- masks -----------------------------------------------------------------

// Default mask set per plan/fusb302.md §4.1.
// "Mask=0" means "interrupt fires", "Mask=1" means "masked off".
//
// INTERRUPT: open BC_LVL, COLLISION, COMP_CHNG, VBUSOK. Mask WAKE, ALERT,
// CRC_CHK (auto-GoodCRC handles), ACTIVITY.
#define DEFAULT_MASK_VAL ((1u << 2 /* WAKE */) | (1u << 3 /* ALERT */) | (1u << 4 /* CRC_CHK */) | (1u << 6 /* ACTIVITY */))

// INTERRUPTA: open HARDRST, TXSENT, HARDSENT, RETRYFAIL, TOGDONE.
// Mask SOFTRST (we detect via RX FIFO), SOFTFAIL (we don't use AUTO_SOFTRESET),
// OCP_TEMP (we don't source VCONN).
#define DEFAULT_MASKA_VAL ((1u << 1 /* SOFTRST */) | (1u << 5 /* SOFTFAIL */) | (1u << 7 /* OCP_TEMP */))

// INTERRUPTB: open GCRCSENT only.
#define DEFAULT_MASKB_VAL 0x00u

// --- helpers for typed setters ---------------------------------------------

static uint8_t mode_to_field(UcsiPpmPhyToggleMode mode) {
    switch(mode) {
    case UcsiPpmPhyToggleModeSrc:
        return CONTROL2_MODE_SRC;
    case UcsiPpmPhyToggleModeSnk:
        return CONTROL2_MODE_SNK;
    case UcsiPpmPhyToggleModeDrp:
    default:
        return CONTROL2_MODE_DRP;
    }
}

static uint8_t rp_current_to_field(UcsiPpmRpCurrent c) {
    switch(c) {
    case UcsiPpmRpCurrent1A5:
        return HOST_CUR_1A5;
    case UcsiPpmRpCurrent3A:
        return HOST_CUR_3A;
    case UcsiPpmRpCurrentUsbDefault:
    default:
        return HOST_CUR_USB_DEFAULT;
    }
}

// --- lifecycle -------------------------------------------------------------

UcsiPpmStatus ucsi_ppm_phy_sw_reset(UcsiPpm* ppm) {
    // RESET.SW_RESET self-clears in hardware.
    return phy_write_reg(ppm, Fusb302RegReset, 0x01u);
}

UcsiPpmStatus ucsi_ppm_phy_pd_reset(UcsiPpm* ppm) {
    // RESET.PD_RESET self-clears.
    return phy_write_reg(ppm, Fusb302RegReset, 0x02u);
}

UcsiPpmStatus ucsi_ppm_phy_init(UcsiPpm* ppm) {
    UcsiPpmStatus s;

    s = ucsi_ppm_phy_sw_reset(ppm);
    if(s != UcsiPpmStatusOk) return s;

    s = phy_write_reg(ppm, Fusb302RegPower, PHY_POWER_ALL_ON);
    if(s != UcsiPpmStatusOk) return s;

    // Open the masks we care about and close the noisy / unused ones.
    s = phy_write_reg(ppm, Fusb302RegMask, DEFAULT_MASK_VAL);
    if(s != UcsiPpmStatusOk) return s;
    s = phy_write_reg(ppm, Fusb302RegMaskA, DEFAULT_MASKA_VAL);
    if(s != UcsiPpmStatusOk) return s;
    s = phy_write_reg(ppm, Fusb302RegMaskB, DEFAULT_MASKB_VAL);
    if(s != UcsiPpmStatusOk) return s;

    // Global INT_MASK = 0 (interrupts enabled to INT pin). CONTROL0 reset
    // value is 0x24 (HOST_CUR=01b, INT_MASK=0); after SW_RESET we just
    // make sure INT_MASK is clear. tx_flush/auto_pre stay 0.
    s = phy_rmw_reg(ppm, Fusb302RegControl0, 1u << 5 /* INT_MASK */, 0u);
    if(s != UcsiPpmStatusOk) return s;

    return UcsiPpmStatusOk;
}

UcsiPpmStatus ucsi_ppm_phy_deinit(UcsiPpm* ppm) {
    // Best-effort: drop terminations, disable AUTO_CRC, stop toggling.
    // Ignore individual errors — we're tearing down.
    (void)phy_write_reg(ppm, Fusb302RegSwitches0, 0x00u);
    (void)phy_write_reg(ppm, Fusb302RegSwitches1, 0x00u);
    (void)phy_write_reg(ppm, Fusb302RegControl2, 0x00u);
    return UcsiPpmStatusOk;
}

// --- Type-C primitives -----------------------------------------------------

UcsiPpmStatus ucsi_ppm_phy_start_toggle(UcsiPpm* ppm, UcsiPpmPhyToggleMode mode) {
    // CONTROL2: TOGGLE=1, MODE, TOG_RD_ONLY=0 (settle on Rd or Ra),
    // TOG_SAVE_PWR=01b (40 ms tDIS), WAKE_EN=0.
    const uint8_t val = (uint8_t)(1u /* TOGGLE */ | ((mode_to_field(mode) & 0x3u) << 1) | (CONTROL2_TOG_SAVE_PWR << 6));
    return phy_write_reg(ppm, Fusb302RegControl2, val);
}

UcsiPpmStatus ucsi_ppm_phy_stop_toggle(UcsiPpm* ppm) {
    // CONTROL2.TOGGLE = 0; preserve other bits (use RMW).
    return phy_rmw_reg(ppm, Fusb302RegControl2, 1u /* TOGGLE */, 0u);
}

UcsiPpmStatus ucsi_ppm_phy_read_toggle_result(UcsiPpm* ppm, UcsiPpmPhyTogss* out) {
    uint8_t status1a;
    UcsiPpmStatus s = phy_read_reg(ppm, Fusb302RegStatus1A, &status1a);
    if(s != UcsiPpmStatusOk) return s;
    // TOGSS is bits 5:3 (datasheet, Fusb302Status1ARegBits).
    *out = (uint8_t)((status1a >> 3) & 0x07u);
    return UcsiPpmStatusOk;
}

UcsiPpmStatus ucsi_ppm_phy_lock_polarity(UcsiPpm* ppm, UcsiPpmPhyCc cc) {
    // SWITCHES0: PDWN1/PDWN2 set when used as sink; MEAS_CC*; PU_EN* when source.
    // Here we only touch MEAS_CC* — Rp/Rd enable bits are managed by the
    // role/toggle setters separately to avoid stomping each other.
    uint8_t sw0;
    UcsiPpmStatus s = phy_read_reg(ppm, Fusb302RegSwitches0, &sw0);
    if(s != UcsiPpmStatusOk) return s;
    sw0 = (uint8_t)(sw0 & ~((1u << 2 /* MEAS_CC1 */) | (1u << 3 /* MEAS_CC2 */)));
    if(cc == UcsiPpmPhyCc1) sw0 |= (1u << 2);
    if(cc == UcsiPpmPhyCc2) sw0 |= (1u << 3);
    s = phy_write_reg(ppm, Fusb302RegSwitches0, sw0);
    if(s != UcsiPpmStatusOk) return s;

    // SWITCHES1: TX_CC1/TX_CC2 select the BMC transmit driver.
    uint8_t sw1;
    s = phy_read_reg(ppm, Fusb302RegSwitches1, &sw1);
    if(s != UcsiPpmStatusOk) return s;
    sw1 = (uint8_t)(sw1 & ~((1u << 0 /* TX_CC1 */) | (1u << 1 /* TX_CC2 */)));
    if(cc == UcsiPpmPhyCc1) sw1 |= (1u << 0);
    if(cc == UcsiPpmPhyCc2) sw1 |= (1u << 1);
    return phy_write_reg(ppm, Fusb302RegSwitches1, sw1);
}

UcsiPpmStatus ucsi_ppm_phy_set_rp_current(UcsiPpm* ppm, UcsiPpmRpCurrent current) {
    // CONTROL0.HOST_CUR bits 3:2.
    const uint8_t v = (uint8_t)((rp_current_to_field(current) & 0x3u) << 2);
    return phy_rmw_reg(ppm, Fusb302RegControl0, 0xCu /* HOST_CUR mask */, v);
}

UcsiPpmStatus ucsi_ppm_phy_set_msg_header_bits(UcsiPpm* ppm, bool power_role_src, bool data_role_dfp, uint8_t spec_rev) {
    // SWITCHES1: AUTO_CRC (bit 2), DATA_ROLE (bit 4), SPEC_REV (bits 6:5),
    // POWER_ROLE (bit 7). We touch the role/rev bits and leave AUTO_CRC alone.
    const uint8_t mask = (uint8_t)((1u << 4) | (3u << 5) | (1u << 7));
    const uint8_t value = (uint8_t)(((data_role_dfp ? 1u : 0u) << 4) | (((uint32_t)spec_rev & 0x3u) << 5) | ((power_role_src ? 1u : 0u) << 7));
    return phy_rmw_reg(ppm, Fusb302RegSwitches1, mask, value);
}

// --- PD path ---------------------------------------------------------------

UcsiPpmStatus ucsi_ppm_phy_enable_pd(UcsiPpm* ppm, uint8_t n_retries) {
    // SWITCHES1.AUTO_CRC = 1.
    UcsiPpmStatus s = phy_rmw_reg(ppm, Fusb302RegSwitches1, 1u << 2, 1u << 2);
    if(s != UcsiPpmStatusOk) return s;

    // CONTROL3: AUTO_RETRY=1, N_RETRIES, AUTO_SOFTRESET=0, AUTO_HARDRESET=0.
    // We keep the higher-level reset logic in PE rather than letting the
    // chip auto-fire it (plan/fusb302.md §1.4).
    if(n_retries > 3u) n_retries = 3u;
    const uint8_t mask = (uint8_t)((1u << 0 /* AUTO_RETRY */) | (3u << 1 /* N_RETRIES */) | (1u << 3 /* AUTO_SOFTRESET */) | (1u << 4 /* AUTO_HARDRESET */));
    const uint8_t val = (uint8_t)((1u << 0) | ((uint32_t)n_retries << 1));
    return phy_rmw_reg(ppm, Fusb302RegControl3, mask, val);
}

UcsiPpmStatus ucsi_ppm_phy_disable_pd(UcsiPpm* ppm) {
    return phy_rmw_reg(ppm, Fusb302RegSwitches1, 1u << 2 /* AUTO_CRC */, 0u);
}

UcsiPpmStatus ucsi_ppm_phy_send_hard_reset(UcsiPpm* ppm) {
    // CONTROL3.SEND_HARD_RESET = 1 (self-clearing).
    return phy_rmw_reg(ppm, Fusb302RegControl3, 1u << 6, 1u << 6);
}

UcsiPpmStatus ucsi_ppm_phy_flush_tx(UcsiPpm* ppm) {
    // CONTROL0.TX_FLUSH = 1 (self-clearing). No blocking wait — caller
    // observes via STATUS1.TX_EMPTY if needed. plan/fusb302.md §8.2 fix #1.
    return phy_rmw_reg(ppm, Fusb302RegControl0, 1u << 6, 1u << 6);
}

UcsiPpmStatus ucsi_ppm_phy_flush_rx(UcsiPpm* ppm) {
    // CONTROL1.RX_FLUSH = 1 (self-clearing).
    return phy_rmw_reg(ppm, Fusb302RegControl1, 1u << 2, 1u << 2);
}

// --- Measurements (MDAC + BC_LVL) ------------------------------------------

// MDAC field is 6 bits (MEASURE[5:0]); each step is 42 mV on the comparator
// reference. When MEAS_VBUS=1 the VBUS input is divided by 10, so the
// effective threshold step on actual VBUS is 420 mV.
#define MDAC_LSB_MV       42u
#define MDAC_VBUS_DIVISOR 10u
#define MDAC_VBUS_STEP_MV (MDAC_LSB_MV * MDAC_VBUS_DIVISOR)
#define MDAC_FIELD_MASK   0x3Fu
#define MEASURE_MEAS_VBUS (1u << 6)

// STATUS0.COMP is bit 5 (datasheet Fusb302Status0RegBits).
#define STATUS0_COMP (1u << 5)

// Rounds up so the actual threshold is >= voltage_mv (conservative for
// "above X mV" detection — see header doc).
static uint8_t vbus_mv_to_mdac(uint16_t voltage_mv) {
    uint32_t v = ((uint32_t)voltage_mv + (MDAC_VBUS_STEP_MV - 1u)) / MDAC_VBUS_STEP_MV;
    if(v > MDAC_FIELD_MASK) v = MDAC_FIELD_MASK;
    return (uint8_t)v;
}

UcsiPpmStatus ucsi_ppm_phy_measure_vbus_threshold(UcsiPpm* ppm, uint16_t voltage_mv, bool* above) {
    if(!above) return UcsiPpmStatusInvalidArg;
    const uint8_t mdac = vbus_mv_to_mdac(voltage_mv);
    const uint8_t measure = (uint8_t)(MEASURE_MEAS_VBUS | (mdac & MDAC_FIELD_MASK));
    UcsiPpmStatus s = phy_write_reg(ppm, Fusb302RegMeasure, measure);
    if(s != UcsiPpmStatusOk) return s;
    // The two I²C round-trips (write MEASURE then read STATUS0) far exceed
    // the ~30 µs comparator settling time, so we read once and trust it.
    uint8_t status0;
    s = phy_read_reg(ppm, Fusb302RegStatus0, &status0);
    if(s != UcsiPpmStatusOk) return s;
    *above = (status0 & STATUS0_COMP) != 0u;
    return UcsiPpmStatusOk;
}

UcsiPpmStatus ucsi_ppm_phy_arm_vbus_compare(UcsiPpm* ppm, uint16_t voltage_mv) {
    const uint8_t mdac = vbus_mv_to_mdac(voltage_mv);
    const uint8_t measure = (uint8_t)(MEASURE_MEAS_VBUS | (mdac & MDAC_FIELD_MASK));
    return phy_write_reg(ppm, Fusb302RegMeasure, measure);
}

UcsiPpmStatus ucsi_ppm_phy_read_bc_lvl(UcsiPpm* ppm, uint8_t* out_bc_lvl) {
    if(!out_bc_lvl) return UcsiPpmStatusInvalidArg;
    uint8_t status0;
    UcsiPpmStatus s = phy_read_reg(ppm, Fusb302RegStatus0, &status0);
    if(s != UcsiPpmStatusOk) return s;
    *out_bc_lvl = (uint8_t)(status0 & 0x03u); // bits 0:1
    return UcsiPpmStatusOk;
}

// --- Interrupt pump --------------------------------------------------------

static void emit(UcsiPpmPhyEventSink sink, void* ctx, UcsiPpmPhyEventKind kind) {
    UcsiPpmPhyEvent ev = {.kind = kind};
    sink(ctx, &ev);
}

UcsiPpmStatus ucsi_ppm_phy_pump(UcsiPpm* ppm, UcsiPpmPhyEventSink sink, void* sink_ctx) {
    if(!sink) return UcsiPpmStatusInvalidArg;

    // Read the three interrupt registers in one burst. They are adjacent
    // (INTERRUPTA = 0x3E, INTERRUPTB = 0x3F, INTERRUPT = 0x42) but not
    // contiguous, so we issue two reads: A+B together, then INTERRUPT.
    uint8_t inta_intb[2];
    UcsiPpmStatus s = phy_read_regs(ppm, Fusb302RegInterruptA, inta_intb, 2);
    if(s != UcsiPpmStatusOk) return s;
    const uint8_t inta = inta_intb[0];
    const uint8_t intb = inta_intb[1];

    uint8_t intr;
    s = phy_read_reg(ppm, Fusb302RegInterrupt, &intr);
    if(s != UcsiPpmStatusOk) return s;

    // INTERRUPT bits (datasheet Fusb302InterruptRegBits but unused — we go
    // by raw bit positions per fusb302.md §4.1).
    if(intr & (1u << 0)) { // I_BC_LVL
        uint8_t status0;
        if(phy_read_reg(ppm, Fusb302RegStatus0, &status0) == UcsiPpmStatusOk) {
            UcsiPpmPhyEvent ev = {
                .kind = UcsiPpmPhyEventBcLvlChanged,
                .u.bc_lvl = (uint8_t)(status0 & 0x03u),
            };
            sink(sink_ctx, &ev);
        }
    }
    if(intr & (1u << 1)) { // I_COLLISION
        emit(sink, sink_ctx, UcsiPpmPhyEventCollision);
    }
    if(intr & (1u << 5)) { // I_COMP_CHNG
        uint8_t status0;
        if(phy_read_reg(ppm, Fusb302RegStatus0, &status0) == UcsiPpmStatusOk) {
            UcsiPpmPhyEvent ev = {
                .kind = UcsiPpmPhyEventCompChanged,
                .u.comp_above = ((status0 & (1u << 5)) != 0u), // STATUS0.COMP
            };
            sink(sink_ctx, &ev);
        }
    }
    if(intr & (1u << 7)) { // I_VBUSOK
        uint8_t status0;
        if(phy_read_reg(ppm, Fusb302RegStatus0, &status0) == UcsiPpmStatusOk) {
            UcsiPpmPhyEvent ev = {
                .kind = UcsiPpmPhyEventVbusChanged,
                .u.vbus_ok = ((status0 & (1u << 7)) != 0u), // STATUS0.VBUSOK
            };
            sink(sink_ctx, &ev);
        }
    }

    // INTERRUPTA bits (Fusb302InterruptARegBits).
    if(inta & (1u << 0)) emit(sink, sink_ctx, UcsiPpmPhyEventHardResetRx); // I_HARDRST
    if(inta & (1u << 2)) emit(sink, sink_ctx, UcsiPpmPhyEventTxSuccess); // I_TXSENT
    if(inta & (1u << 3)) emit(sink, sink_ctx, UcsiPpmPhyEventHardResetSent); // I_HARDSENT
    if(inta & (1u << 4)) emit(sink, sink_ctx, UcsiPpmPhyEventTxRetryFail); // I_RETRYFAIL
    if(inta & (1u << 6)) { // I_TOGDONE
        UcsiPpmPhyTogss togss = 0u;
        (void)ucsi_ppm_phy_read_toggle_result(ppm, &togss);
        UcsiPpmPhyEvent ev = {
            .kind = UcsiPpmPhyEventToggleDone,
            .u.togss = togss,
        };
        sink(sink_ctx, &ev);
    }

    // INTERRUPTB bit 0 == I_GCRCSENT — auto-GoodCRC fired in response to a
    // good incoming packet. Treat as "RX has a message" so PRL pulls it.
    if(intb & (1u << 0)) {
        emit(sink, sink_ctx, UcsiPpmPhyEventMessageRx);
    }

    return UcsiPpmStatusOk;
}
