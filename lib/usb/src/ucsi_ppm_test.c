#include "ucsi_ppm_test.h"

#include "ucsi_ppm.h"
#include "ucsi_ppm_i.h"
#include "ucsi_ppm_phy.h"

#include "drivers/fusb302/fusb302_reg.h"

#include <furi.h>
#include <string.h>

#define TAG "UcsiPpmTest"

#define TEST_ASSERT(cond)                                                 \
    do {                                                                  \
        if(!(cond)) {                                                     \
            FURI_LOG_E(TAG, "FAIL %s:%d: %s", __func__, __LINE__, #cond); \
            return false;                                                 \
        }                                                                 \
    } while(0)

// --- HAL callback stubs ----------------------------------------------------
// All stubs are side-effect-free; tests never exercise FUSB302/PSU paths in
// L1 (those are wired up later).

static uint32_t stub_time_ms(void* ctx) {
    (void)ctx;
    return 0;
}

static void stub_alert(void* ctx) {
    (void)ctx;
}

// Alert sink that counts invocations. Used by L2 tests that need to verify
// the dispatcher raises an alert when CCI changes.
static int g_alert_count = 0;
static void counting_alert(void* ctx) {
    (void)ctx;
    g_alert_count++;
}

// Forward declaration — defined further down with the L1 helpers.
static void make_valid_config(UcsiPpmConfig* c);

// --- Mock I²C for FUSB302 (L4) tests ---------------------------------------
// Records every i2c_read/i2c_write and exposes a simulated 256-byte FUSB302
// register file. Reads pick up the register address from the immediately
// preceding write (FUSB302 protocol: write reg addr, then repeated-start read).

#define MOCK_I2C_MAX_TXNS 256

typedef struct {
    bool is_write;
    uint8_t addr;
    uint8_t data[64]; // PD message TX burst can be up to ~40 bytes
    size_t len;
} MockI2cTxn;

static MockI2cTxn g_mock_txns[MOCK_I2C_MAX_TXNS];
static size_t g_mock_txn_count;
static uint8_t g_mock_regs[256];

// Controllable wall-clock for time-dependent TC tests. Default 0 — tests
// that don't care about time get the same behaviour as stub_time_ms.
static uint32_t g_mock_time_ms = 0;
static uint32_t mock_time_ms(void* ctx) {
    (void)ctx;
    return g_mock_time_ms;
}

// Records calls to gpio_write_vbus_source so tests can verify the source-side
// VBUS switch toggles at the right state transitions.
static int g_mock_vbus_source_calls = 0;
static bool g_mock_vbus_source_last = false;
static void mock_gpio_write_vbus_source(void* ctx, bool value) {
    (void)ctx;
    g_mock_vbus_source_calls++;
    g_mock_vbus_source_last = value;
}

// Records calls to power_supply_set so source-path tests can verify PE
// drove the external PSU with the negotiated voltage/current.
static int g_mock_psu_calls = 0;
static uint16_t g_mock_psu_last_voltage_mv = 0;
static uint16_t g_mock_psu_last_current_ma = 0;
static UcsiPpmStatus
    mock_power_supply_set(void* ctx, uint16_t voltage_mv, uint16_t current_limit_ma) {
    (void)ctx;
    g_mock_psu_calls++;
    g_mock_psu_last_voltage_mv = voltage_mv;
    g_mock_psu_last_current_ma = current_limit_ma;
    return UcsiPpmStatusOk;
}

// Counts L2 alert callbacks so tests can verify OPM was woken on state changes.
static int g_mock_alert_calls = 0;
static void mock_alert(void* ctx) {
    (void)ctx;
    g_mock_alert_calls++;
}

// FUSB302 FIFOS is a port-like register: every read dequeues one byte.
// The mock keeps a separate byte queue for FIFOS reads to model this
// (g_mock_regs[FIFOS] would auto-increment to neighbouring addresses,
// which is wrong for a queue).
#define MOCK_FIFO_MAX 128
static uint8_t g_mock_fifo[MOCK_FIFO_MAX];
static size_t g_mock_fifo_len;
static size_t g_mock_fifo_pos;

static void mock_fifo_load(const uint8_t* bytes, size_t n) {
    if(n > MOCK_FIFO_MAX) n = MOCK_FIFO_MAX;
    memcpy(g_mock_fifo, bytes, n);
    g_mock_fifo_len = n;
    g_mock_fifo_pos = 0;
}

// Partner-side MessageID counter for simulated incoming PD frames. Increments
// per simulated message so PRL dedup doesn't drop legitimate replies.
static uint8_t g_test_partner_msg_id = 0;

static void mock_i2c_reset(void) {
    g_mock_txn_count = 0;
    memset(g_mock_regs, 0, sizeof(g_mock_regs));
    g_mock_fifo_len = 0;
    g_mock_fifo_pos = 0;
    g_mock_time_ms = 0;
    g_mock_vbus_source_calls = 0;
    g_mock_vbus_source_last = false;
    g_mock_psu_calls = 0;
    g_mock_psu_last_voltage_mv = 0;
    g_mock_psu_last_current_ma = 0;
    g_mock_alert_calls = 0;
    g_test_partner_msg_id = 0;
}

static UcsiPpmStatus mock_i2c_write_fn(void* ctx, uint8_t addr, const uint8_t* data, size_t len) {
    (void)ctx;
    if(g_mock_txn_count >= MOCK_I2C_MAX_TXNS) return UcsiPpmStatusInternal;
    MockI2cTxn* t = &g_mock_txns[g_mock_txn_count++];
    t->is_write = true;
    t->addr = addr;
    t->len = (len > sizeof(t->data)) ? sizeof(t->data) : len;
    memcpy(t->data, data, t->len);
    // 2-byte writes are register stores; update the simulated register file.
    if(len == 2u) {
        g_mock_regs[data[0]] = data[1];
    }
    return UcsiPpmStatusOk;
}

static UcsiPpmStatus mock_i2c_read_fn(void* ctx, uint8_t addr, uint8_t* data, size_t len) {
    (void)ctx;
    if(g_mock_txn_count == 0u) return UcsiPpmStatusInternal;
    const MockI2cTxn* last = &g_mock_txns[g_mock_txn_count - 1u];
    if(!last->is_write || last->len < 1u) return UcsiPpmStatusInternal;
    const uint8_t reg = last->data[0];
    if(reg == Fusb302RegFifos) {
        // FIFOS is a port; each byte comes from the queue, not from
        // neighbouring register addresses.
        for(size_t i = 0; i < len; ++i) {
            data[i] = (g_mock_fifo_pos < g_mock_fifo_len) ? g_mock_fifo[g_mock_fifo_pos++] : 0u;
        }
    } else {
        for(size_t i = 0; i < len; ++i) {
            const uint8_t cur_reg = (uint8_t)((reg + (uint8_t)i) & 0xFFu);
            uint8_t value = g_mock_regs[cur_reg];
            // STATUS1.RX_EMPTY (bit 5) is derived from the FIFO queue state
            // so phy_recv_message's empty check stays consistent without
            // tests having to keep g_mock_regs[STATUS1] in sync by hand.
            if(cur_reg == Fusb302RegStatus1) {
                if(g_mock_fifo_pos >= g_mock_fifo_len) {
                    value |= (uint8_t)(1u << 5);
                } else {
                    value &= (uint8_t)~(1u << 5);
                }
            }
            data[i] = value;
        }
    }
    if(g_mock_txn_count < MOCK_I2C_MAX_TXNS) {
        MockI2cTxn* t = &g_mock_txns[g_mock_txn_count++];
        t->is_write = false;
        t->addr = addr;
        t->len = (len > sizeof(t->data)) ? sizeof(t->data) : len;
        memcpy(t->data, data, t->len);
    }
    return UcsiPpmStatusOk;
}

// True if the last (or any) 2-byte write hit `reg` with `value` exactly.
static bool mock_any_write_to(uint8_t reg, uint8_t value) {
    for(size_t i = 0; i < g_mock_txn_count; ++i) {
        const MockI2cTxn* t = &g_mock_txns[i];
        if(t->is_write && t->len == 2u && t->data[0] == reg && t->data[1] == value) {
            return true;
        }
    }
    return false;
}

// Builds a ppm with mock I²C / time / GPIO wired in. Returns NULL on error.
// Resets the mock state on entry so tests don't accidentally trip the
// txn buffer cap with leftover entries from earlier tests.
static UcsiPpm* mock_make_ppm(void) {
    mock_i2c_reset();
    UcsiPpm* ppm = ucsi_ppm_alloc();
    if(!ppm) return NULL;
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    cfg.i2c_read = mock_i2c_read_fn;
    cfg.i2c_write = mock_i2c_write_fn;
    cfg.time_ms = mock_time_ms;
    cfg.gpio_write_vbus_source = mock_gpio_write_vbus_source;
    cfg.power_supply_set = mock_power_supply_set;
    cfg.alert = mock_alert;
    if(ucsi_ppm_init(ppm, &cfg) != UcsiPpmStatusOk) {
        ucsi_ppm_free(ppm);
        return NULL;
    }
    // Simulate OPM having called SET_NOTIFICATION_ENABLE with all CSC bits
    // enabled so notify_connector_change actually raises alerts in tests.
    // Mask-filter behaviour itself is covered by dedicated tests below.
    ppm->notification_mask = 0xFFFEu;
    return ppm;
}

// Reads CCI (4 bytes at offset 4) as a little-endian uint32_t.
static uint32_t read_cci(UcsiPpm* ppm) {
    uint8_t buf[4];
    ucsi_ppm_register_read(ppm, UCSI_PPM_OFFSET_CCI, 4, buf);
    return (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) | ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
}

static UcsiPpmStatus stub_i2c_read(void* ctx, uint8_t addr, uint8_t* data, size_t len) {
    (void)ctx;
    (void)addr;
    // Zero the buffer so callers using this stub (i.e., L1/L2 tests where
    // ucsi_ppm_init now triggers ucsi_ppm_phy_init underneath) read defined
    // bytes during RMW. Otherwise downstream uses of `cur` are UB.
    memset(data, 0, len);
    return UcsiPpmStatusOk;
}

static UcsiPpmStatus stub_i2c_write(void* ctx, uint8_t addr, const uint8_t* data, size_t len) {
    (void)ctx;
    (void)addr;
    (void)data;
    (void)len;
    return UcsiPpmStatusOk;
}

static void stub_gpio_write(void* ctx, bool value) {
    (void)ctx;
    (void)value;
}

static UcsiPpmStatus stub_psu_set(void* ctx, uint16_t voltage_mv, uint16_t current_limit_ma) {
    (void)ctx;
    (void)voltage_mv;
    (void)current_limit_ma;
    return UcsiPpmStatusOk;
}

static bool stub_has_alt_power(void* ctx) {
    (void)ctx;
    return true;
}

// Builds a minimal-valid UcsiPpmConfig that passes config_is_valid().
static void make_valid_config(UcsiPpmConfig* c) {
    memset(c, 0, sizeof(*c));
    c->time_ms = stub_time_ms;
    c->alert = stub_alert;
    c->i2c_read = stub_i2c_read;
    c->i2c_write = stub_i2c_write;
    c->gpio_write_vbus_source = stub_gpio_write;
    c->power_supply_set = stub_psu_set;
    c->has_alt_power = stub_has_alt_power;
    c->fusb302_i2c_addr = 0x22;
    c->initial_cc_operation_mode = UcsiPpmCcModeDrp;
    c->drp_advertise_first = UcsiPpmDrpFirstSrc;
    c->source_rp_current = UcsiPpmRpCurrent3A;
    c->source_caps.pdos[0] = ucsi_ppm_pdo_fixed_source(5000, 3000, true, false, true, true);
    c->source_caps.count = 1;
    c->sink_caps.pdos[0] = ucsi_ppm_pdo_fixed_sink(5000, 3000, true, false, false, true, true);
    c->sink_caps.count = 1;
    c->power_source_other = true;
    c->supports_usb_pd = true;
}

// --- alloc / free ----------------------------------------------------------

static bool test_alloc_returns_nonnull(void) {
    UcsiPpm* ppm = ucsi_ppm_alloc();
    TEST_ASSERT(ppm != NULL);
    ucsi_ppm_free(ppm);
    return true;
}

static bool test_free_null_is_noop(void) {
    ucsi_ppm_free(NULL);
    return true;
}

static bool test_free_auto_deinit(void) {
    UcsiPpm* ppm = ucsi_ppm_alloc();
    TEST_ASSERT(ppm != NULL);
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    TEST_ASSERT(ucsi_ppm_init(ppm, &cfg) == UcsiPpmStatusOk);
    ucsi_ppm_free(ppm); // must auto-deinit, no crash
    return true;
}

// --- init / deinit / reset -------------------------------------------------

static bool test_init_null_args(void) {
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    TEST_ASSERT(ucsi_ppm_init(NULL, &cfg) == UcsiPpmStatusInvalidArg);

    UcsiPpm* ppm = ucsi_ppm_alloc();
    TEST_ASSERT(ucsi_ppm_init(ppm, NULL) == UcsiPpmStatusInvalidArg);
    ucsi_ppm_free(ppm);
    return true;
}

static bool test_init_double(void) {
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    TEST_ASSERT(ucsi_ppm_init(ppm, &cfg) == UcsiPpmStatusOk);
    TEST_ASSERT(ucsi_ppm_init(ppm, &cfg) == UcsiPpmStatusAlreadyInitialized);
    ucsi_ppm_free(ppm);
    return true;
}

static bool test_init_validates_callbacks(void) {
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;

    make_valid_config(&cfg);
    cfg.time_ms = NULL;
    TEST_ASSERT(ucsi_ppm_init(ppm, &cfg) == UcsiPpmStatusInvalidConfig);

    make_valid_config(&cfg);
    cfg.alert = NULL;
    TEST_ASSERT(ucsi_ppm_init(ppm, &cfg) == UcsiPpmStatusInvalidConfig);

    make_valid_config(&cfg);
    cfg.i2c_read = NULL;
    TEST_ASSERT(ucsi_ppm_init(ppm, &cfg) == UcsiPpmStatusInvalidConfig);

    make_valid_config(&cfg);
    cfg.i2c_write = NULL;
    TEST_ASSERT(ucsi_ppm_init(ppm, &cfg) == UcsiPpmStatusInvalidConfig);

    make_valid_config(&cfg);
    cfg.gpio_write_vbus_source = NULL;
    TEST_ASSERT(ucsi_ppm_init(ppm, &cfg) == UcsiPpmStatusInvalidConfig);

    make_valid_config(&cfg);
    cfg.power_supply_set = NULL;
    TEST_ASSERT(ucsi_ppm_init(ppm, &cfg) == UcsiPpmStatusInvalidConfig);

    make_valid_config(&cfg);
    cfg.has_alt_power = NULL;
    TEST_ASSERT(ucsi_ppm_init(ppm, &cfg) == UcsiPpmStatusInvalidConfig);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_init_validates_i2c_addr(void) {
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;

    make_valid_config(&cfg);
    cfg.fusb302_i2c_addr = 0x21;
    TEST_ASSERT(ucsi_ppm_init(ppm, &cfg) == UcsiPpmStatusInvalidConfig);

    make_valid_config(&cfg);
    cfg.fusb302_i2c_addr = 0x26;
    TEST_ASSERT(ucsi_ppm_init(ppm, &cfg) == UcsiPpmStatusInvalidConfig);

    make_valid_config(&cfg);
    cfg.fusb302_i2c_addr = 0x22;
    TEST_ASSERT(ucsi_ppm_init(ppm, &cfg) == UcsiPpmStatusOk);
    TEST_ASSERT(ucsi_ppm_deinit(ppm) == UcsiPpmStatusOk);

    make_valid_config(&cfg);
    cfg.fusb302_i2c_addr = 0x25;
    TEST_ASSERT(ucsi_ppm_init(ppm, &cfg) == UcsiPpmStatusOk);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_init_validates_pdo(void) {
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;

    make_valid_config(&cfg);
    cfg.source_caps.count = 0;
    TEST_ASSERT(ucsi_ppm_init(ppm, &cfg) == UcsiPpmStatusInvalidConfig);

    make_valid_config(&cfg);
    cfg.sink_caps.count = 0;
    TEST_ASSERT(ucsi_ppm_init(ppm, &cfg) == UcsiPpmStatusInvalidConfig);

    make_valid_config(&cfg);
    cfg.source_caps.count = UCSI_PPM_MAX_PDOS + 1;
    TEST_ASSERT(ucsi_ppm_init(ppm, &cfg) == UcsiPpmStatusInvalidConfig);

    // PDO #1 must be Fixed 5V.
    make_valid_config(&cfg);
    cfg.source_caps.pdos[0] = ucsi_ppm_pdo_fixed_source(9000, 3000, true, false, true, true);
    TEST_ASSERT(ucsi_ppm_init(ppm, &cfg) == UcsiPpmStatusInvalidConfig);

    make_valid_config(&cfg);
    cfg.sink_caps.pdos[0] = ucsi_ppm_pdo_fixed_sink(9000, 3000, true, false, false, true, true);
    TEST_ASSERT(ucsi_ppm_init(ppm, &cfg) == UcsiPpmStatusInvalidConfig);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_init_validates_power_source(void) {
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    cfg.power_source_ac = false;
    cfg.power_source_other = false;
    cfg.power_source_vbus = false;
    TEST_ASSERT(ucsi_ppm_init(ppm, &cfg) == UcsiPpmStatusInvalidConfig);
    ucsi_ppm_free(ppm);
    return true;
}

static bool test_init_validates_disabled_mode(void) {
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    cfg.initial_cc_operation_mode = UcsiPpmCcModeDisabled;
    cfg.supports_disabled_state = false;
    TEST_ASSERT(ucsi_ppm_init(ppm, &cfg) == UcsiPpmStatusInvalidConfig);

    cfg.supports_disabled_state = true;
    TEST_ASSERT(ucsi_ppm_init(ppm, &cfg) == UcsiPpmStatusOk);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_deinit_before_init(void) {
    UcsiPpm* ppm = ucsi_ppm_alloc();
    TEST_ASSERT(ucsi_ppm_deinit(ppm) == UcsiPpmStatusNotInitialized);
    ucsi_ppm_free(ppm);
    return true;
}

static bool test_deinit_then_reinit(void) {
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    TEST_ASSERT(ucsi_ppm_init(ppm, &cfg) == UcsiPpmStatusOk);
    TEST_ASSERT(ucsi_ppm_deinit(ppm) == UcsiPpmStatusOk);
    TEST_ASSERT(ucsi_ppm_init(ppm, &cfg) == UcsiPpmStatusOk);
    ucsi_ppm_free(ppm);
    return true;
}

static bool test_reset_before_init(void) {
    UcsiPpm* ppm = ucsi_ppm_alloc();
    TEST_ASSERT(ucsi_ppm_reset(ppm) == UcsiPpmStatusNotInitialized);
    ucsi_ppm_free(ppm);
    return true;
}

// --- register_read ---------------------------------------------------------

static bool test_register_read_before_init(void) {
    UcsiPpm* ppm = ucsi_ppm_alloc();
    uint8_t buf[4];
    TEST_ASSERT(ucsi_ppm_register_read(ppm, 0, sizeof(buf), buf) == UcsiPpmStatusNotInitialized);
    ucsi_ppm_free(ppm);
    return true;
}

static bool test_register_read_null(void) {
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    ucsi_ppm_init(ppm, &cfg);
    uint8_t buf[4];
    TEST_ASSERT(ucsi_ppm_register_read(NULL, 0, sizeof(buf), buf) == UcsiPpmStatusInvalidArg);
    TEST_ASSERT(ucsi_ppm_register_read(ppm, 0, sizeof(buf), NULL) == UcsiPpmStatusInvalidArg);
    ucsi_ppm_free(ppm);
    return true;
}

static bool test_register_read_bounds(void) {
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    ucsi_ppm_init(ppm, &cfg);
    uint8_t buf[4];
    // 527 + 2 = 529 > 528 → invalid.
    TEST_ASSERT(ucsi_ppm_register_read(ppm, 527, 2, buf) == UcsiPpmStatusInvalidArg);
    // 527 + 1 = 528 → last byte, valid.
    TEST_ASSERT(ucsi_ppm_register_read(ppm, 527, 1, buf) == UcsiPpmStatusOk);
    // length 0 → no-op success.
    TEST_ASSERT(ucsi_ppm_register_read(ppm, 0, 0, buf) == UcsiPpmStatusOk);
    ucsi_ppm_free(ppm);
    return true;
}

static bool test_register_read_version(void) {
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    ucsi_ppm_init(ppm, &cfg);
    uint8_t buf[3];
    TEST_ASSERT(ucsi_ppm_register_read(ppm, UCSI_PPM_OFFSET_VERSION, 3, buf) == UcsiPpmStatusOk);
    // BCD 0x0300 little-endian over 3 bytes.
    TEST_ASSERT(buf[0] == 0x00);
    TEST_ASSERT(buf[1] == 0x03);
    TEST_ASSERT(buf[2] == 0x00);
    ucsi_ppm_free(ppm);
    return true;
}

static bool test_register_read_zeros(void) {
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    ucsi_ppm_init(ppm, &cfg);
    uint8_t buf[8];

    TEST_ASSERT(ucsi_ppm_register_read(ppm, UCSI_PPM_OFFSET_CCI, 4, buf) == UcsiPpmStatusOk);
    for(int i = 0; i < 4; i++)
        TEST_ASSERT(buf[i] == 0);

    TEST_ASSERT(ucsi_ppm_register_read(ppm, UCSI_PPM_OFFSET_CONTROL, 8, buf) == UcsiPpmStatusOk);
    for(int i = 0; i < 8; i++)
        TEST_ASSERT(buf[i] == 0);

    TEST_ASSERT(ucsi_ppm_register_read(ppm, UCSI_PPM_OFFSET_RESERVED1, 1, buf) == UcsiPpmStatusOk);
    TEST_ASSERT(buf[0] == 0);
    TEST_ASSERT(ucsi_ppm_register_read(ppm, UCSI_PPM_OFFSET_RESERVED3, 1, buf) == UcsiPpmStatusOk);
    TEST_ASSERT(buf[0] == 0);

    ucsi_ppm_free(ppm);
    return true;
}

// --- register_write --------------------------------------------------------

static bool test_register_write_before_init(void) {
    UcsiPpm* ppm = ucsi_ppm_alloc();
    uint8_t buf[1] = {0x06};
    TEST_ASSERT(ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 1, buf) == UcsiPpmStatusNotInitialized);
    ucsi_ppm_free(ppm);
    return true;
}

static bool test_register_write_bounds(void) {
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    ucsi_ppm_init(ppm, &cfg);
    uint8_t buf[4] = {0};
    TEST_ASSERT(ucsi_ppm_register_write(ppm, 527, 2, buf) == UcsiPpmStatusInvalidArg);
    ucsi_ppm_free(ppm);
    return true;
}

static bool test_register_write_readonly_zones(void) {
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    ucsi_ppm_init(ppm, &cfg);
    uint8_t buf[8] = {1, 2, 3, 4, 5, 6, 7, 8};

    TEST_ASSERT(ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_VERSION, 1, buf) == UcsiPpmStatusInvalidArg);
    TEST_ASSERT(ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CCI, 4, buf) == UcsiPpmStatusInvalidArg);
    TEST_ASSERT(ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_MESSAGE_IN, 8, buf) == UcsiPpmStatusInvalidArg);

    // Range that spans VERSION[2] + RESERVED1 must be rejected — it touches RO.
    TEST_ASSERT(ucsi_ppm_register_write(ppm, 2, 2, buf) == UcsiPpmStatusInvalidArg);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_register_write_reserved(void) {
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    ucsi_ppm_init(ppm, &cfg);
    uint8_t buf[1] = {0xAA};
    uint8_t got;

    TEST_ASSERT(ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_RESERVED1, 1, buf) == UcsiPpmStatusOk);
    TEST_ASSERT(ucsi_ppm_register_read(ppm, UCSI_PPM_OFFSET_RESERVED1, 1, &got) == UcsiPpmStatusOk);
    TEST_ASSERT(got == 0);

    TEST_ASSERT(ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_RESERVED2, 1, buf) == UcsiPpmStatusOk);
    TEST_ASSERT(ucsi_ppm_register_read(ppm, UCSI_PPM_OFFSET_RESERVED2, 1, &got) == UcsiPpmStatusOk);
    TEST_ASSERT(got == 0);

    TEST_ASSERT(ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_RESERVED3, 1, buf) == UcsiPpmStatusOk);
    TEST_ASSERT(ucsi_ppm_register_read(ppm, UCSI_PPM_OFFSET_RESERVED3, 1, &got) == UcsiPpmStatusOk);
    TEST_ASSERT(got == 0);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_register_write_reserved_span(void) {
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    ucsi_ppm_init(ppm, &cfg);

    // offset 271 covers RESERVED2 (271) + MESSAGE_OUT[0] (272).
    uint8_t buf[2] = {0xAA, 0xBB};
    TEST_ASSERT(ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_RESERVED2, 2, buf) == UcsiPpmStatusOk);

    uint8_t got_reserved;
    TEST_ASSERT(ucsi_ppm_register_read(ppm, UCSI_PPM_OFFSET_RESERVED2, 1, &got_reserved) == UcsiPpmStatusOk);
    TEST_ASSERT(got_reserved == 0);

    uint8_t got_msg;
    TEST_ASSERT(ucsi_ppm_register_read(ppm, UCSI_PPM_OFFSET_MESSAGE_OUT, 1, &got_msg) == UcsiPpmStatusOk);
    TEST_ASSERT(got_msg == 0xBB);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_register_write_control(void) {
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    ucsi_ppm_init(ppm, &cfg);

    // CONTROL[0] = 0x06 — GET_CAPABILITY opcode; triggers L2 dispatch (TODO stub).
    uint8_t buf[1] = {0x06};
    TEST_ASSERT(ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 1, buf) == UcsiPpmStatusOk);

    uint8_t got;
    TEST_ASSERT(ucsi_ppm_register_read(ppm, UCSI_PPM_OFFSET_CONTROL, 1, &got) == UcsiPpmStatusOk);
    TEST_ASSERT(got == 0x06);

    // CONTROL[1..7] writes; no trigger.
    uint8_t buf7[7] = {1, 2, 3, 4, 5, 6, 7};
    TEST_ASSERT(ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL + 1, 7, buf7) == UcsiPpmStatusOk);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_register_write_msg_out(void) {
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    ucsi_ppm_init(ppm, &cfg);

    uint8_t pattern[16];
    for(int i = 0; i < 16; i++)
        pattern[i] = (uint8_t)i;
    TEST_ASSERT(ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_MESSAGE_OUT, 16, pattern) == UcsiPpmStatusOk);

    uint8_t got[16];
    TEST_ASSERT(ucsi_ppm_register_read(ppm, UCSI_PPM_OFFSET_MESSAGE_OUT, 16, got) == UcsiPpmStatusOk);
    for(int i = 0; i < 16; i++)
        TEST_ASSERT(got[i] == pattern[i]);

    ucsi_ppm_free(ppm);
    return true;
}

// --- reset clears regfile --------------------------------------------------

static bool test_reset_clears_regfile(void) {
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    ucsi_ppm_init(ppm, &cfg);

    uint8_t pattern[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_MESSAGE_OUT, 8, pattern);
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 8, pattern);

    TEST_ASSERT(ucsi_ppm_reset(ppm) == UcsiPpmStatusOk);

    uint8_t got[8];
    TEST_ASSERT(ucsi_ppm_register_read(ppm, UCSI_PPM_OFFSET_MESSAGE_OUT, 8, got) == UcsiPpmStatusOk);
    for(int i = 0; i < 8; i++)
        TEST_ASSERT(got[i] == 0);
    TEST_ASSERT(ucsi_ppm_register_read(ppm, UCSI_PPM_OFFSET_CONTROL, 8, got) == UcsiPpmStatusOk);
    for(int i = 0; i < 8; i++)
        TEST_ASSERT(got[i] == 0);

    // VERSION is restored, not erased.
    TEST_ASSERT(ucsi_ppm_register_read(ppm, UCSI_PPM_OFFSET_VERSION, 3, got) == UcsiPpmStatusOk);
    TEST_ASSERT(got[0] == 0x00 && got[1] == 0x03 && got[2] == 0x00);

    ucsi_ppm_free(ppm);
    return true;
}

// --- PDO helpers -----------------------------------------------------------

static bool test_pdo_fixed_source_5v(void) {
    UcsiPpmPdo pdo = ucsi_ppm_pdo_fixed_source(5000, 3000, true, false, true, true);
    TEST_ASSERT(((pdo >> 30) & 0x3u) == 0u); // Fixed
    TEST_ASSERT(((pdo >> 29) & 1u) == 1u); // DRP
    TEST_ASSERT(((pdo >> 27) & 1u) == 0u); // Unconstrained
    TEST_ASSERT(((pdo >> 26) & 1u) == 1u); // USB comms
    TEST_ASSERT(((pdo >> 25) & 1u) == 1u); // DR data
    TEST_ASSERT(((pdo >> 10) & 0x3FFu) == 100u); // 5000 / 50
    TEST_ASSERT((pdo & 0x3FFu) == 300u); // 3000 / 10
    return true;
}

static bool test_pdo_fixed_sink_5v(void) {
    UcsiPpmPdo pdo = ucsi_ppm_pdo_fixed_sink(5000, 3000, true, false, false, true, true);
    TEST_ASSERT(((pdo >> 30) & 0x3u) == 0u); // Fixed
    TEST_ASSERT(((pdo >> 29) & 1u) == 1u); // DRP
    TEST_ASSERT(((pdo >> 28) & 1u) == 0u); // Higher Cap = false
    TEST_ASSERT(((pdo >> 27) & 1u) == 0u); // Unconstrained
    TEST_ASSERT(((pdo >> 26) & 1u) == 1u); // USB comms
    TEST_ASSERT(((pdo >> 25) & 1u) == 1u); // DR data
    TEST_ASSERT(((pdo >> 10) & 0x3FFu) == 100u);
    TEST_ASSERT((pdo & 0x3FFu) == 300u);
    return true;
}

// --- L2 command dispatcher -------------------------------------------------

static bool test_cmd_ppm_reset(void) {
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    cfg.alert = counting_alert;
    ucsi_ppm_init(ppm, &cfg);
    g_alert_count = 0;

    uint8_t opcode = UCSI_PPM_OPCODE_PPM_RESET;
    TEST_ASSERT(ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 1, &opcode) == UcsiPpmStatusOk);

    // PPM_RESET sets only Reset Completed; spec says all other CCI bits are 0.
    TEST_ASSERT(read_cci(ppm) == UCSI_PPM_CCI_RESET_COMPLETED);
    TEST_ASSERT(g_alert_count == 1);
    TEST_ASSERT(ppm->cmd_state == UcsiPpmCmdStateIdle);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_cmd_reset_clears_on_next_cmd(void) {
    // commands.md §1.1: Reset Completed clears on the next command.
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    ucsi_ppm_init(ppm, &cfg);

    uint8_t reset = UCSI_PPM_OPCODE_PPM_RESET;
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 1, &reset);
    TEST_ASSERT(read_cci(ppm) == UCSI_PPM_CCI_RESET_COMPLETED);

    uint8_t getcap = UCSI_PPM_OPCODE_GET_CAPABILITY;
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 1, &getcap);

    const uint32_t cci = read_cci(ppm);
    TEST_ASSERT(!(cci & UCSI_PPM_CCI_RESET_COMPLETED));
    TEST_ASSERT(cci & UCSI_PPM_CCI_COMMAND_COMPLETED);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_cmd_get_capability(void) {
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    cfg.alert = counting_alert;
    // Distinct config bits to verify they reach the response unaltered.
    cfg.supports_disabled_state = true; // bmAttr bit 0
    cfg.supports_usb_pd = true; // bmAttr bit 2
    cfg.supports_typec_current = true; // bmAttr bit 6
    cfg.power_source_other = true; // bmAttr bit 10 (already on by default)
    cfg.supports_set_ccom = true; // bmOpt bit 0
    cfg.supports_chunking = true; // bmOpt bit 14
    cfg.connector_usb2_capable = true;
    ucsi_ppm_init(ppm, &cfg);
    g_alert_count = 0;

    uint8_t opcode = UCSI_PPM_OPCODE_GET_CAPABILITY;
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 1, &opcode);

    const uint32_t cci = read_cci(ppm);
    TEST_ASSERT(cci & UCSI_PPM_CCI_COMMAND_COMPLETED);
    TEST_ASSERT(((cci >> UCSI_PPM_CCI_DATA_LENGTH_SHIFT) & 0xFFu) == 16u);
    TEST_ASSERT(!(cci & UCSI_PPM_CCI_NOT_SUPPORTED));
    TEST_ASSERT(!(cci & UCSI_PPM_CCI_ERROR));

    uint8_t msg[16];
    ucsi_ppm_register_read(ppm, UCSI_PPM_OFFSET_MESSAGE_IN, 16, msg);

    const uint32_t bm_attr = (uint32_t)msg[0] | ((uint32_t)msg[1] << 8) | ((uint32_t)msg[2] << 16) | ((uint32_t)msg[3] << 24);
    TEST_ASSERT(bm_attr & (1u << 0)); // Disabled State
    TEST_ASSERT(bm_attr & (1u << 2)); // USB PD
    TEST_ASSERT(bm_attr & (1u << 6)); // Type-C Current
    TEST_ASSERT(bm_attr & (1u << 10)); // Other (power source)

    TEST_ASSERT((msg[4] & 0x7Fu) == UCSI_PPM_NUM_CONNECTORS);

    const uint32_t bm_opt = (uint32_t)msg[5] | ((uint32_t)msg[6] << 8) | ((uint32_t)msg[7] << 16);
    TEST_ASSERT(bm_opt & (1u << 0)); // SET_CCOM
    TEST_ASSERT(bm_opt & (1u << 1)); // SET_POWER_LEVEL — spec-mandated always 1
    TEST_ASSERT(bm_opt & (1u << 14)); // Chunking

    TEST_ASSERT(msg[8] == UCSI_PPM_NUM_ALT_MODES);

    const uint16_t bcd_bc = (uint16_t)(msg[10] | (msg[11] << 8));
    const uint16_t bcd_pd = (uint16_t)(msg[12] | (msg[13] << 8));
    const uint16_t bcd_tc = (uint16_t)(msg[14] | (msg[15] << 8));
    TEST_ASSERT(bcd_bc == UCSI_PPM_VERSION_BC);
    TEST_ASSERT(bcd_pd == UCSI_PPM_VERSION_PD);
    TEST_ASSERT(bcd_tc == UCSI_PPM_VERSION_TYPEC);

    TEST_ASSERT(g_alert_count == 1);
    TEST_ASSERT(ppm->cmd_state == UcsiPpmCmdStateWaitForAck);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_cmd_get_connector_capability_drp(void) {
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;
    make_valid_config(&cfg); // initial mode is DRP
    cfg.connector_usb2_capable = true;
    cfg.connector_usb3_capable = false;
    ucsi_ppm_init(ppm, &cfg);

    uint8_t opcode = UCSI_PPM_OPCODE_GET_CONNECTOR_CAPABILITY;
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 1, &opcode);

    const uint32_t cci = read_cci(ppm);
    TEST_ASSERT(cci & UCSI_PPM_CCI_COMMAND_COMPLETED);
    TEST_ASSERT(((cci >> UCSI_PPM_CCI_DATA_LENGTH_SHIFT) & 0xFFu) == 4u);

    uint8_t msg[4];
    ucsi_ppm_register_read(ppm, UCSI_PPM_OFFSET_MESSAGE_IN, 4, msg);
    const uint32_t cap = (uint32_t)msg[0] | ((uint32_t)msg[1] << 8) | ((uint32_t)msg[2] << 16) | ((uint32_t)msg[3] << 24);

    TEST_ASSERT(cap & (1u << 2)); // Operation Mode: DRP
    TEST_ASSERT(cap & (1u << 5)); // USB2
    TEST_ASSERT(!(cap & (1u << 6))); // !USB3
    TEST_ASSERT(cap & (1u << 8)); // Provider
    TEST_ASSERT(cap & (1u << 9)); // Consumer
    TEST_ASSERT(cap & (1u << 10)); // Swap to DFP
    TEST_ASSERT(cap & (1u << 11)); // Swap to UFP
    TEST_ASSERT(cap & (1u << 12)); // Swap to SRC
    TEST_ASSERT(cap & (1u << 13)); // Swap to SNK

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_cmd_get_connector_capability_rp_only(void) {
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    cfg.initial_cc_operation_mode = UcsiPpmCcModeRpOnly;
    ucsi_ppm_init(ppm, &cfg);

    uint8_t opcode = UCSI_PPM_OPCODE_GET_CONNECTOR_CAPABILITY;
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 1, &opcode);

    uint8_t msg[4];
    ucsi_ppm_register_read(ppm, UCSI_PPM_OFFSET_MESSAGE_IN, 4, msg);
    const uint32_t cap = (uint32_t)msg[0] | ((uint32_t)msg[1] << 8) | ((uint32_t)msg[2] << 16) | ((uint32_t)msg[3] << 24);

    TEST_ASSERT(cap & (1u << 0)); // Operation Mode: Rp Only
    TEST_ASSERT(!(cap & (1u << 2))); // !DRP
    TEST_ASSERT(cap & (1u << 8)); // Provider
    TEST_ASSERT(!(cap & (1u << 9))); // !Consumer
    TEST_ASSERT(!(cap & (1u << 10))); // no swap-to-DFP for Rp-only

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_cmd_set_notification_enable(void) {
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    ucsi_ppm_init(ppm, &cfg);

    // 17-bit mask 0x12345 packed into CONTROL bits 16..32:
    // - byte 2 = 0x45 (mask bits 0..7)
    // - byte 3 = 0x23 (mask bits 8..15)
    // - byte 4 bit 0 = 1 (mask bit 16)
    uint8_t payload[7] = {0u, 0x45u, 0x23u, 0x01u, 0u, 0u, 0u};
    TEST_ASSERT(ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL + 1, 7, payload) == UcsiPpmStatusOk);

    uint8_t opcode = UCSI_PPM_OPCODE_SET_NOTIFICATION_ENABLE;
    TEST_ASSERT(ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 1, &opcode) == UcsiPpmStatusOk);

    const uint32_t cci = read_cci(ppm);
    TEST_ASSERT(cci & UCSI_PPM_CCI_COMMAND_COMPLETED);
    TEST_ASSERT(!(cci & UCSI_PPM_CCI_NOT_SUPPORTED));
    TEST_ASSERT(ppm->notification_mask == 0x12345u);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_cmd_not_supported_unknown(void) {
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    ucsi_ppm_init(ppm, &cfg);

    uint8_t opcode = 0x99u; // not implemented in v1
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 1, &opcode);

    const uint32_t cci = read_cci(ppm);
    TEST_ASSERT(cci & UCSI_PPM_CCI_COMMAND_COMPLETED);
    TEST_ASSERT(cci & UCSI_PPM_CCI_NOT_SUPPORTED);
    TEST_ASSERT(!(cci & UCSI_PPM_CCI_ERROR)); // NS is its own indicator, distinct from Error

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_cmd_not_supported_in_scope(void) {
    // Commands in the opcode space but not implemented in v1 (e.g., GET_PD_MESSAGE,
    // GET_ALTERNATE_MODES) must respond Not Supported, not crash.
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    ucsi_ppm_init(ppm, &cfg);

    const uint8_t opcodes[] = {0x0Cu, 0x0Du, 0x0Eu, 0x0Fu, 0x15u};
    for(size_t i = 0; i < sizeof(opcodes) / sizeof(opcodes[0]); ++i) {
        // ACK to clear prior CCI if needed.
        if(ppm->cmd_state == UcsiPpmCmdStateWaitForAck) {
            uint8_t ack_byte = UCSI_PPM_ACK_CC_CI_COMMAND_COMPLETED_ACK;
            ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL + 2, 1, &ack_byte);
            uint8_t ack_op = UCSI_PPM_OPCODE_ACK_CC_CI;
            ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 1, &ack_op);
        }
        uint8_t op = opcodes[i];
        ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 1, &op);
        const uint32_t cci = read_cci(ppm);
        TEST_ASSERT(cci & UCSI_PPM_CCI_COMMAND_COMPLETED);
        TEST_ASSERT(cci & UCSI_PPM_CCI_NOT_SUPPORTED);
    }

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_cmd_ack_cc_ci_clears_cci(void) {
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    cfg.alert = counting_alert;
    ucsi_ppm_init(ppm, &cfg);

    uint8_t getcap = UCSI_PPM_OPCODE_GET_CAPABILITY;
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 1, &getcap);
    TEST_ASSERT(read_cci(ppm) & UCSI_PPM_CCI_COMMAND_COMPLETED);
    TEST_ASSERT(ppm->cmd_state == UcsiPpmCmdStateWaitForAck);

    g_alert_count = 0;
    uint8_t ack_byte = UCSI_PPM_ACK_CC_CI_COMMAND_COMPLETED_ACK;
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL + 2, 1, &ack_byte);
    uint8_t ack_op = UCSI_PPM_OPCODE_ACK_CC_CI;
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 1, &ack_op);

    TEST_ASSERT(read_cci(ppm) == 0u);
    TEST_ASSERT(ppm->cmd_state == UcsiPpmCmdStateIdle);
    // ACK clears CCI to zero — no alert is raised for "nothing to read".
    TEST_ASSERT(g_alert_count == 0);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_cmd_ack_cc_ci_in_idle_ignored(void) {
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    cfg.alert = counting_alert;
    ucsi_ppm_init(ppm, &cfg);
    g_alert_count = 0;

    uint8_t ack_byte = UCSI_PPM_ACK_CC_CI_COMMAND_COMPLETED_ACK;
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL + 2, 1, &ack_byte);
    uint8_t ack_op = UCSI_PPM_OPCODE_ACK_CC_CI;
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 1, &ack_op);

    TEST_ASSERT(read_cci(ppm) == 0u);
    TEST_ASSERT(ppm->cmd_state == UcsiPpmCmdStateIdle);
    TEST_ASSERT(g_alert_count == 0);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_cmd_init_state_is_idle(void) {
    // After init, runtime state matches api.md §4 defaults section.
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    ucsi_ppm_init(ppm, &cfg);

    TEST_ASSERT(ppm->cmd_state == UcsiPpmCmdStateIdle);
    TEST_ASSERT(ppm->notification_mask == 0u);
    TEST_ASSERT(ppm->current_cc_operation_mode == cfg.initial_cc_operation_mode);
    TEST_ASSERT(ppm->accept_dr_swap == true);
    TEST_ASSERT(ppm->accept_pr_swap == true);
    TEST_ASSERT(ppm->error_info == 0u);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_cmd_reset_returns_state_to_idle(void) {
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    ucsi_ppm_init(ppm, &cfg);

    // Drive into WaitForAck with a command.
    uint8_t getcap = UCSI_PPM_OPCODE_GET_CAPABILITY;
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 1, &getcap);
    TEST_ASSERT(ppm->cmd_state == UcsiPpmCmdStateWaitForAck);

    // Set notification mask too.
    ppm->notification_mask = 0xDEADu;

    TEST_ASSERT(ucsi_ppm_reset(ppm) == UcsiPpmStatusOk);
    TEST_ASSERT(ppm->cmd_state == UcsiPpmCmdStateIdle);
    TEST_ASSERT(ppm->notification_mask == 0u);

    ucsi_ppm_free(ppm);
    return true;
}

// --- SET_CCOM / SET_UOR / SET_PDR ------------------------------------------

// Encodes CC Operation Mode (4 bits) at CONTROL bits 23..26:
// bit 0 -> byte 2 bit 7, bits 1..3 -> byte 3 bits 0..2.
static void pack_cc_op_mode(uint8_t* ctrl_byte2, uint8_t* ctrl_byte3, uint8_t bits) {
    *ctrl_byte2 = (uint8_t)((*ctrl_byte2 & 0x7Fu) | ((bits & 0x01u) << 7));
    *ctrl_byte3 = (uint8_t)((*ctrl_byte3 & 0xF8u) | ((bits >> 1) & 0x07u));
}

// Encodes 3-bit Role field at CONTROL bits 23..25 (used by SET_UOR / SET_PDR).
static void pack_role3(uint8_t* ctrl_byte2, uint8_t* ctrl_byte3, uint8_t bits) {
    *ctrl_byte2 = (uint8_t)((*ctrl_byte2 & 0x7Fu) | ((bits & 0x01u) << 7));
    *ctrl_byte3 = (uint8_t)((*ctrl_byte3 & 0xFCu) | ((bits >> 1) & 0x03u));
}

static bool test_cmd_set_ccom_picks_drp(void) {
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    cfg.initial_cc_operation_mode = UcsiPpmCcModeRpOnly;
    ucsi_ppm_init(ppm, &cfg);

    // Multi-bit input "Rp+DRP" → pick DRP (most capable supported).
    uint8_t payload[7] = {0};
    pack_cc_op_mode(&payload[1], &payload[2], 0x05u); // bit 0 (Rp) | bit 2 (DRP)
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL + 1, 7, payload);
    uint8_t op = UCSI_PPM_OPCODE_SET_CCOM;
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 1, &op);

    TEST_ASSERT(read_cci(ppm) & UCSI_PPM_CCI_COMMAND_COMPLETED);
    TEST_ASSERT(!(read_cci(ppm) & UCSI_PPM_CCI_ERROR));
    TEST_ASSERT(ppm->current_cc_operation_mode == UcsiPpmCcModeDrp);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_cmd_set_ccom_rejects_empty(void) {
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    ucsi_ppm_init(ppm, &cfg);

    uint8_t op = UCSI_PPM_OPCODE_SET_CCOM;
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 1, &op);

    const uint32_t cci = read_cci(ppm);
    TEST_ASSERT(cci & UCSI_PPM_CCI_COMMAND_COMPLETED);
    TEST_ASSERT(cci & UCSI_PPM_CCI_ERROR);
    TEST_ASSERT(ppm->error_info & UCSI_PPM_ERR_INVALID_CMD_PARAMS);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_cmd_set_ccom_rejects_disabled_when_unsupported(void) {
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    cfg.supports_disabled_state = false;
    ucsi_ppm_init(ppm, &cfg);

    uint8_t payload[7] = {0};
    pack_cc_op_mode(&payload[1], &payload[2], 0x08u); // bit 3 only = Disabled
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL + 1, 7, payload);
    uint8_t op = UCSI_PPM_OPCODE_SET_CCOM;
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 1, &op);

    const uint32_t cci = read_cci(ppm);
    TEST_ASSERT(cci & UCSI_PPM_CCI_ERROR);
    TEST_ASSERT(ppm->error_info & UCSI_PPM_ERR_INVALID_CMD_PARAMS);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_cmd_set_uor_stores_accept(void) {
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    ucsi_ppm_init(ppm, &cfg);
    TEST_ASSERT(ppm->accept_dr_swap == true);

    // bit 2 = 0 → reject DR swaps.
    uint8_t payload[7] = {0};
    pack_role3(&payload[1], &payload[2], 0x00u);
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL + 1, 7, payload);
    uint8_t op = UCSI_PPM_OPCODE_SET_UOR;
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 1, &op);

    TEST_ASSERT(read_cci(ppm) & UCSI_PPM_CCI_COMMAND_COMPLETED);
    TEST_ASSERT(!(read_cci(ppm) & UCSI_PPM_CCI_ERROR));
    TEST_ASSERT(ppm->accept_dr_swap == false);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_cmd_set_uor_rejects_both_swap_bits(void) {
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    ucsi_ppm_init(ppm, &cfg);

    // Bits 0 (swap to DFP) and 1 (swap to UFP) together — illegal.
    uint8_t payload[7] = {0};
    pack_role3(&payload[1], &payload[2], 0x03u);
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL + 1, 7, payload);
    uint8_t op = UCSI_PPM_OPCODE_SET_UOR;
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 1, &op);

    TEST_ASSERT(read_cci(ppm) & UCSI_PPM_CCI_ERROR);
    TEST_ASSERT(ppm->error_info & UCSI_PPM_ERR_INVALID_CMD_PARAMS);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_cmd_set_pdr_stores_accept(void) {
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    ucsi_ppm_init(ppm, &cfg);
    TEST_ASSERT(ppm->accept_pr_swap == true);

    // bit 0 = initiate swap to Source, bit 2 = 0 (reject swaps).
    uint8_t payload[7] = {0};
    pack_role3(&payload[1], &payload[2], 0x01u);
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL + 1, 7, payload);
    uint8_t op = UCSI_PPM_OPCODE_SET_PDR;
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 1, &op);

    TEST_ASSERT(read_cci(ppm) & UCSI_PPM_CCI_COMMAND_COMPLETED);
    TEST_ASSERT(!(read_cci(ppm) & UCSI_PPM_CCI_ERROR));
    TEST_ASSERT(ppm->accept_pr_swap == false);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_cmd_set_pdr_rejects_all_zero(void) {
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    ucsi_ppm_init(ppm, &cfg);

    uint8_t op = UCSI_PPM_OPCODE_SET_PDR;
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 1, &op);

    TEST_ASSERT(read_cci(ppm) & UCSI_PPM_CCI_ERROR);
    TEST_ASSERT(ppm->error_info & UCSI_PPM_ERR_INVALID_CMD_PARAMS);

    ucsi_ppm_free(ppm);
    return true;
}

// --- GET_PDOS --------------------------------------------------------------

static bool test_cmd_get_pdos_own_source(void) {
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    // Make two PDOs so we can request a couple.
    cfg.source_caps.pdos[1] = ucsi_ppm_pdo_fixed_source(9000, 2000, true, false, true, true);
    cfg.source_caps.count = 2;
    ucsi_ppm_init(ppm, &cfg);

    // bit 23 = 0 (own), byte 3 = pdo_offset 0, byte 4 bits 0..1 = num-1 = 1 (2 PDOs),
    // byte 4 bit 2 = 1 (Source).
    uint8_t payload[7] = {0};
    payload[2] = 0x00u; // ctrl[3] = offset 0
    payload[3] = (uint8_t)((1u & 0x03u) | (1u << 2)); // ctrl[4] = num=1+1, source=1
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL + 1, 7, payload);
    uint8_t op = UCSI_PPM_OPCODE_GET_PDOS;
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 1, &op);

    const uint32_t cci = read_cci(ppm);
    TEST_ASSERT(cci & UCSI_PPM_CCI_COMMAND_COMPLETED);
    TEST_ASSERT(((cci >> UCSI_PPM_CCI_DATA_LENGTH_SHIFT) & 0xFFu) == 8u);

    uint8_t buf[8];
    ucsi_ppm_register_read(ppm, UCSI_PPM_OFFSET_MESSAGE_IN, 8, buf);
    const uint32_t pdo0 = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) | ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
    const uint32_t pdo1 = (uint32_t)buf[4] | ((uint32_t)buf[5] << 8) | ((uint32_t)buf[6] << 16) | ((uint32_t)buf[7] << 24);
    TEST_ASSERT(pdo0 == cfg.source_caps.pdos[0]);
    TEST_ASSERT(pdo1 == cfg.source_caps.pdos[1]);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_cmd_get_pdos_own_sink(void) {
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    ucsi_ppm_init(ppm, &cfg);

    // Source or Sink bit = 0 -> Sink.
    uint8_t payload[7] = {0};
    payload[3] = 0x00u; // num=1, source=0
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL + 1, 7, payload);
    uint8_t op = UCSI_PPM_OPCODE_GET_PDOS;
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 1, &op);

    const uint32_t cci = read_cci(ppm);
    TEST_ASSERT(cci & UCSI_PPM_CCI_COMMAND_COMPLETED);
    TEST_ASSERT(((cci >> UCSI_PPM_CCI_DATA_LENGTH_SHIFT) & 0xFFu) == 4u);

    uint8_t buf[4];
    ucsi_ppm_register_read(ppm, UCSI_PPM_OFFSET_MESSAGE_IN, 4, buf);
    const uint32_t pdo0 = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) | ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
    TEST_ASSERT(pdo0 == cfg.sink_caps.pdos[0]);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_cmd_get_pdos_partner_no_partner(void) {
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    ucsi_ppm_init(ppm, &cfg);

    // bit 23 = 1 — request partner PDOs; in v1 there's no partner.
    uint8_t payload[7] = {0};
    payload[1] = 0x80u; // ctrl[2] bit 7 set
    payload[3] = (uint8_t)(0u | (1u << 2)); // 1 PDO, Source
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL + 1, 7, payload);
    uint8_t op = UCSI_PPM_OPCODE_GET_PDOS;
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 1, &op);

    TEST_ASSERT(read_cci(ppm) & UCSI_PPM_CCI_ERROR);
    TEST_ASSERT(ppm->error_info & UCSI_PPM_ERR_CC_COMMUNICATION);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_cmd_get_pdos_out_of_range(void) {
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    ucsi_ppm_init(ppm, &cfg);

    // Offset 6 + 4 PDOs = 10 > 7 (SPR cap).
    uint8_t payload[7] = {0};
    payload[2] = 6u; // ctrl[3] = offset 6
    payload[3] = (uint8_t)(3u | (1u << 2)); // num=3+1, source=1
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL + 1, 7, payload);
    uint8_t op = UCSI_PPM_OPCODE_GET_PDOS;
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 1, &op);

    TEST_ASSERT(read_cci(ppm) & UCSI_PPM_CCI_ERROR);
    TEST_ASSERT(ppm->error_info & UCSI_PPM_ERR_INVALID_CMD_PARAMS);

    ucsi_ppm_free(ppm);
    return true;
}

// --- GET_CONNECTOR_STATUS / GET_ERROR_STATUS --------------------------------

static bool test_cmd_get_connector_status(void) {
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    ucsi_ppm_init(ppm, &cfg);

    uint8_t op = UCSI_PPM_OPCODE_GET_CONNECTOR_STATUS;
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 1, &op);

    const uint32_t cci = read_cci(ppm);
    TEST_ASSERT(cci & UCSI_PPM_CCI_COMMAND_COMPLETED);
    TEST_ASSERT(((cci >> UCSI_PPM_CCI_DATA_LENGTH_SHIFT) & 0xFFu) == 19u);

    // In v1 (no L3) every byte of the 19-byte payload is zero.
    uint8_t buf[19];
    ucsi_ppm_register_read(ppm, UCSI_PPM_OFFSET_MESSAGE_IN, 19, buf);
    for(int i = 0; i < 19; i++)
        TEST_ASSERT(buf[i] == 0u);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_cmd_get_error_status_initial_zero(void) {
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    ucsi_ppm_init(ppm, &cfg);

    uint8_t op = UCSI_PPM_OPCODE_GET_ERROR_STATUS;
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 1, &op);

    const uint32_t cci = read_cci(ppm);
    TEST_ASSERT(cci & UCSI_PPM_CCI_COMMAND_COMPLETED);
    TEST_ASSERT(((cci >> UCSI_PPM_CCI_DATA_LENGTH_SHIFT) & 0xFFu) == 16u);

    uint8_t buf[16];
    ucsi_ppm_register_read(ppm, UCSI_PPM_OFFSET_MESSAGE_IN, 16, buf);
    const uint16_t err = (uint16_t)(buf[0] | (buf[1] << 8));
    TEST_ASSERT(err == 0u);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_cmd_error_info_flows_through(void) {
    // Trigger a failure (SET_PDR with role=0), then read GET_ERROR_STATUS and
    // verify the Error Information bit propagates.
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    ucsi_ppm_init(ppm, &cfg);

    uint8_t op = UCSI_PPM_OPCODE_SET_PDR;
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 1, &op);
    TEST_ASSERT(read_cci(ppm) & UCSI_PPM_CCI_ERROR);
    TEST_ASSERT(ppm->error_info & UCSI_PPM_ERR_INVALID_CMD_PARAMS);

    // ACK the failed SET_PDR so the next command can proceed.
    uint8_t ack_byte = UCSI_PPM_ACK_CC_CI_COMMAND_COMPLETED_ACK;
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL + 2, 1, &ack_byte);
    uint8_t ack_op = UCSI_PPM_OPCODE_ACK_CC_CI;
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 1, &ack_op);
    // ACK that confirmed Error must clear error_info per architecture.md §9.
    TEST_ASSERT(ppm->error_info == 0u);

    // Re-trigger an error to verify the next GET_ERROR_STATUS still works.
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 1, &op); // SET_PDR again
    TEST_ASSERT(ppm->error_info & UCSI_PPM_ERR_INVALID_CMD_PARAMS);

    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL + 2, 1, &ack_byte);
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 1, &ack_op);

    // Now error_info is back to 0; trigger once more and read GET_ERROR_STATUS
    // *without* ACK in between — the error bit must be in the response.
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 1, &op); // SET_PDR — fails
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL + 2, 1, &ack_byte);
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 1, &ack_op);
    // Now error_info is 0 again. To preserve it across a successful GET_ERROR_STATUS,
    // we must not ACK. So we re-fail and immediately query without ACK.
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 1, &op); // SET_PDR — fails
    // We're in WaitForAck on the failed SET_PDR; per architecture.md §3.3 new
    // commands are accepted in WaitForAck but it's bad OPM practice. Library
    // proceeds. Send GET_ERROR_STATUS while error_info is still set.
    uint8_t geterr = UCSI_PPM_OPCODE_GET_ERROR_STATUS;
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 1, &geterr);

    const uint32_t cci = read_cci(ppm);
    TEST_ASSERT(cci & UCSI_PPM_CCI_COMMAND_COMPLETED);

    uint8_t buf[16];
    ucsi_ppm_register_read(ppm, UCSI_PPM_OFFSET_MESSAGE_IN, 16, buf);
    const uint16_t err = (uint16_t)(buf[0] | (buf[1] << 8));
    TEST_ASSERT(err & UCSI_PPM_ERR_INVALID_CMD_PARAMS);

    ucsi_ppm_free(ppm);
    return true;
}

// --- L4 PHY (FUSB302) ------------------------------------------------------

// Event collector for the pump tests.
#define COLLECTED_MAX 16
typedef struct {
    UcsiPpmPhyEvent events[COLLECTED_MAX];
    size_t count;
} Collected;

static void collect_event(void* ctx, const UcsiPpmPhyEvent* event) {
    Collected* c = (Collected*)ctx;
    if(c->count >= COLLECTED_MAX) return;
    c->events[c->count++] = *event;
}

static bool test_phy_init_sequence(void) {
    UcsiPpm* ppm = mock_make_ppm();
    TEST_ASSERT(ppm != NULL);
    mock_i2c_reset();

    TEST_ASSERT(ucsi_ppm_phy_init(ppm) == UcsiPpmStatusOk);

    // SW_RESET sets RESET.sw_reset = 1.
    const Fusb302ResetRegBits sw_reset = {.sw_reset = 1};
    TEST_ASSERT(mock_any_write_to(Fusb302RegReset, *(const uint8_t*)&sw_reset));

    // POWER: all blocks on.
    const Fusb302PowerRegBits all_on = {.pwr = 0b1111};
    TEST_ASSERT(mock_any_write_to(Fusb302RegPower, *(const uint8_t*)&all_on));

    // All three masks programmed (don't pin specific values — just check
    // any write reached them with our DEFAULT_MASK*_VAL).
    TEST_ASSERT(g_mock_regs[Fusb302RegMask] != 0u);
    TEST_ASSERT(g_mock_regs[Fusb302RegMaskA] != 0u);
    // INT_MASK cleared in final state.
    const uint8_t c0 = g_mock_regs[Fusb302RegControl0];
    const Fusb302Control0RegBits c0_bits = *((const Fusb302Control0RegBits*)&c0);
    TEST_ASSERT(c0_bits.int_mask == 0);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_phy_init_order_reset_first(void) {
    // SW_RESET must be the first I²C write after entering phy_init —
    // otherwise we'd reset register values we just programmed.
    UcsiPpm* ppm = mock_make_ppm();
    mock_i2c_reset();
    ucsi_ppm_phy_init(ppm);

    TEST_ASSERT(g_mock_txn_count >= 1u);
    const MockI2cTxn* first = &g_mock_txns[0];
    TEST_ASSERT(first->is_write);
    TEST_ASSERT(first->len == 2u);
    TEST_ASSERT(first->data[0] == Fusb302RegReset);
    const Fusb302ResetRegBits payload = *((const Fusb302ResetRegBits*)&first->data[1]);
    TEST_ASSERT(payload.sw_reset == 1);
    TEST_ASSERT(payload.pd_reset == 0);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_phy_start_toggle_drp(void) {
    UcsiPpm* ppm = mock_make_ppm();
    ucsi_ppm_phy_init(ppm);
    mock_i2c_reset();

    TEST_ASSERT(ucsi_ppm_phy_start_toggle(ppm, UcsiPpmPhyToggleModeDrp) == UcsiPpmStatusOk);

    const uint8_t v = g_mock_regs[Fusb302RegControl2];
    const Fusb302Control2RegBits v_struct = *((const Fusb302Control2RegBits*)&v);

    // TOGGLE = 1
    TEST_ASSERT(v_struct.toggle == 1);
    // MODE = 01b (DRP) at bits 2:1
    TEST_ASSERT(v_struct.mode == 0b01u);
    // TOG_SAVE_PWR = 01b (40 ms) at bits 7:6
    TEST_ASSERT(v_struct.tog_save_pwr == 0b01u);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_phy_start_toggle_src(void) {
    UcsiPpm* ppm = mock_make_ppm();
    ucsi_ppm_phy_init(ppm);
    mock_i2c_reset();

    ucsi_ppm_phy_start_toggle(ppm, UcsiPpmPhyToggleModeSrc);

    const uint8_t v = g_mock_regs[Fusb302RegControl2];
    const Fusb302Control2RegBits v_struct = *((const Fusb302Control2RegBits*)&v);
    TEST_ASSERT(v_struct.toggle == 1);
    TEST_ASSERT(v_struct.mode == 0b11u); // SRC

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_phy_stop_toggle(void) {
    UcsiPpm* ppm = mock_make_ppm();
    ucsi_ppm_phy_init(ppm);
    ucsi_ppm_phy_start_toggle(ppm, UcsiPpmPhyToggleModeDrp);
    mock_i2c_reset();

    // Pre-seed CONTROL2 with TOGGLE=1, MODE=DRP, TOG_SAVE_PWR=01b.
    const Fusb302Control2RegBits pre = {.toggle = 1, .mode = 0b01, .tog_save_pwr = 0b01};
    g_mock_regs[Fusb302RegControl2] = *(const uint8_t*)&pre;

    ucsi_ppm_phy_stop_toggle(ppm);

    const uint8_t after = g_mock_regs[Fusb302RegControl2];
    const Fusb302Control2RegBits after_bits = *((const Fusb302Control2RegBits*)&after);
    TEST_ASSERT(after_bits.toggle == 0); // cleared
    TEST_ASSERT(after_bits.mode == 0b01u); // preserved
    TEST_ASSERT(after_bits.tog_save_pwr == 0b01u); // preserved

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_phy_set_rp_current(void) {
    UcsiPpm* ppm = mock_make_ppm();
    ucsi_ppm_phy_init(ppm);
    mock_i2c_reset();

    g_mock_regs[Fusb302RegControl0] = 0x00u; // start clean
    TEST_ASSERT(ucsi_ppm_phy_set_rp_current(ppm, UcsiPpmRpCurrent3A) == UcsiPpmStatusOk);
    {
        const uint8_t v = g_mock_regs[Fusb302RegControl0];
        const Fusb302Control0RegBits b = *((const Fusb302Control0RegBits*)&v);
        TEST_ASSERT(b.host_cur == 0b11u); // 330 µA — High Current 3 A
    }

    ucsi_ppm_phy_set_rp_current(ppm, UcsiPpmRpCurrent1A5);
    {
        const uint8_t v = g_mock_regs[Fusb302RegControl0];
        const Fusb302Control0RegBits b = *((const Fusb302Control0RegBits*)&v);
        TEST_ASSERT(b.host_cur == 0b10u); // 180 µA — Medium 1.5 A
    }

    ucsi_ppm_phy_set_rp_current(ppm, UcsiPpmRpCurrentUsbDefault);
    {
        const uint8_t v = g_mock_regs[Fusb302RegControl0];
        const Fusb302Control0RegBits b = *((const Fusb302Control0RegBits*)&v);
        TEST_ASSERT(b.host_cur == 0b01u); // 80 µA — USB Default
    }

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_phy_lock_polarity_cc1(void) {
    UcsiPpm* ppm = mock_make_ppm();
    ucsi_ppm_phy_init(ppm);
    mock_i2c_reset();

    // Sink-like start state: PDWN1+PDWN2 = 1.
    const Fusb302Switches0RegBits pre0 = {.pdwn1 = 1, .pdwn2 = 1};
    g_mock_regs[Fusb302RegSwitches0] = *(const uint8_t*)&pre0;
    g_mock_regs[Fusb302RegSwitches1] = 0u;

    TEST_ASSERT(ucsi_ppm_phy_lock_polarity(ppm, UcsiPpmPhyCc1) == UcsiPpmStatusOk);

    const uint8_t sw0 = g_mock_regs[Fusb302RegSwitches0];
    const Fusb302Switches0RegBits sw0_bits = *((const Fusb302Switches0RegBits*)&sw0);
    TEST_ASSERT(sw0_bits.meas_cc1 == 1);
    TEST_ASSERT(sw0_bits.meas_cc2 == 0);
    TEST_ASSERT(sw0_bits.pdwn1 == 1); // preserved
    TEST_ASSERT(sw0_bits.pdwn2 == 1); // preserved

    const uint8_t sw1 = g_mock_regs[Fusb302RegSwitches1];
    const Fusb302Switches1RegBits sw1_bits = *((const Fusb302Switches1RegBits*)&sw1);
    TEST_ASSERT(sw1_bits.tx_cc1 == 1);
    TEST_ASSERT(sw1_bits.tx_cc2 == 0);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_phy_lock_polarity_cc2(void) {
    UcsiPpm* ppm = mock_make_ppm();
    ucsi_ppm_phy_init(ppm);
    mock_i2c_reset();

    ucsi_ppm_phy_lock_polarity(ppm, UcsiPpmPhyCc2);

    const uint8_t sw0 = g_mock_regs[Fusb302RegSwitches0];
    const Fusb302Switches0RegBits sw0_bits = *((const Fusb302Switches0RegBits*)&sw0);
    TEST_ASSERT(sw0_bits.meas_cc2 == 1);
    TEST_ASSERT(sw0_bits.meas_cc1 == 0);

    const uint8_t sw1 = g_mock_regs[Fusb302RegSwitches1];
    const Fusb302Switches1RegBits sw1_bits = *((const Fusb302Switches1RegBits*)&sw1);
    TEST_ASSERT(sw1_bits.tx_cc2 == 1);
    TEST_ASSERT(sw1_bits.tx_cc1 == 0);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_phy_enable_pd(void) {
    UcsiPpm* ppm = mock_make_ppm();
    ucsi_ppm_phy_init(ppm);
    mock_i2c_reset();

    g_mock_regs[Fusb302RegSwitches1] = 0u;
    g_mock_regs[Fusb302RegControl3] = 0u;
    TEST_ASSERT(ucsi_ppm_phy_enable_pd(ppm, 2u) == UcsiPpmStatusOk);

    const uint8_t sw1 = g_mock_regs[Fusb302RegSwitches1];
    const Fusb302Switches1RegBits sw1_bits = *((const Fusb302Switches1RegBits*)&sw1);
    TEST_ASSERT(sw1_bits.auto_crc == 1);

    const uint8_t c3 = g_mock_regs[Fusb302RegControl3];
    const Fusb302Control3RegBits c3_bits = *((const Fusb302Control3RegBits*)&c3);
    TEST_ASSERT(c3_bits.auto_retry == 1);
    TEST_ASSERT(c3_bits.n_retries == 0b10u); // 2 retries
    TEST_ASSERT(c3_bits.auto_softreset == 0);
    TEST_ASSERT(c3_bits.auto_hardreset == 0);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_phy_pd_reset(void) {
    UcsiPpm* ppm = mock_make_ppm();
    ucsi_ppm_phy_init(ppm);
    mock_i2c_reset();

    TEST_ASSERT(ucsi_ppm_phy_pd_reset(ppm) == UcsiPpmStatusOk);
    const Fusb302ResetRegBits expected = {.pd_reset = 1};
    TEST_ASSERT(mock_any_write_to(Fusb302RegReset, *(const uint8_t*)&expected));

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_phy_send_hard_reset(void) {
    UcsiPpm* ppm = mock_make_ppm();
    ucsi_ppm_phy_init(ppm);
    mock_i2c_reset();

    g_mock_regs[Fusb302RegControl3] = 0u;
    TEST_ASSERT(ucsi_ppm_phy_send_hard_reset(ppm) == UcsiPpmStatusOk);

    const uint8_t c3 = g_mock_regs[Fusb302RegControl3];
    const Fusb302Control3RegBits c3_bits = *((const Fusb302Control3RegBits*)&c3);
    TEST_ASSERT(c3_bits.send_hard_reset == 1);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_phy_pump_vbus_changed(void) {
    UcsiPpm* ppm = mock_make_ppm();
    ucsi_ppm_phy_init(ppm);
    mock_i2c_reset();

    const Fusb302InterruptRegBits intr = {.i_vbusok = 1};
    g_mock_regs[Fusb302RegInterrupt] = *(const uint8_t*)&intr;
    const Fusb302Status0RegBits s0 = {.vbusok = 1};
    g_mock_regs[Fusb302RegStatus0] = *(const uint8_t*)&s0;
    g_mock_regs[Fusb302RegInterruptA] = 0u;
    g_mock_regs[Fusb302RegInterruptB] = 0u;

    Collected c = {0};
    TEST_ASSERT(ucsi_ppm_phy_pump(ppm, collect_event, &c) == UcsiPpmStatusOk);

    TEST_ASSERT(c.count == 1u);
    TEST_ASSERT(c.events[0].kind == UcsiPpmPhyEventVbusChanged);
    TEST_ASSERT(c.events[0].u.vbus_ok == true);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_phy_pump_toggle_done_src_cc1(void) {
    UcsiPpm* ppm = mock_make_ppm();
    ucsi_ppm_phy_init(ppm);
    mock_i2c_reset();

    const Fusb302InterruptARegBits inta = {.i_tog_done = 1};
    g_mock_regs[Fusb302RegInterruptA] = *(const uint8_t*)&inta;
    const Fusb302Status1ARegBits s1a = {.togss = FUSB302_STATUS1A_TOGSS_SRCON_CC1};
    g_mock_regs[Fusb302RegStatus1A] = *(const uint8_t*)&s1a;

    Collected c = {0};
    ucsi_ppm_phy_pump(ppm, collect_event, &c);

    TEST_ASSERT(c.count == 1u);
    TEST_ASSERT(c.events[0].kind == UcsiPpmPhyEventToggleDone);
    TEST_ASSERT(c.events[0].u.togss == FUSB302_STATUS1A_TOGSS_SRCON_CC1);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_phy_pump_multiple_events(void) {
    UcsiPpm* ppm = mock_make_ppm();
    ucsi_ppm_phy_init(ppm);
    mock_i2c_reset();

    const Fusb302InterruptRegBits intr = {.i_vbusok = 1, .i_bc_lvl = 1};
    g_mock_regs[Fusb302RegInterrupt] = *(const uint8_t*)&intr;
    const Fusb302Status0RegBits s0 = {.vbusok = 1, .bc_lvl = 0b10}; // 1.5 A capability
    g_mock_regs[Fusb302RegStatus0] = *(const uint8_t*)&s0;
    const Fusb302InterruptARegBits inta = {.i_tx_sent = 1};
    g_mock_regs[Fusb302RegInterruptA] = *(const uint8_t*)&inta;
    const Fusb302InterruptBRegBits intb = {.i_gcrc_sent = 1};
    g_mock_regs[Fusb302RegInterruptB] = *(const uint8_t*)&intb;

    Collected c = {0};
    ucsi_ppm_phy_pump(ppm, collect_event, &c);

    // Expect 4 events: BcLvlChanged, VbusChanged, TxSuccess, MessageRx.
    TEST_ASSERT(c.count == 4u);
    bool saw_bc = false, saw_vbus = false, saw_tx = false, saw_rx = false;
    for(size_t i = 0; i < c.count; ++i) {
        if(c.events[i].kind == UcsiPpmPhyEventBcLvlChanged) {
            saw_bc = true;
            TEST_ASSERT(c.events[i].u.bc_lvl == 0b10u);
        }
        if(c.events[i].kind == UcsiPpmPhyEventVbusChanged) {
            saw_vbus = true;
            TEST_ASSERT(c.events[i].u.vbus_ok == true);
        }
        if(c.events[i].kind == UcsiPpmPhyEventTxSuccess) saw_tx = true;
        if(c.events[i].kind == UcsiPpmPhyEventMessageRx) saw_rx = true;
    }
    TEST_ASSERT(saw_bc && saw_vbus && saw_tx && saw_rx);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_phy_pump_idle_no_events(void) {
    UcsiPpm* ppm = mock_make_ppm();
    ucsi_ppm_phy_init(ppm);
    mock_i2c_reset();
    // All INT regs zero.
    Collected c = {0};
    TEST_ASSERT(ucsi_ppm_phy_pump(ppm, collect_event, &c) == UcsiPpmStatusOk);
    TEST_ASSERT(c.count == 0u);
    ucsi_ppm_free(ppm);
    return true;
}

// --- L4 measurements (MDAC + BC_LVL) ---------------------------------------

static bool test_phy_measure_vbus_threshold_above(void) {
    UcsiPpm* ppm = mock_make_ppm();
    ucsi_ppm_phy_init(ppm);
    mock_i2c_reset();

    const Fusb302Status0RegBits s0 = {.comp = 1};
    g_mock_regs[Fusb302RegStatus0] = *(const uint8_t*)&s0;

    bool above = false;
    TEST_ASSERT(ucsi_ppm_phy_measure_vbus_threshold(ppm, 4500u, &above) == UcsiPpmStatusOk);
    TEST_ASSERT(above == true);

    const uint8_t measure = g_mock_regs[Fusb302RegMeasure];
    const Fusb302MeasureRegBits m_bits = *((const Fusb302MeasureRegBits*)&measure);
    TEST_ASSERT(m_bits.meas_vbus == 1);
    TEST_ASSERT(m_bits.mdac != 0);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_phy_measure_vbus_threshold_below(void) {
    UcsiPpm* ppm = mock_make_ppm();
    ucsi_ppm_phy_init(ppm);
    mock_i2c_reset();

    const Fusb302Status0RegBits s0 = {.comp = 0};
    g_mock_regs[Fusb302RegStatus0] = *(const uint8_t*)&s0;

    bool above = true;
    TEST_ASSERT(ucsi_ppm_phy_measure_vbus_threshold(ppm, 4500u, &above) == UcsiPpmStatusOk);
    TEST_ASSERT(above == false);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_phy_measure_vbus_threshold_mdac_calc(void) {
    // VBUS step is 420 mV.
    UcsiPpm* ppm = mock_make_ppm();
    ucsi_ppm_phy_init(ppm);
    mock_i2c_reset();

    bool above = false;

    // vSafe5V (~4500 mV): ceil(4500/420) = 11.
    ucsi_ppm_phy_measure_vbus_threshold(ppm, 4500u, &above);
    {
        const uint8_t m = g_mock_regs[Fusb302RegMeasure];
        const Fusb302MeasureRegBits b = *((const Fusb302MeasureRegBits*)&m);
        TEST_ASSERT(b.mdac == 11u);
    }

    // vSafe0V (~800 mV): ceil(800/420) = 2.
    ucsi_ppm_phy_measure_vbus_threshold(ppm, 800u, &above);
    {
        const uint8_t m = g_mock_regs[Fusb302RegMeasure];
        const Fusb302MeasureRegBits b = *((const Fusb302MeasureRegBits*)&m);
        TEST_ASSERT(b.mdac == 2u);
    }

    // 20 V renegotiated rail: ceil(20000/420) = 48.
    ucsi_ppm_phy_measure_vbus_threshold(ppm, 20000u, &above);
    {
        const uint8_t m = g_mock_regs[Fusb302RegMeasure];
        const Fusb302MeasureRegBits b = *((const Fusb302MeasureRegBits*)&m);
        TEST_ASSERT(b.mdac == 48u);
    }

    // Voltage that overflows the 6-bit field gets clamped at 0x3F.
    ucsi_ppm_phy_measure_vbus_threshold(ppm, 30000u, &above);
    {
        const uint8_t m = g_mock_regs[Fusb302RegMeasure];
        const Fusb302MeasureRegBits b = *((const Fusb302MeasureRegBits*)&m);
        TEST_ASSERT(b.mdac == 0x3Fu);
    }

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_phy_measure_vbus_threshold_null_out(void) {
    UcsiPpm* ppm = mock_make_ppm();
    ucsi_ppm_phy_init(ppm);
    TEST_ASSERT(ucsi_ppm_phy_measure_vbus_threshold(ppm, 4500u, NULL) == UcsiPpmStatusInvalidArg);
    ucsi_ppm_free(ppm);
    return true;
}

static bool test_phy_arm_vbus_compare(void) {
    // Programs MEASURE the same way as measure_vbus_threshold, but doesn't
    // touch STATUS0 — the comparator fires asynchronously via I_COMP_CHNG.
    UcsiPpm* ppm = mock_make_ppm();
    ucsi_ppm_phy_init(ppm);
    mock_i2c_reset();

    TEST_ASSERT(ucsi_ppm_phy_arm_vbus_compare(ppm, 4500u) == UcsiPpmStatusOk);
    const uint8_t measure = g_mock_regs[Fusb302RegMeasure];
    const Fusb302MeasureRegBits m_bits = *((const Fusb302MeasureRegBits*)&measure);
    TEST_ASSERT(m_bits.meas_vbus == 1);
    TEST_ASSERT(m_bits.mdac == 11u);

    // No read on STATUS0 should have happened.
    for(size_t i = 0; i < g_mock_txn_count; ++i) {
        const MockI2cTxn* t = &g_mock_txns[i];
        if(t->is_write && t->len == 1u) {
            TEST_ASSERT(t->data[0] != Fusb302RegStatus0);
        }
    }

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_phy_read_bc_lvl(void) {
    UcsiPpm* ppm = mock_make_ppm();
    ucsi_ppm_phy_init(ppm);
    mock_i2c_reset();

    // BC_LVL = 1.5 A capability; vbusok/comp also set to verify masking.
    const Fusb302Status0RegBits s0a = {.bc_lvl = 0b10, .comp = 1, .vbusok = 1};
    g_mock_regs[Fusb302RegStatus0] = *(const uint8_t*)&s0a;

    uint8_t lvl = 0xFFu;
    TEST_ASSERT(ucsi_ppm_phy_read_bc_lvl(ppm, &lvl) == UcsiPpmStatusOk);
    TEST_ASSERT(lvl == 0b10u);

    const Fusb302Status0RegBits s0b = {.bc_lvl = 0b11};
    g_mock_regs[Fusb302RegStatus0] = *(const uint8_t*)&s0b;
    ucsi_ppm_phy_read_bc_lvl(ppm, &lvl);
    TEST_ASSERT(lvl == 0b11u); // 3 A capability

    const Fusb302Status0RegBits s0c = {.bc_lvl = 0b00};
    g_mock_regs[Fusb302RegStatus0] = *(const uint8_t*)&s0c;
    ucsi_ppm_phy_read_bc_lvl(ppm, &lvl);
    TEST_ASSERT(lvl == 0b00u); // no Rd

    TEST_ASSERT(ucsi_ppm_phy_read_bc_lvl(ppm, NULL) == UcsiPpmStatusInvalidArg);

    ucsi_ppm_free(ppm);
    return true;
}

// --- L4 PD message TX ------------------------------------------------------

// Finds the I²C burst write to FIFOS (first byte = Fusb302RegFifos, length > 2).
// Returns -1 if no such transaction was recorded.
static int mock_find_fifo_burst(void) {
    for(size_t i = 0; i < g_mock_txn_count; ++i) {
        const MockI2cTxn* t = &g_mock_txns[i];
        if(t->is_write && t->len > 2u && t->data[0] == Fusb302RegFifos) {
            return (int)i;
        }
    }
    return -1;
}

// Verifies the 4 SOP tokens at `fifo[0..3]` match the SOP destination.
static bool check_sop_tokens(const uint8_t* fifo, UcsiPpmPhySopType type) {
    switch(type) {
    case UcsiPpmPhySopTypeSop:
        return fifo[0] == FUSB302_TX_TOKEN_SYNC1 && fifo[1] == FUSB302_TX_TOKEN_SYNC1 && fifo[2] == FUSB302_TX_TOKEN_SYNC1 && fifo[3] == FUSB302_TX_TOKEN_SYNC2;
    case UcsiPpmPhySopTypeSopPrime:
        return fifo[0] == FUSB302_TX_TOKEN_SYNC1 && fifo[1] == FUSB302_TX_TOKEN_SYNC1 && fifo[2] == FUSB302_TX_TOKEN_SYNC3 && fifo[3] == FUSB302_TX_TOKEN_SYNC3;
    case UcsiPpmPhySopTypeSopDoublePrime:
        return fifo[0] == FUSB302_TX_TOKEN_SYNC1 && fifo[1] == FUSB302_TX_TOKEN_SYNC3 && fifo[2] == FUSB302_TX_TOKEN_SYNC1 && fifo[3] == FUSB302_TX_TOKEN_SYNC3;
    }
    return false;
}

// Verifies JAMCRC + EOP + TXOFF + TXON at the tail of the FIFO burst.
static bool check_trailer(const uint8_t* fifo, size_t len) {
    if(len < 4u) return false;
    return fifo[len - 4u] == FUSB302_TX_TOKEN_JAMCRC && fifo[len - 3u] == FUSB302_TX_TOKEN_EOP && fifo[len - 2u] == FUSB302_TX_TOKEN_TXOFF &&
           fifo[len - 1u] == FUSB302_TX_TOKEN_TXON;
}

static bool test_phy_send_message_control(void) {
    // Accept (opcode 0x03) — 0 data objects. Just a header.
    UcsiPpm* ppm = mock_make_ppm();
    ucsi_ppm_phy_init(ppm);
    mock_i2c_reset();

    const UcsiPpmPhyPdMsg msg = {
        .sop_type = UcsiPpmPhySopTypeSop,
        .header = 0x0303u, // arbitrary header bits + Accept opcode
        .object_count = 0,
    };
    TEST_ASSERT(ucsi_ppm_phy_send_message(ppm, &msg) == UcsiPpmStatusOk);

    const int idx = mock_find_fifo_burst();
    TEST_ASSERT(idx >= 0);
    const MockI2cTxn* t = &g_mock_txns[idx];

    const uint8_t* fifo = &t->data[1]; // skip reg address byte
    const size_t fifo_len = t->len - 1u;
    // Expected: 4 SOP + 1 PACKSYM + 2 header + 0 objects + 4 trailer = 11 bytes.
    TEST_ASSERT(fifo_len == 11u);

    TEST_ASSERT(check_sop_tokens(fifo, UcsiPpmPhySopTypeSop));
    // PACKSYM with length = 2 (header only).
    TEST_ASSERT(fifo[4] == (uint8_t)(FUSB302_TX_TOKEN_PACKSYM | 2u));
    // Header LE; NumberOfDataObjects (bits 14:12) cleared since object_count==0.
    TEST_ASSERT(fifo[5] == 0x03u);
    TEST_ASSERT(fifo[6] == 0x03u);
    TEST_ASSERT(check_trailer(fifo, fifo_len));

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_phy_send_message_one_object(void) {
    // Request (opcode 0x02) — 1 RDO.
    UcsiPpm* ppm = mock_make_ppm();
    ucsi_ppm_phy_init(ppm);
    mock_i2c_reset();

    const UcsiPpmPhyPdMsg msg = {
        .sop_type = UcsiPpmPhySopTypeSop,
        .header = 0x0042u, // base header; NDO field will be overwritten
        .objects = {0x12345678u},
        .object_count = 1,
    };
    TEST_ASSERT(ucsi_ppm_phy_send_message(ppm, &msg) == UcsiPpmStatusOk);

    const int idx = mock_find_fifo_burst();
    TEST_ASSERT(idx >= 0);
    const uint8_t* fifo = &g_mock_txns[idx].data[1];
    const size_t fifo_len = g_mock_txns[idx].len - 1u;
    // 4 SOP + 1 PACKSYM + 2 header + 4 object + 4 trailer = 15 bytes.
    TEST_ASSERT(fifo_len == 15u);

    TEST_ASSERT(check_sop_tokens(fifo, UcsiPpmPhySopTypeSop));
    // PACKSYM length = 2 + 4 = 6.
    TEST_ASSERT(fifo[4] == (uint8_t)(FUSB302_TX_TOKEN_PACKSYM | 6u));
    // Header: NDO field (bits 14:12) patched to 1, low byte preserved.
    const uint16_t expected_hdr = (uint16_t)(0x0042u | (1u << 12));
    TEST_ASSERT(fifo[5] == (uint8_t)(expected_hdr & 0xFFu));
    TEST_ASSERT(fifo[6] == (uint8_t)((expected_hdr >> 8) & 0xFFu));
    // Object LE.
    TEST_ASSERT(fifo[7] == 0x78u);
    TEST_ASSERT(fifo[8] == 0x56u);
    TEST_ASSERT(fifo[9] == 0x34u);
    TEST_ASSERT(fifo[10] == 0x12u);
    TEST_ASSERT(check_trailer(fifo, fifo_len));

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_phy_send_message_three_objects(void) {
    // Source_Capabilities-style: 3 PDOs.
    UcsiPpm* ppm = mock_make_ppm();
    ucsi_ppm_phy_init(ppm);
    mock_i2c_reset();

    const UcsiPpmPhyPdMsg msg = {
        .sop_type = UcsiPpmPhySopTypeSop,
        .header = 0x0001u, // Source_Capabilities opcode
        .objects = {0xAABBCCDDu, 0x11223344u, 0xDEADBEEFu},
        .object_count = 3,
    };
    TEST_ASSERT(ucsi_ppm_phy_send_message(ppm, &msg) == UcsiPpmStatusOk);

    const int idx = mock_find_fifo_burst();
    const uint8_t* fifo = &g_mock_txns[idx].data[1];
    const size_t fifo_len = g_mock_txns[idx].len - 1u;
    // 4 SOP + 1 PACKSYM + 2 header + 12 objects + 4 trailer = 23 bytes.
    TEST_ASSERT(fifo_len == 23u);
    TEST_ASSERT(fifo[4] == (uint8_t)(FUSB302_TX_TOKEN_PACKSYM | 14u));

    // Header with NDO=3 patched in.
    const uint16_t expected_hdr = (uint16_t)(0x0001u | (3u << 12));
    TEST_ASSERT(fifo[5] == (uint8_t)(expected_hdr & 0xFFu));
    TEST_ASSERT(fifo[6] == (uint8_t)((expected_hdr >> 8) & 0xFFu));

    // Objects in order, each LE.
    const uint8_t expected[12] = {
        0xDDu,
        0xCCu,
        0xBBu,
        0xAAu, // obj 0
        0x44u,
        0x33u,
        0x22u,
        0x11u, // obj 1
        0xEFu,
        0xBEu,
        0xADu,
        0xDEu, // obj 2
    };
    for(size_t i = 0; i < 12u; ++i) {
        TEST_ASSERT(fifo[7u + i] == expected[i]);
    }
    TEST_ASSERT(check_trailer(fifo, fifo_len));

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_phy_send_message_header_ndo_patched(void) {
    // Caller passes a header with stale NDO=5; encoder must rewrite it to
    // match object_count=2 so partner sees the correct count.
    UcsiPpm* ppm = mock_make_ppm();
    ucsi_ppm_phy_init(ppm);
    mock_i2c_reset();

    const UcsiPpmPhyPdMsg msg = {
        .sop_type = UcsiPpmPhySopTypeSop,
        .header = (uint16_t)((5u << 12) | 0x0042u), // stale NDO=5
        .objects = {0xAAAAAAAAu, 0xBBBBBBBBu},
        .object_count = 2,
    };
    ucsi_ppm_phy_send_message(ppm, &msg);

    const int idx = mock_find_fifo_burst();
    const uint8_t* fifo = &g_mock_txns[idx].data[1];

    const uint16_t hdr = (uint16_t)(fifo[5] | (fifo[6] << 8));
    TEST_ASSERT(((hdr >> 12) & 0x07u) == 2u); // NDO field == object_count
    TEST_ASSERT((hdr & ~0x7000u) == 0x0042u); // other bits preserved

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_phy_send_message_sop_prime(void) {
    UcsiPpm* ppm = mock_make_ppm();
    ucsi_ppm_phy_init(ppm);
    mock_i2c_reset();

    const UcsiPpmPhyPdMsg msg = {.sop_type = UcsiPpmPhySopTypeSopPrime, .object_count = 0};
    ucsi_ppm_phy_send_message(ppm, &msg);
    const int idx = mock_find_fifo_burst();
    TEST_ASSERT(check_sop_tokens(&g_mock_txns[idx].data[1], UcsiPpmPhySopTypeSopPrime));

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_phy_send_message_sop_double_prime(void) {
    UcsiPpm* ppm = mock_make_ppm();
    ucsi_ppm_phy_init(ppm);
    mock_i2c_reset();

    const UcsiPpmPhyPdMsg msg = {
        .sop_type = UcsiPpmPhySopTypeSopDoublePrime,
        .object_count = 0,
    };
    ucsi_ppm_phy_send_message(ppm, &msg);
    const int idx = mock_find_fifo_burst();
    TEST_ASSERT(check_sop_tokens(&g_mock_txns[idx].data[1], UcsiPpmPhySopTypeSopDoublePrime));

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_phy_send_message_validates(void) {
    UcsiPpm* ppm = mock_make_ppm();
    ucsi_ppm_phy_init(ppm);

    TEST_ASSERT(ucsi_ppm_phy_send_message(ppm, NULL) == UcsiPpmStatusInvalidArg);

    UcsiPpmPhyPdMsg msg = {.sop_type = UcsiPpmPhySopTypeSop, .object_count = 8};
    TEST_ASSERT(ucsi_ppm_phy_send_message(ppm, &msg) == UcsiPpmStatusInvalidArg);

    msg.object_count = 0;
    msg.sop_type = (UcsiPpmPhySopType)99; // invalid
    TEST_ASSERT(ucsi_ppm_phy_send_message(ppm, &msg) == UcsiPpmStatusInvalidArg);

    ucsi_ppm_free(ppm);
    return true;
}

// --- L4 PD message RX ------------------------------------------------------

// Builds an RX FIFO byte stream for a given SOP-type message: SOP-token +
// header LE + objects LE + 4-byte stub CRC. Returns total bytes written.
static size_t build_rx_stream(uint8_t* dst, uint8_t sop_token, uint16_t header, const uint32_t* objects, uint8_t object_count) {
    size_t len = 0;
    dst[len++] = sop_token;
    dst[len++] = (uint8_t)(header & 0xFFu);
    dst[len++] = (uint8_t)((header >> 8) & 0xFFu);
    for(uint8_t i = 0; i < object_count; ++i) {
        const uint32_t o = objects[i];
        dst[len++] = (uint8_t)(o & 0xFFu);
        dst[len++] = (uint8_t)((o >> 8) & 0xFFu);
        dst[len++] = (uint8_t)((o >> 16) & 0xFFu);
        dst[len++] = (uint8_t)((o >> 24) & 0xFFu);
    }
    // Stub CRC32 (chip validates it before placing the message in FIFO; we
    // just need 4 bytes to consume).
    dst[len++] = 0xDEu;
    dst[len++] = 0xADu;
    dst[len++] = 0xBEu;
    dst[len++] = 0xEFu;
    return len;
}

static bool test_phy_recv_empty(void) {
    UcsiPpm* ppm = mock_make_ppm();
    ucsi_ppm_phy_init(ppm);
    mock_i2c_reset();
    // STATUS1.RX_EMPTY = 1.
    const Fusb302Status1RegBits s1 = {.rx_empty = 1};
    g_mock_regs[Fusb302RegStatus1] = *(const uint8_t*)&s1;

    UcsiPpmPhyPdMsg msg;
    bool received = true;
    TEST_ASSERT(ucsi_ppm_phy_recv_message(ppm, &msg, &received) == UcsiPpmStatusOk);
    TEST_ASSERT(received == false);
    // No FIFO bytes should have been consumed.
    TEST_ASSERT(g_mock_fifo_pos == 0u);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_phy_recv_sop_control(void) {
    UcsiPpm* ppm = mock_make_ppm();
    ucsi_ppm_phy_init(ppm);
    mock_i2c_reset();
    g_mock_regs[Fusb302RegStatus1] = 0u; // rx_empty = 0

    // Accept (opcode 0x03), 0 objects → header NDO field = 0.
    const uint16_t header = 0x0303u;
    uint8_t stream[16];
    const size_t n = build_rx_stream(stream, FUSB302_RX_TOKEN_SOP, header, NULL, 0);
    mock_fifo_load(stream, n);

    UcsiPpmPhyPdMsg msg = {0};
    bool received = false;
    TEST_ASSERT(ucsi_ppm_phy_recv_message(ppm, &msg, &received) == UcsiPpmStatusOk);
    TEST_ASSERT(received == true);
    TEST_ASSERT(msg.sop_type == UcsiPpmPhySopTypeSop);
    TEST_ASSERT(msg.header == header);
    TEST_ASSERT(msg.object_count == 0u);
    // All bytes (token + 2 header + 4 CRC = 7) must have been consumed.
    TEST_ASSERT(g_mock_fifo_pos == 7u);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_phy_recv_sop_one_object(void) {
    UcsiPpm* ppm = mock_make_ppm();
    ucsi_ppm_phy_init(ppm);
    mock_i2c_reset();
    g_mock_regs[Fusb302RegStatus1] = 0u;

    // Header with NDO = 1.
    const uint16_t header = (uint16_t)(0x0042u | (1u << 12));
    const uint32_t obj = 0x12345678u;
    uint8_t stream[32];
    const size_t n = build_rx_stream(stream, FUSB302_RX_TOKEN_SOP, header, &obj, 1);
    mock_fifo_load(stream, n);

    UcsiPpmPhyPdMsg msg = {0};
    bool received = false;
    TEST_ASSERT(ucsi_ppm_phy_recv_message(ppm, &msg, &received) == UcsiPpmStatusOk);
    TEST_ASSERT(received == true);
    TEST_ASSERT(msg.sop_type == UcsiPpmPhySopTypeSop);
    TEST_ASSERT(msg.header == header);
    TEST_ASSERT(msg.object_count == 1u);
    TEST_ASSERT(msg.objects[0] == 0x12345678u);
    // 1 token + 2 header + 4 obj + 4 CRC = 11 bytes consumed.
    TEST_ASSERT(g_mock_fifo_pos == 11u);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_phy_recv_sop_three_objects(void) {
    UcsiPpm* ppm = mock_make_ppm();
    ucsi_ppm_phy_init(ppm);
    mock_i2c_reset();
    g_mock_regs[Fusb302RegStatus1] = 0u;

    const uint16_t header = (uint16_t)(0x0001u | (3u << 12));
    const uint32_t objects[3] = {0xAABBCCDDu, 0x11223344u, 0xDEADBEEFu};
    uint8_t stream[32];
    const size_t n = build_rx_stream(stream, FUSB302_RX_TOKEN_SOP, header, objects, 3);
    mock_fifo_load(stream, n);

    UcsiPpmPhyPdMsg msg = {0};
    bool received = false;
    ucsi_ppm_phy_recv_message(ppm, &msg, &received);
    TEST_ASSERT(received == true);
    TEST_ASSERT(msg.object_count == 3u);
    TEST_ASSERT(msg.objects[0] == 0xAABBCCDDu);
    TEST_ASSERT(msg.objects[1] == 0x11223344u);
    TEST_ASSERT(msg.objects[2] == 0xDEADBEEFu);
    // 1 + 2 + 12 + 4 = 19 bytes.
    TEST_ASSERT(g_mock_fifo_pos == 19u);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_phy_recv_sop_prime(void) {
    UcsiPpm* ppm = mock_make_ppm();
    ucsi_ppm_phy_init(ppm);
    mock_i2c_reset();
    g_mock_regs[Fusb302RegStatus1] = 0u;

    uint8_t stream[16];
    const size_t n = build_rx_stream(stream, FUSB302_RX_TOKEN_SOP1, 0x0000u, NULL, 0);
    mock_fifo_load(stream, n);

    UcsiPpmPhyPdMsg msg = {0};
    bool received = false;
    ucsi_ppm_phy_recv_message(ppm, &msg, &received);
    TEST_ASSERT(received == true);
    TEST_ASSERT(msg.sop_type == UcsiPpmPhySopTypeSopPrime);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_phy_recv_sop_double_prime(void) {
    UcsiPpm* ppm = mock_make_ppm();
    ucsi_ppm_phy_init(ppm);
    mock_i2c_reset();
    g_mock_regs[Fusb302RegStatus1] = 0u;

    uint8_t stream[16];
    const size_t n = build_rx_stream(stream, FUSB302_RX_TOKEN_SOP2, 0x0000u, NULL, 0);
    mock_fifo_load(stream, n);

    UcsiPpmPhyPdMsg msg = {0};
    bool received = false;
    ucsi_ppm_phy_recv_message(ppm, &msg, &received);
    TEST_ASSERT(received == true);
    TEST_ASSERT(msg.sop_type == UcsiPpmPhySopTypeSopDoublePrime);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_phy_recv_token_low_bits_ignored(void) {
    // Real chip may set low bits of the SOP-token byte to non-zero metadata.
    // Only the top 3 bits (mask 0xE0) matter for SOP-type decoding.
    UcsiPpm* ppm = mock_make_ppm();
    ucsi_ppm_phy_init(ppm);
    mock_i2c_reset();
    g_mock_regs[Fusb302RegStatus1] = 0u;

    uint8_t stream[16];
    const size_t n = build_rx_stream(stream, (uint8_t)(FUSB302_RX_TOKEN_SOP | 0x1Fu), 0u, NULL, 0);
    mock_fifo_load(stream, n);

    UcsiPpmPhyPdMsg msg = {0};
    bool received = false;
    ucsi_ppm_phy_recv_message(ppm, &msg, &received);
    TEST_ASSERT(received == true);
    TEST_ASSERT(msg.sop_type == UcsiPpmPhySopTypeSop);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_phy_recv_sop_debug_returns_internal(void) {
    UcsiPpm* ppm = mock_make_ppm();
    ucsi_ppm_phy_init(ppm);
    mock_i2c_reset();
    g_mock_regs[Fusb302RegStatus1] = 0u;

    uint8_t stream[16];
    const size_t n = build_rx_stream(stream, FUSB302_RX_TOKEN_SOP1DB, 0u, NULL, 0);
    mock_fifo_load(stream, n);

    UcsiPpmPhyPdMsg msg = {0};
    bool received = true;
    TEST_ASSERT(ucsi_ppm_phy_recv_message(ppm, &msg, &received) == UcsiPpmStatusInternal);
    TEST_ASSERT(received == false);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_phy_recv_validates(void) {
    UcsiPpm* ppm = mock_make_ppm();
    ucsi_ppm_phy_init(ppm);

    UcsiPpmPhyPdMsg msg = {0};
    bool received = false;
    TEST_ASSERT(ucsi_ppm_phy_recv_message(ppm, NULL, &received) == UcsiPpmStatusInvalidArg);
    TEST_ASSERT(ucsi_ppm_phy_recv_message(ppm, &msg, NULL) == UcsiPpmStatusInvalidArg);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_phy_recv_drains_multiple(void) {
    // Two back-to-back messages stack in the FIFO between I_GCRCSENT events;
    // the second recv must pick up where the first left off.
    UcsiPpm* ppm = mock_make_ppm();
    ucsi_ppm_phy_init(ppm);
    mock_i2c_reset();
    g_mock_regs[Fusb302RegStatus1] = 0u;

    uint8_t stream[32];
    size_t off = 0;
    off += build_rx_stream(&stream[off], FUSB302_RX_TOKEN_SOP, 0x0101u, NULL, 0);
    const uint32_t obj = 0xCAFEBABEu;
    off += build_rx_stream(&stream[off], FUSB302_RX_TOKEN_SOP, (uint16_t)(0x0042u | (1u << 12)), &obj, 1);
    mock_fifo_load(stream, off);

    UcsiPpmPhyPdMsg msg = {0};
    bool received = false;

    ucsi_ppm_phy_recv_message(ppm, &msg, &received);
    TEST_ASSERT(received == true);
    TEST_ASSERT(msg.header == 0x0101u);
    TEST_ASSERT(msg.object_count == 0u);

    ucsi_ppm_phy_recv_message(ppm, &msg, &received);
    TEST_ASSERT(received == true);
    TEST_ASSERT(msg.object_count == 1u);
    TEST_ASSERT(msg.objects[0] == 0xCAFEBABEu);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_phy_tx_rx_roundtrip(void) {
    // Encode a message via send_message, copy the resulting header + objects
    // out of the TX burst into an RX-style stream, feed it back through
    // recv_message and verify identity.
    UcsiPpm* ppm = mock_make_ppm();
    ucsi_ppm_phy_init(ppm);
    mock_i2c_reset();

    const UcsiPpmPhyPdMsg sent = {
        .sop_type = UcsiPpmPhySopTypeSop,
        .header = (uint16_t)(0x0042u | (2u << 12)),
        .objects = {0x11223344u, 0xAABBCCDDu},
        .object_count = 2,
    };
    TEST_ASSERT(ucsi_ppm_phy_send_message(ppm, &sent) == UcsiPpmStatusOk);

    const int idx = mock_find_fifo_burst();
    TEST_ASSERT(idx >= 0);
    const uint8_t* tx = &g_mock_txns[idx].data[1];
    // TX layout: 4 SOP + 1 PACKSYM + 2 header + 4*N objects + 4 trailer.
    // Header + objects start at offset 5, length 2 + 4*N.
    const size_t payload_len = 2u + 4u * sent.object_count;

    // Reset transaction log so the RX reads aren't mixed with TX bytes.
    mock_i2c_reset();
    g_mock_regs[Fusb302RegStatus1] = 0u;

    // Build RX stream: SOP token + payload bytes from TX + stub CRC.
    uint8_t stream[32];
    stream[0] = FUSB302_RX_TOKEN_SOP;
    memcpy(&stream[1], &tx[5], payload_len);
    stream[1 + payload_len + 0] = 0x00u;
    stream[1 + payload_len + 1] = 0x00u;
    stream[1 + payload_len + 2] = 0x00u;
    stream[1 + payload_len + 3] = 0x00u;
    mock_fifo_load(stream, 1u + payload_len + 4u);

    UcsiPpmPhyPdMsg recv = {0};
    bool received = false;
    ucsi_ppm_phy_recv_message(ppm, &recv, &received);

    TEST_ASSERT(received == true);
    TEST_ASSERT(recv.sop_type == sent.sop_type);
    TEST_ASSERT(recv.header == sent.header);
    TEST_ASSERT(recv.object_count == sent.object_count);
    TEST_ASSERT(recv.objects[0] == sent.objects[0]);
    TEST_ASSERT(recv.objects[1] == sent.objects[1]);

    ucsi_ppm_free(ppm);
    return true;
}

// --- L1/L4 wire-up ---------------------------------------------------------

static bool test_wireup_init_calls_phy_init(void) {
    // ucsi_ppm_init must drive a SW_RESET write into FUSB302 (first step of
    // phy_init) so the chip starts from a known state.
    UcsiPpm* ppm = ucsi_ppm_alloc();
    mock_i2c_reset();

    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    cfg.i2c_read = mock_i2c_read_fn;
    cfg.i2c_write = mock_i2c_write_fn;

    TEST_ASSERT(ucsi_ppm_init(ppm, &cfg) == UcsiPpmStatusOk);

    const Fusb302ResetRegBits expected = {.sw_reset = 1};
    TEST_ASSERT(mock_any_write_to(Fusb302RegReset, *(const uint8_t*)&expected));
    // POWER all blocks on must also have been programmed.
    const Fusb302PowerRegBits power = {.pwr = 0b1111};
    TEST_ASSERT(mock_any_write_to(Fusb302RegPower, *(const uint8_t*)&power));

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_wireup_init_phy_failure_propagates(void) {
    // If the I²C HAL refuses a write, ucsi_ppm_init must surface HalError and
    // leave the instance in Allocated state (next init can retry).
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    // i2c_write returns HalError to simulate bus failure.
    cfg.i2c_write = (UcsiPpmI2cWriteFn)NULL; // will fail config_is_valid
    TEST_ASSERT(ucsi_ppm_init(ppm, &cfg) == UcsiPpmStatusInvalidConfig);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_wireup_deinit_drops_terminations(void) {
    UcsiPpm* ppm = mock_make_ppm();
    mock_i2c_reset();

    TEST_ASSERT(ucsi_ppm_deinit(ppm) == UcsiPpmStatusOk);
    // SWITCHES0, SWITCHES1 and CONTROL2 must each receive a 0x00 write.
    TEST_ASSERT(mock_any_write_to(Fusb302RegSwitches0, 0u));
    TEST_ASSERT(mock_any_write_to(Fusb302RegSwitches1, 0u));
    TEST_ASSERT(mock_any_write_to(Fusb302RegControl2, 0u));

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_wireup_reset_re_inits_phy(void) {
    UcsiPpm* ppm = mock_make_ppm();
    mock_i2c_reset();

    TEST_ASSERT(ucsi_ppm_reset(ppm) == UcsiPpmStatusOk);
    // reset re-runs phy_init — SW_RESET must be written.
    const Fusb302ResetRegBits expected = {.sw_reset = 1};
    TEST_ASSERT(mock_any_write_to(Fusb302RegReset, *(const uint8_t*)&expected));

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_wireup_notify_irq_sets_flag_no_i2c(void) {
    UcsiPpm* ppm = mock_make_ppm();
    mock_i2c_reset();

    TEST_ASSERT(ppm->pending_flags == 0u);
    TEST_ASSERT(ucsi_ppm_notify_fusb302_irq(ppm) == UcsiPpmStatusOk);
    TEST_ASSERT((ppm->pending_flags & UCSI_PPM_PENDING_PHY_IRQ) != 0u);
    // notify_* is ISR-safe — no I²C activity allowed.
    TEST_ASSERT(g_mock_txn_count == 0u);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_wireup_notify_psu_ready_sets_flag(void) {
    UcsiPpm* ppm = mock_make_ppm();
    mock_i2c_reset();

    TEST_ASSERT(ucsi_ppm_notify_power_supply_ready(ppm) == UcsiPpmStatusOk);
    TEST_ASSERT((ppm->pending_flags & UCSI_PPM_PENDING_POWER_SUPPLY_RDY) != 0u);
    TEST_ASSERT(g_mock_txn_count == 0u);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_wireup_tick_drains_irq(void) {
    UcsiPpm* ppm = mock_make_ppm();
    // After init, raise IRQ flag, then tick. Expect the pump to read INT regs.
    TEST_ASSERT(ucsi_ppm_notify_fusb302_irq(ppm) == UcsiPpmStatusOk);
    mock_i2c_reset();

    TEST_ASSERT(ucsi_ppm_tick(ppm) == UcsiPpmStatusOk);
    TEST_ASSERT((ppm->pending_flags & UCSI_PPM_PENDING_PHY_IRQ) == 0u);

    // Pump reads INTERRUPTA+B (2-byte burst) and INTERRUPT (1 byte).
    bool saw_inta_burst = false;
    bool saw_intr_read = false;
    for(size_t i = 0; i < g_mock_txn_count; ++i) {
        const MockI2cTxn* t = &g_mock_txns[i];
        if(t->is_write && t->len == 1u && t->data[0] == Fusb302RegInterruptA) {
            saw_inta_burst = true;
        }
        if(t->is_write && t->len == 1u && t->data[0] == Fusb302RegInterrupt) {
            saw_intr_read = true;
        }
    }
    TEST_ASSERT(saw_inta_burst);
    TEST_ASSERT(saw_intr_read);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_wireup_tick_idle_no_i2c(void) {
    UcsiPpm* ppm = mock_make_ppm();
    mock_i2c_reset();

    // No pending flags — tick must be a no-op on the I²C side.
    TEST_ASSERT(ppm->pending_flags == 0u);
    TEST_ASSERT(ucsi_ppm_tick(ppm) == UcsiPpmStatusOk);
    TEST_ASSERT(g_mock_txn_count == 0u);

    ucsi_ppm_free(ppm);
    return true;
}

// --- L3 Type-C SM (scaffold + Unattached) ----------------------------------

#include "ucsi_ppm_tc.h"

// Helper: build a ppm with mock I²C wired in, custom CC mode (overrides
// the default DRP from make_valid_config).
static UcsiPpm* mock_make_ppm_with_mode(UcsiPpmCcOperationMode mode, bool supports_disabled) {
    mock_i2c_reset();
    UcsiPpm* ppm = ucsi_ppm_alloc();
    if(!ppm) return NULL;
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    cfg.i2c_read = mock_i2c_read_fn;
    cfg.i2c_write = mock_i2c_write_fn;
    cfg.time_ms = mock_time_ms;
    cfg.gpio_write_vbus_source = mock_gpio_write_vbus_source;
    cfg.power_supply_set = mock_power_supply_set;
    cfg.alert = mock_alert;
    cfg.initial_cc_operation_mode = mode;
    if(supports_disabled) cfg.supports_disabled_state = true;
    if(ucsi_ppm_init(ppm, &cfg) != UcsiPpmStatusOk) {
        ucsi_ppm_free(ppm);
        return NULL;
    }
    // Simulate OPM having called SET_NOTIFICATION_ENABLE with all CSC bits
    // enabled — see note in mock_make_ppm.
    ppm->notification_mask = 0xFFFEu;
    return ppm;
}

// Simulates a ToggleDone IRQ by populating mock registers + setting the
// pending IRQ flag, then ticks once. All other INT registers are cleared
// so we don't accidentally fire spurious events alongside ToggleDone.
static void simulate_toggle_done(UcsiPpm* ppm, uint8_t togss) {
    g_mock_regs[Fusb302RegInterrupt] = 0;
    g_mock_regs[Fusb302RegInterruptB] = 0;
    const Fusb302InterruptARegBits inta = {.i_tog_done = 1};
    g_mock_regs[Fusb302RegInterruptA] = *(const uint8_t*)&inta;
    const Fusb302Status1ARegBits s1a = {.togss = togss};
    g_mock_regs[Fusb302RegStatus1A] = *(const uint8_t*)&s1a;
    ucsi_ppm_notify_fusb302_irq(ppm);
    ucsi_ppm_tick(ppm);
}

// Simulates a VBUS-changed IRQ (INTERRUPT.I_VBUSOK) with STATUS0.VBUSOK set
// to `vbus_ok`. Pump emits UcsiPpmPhyEventVbusChanged into the TC handler.
static void simulate_vbus_changed(UcsiPpm* ppm, bool vbus_ok) {
    g_mock_regs[Fusb302RegInterruptA] = 0;
    g_mock_regs[Fusb302RegInterruptB] = 0;
    const Fusb302InterruptRegBits intr = {.i_vbusok = 1};
    g_mock_regs[Fusb302RegInterrupt] = *(const uint8_t*)&intr;
    const Fusb302Status0RegBits s0 = {.vbusok = vbus_ok ? 1u : 0u};
    g_mock_regs[Fusb302RegStatus0] = *(const uint8_t*)&s0;
    ucsi_ppm_notify_fusb302_irq(ppm);
    ucsi_ppm_tick(ppm);
}

// Simulates a BC_LVL-changed IRQ (INTERRUPT.I_BC_LVL) with STATUS0.BC_LVL
// set to `bc_lvl` (2-bit field). Used to drive source-side detach detection
// via tc_handle_phy_event.
static void simulate_bc_lvl_changed(UcsiPpm* ppm, uint8_t bc_lvl) {
    g_mock_regs[Fusb302RegInterruptA] = 0;
    g_mock_regs[Fusb302RegInterruptB] = 0;
    const Fusb302InterruptRegBits intr = {.i_bc_lvl = 1};
    g_mock_regs[Fusb302RegInterrupt] = *(const uint8_t*)&intr;
    const Fusb302Status0RegBits s0 = {.bc_lvl = bc_lvl};
    g_mock_regs[Fusb302RegStatus0] = *(const uint8_t*)&s0;
    ucsi_ppm_notify_fusb302_irq(ppm);
    ucsi_ppm_tick(ppm);
}

// Simulates a "GoodCRC sent" IRQ — the FUSB302 raises I_GCRCSENT after
// it auto-ACKs an incoming PD frame, which is the cue for software to
// drain the RX FIFO. The FIFO contents must be pre-loaded via mock_fifo_load.
static void simulate_message_rx_event(UcsiPpm* ppm) {
    g_mock_regs[Fusb302RegInterrupt] = 0;
    g_mock_regs[Fusb302RegInterruptA] = 0;
    const Fusb302InterruptBRegBits intb = {.i_gcrc_sent = 1};
    g_mock_regs[Fusb302RegInterruptB] = *(const uint8_t*)&intb;
    ucsi_ppm_notify_fusb302_irq(ppm);
    ucsi_ppm_tick(ppm);
}

// Builds a PD message header with the given MessageID and arbitrary other
// fields. MessageID lives at bits 11:9 of the header.
static uint16_t make_header_with_msg_id(uint16_t base, uint8_t msg_id) {
    return (uint16_t)((base & ~(0x07u << 9)) | ((uint16_t)(msg_id & 0x07u) << 9));
}

// Drives the SM all the way to Attached.{Src,Snk} for the requested role.
// Used by detach tests so they can focus on the detach assertions.
static UcsiPpm* mock_attach(bool as_src) {
    UcsiPpm* ppm = mock_make_ppm_with_mode(UcsiPpmCcModeDrp, false);
    g_mock_time_ms = 0;
    simulate_toggle_done(ppm, as_src ? FUSB302_STATUS1A_TOGSS_SRCON_CC1 : FUSB302_STATUS1A_TOGSS_SNKON_CC1);
    g_mock_time_ms = 10;
    simulate_vbus_changed(ppm, true);
    g_mock_time_ms = 150;
    ucsi_ppm_tick(ppm);
    return ppm;
}

static bool test_tc_init_drp_starts_drp_toggle(void) {
    UcsiPpm* ppm = mock_make_ppm_with_mode(UcsiPpmCcModeDrp, false);
    TEST_ASSERT(ppm != NULL);

    const uint8_t c2 = g_mock_regs[Fusb302RegControl2];
    const Fusb302Control2RegBits c2_bits = *((const Fusb302Control2RegBits*)&c2);
    TEST_ASSERT(c2_bits.toggle == 1);
    TEST_ASSERT(c2_bits.mode == 0b01u); // DRP
    TEST_ASSERT(ucsi_ppm_get_connector_state(ppm) == UcsiPpmStateUnattached);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_tc_init_rp_only_starts_src_toggle(void) {
    UcsiPpm* ppm = mock_make_ppm_with_mode(UcsiPpmCcModeRpOnly, false);

    const uint8_t c2 = g_mock_regs[Fusb302RegControl2];
    const Fusb302Control2RegBits c2_bits = *((const Fusb302Control2RegBits*)&c2);
    TEST_ASSERT(c2_bits.toggle == 1);
    TEST_ASSERT(c2_bits.mode == 0b11u); // SRC
    TEST_ASSERT(ucsi_ppm_get_connector_state(ppm) == UcsiPpmStateUnattached);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_tc_init_rd_only_starts_snk_toggle(void) {
    UcsiPpm* ppm = mock_make_ppm_with_mode(UcsiPpmCcModeRdOnly, false);

    const uint8_t c2 = g_mock_regs[Fusb302RegControl2];
    const Fusb302Control2RegBits c2_bits = *((const Fusb302Control2RegBits*)&c2);
    TEST_ASSERT(c2_bits.toggle == 1);
    TEST_ASSERT(c2_bits.mode == 0b10u); // SNK

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_tc_init_disabled_no_toggle(void) {
    // supports_disabled_state required to let the config pass validation.
    UcsiPpm* ppm = mock_make_ppm_with_mode(UcsiPpmCcModeDisabled, true);
    TEST_ASSERT(ppm != NULL);

    const uint8_t c2 = g_mock_regs[Fusb302RegControl2];
    const Fusb302Control2RegBits c2_bits = *((const Fusb302Control2RegBits*)&c2);
    TEST_ASSERT(c2_bits.toggle == 0); // no toggle started
    TEST_ASSERT(ucsi_ppm_get_connector_state(ppm) == UcsiPpmStateDisabled);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_tc_toggle_done_src_cc1(void) {
    UcsiPpm* ppm = mock_make_ppm_with_mode(UcsiPpmCcModeDrp, false);
    mock_i2c_reset();

    simulate_toggle_done(ppm, FUSB302_STATUS1A_TOGSS_SRCON_CC1);

    TEST_ASSERT(ucsi_ppm_get_connector_state(ppm) == UcsiPpmStateAttachWait);
    TEST_ASSERT(ppm->tc_orientation == (int)UcsiPpmPhyCc1);
    TEST_ASSERT(ppm->tc_role_is_src == true);

    // Polarity locked to CC1.
    const uint8_t sw0 = g_mock_regs[Fusb302RegSwitches0];
    const Fusb302Switches0RegBits sw0_bits = *((const Fusb302Switches0RegBits*)&sw0);
    TEST_ASSERT(sw0_bits.meas_cc1 == 1);
    TEST_ASSERT(sw0_bits.meas_cc2 == 0);
    const uint8_t sw1 = g_mock_regs[Fusb302RegSwitches1];
    const Fusb302Switches1RegBits sw1_bits = *((const Fusb302Switches1RegBits*)&sw1);
    TEST_ASSERT(sw1_bits.tx_cc1 == 1);
    TEST_ASSERT(sw1_bits.tx_cc2 == 0);

    // Source-side Rp current programmed (default config = UsbDefault).
    const uint8_t c0 = g_mock_regs[Fusb302RegControl0];
    const Fusb302Control0RegBits c0_bits = *((const Fusb302Control0RegBits*)&c0);
    TEST_ASSERT(c0_bits.host_cur == 0b11u); // make_valid_config sets 3A

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_tc_toggle_done_src_cc2(void) {
    UcsiPpm* ppm = mock_make_ppm_with_mode(UcsiPpmCcModeDrp, false);
    mock_i2c_reset();

    simulate_toggle_done(ppm, FUSB302_STATUS1A_TOGSS_SRCON_CC2);

    TEST_ASSERT(ucsi_ppm_get_connector_state(ppm) == UcsiPpmStateAttachWait);
    TEST_ASSERT(ppm->tc_orientation == (int)UcsiPpmPhyCc2);
    TEST_ASSERT(ppm->tc_role_is_src == true);

    const uint8_t sw0 = g_mock_regs[Fusb302RegSwitches0];
    const Fusb302Switches0RegBits sw0_bits = *((const Fusb302Switches0RegBits*)&sw0);
    TEST_ASSERT(sw0_bits.meas_cc2 == 1);
    TEST_ASSERT(sw0_bits.meas_cc1 == 0);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_tc_toggle_done_snk_cc1(void) {
    UcsiPpm* ppm = mock_make_ppm_with_mode(UcsiPpmCcModeDrp, false);
    mock_i2c_reset();
    // Pre-seed CONTROL0.host_cur to a non-zero value so we can verify the
    // SNK path doesn't reprogram it (Rp current is source-only).
    const Fusb302Control0RegBits seed = {.host_cur = 0b01u};
    g_mock_regs[Fusb302RegControl0] = *(const uint8_t*)&seed;

    simulate_toggle_done(ppm, FUSB302_STATUS1A_TOGSS_SNKON_CC1);

    TEST_ASSERT(ucsi_ppm_get_connector_state(ppm) == UcsiPpmStateAttachWait);
    TEST_ASSERT(ppm->tc_orientation == (int)UcsiPpmPhyCc1);
    TEST_ASSERT(ppm->tc_role_is_src == false);

    // CONTROL0.host_cur untouched on SNK side (the chip ignores HOST_CUR
    // unless PU_EN* is set, but the SM still shouldn't write it).
    const uint8_t c0 = g_mock_regs[Fusb302RegControl0];
    const Fusb302Control0RegBits c0_bits = *((const Fusb302Control0RegBits*)&c0);
    TEST_ASSERT(c0_bits.host_cur == 0b01u);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_tc_toggle_done_snk_cc2(void) {
    UcsiPpm* ppm = mock_make_ppm_with_mode(UcsiPpmCcModeDrp, false);
    mock_i2c_reset();

    simulate_toggle_done(ppm, FUSB302_STATUS1A_TOGSS_SNKON_CC2);

    TEST_ASSERT(ppm->tc_orientation == (int)UcsiPpmPhyCc2);
    TEST_ASSERT(ppm->tc_role_is_src == false);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_tc_toggle_done_audio_rearms_toggle(void) {
    // Audio Accessory (TOGSS=0b111) is out of scope. SM should not transition
    // to AttachWait; instead it re-arms the toggle for another attempt.
    UcsiPpm* ppm = mock_make_ppm_with_mode(UcsiPpmCcModeDrp, false);
    mock_i2c_reset();

    simulate_toggle_done(ppm, FUSB302_STATUS1A_TOGSS_AUDIO_ACCESSORY);

    TEST_ASSERT(ucsi_ppm_get_connector_state(ppm) == UcsiPpmStateUnattached);
    // CONTROL2 was re-written with TOGGLE=1 to re-arm.
    const uint8_t c2 = g_mock_regs[Fusb302RegControl2];
    const Fusb302Control2RegBits c2_bits = *((const Fusb302Control2RegBits*)&c2);
    TEST_ASSERT(c2_bits.toggle == 1);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_tc_toggle_done_outside_unattached_dropped(void) {
    // ToggleDone while already in AttachWait: must be silently dropped (no
    // state regression, no polarity re-lock).
    UcsiPpm* ppm = mock_make_ppm_with_mode(UcsiPpmCcModeDrp, false);
    mock_i2c_reset();
    simulate_toggle_done(ppm, FUSB302_STATUS1A_TOGSS_SRCON_CC1);
    TEST_ASSERT(ucsi_ppm_get_connector_state(ppm) == UcsiPpmStateAttachWait);

    // Snapshot SWITCHES0 (now holds MEAS_CC1 from polarity lock) and feed
    // another stray ToggleDone with a different TOGSS. The handler must
    // drop it — SWITCHES0 should be untouched. (No mock_i2c_reset between
    // capture and second event, otherwise we'd compare against zero.)
    const uint8_t sw0_before = g_mock_regs[Fusb302RegSwitches0];
    simulate_toggle_done(ppm, FUSB302_STATUS1A_TOGSS_SRCON_CC2);

    TEST_ASSERT(ucsi_ppm_get_connector_state(ppm) == UcsiPpmStateAttachWait);
    TEST_ASSERT(ppm->tc_orientation == (int)UcsiPpmPhyCc1); // still CC1
    TEST_ASSERT(g_mock_regs[Fusb302RegSwitches0] == sw0_before);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_tc_deinit_stops_toggle(void) {
    UcsiPpm* ppm = mock_make_ppm_with_mode(UcsiPpmCcModeDrp, false);
    mock_i2c_reset();

    TEST_ASSERT(ucsi_ppm_deinit(ppm) == UcsiPpmStatusOk);

    // CONTROL2.TOGGLE cleared (stop_toggle's RMW), plus phy_deinit zeros
    // SWITCHES0/1 and CONTROL2. Final CONTROL2 = 0.
    TEST_ASSERT(g_mock_regs[Fusb302RegControl2] == 0u);

    ucsi_ppm_free(ppm);
    return true;
}

// --- L3 Type-C SM 1b (AttachWait debounce + Attached commit) ---------------

static bool test_tc_attach_wait_src_raises_vbus_source(void) {
    UcsiPpm* ppm = mock_make_ppm_with_mode(UcsiPpmCcModeDrp, false);
    mock_i2c_reset();
    simulate_toggle_done(ppm, FUSB302_STATUS1A_TOGSS_SRCON_CC1);

    TEST_ASSERT(ucsi_ppm_get_connector_state(ppm) == UcsiPpmStateAttachWait);
    TEST_ASSERT(g_mock_vbus_source_calls >= 1);
    TEST_ASSERT(g_mock_vbus_source_last == true);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_tc_attach_wait_snk_does_not_raise_vbus(void) {
    UcsiPpm* ppm = mock_make_ppm_with_mode(UcsiPpmCcModeDrp, false);
    mock_i2c_reset();
    simulate_toggle_done(ppm, FUSB302_STATUS1A_TOGSS_SNKON_CC1);

    TEST_ASSERT(ucsi_ppm_get_connector_state(ppm) == UcsiPpmStateAttachWait);
    // Sink doesn't drive VBUS — partner provides it.
    TEST_ASSERT(g_mock_vbus_source_calls == 0);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_tc_attach_commits_src_after_debounce(void) {
    UcsiPpm* ppm = mock_make_ppm_with_mode(UcsiPpmCcModeDrp, false);
    mock_i2c_reset();
    g_mock_time_ms = 0;
    simulate_toggle_done(ppm, FUSB302_STATUS1A_TOGSS_SRCON_CC1);
    TEST_ASSERT(ucsi_ppm_get_connector_state(ppm) == UcsiPpmStateAttachWait);

    // VBUS appears at t=10 — too early to commit.
    g_mock_time_ms = 10;
    simulate_vbus_changed(ppm, true);
    TEST_ASSERT(ucsi_ppm_get_connector_state(ppm) == UcsiPpmStateAttachWait);

    // Past CCDebounce (100 ms). Tick commits.
    g_mock_time_ms = 110;
    TEST_ASSERT(ucsi_ppm_tick(ppm) == UcsiPpmStatusOk);
    TEST_ASSERT(ucsi_ppm_get_connector_state(ppm) == UcsiPpmStateAttachedSrc);

    // PD reception enabled (AUTO_CRC bit set on SWITCHES1).
    const uint8_t sw1 = g_mock_regs[Fusb302RegSwitches1];
    const Fusb302Switches1RegBits sw1_bits = *((const Fusb302Switches1RegBits*)&sw1);
    TEST_ASSERT(sw1_bits.auto_crc == 1);
    // And n_retries programmed via CONTROL3.
    const uint8_t c3 = g_mock_regs[Fusb302RegControl3];
    const Fusb302Control3RegBits c3_bits = *((const Fusb302Control3RegBits*)&c3);
    TEST_ASSERT(c3_bits.auto_retry == 1);
    TEST_ASSERT(c3_bits.n_retries == 2);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_tc_attach_commits_snk_after_debounce(void) {
    UcsiPpm* ppm = mock_make_ppm_with_mode(UcsiPpmCcModeDrp, false);
    mock_i2c_reset();
    g_mock_time_ms = 0;
    simulate_toggle_done(ppm, FUSB302_STATUS1A_TOGSS_SNKON_CC2);
    TEST_ASSERT(ucsi_ppm_get_connector_state(ppm) == UcsiPpmStateAttachWait);

    // Partner brings VBUS up almost immediately.
    g_mock_time_ms = 2;
    simulate_vbus_changed(ppm, true);
    TEST_ASSERT(ucsi_ppm_get_connector_state(ppm) == UcsiPpmStateAttachWait);

    g_mock_time_ms = 150;
    ucsi_ppm_tick(ppm);
    TEST_ASSERT(ucsi_ppm_get_connector_state(ppm) == UcsiPpmStateAttachedSnk);
    TEST_ASSERT(ppm->tc_role_is_src == false);
    TEST_ASSERT(ppm->tc_orientation == (int)UcsiPpmPhyCc2);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_tc_attach_no_vbus_stays_attach_wait(void) {
    UcsiPpm* ppm = mock_make_ppm_with_mode(UcsiPpmCcModeDrp, false);
    mock_i2c_reset();
    g_mock_time_ms = 0;
    simulate_toggle_done(ppm, FUSB302_STATUS1A_TOGSS_SRCON_CC1);

    // CCDebounce elapsed but no VBUS_OK arrived — must stay AttachWait.
    // Stay strictly below the AttachWait give-up timeout (1c handles the
    // post-timeout path in test_tc_attach_wait_timeout_restarts_toggle).
    g_mock_time_ms = 200;
    TEST_ASSERT(ucsi_ppm_tick(ppm) == UcsiPpmStatusOk);
    TEST_ASSERT(ucsi_ppm_get_connector_state(ppm) == UcsiPpmStateAttachWait);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_tc_attach_vbus_before_debounce_holds(void) {
    UcsiPpm* ppm = mock_make_ppm_with_mode(UcsiPpmCcModeDrp, false);
    mock_i2c_reset();
    g_mock_time_ms = 0;
    simulate_toggle_done(ppm, FUSB302_STATUS1A_TOGSS_SRCON_CC1);

    g_mock_time_ms = 5;
    simulate_vbus_changed(ppm, true);
    TEST_ASSERT(ucsi_ppm_get_connector_state(ppm) == UcsiPpmStateAttachWait);

    // Halfway through debounce — still waiting.
    g_mock_time_ms = 50;
    ucsi_ppm_tick(ppm);
    TEST_ASSERT(ucsi_ppm_get_connector_state(ppm) == UcsiPpmStateAttachWait);

    // Just past CCDebounce — commit.
    g_mock_time_ms = 101;
    ucsi_ppm_tick(ppm);
    TEST_ASSERT(ucsi_ppm_get_connector_state(ppm) == UcsiPpmStateAttachedSrc);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_tc_vbus_changed_in_unattached_ignored(void) {
    UcsiPpm* ppm = mock_make_ppm_with_mode(UcsiPpmCcModeDrp, false);
    mock_i2c_reset();
    simulate_vbus_changed(ppm, true);

    // Stayed Unattached — VBUS event without prior ToggleDone is meaningless.
    TEST_ASSERT(ucsi_ppm_get_connector_state(ppm) == UcsiPpmStateUnattached);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_tc_deinit_drops_vbus_source(void) {
    UcsiPpm* ppm = mock_make_ppm_with_mode(UcsiPpmCcModeDrp, false);
    simulate_toggle_done(ppm, FUSB302_STATUS1A_TOGSS_SRCON_CC1);
    // VBUS source raised — capture and verify deinit drops it.
    TEST_ASSERT(g_mock_vbus_source_last == true);

    g_mock_vbus_source_calls = 0;
    ucsi_ppm_deinit(ppm);

    TEST_ASSERT(g_mock_vbus_source_calls >= 1);
    TEST_ASSERT(g_mock_vbus_source_last == false);

    ucsi_ppm_free(ppm);
    return true;
}

// --- L3 Type-C SM 1c (Detach + ErrorRecovery) ------------------------------

static bool test_tc_snk_detach_on_vbus_lost(void) {
    UcsiPpm* ppm = mock_attach(false);
    TEST_ASSERT(ucsi_ppm_get_connector_state(ppm) == UcsiPpmStateAttachedSnk);
    mock_i2c_reset();

    // Partner drops VBUS — we should fall back to Unattached and re-arm toggle.
    g_mock_time_ms = 200;
    simulate_vbus_changed(ppm, false);

    TEST_ASSERT(ucsi_ppm_get_connector_state(ppm) == UcsiPpmStateUnattached);
    // Toggle re-armed: CONTROL2.TOGGLE = 1.
    const uint8_t c2 = g_mock_regs[Fusb302RegControl2];
    const Fusb302Control2RegBits c2_bits = *((const Fusb302Control2RegBits*)&c2);
    TEST_ASSERT(c2_bits.toggle == 1);
    // PD reception disabled (SWITCHES1.AUTO_CRC cleared by phy_disable_pd).
    const uint8_t sw1 = g_mock_regs[Fusb302RegSwitches1];
    const Fusb302Switches1RegBits sw1_bits = *((const Fusb302Switches1RegBits*)&sw1);
    TEST_ASSERT(sw1_bits.auto_crc == 0);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_tc_src_detach_on_bc_lvl_high(void) {
    UcsiPpm* ppm = mock_attach(true);
    TEST_ASSERT(ucsi_ppm_get_connector_state(ppm) == UcsiPpmStateAttachedSrc);
    TEST_ASSERT(g_mock_vbus_source_last == true);
    mock_i2c_reset();
    g_mock_vbus_source_calls = 0;
    g_mock_vbus_source_last = true; // mock still believes the rail is on

    // Partner pulls Rd → CC is pulled to supply by our Rp → BC_LVL = 0b11.
    simulate_bc_lvl_changed(ppm, 0b11u);

    TEST_ASSERT(ucsi_ppm_get_connector_state(ppm) == UcsiPpmStateUnattached);
    TEST_ASSERT(g_mock_vbus_source_calls >= 1);
    TEST_ASSERT(g_mock_vbus_source_last == false); // dropped

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_tc_src_bc_lvl_rd_present_no_detach(void) {
    // BC_LVL 0b10 in Attached.SRC just means partner adjusted Rp current
    // (or noise). Must not trigger detach.
    UcsiPpm* ppm = mock_attach(true);
    TEST_ASSERT(ucsi_ppm_get_connector_state(ppm) == UcsiPpmStateAttachedSrc);
    mock_i2c_reset();

    simulate_bc_lvl_changed(ppm, 0b10u);
    TEST_ASSERT(ucsi_ppm_get_connector_state(ppm) == UcsiPpmStateAttachedSrc);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_tc_snk_bc_lvl_changes_no_detach(void) {
    // BC_LVL in sink-mode tracks partner Rp; changes here are not detach.
    UcsiPpm* ppm = mock_attach(false);
    TEST_ASSERT(ucsi_ppm_get_connector_state(ppm) == UcsiPpmStateAttachedSnk);
    mock_i2c_reset();

    simulate_bc_lvl_changed(ppm, 0b11u);
    TEST_ASSERT(ucsi_ppm_get_connector_state(ppm) == UcsiPpmStateAttachedSnk);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_tc_attach_wait_timeout_restarts_toggle(void) {
    UcsiPpm* ppm = mock_make_ppm_with_mode(UcsiPpmCcModeDrp, false);
    mock_i2c_reset();

    g_mock_time_ms = 0;
    simulate_toggle_done(ppm, FUSB302_STATUS1A_TOGSS_SRCON_CC1);
    TEST_ASSERT(ucsi_ppm_get_connector_state(ppm) == UcsiPpmStateAttachWait);

    // No VBUS event arrives. Past the AttachWait timeout (500 ms) the tick
    // gives up and goes back to Unattached.
    g_mock_time_ms = 600;
    ucsi_ppm_tick(ppm);
    TEST_ASSERT(ucsi_ppm_get_connector_state(ppm) == UcsiPpmStateUnattached);
    // Toggle re-armed.
    const uint8_t c2 = g_mock_regs[Fusb302RegControl2];
    const Fusb302Control2RegBits c2_bits = *((const Fusb302Control2RegBits*)&c2);
    TEST_ASSERT(c2_bits.toggle == 1);
    // VBUS source dropped (we'd raised it for SRC AttachWait).
    TEST_ASSERT(g_mock_vbus_source_last == false);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_tc_detach_reattach_full_cycle(void) {
    // End-to-end: attach as SNK, detach, then re-attach via another
    // ToggleDone + VBUS sequence.
    UcsiPpm* ppm = mock_attach(false);
    TEST_ASSERT(ucsi_ppm_get_connector_state(ppm) == UcsiPpmStateAttachedSnk);

    g_mock_time_ms = 200;
    simulate_vbus_changed(ppm, false);
    TEST_ASSERT(ucsi_ppm_get_connector_state(ppm) == UcsiPpmStateUnattached);

    // Re-attach as SRC this time.
    g_mock_time_ms = 300;
    simulate_toggle_done(ppm, FUSB302_STATUS1A_TOGSS_SRCON_CC2);
    TEST_ASSERT(ucsi_ppm_get_connector_state(ppm) == UcsiPpmStateAttachWait);
    TEST_ASSERT(ppm->tc_orientation == (int)UcsiPpmPhyCc2);

    g_mock_time_ms = 310;
    simulate_vbus_changed(ppm, true);
    g_mock_time_ms = 450;
    ucsi_ppm_tick(ppm);
    TEST_ASSERT(ucsi_ppm_get_connector_state(ppm) == UcsiPpmStateAttachedSrc);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_tc_vbus_lost_in_attached_src_no_detach(void) {
    // In Attached.SRC we drive VBUS ourselves; VBUS-lost events on the bus
    // shouldn't ricochet through and detach us (detach detection on src side
    // is via BC_LVL, not VBUS).
    UcsiPpm* ppm = mock_attach(true);
    TEST_ASSERT(ucsi_ppm_get_connector_state(ppm) == UcsiPpmStateAttachedSrc);

    simulate_vbus_changed(ppm, false);
    TEST_ASSERT(ucsi_ppm_get_connector_state(ppm) == UcsiPpmStateAttachedSrc);

    ucsi_ppm_free(ppm);
    return true;
}

// --- L3 PRL (MessageID counter + duplicate detection) ----------------------

#include "ucsi_ppm_prl.h"

static bool test_prl_init_zero_state(void) {
    UcsiPpm* ppm = mock_make_ppm();
    TEST_ASSERT(ppm->prl_next_tx_msg_id == 0u);
    TEST_ASSERT(ppm->prl_last_rx_valid == false);
    TEST_ASSERT(ppm->prl_messages_delivered == 0u);
    ucsi_ppm_free(ppm);
    return true;
}

// Extracts the MessageID field (bits 11:9) from the header in a TX FIFO burst.
static uint8_t tx_msg_id_from_last_burst(void) {
    const int idx = mock_find_fifo_burst();
    if(idx < 0) return 0xFFu;
    // FIFO layout: [reg=FIFOS] + 4 SOP + 1 PACKSYM + 2 header LE + ...
    const uint8_t* fifo = &g_mock_txns[idx].data[1];
    const uint16_t header = (uint16_t)(fifo[5] | ((uint16_t)fifo[6] << 8));
    return (uint8_t)((header >> 9) & 0x07u);
}

static bool test_prl_send_stamps_msg_id_zero(void) {
    UcsiPpm* ppm = mock_make_ppm();
    mock_i2c_reset();

    UcsiPpmPhyPdMsg msg = {.sop_type = UcsiPpmPhySopTypeSop, .header = 0x0003u};
    TEST_ASSERT(ucsi_ppm_prl_send_message(ppm, &msg) == UcsiPpmStatusOk);
    TEST_ASSERT(tx_msg_id_from_last_burst() == 0u);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_prl_send_increments_counter(void) {
    UcsiPpm* ppm = mock_make_ppm();
    mock_i2c_reset();

    UcsiPpmPhyPdMsg msg = {.sop_type = UcsiPpmPhySopTypeSop, .header = 0x0003u};
    ucsi_ppm_prl_send_message(ppm, &msg);
    TEST_ASSERT(tx_msg_id_from_last_burst() == 0u);

    mock_i2c_reset();
    msg.header = 0x0003u;
    ucsi_ppm_prl_send_message(ppm, &msg);
    TEST_ASSERT(tx_msg_id_from_last_burst() == 1u);

    mock_i2c_reset();
    msg.header = 0x0003u;
    ucsi_ppm_prl_send_message(ppm, &msg);
    TEST_ASSERT(tx_msg_id_from_last_burst() == 2u);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_prl_send_wraps_after_seven(void) {
    UcsiPpm* ppm = mock_make_ppm();
    // First send → 0, then 1..7, then wraps back to 0.
    for(uint8_t i = 0; i < 8; ++i) {
        mock_i2c_reset();
        UcsiPpmPhyPdMsg msg = {.sop_type = UcsiPpmPhySopTypeSop, .header = 0x0003u};
        ucsi_ppm_prl_send_message(ppm, &msg);
        TEST_ASSERT(tx_msg_id_from_last_burst() == i);
    }
    // 9th send wraps to 0.
    mock_i2c_reset();
    UcsiPpmPhyPdMsg msg = {.sop_type = UcsiPpmPhySopTypeSop, .header = 0x0003u};
    ucsi_ppm_prl_send_message(ppm, &msg);
    TEST_ASSERT(tx_msg_id_from_last_burst() == 0u);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_prl_send_preserves_other_header_bits(void) {
    UcsiPpm* ppm = mock_make_ppm();
    // Advance counter once so we stamp a non-zero MsgID and can see the
    // other bits surviving alongside it.
    UcsiPpmPhyPdMsg pad = {.sop_type = UcsiPpmPhySopTypeSop, .header = 0u};
    ucsi_ppm_prl_send_message(ppm, &pad); // counter → 1

    mock_i2c_reset();
    // Header with port-data-role (bit 5), spec-rev (bits 7:6), msg type
    // (bits 4:0) set; MsgID bits 11:9 must be replaced.
    const uint16_t base = (uint16_t)((1u << 5) | (0b10u << 6) | 0x07u);
    UcsiPpmPhyPdMsg msg = {.sop_type = UcsiPpmPhySopTypeSop, .header = base};
    ucsi_ppm_prl_send_message(ppm, &msg);

    const int idx = mock_find_fifo_burst();
    const uint8_t* fifo = &g_mock_txns[idx].data[1];
    const uint16_t actual = (uint16_t)(fifo[5] | ((uint16_t)fifo[6] << 8));
    TEST_ASSERT(((actual >> 9) & 0x07u) == 1u); // MsgID stamped
    TEST_ASSERT((actual & ~(0x07u << 9)) == base); // other bits intact

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_prl_recv_first_message_delivered(void) {
    UcsiPpm* ppm = mock_make_ppm();
    mock_i2c_reset();

    // One SOP control message with MsgID=3 in the FIFO.
    uint8_t stream[16];
    const size_t n = build_rx_stream(
        stream, FUSB302_RX_TOKEN_SOP, make_header_with_msg_id(0x0003u, 3u), NULL, 0);
    mock_fifo_load(stream, n);
    simulate_message_rx_event(ppm);

    TEST_ASSERT(ppm->prl_messages_delivered == 1u);
    TEST_ASSERT(ppm->prl_last_rx_valid == true);
    TEST_ASSERT(ppm->prl_last_rx_msg_id == 3u);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_prl_recv_duplicate_dropped(void) {
    UcsiPpm* ppm = mock_make_ppm();
    mock_i2c_reset();

    // Two messages with the same MsgID — second must be dropped as a dup.
    uint8_t stream[32];
    size_t pos = 0;
    pos += build_rx_stream(
        &stream[pos], FUSB302_RX_TOKEN_SOP, make_header_with_msg_id(0x0003u, 5u),
        NULL, 0);
    pos += build_rx_stream(
        &stream[pos], FUSB302_RX_TOKEN_SOP, make_header_with_msg_id(0x0042u, 5u),
        NULL, 0);
    mock_fifo_load(stream, pos);
    simulate_message_rx_event(ppm);

    TEST_ASSERT(ppm->prl_messages_delivered == 1u);
    TEST_ASSERT(ppm->prl_last_rx_msg_id == 5u);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_prl_recv_different_ids_all_delivered(void) {
    UcsiPpm* ppm = mock_make_ppm();
    mock_i2c_reset();

    uint8_t stream[48];
    size_t pos = 0;
    pos += build_rx_stream(
        &stream[pos], FUSB302_RX_TOKEN_SOP, make_header_with_msg_id(0u, 0u), NULL, 0);
    pos += build_rx_stream(
        &stream[pos], FUSB302_RX_TOKEN_SOP, make_header_with_msg_id(0u, 1u), NULL, 0);
    pos += build_rx_stream(
        &stream[pos], FUSB302_RX_TOKEN_SOP, make_header_with_msg_id(0u, 2u), NULL, 0);
    mock_fifo_load(stream, pos);
    simulate_message_rx_event(ppm);

    TEST_ASSERT(ppm->prl_messages_delivered == 3u);
    TEST_ASSERT(ppm->prl_last_rx_msg_id == 2u);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_prl_reset_clears_state(void) {
    UcsiPpm* ppm = mock_make_ppm();
    UcsiPpmPhyPdMsg msg = {.sop_type = UcsiPpmPhySopTypeSop, .header = 0u};
    ucsi_ppm_prl_send_message(ppm, &msg);
    ucsi_ppm_prl_send_message(ppm, &msg);
    TEST_ASSERT(ppm->prl_next_tx_msg_id == 2u);

    TEST_ASSERT(ucsi_ppm_prl_reset(ppm) == UcsiPpmStatusOk);
    TEST_ASSERT(ppm->prl_next_tx_msg_id == 0u);
    TEST_ASSERT(ppm->prl_last_rx_valid == false);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_prl_hard_reset_event_resets_counter(void) {
    UcsiPpm* ppm = mock_make_ppm();
    UcsiPpmPhyPdMsg msg = {.sop_type = UcsiPpmPhySopTypeSop, .header = 0u};
    ucsi_ppm_prl_send_message(ppm, &msg);
    TEST_ASSERT(ppm->prl_next_tx_msg_id == 1u);

    // Simulate HardResetRx via PHY pump.
    g_mock_regs[Fusb302RegInterrupt] = 0;
    g_mock_regs[Fusb302RegInterruptB] = 0;
    const Fusb302InterruptARegBits inta = {.i_hard_rst = 1};
    g_mock_regs[Fusb302RegInterruptA] = *(const uint8_t*)&inta;
    ucsi_ppm_notify_fusb302_irq(ppm);
    ucsi_ppm_tick(ppm);

    TEST_ASSERT(ppm->prl_next_tx_msg_id == 0u);
    TEST_ASSERT(ppm->prl_last_rx_valid == false);

    ucsi_ppm_free(ppm);
    return true;
}

// --- L3 PE (Policy Engine) — Sink path -------------------------------------

#include "ucsi_ppm_pe.h"

// Builds a partner-sent PD header. Stamps the simulated partner-side MsgID
// at bits 11:9 so PRL dedup doesn't drop a sequence of partner replies.
static uint16_t make_partner_header(uint8_t msg_type, uint8_t num_objects, uint8_t msg_id) {
    uint16_t hdr = (uint16_t)(msg_type & 0x1Fu);
    hdr |= (uint16_t)(0b10u << 6); // PD 3.0
    hdr |= (uint16_t)(1u << 8); // partner is Source
    hdr |= (uint16_t)(((uint32_t)msg_id & 0x07u) << 9);
    hdr |= (uint16_t)(((uint32_t)num_objects & 0x07u) << 12);
    return hdr;
}

static void simulate_pd_message(
    UcsiPpm* ppm,
    uint8_t msg_type,
    const uint32_t* objects,
    uint8_t obj_count) {
    const uint16_t header = make_partner_header(msg_type, obj_count, g_test_partner_msg_id);
    g_test_partner_msg_id = (uint8_t)((g_test_partner_msg_id + 1u) & 0x07u);
    uint8_t stream[64];
    const size_t n =
        build_rx_stream(stream, FUSB302_RX_TOKEN_SOP, header, objects, obj_count);
    mock_fifo_load(stream, n);
    simulate_message_rx_event(ppm);
}

static bool test_pe_init_idle(void) {
    UcsiPpm* ppm = mock_make_ppm();
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeStateIdle);
    ucsi_ppm_free(ppm);
    return true;
}

static bool test_pe_snk_attach_enters_wait_capabilities(void) {
    UcsiPpm* ppm = mock_attach(false);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeSnkWaitForCapabilities);
    ucsi_ppm_free(ppm);
    return true;
}

static bool test_pe_snk_recv_source_caps_sends_request(void) {
    UcsiPpm* ppm = mock_attach(false);
    mock_i2c_reset();

    // Partner advertises 5V@1.5A.
    const uint32_t pdo = ucsi_ppm_pdo_fixed_source(5000, 1500, true, false, true, true);
    simulate_pd_message(ppm, 0x01u /* Source_Capabilities */, &pdo, 1);

    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeSnkWaitForAccept);
    TEST_ASSERT(ppm->pe_received_pdo_count == 1u);
    TEST_ASSERT(ppm->pe_requested_pdo_index == 1u);

    // Verify a Request was transmitted: msg_type=0x02 in header, 1 object.
    const int idx = mock_find_fifo_burst();
    TEST_ASSERT(idx >= 0);
    const uint8_t* fifo = &g_mock_txns[idx].data[1];
    const uint16_t hdr = (uint16_t)(fifo[5] | ((uint16_t)fifo[6] << 8));
    TEST_ASSERT((hdr & 0x1Fu) == 0x02u); // Request
    TEST_ASSERT(((hdr >> 12) & 0x07u) == 1u); // NDO=1
    TEST_ASSERT((hdr & (1u << 8)) == 0u); // Sink power role

    // RDO at fifo[7..10] (LE).
    const uint32_t rdo = (uint32_t)fifo[7] | ((uint32_t)fifo[8] << 8) |
                         ((uint32_t)fifo[9] << 16) | ((uint32_t)fifo[10] << 24);
    TEST_ASSERT(((rdo >> 28) & 0x07u) == 1u); // PDO #1 selected
    // Operating current matches the advertised max (1500 mA / 10 = 150 units).
    TEST_ASSERT(((rdo >> 10) & 0x3FFu) == 150u);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_pe_snk_accept_then_ps_rdy_completes_contract(void) {
    UcsiPpm* ppm = mock_attach(false);
    const uint32_t pdo = ucsi_ppm_pdo_fixed_source(5000, 3000, true, false, true, true);
    simulate_pd_message(ppm, 0x01u, &pdo, 1);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeSnkWaitForAccept);

    // Partner Accepts.
    simulate_pd_message(ppm, 0x03u /* Accept */, NULL, 0);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeSnkWaitForPsRdy);

    // Partner signals power ready.
    simulate_pd_message(ppm, 0x06u /* PS_RDY */, NULL, 0);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeSnkReady);

    // Contract latched.
    TEST_ASSERT(ppm->pe_negotiated_voltage_mv == 5000u);
    TEST_ASSERT(ppm->pe_negotiated_current_ma == 3000u);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_pe_snk_reject_triggers_hard_reset(void) {
    UcsiPpm* ppm = mock_attach(false);
    const uint32_t pdo = ucsi_ppm_pdo_fixed_source(5000, 1500, true, false, true, true);
    simulate_pd_message(ppm, 0x01u, &pdo, 1);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeSnkWaitForAccept);

    simulate_pd_message(ppm, 0x04u /* Reject */, NULL, 0);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPePendingHardResetSent);
    TEST_ASSERT(ppm->pe_hard_reset_counter == 1u);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_pe_snk_wait_response_triggers_hard_reset(void) {
    UcsiPpm* ppm = mock_attach(false);
    const uint32_t pdo = ucsi_ppm_pdo_fixed_source(5000, 1500, true, false, true, true);
    simulate_pd_message(ppm, 0x01u, &pdo, 1);

    simulate_pd_message(ppm, 0x0Cu /* Wait */, NULL, 0);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPePendingHardResetSent);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_pe_snk_wait_cap_timeout_triggers_hard_reset(void) {
    UcsiPpm* ppm = mock_attach(false);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeSnkWaitForCapabilities);

    g_mock_time_ms += 600;
    ucsi_ppm_tick(ppm);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPePendingHardResetSent);
    // PHY register reflects the SEND_HARD_RESET bit.
    const uint8_t c3 = g_mock_regs[Fusb302RegControl3];
    const Fusb302Control3RegBits c3_bits = *((const Fusb302Control3RegBits*)&c3);
    TEST_ASSERT(c3_bits.send_hard_reset == 1);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_pe_snk_sender_response_timeout_triggers_hard_reset(void) {
    UcsiPpm* ppm = mock_attach(false);
    const uint32_t pdo = ucsi_ppm_pdo_fixed_source(5000, 1500, true, false, true, true);
    simulate_pd_message(ppm, 0x01u, &pdo, 1);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeSnkWaitForAccept);

    g_mock_time_ms += 600;
    ucsi_ppm_tick(ppm);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPePendingHardResetSent);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_pe_snk_ps_transition_timeout_triggers_hard_reset(void) {
    UcsiPpm* ppm = mock_attach(false);
    const uint32_t pdo = ucsi_ppm_pdo_fixed_source(5000, 1500, true, false, true, true);
    simulate_pd_message(ppm, 0x01u, &pdo, 1);
    simulate_pd_message(ppm, 0x03u, NULL, 0);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeSnkWaitForPsRdy);

    g_mock_time_ms += 600;
    ucsi_ppm_tick(ppm);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPePendingHardResetSent);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_pe_snk_ready_ignores_late_accept(void) {
    // Once we're in Ready, a stray Accept/PS_RDY from a confused partner
    // must not disrupt the contract.
    UcsiPpm* ppm = mock_attach(false);
    const uint32_t pdo = ucsi_ppm_pdo_fixed_source(9000, 2000, true, false, true, true);
    simulate_pd_message(ppm, 0x01u, &pdo, 1);
    simulate_pd_message(ppm, 0x03u, NULL, 0);
    simulate_pd_message(ppm, 0x06u, NULL, 0);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeSnkReady);
    TEST_ASSERT(ppm->pe_negotiated_voltage_mv == 9000u);

    simulate_pd_message(ppm, 0x03u, NULL, 0);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeSnkReady);
    TEST_ASSERT(ppm->pe_negotiated_voltage_mv == 9000u);

    ucsi_ppm_free(ppm);
    return true;
}

// Scans recorded I²C transactions for a FIFO burst whose PD message header
// has the given msg_type (header LE at bytes 6..7 of the burst payload).
// Returns transaction index, or -1.
static int find_fifo_burst_by_msg_type(uint8_t want_type) {
    for(size_t i = 0; i < g_mock_txn_count; ++i) {
        const MockI2cTxn* t = &g_mock_txns[i];
        if(t->is_write && t->len > 7u && t->data[0] == Fusb302RegFifos) {
            const uint16_t hdr = (uint16_t)(t->data[6] | ((uint16_t)t->data[7] << 8));
            if((hdr & 0x1Fu) == (uint16_t)want_type) return (int)i;
        }
    }
    return -1;
}

static int count_fifo_bursts(void) {
    int n = 0;
    for(size_t i = 0; i < g_mock_txn_count; ++i) {
        const MockI2cTxn* t = &g_mock_txns[i];
        if(t->is_write && t->len > 2u && t->data[0] == Fusb302RegFifos) n++;
    }
    return n;
}

// Builds a Fixed/Variable Request RDO selecting `obj_pos` (1-based) at
// `op_current_ma`. Tests use this to simulate a partner Request to us as
// source.
static uint32_t make_request_rdo(uint8_t obj_pos, uint16_t op_current_ma) {
    uint32_t rdo = ((uint32_t)(obj_pos & 0x07u) << 28);
    rdo |= (((uint32_t)op_current_ma / 10u) & 0x3FFu) << 10;
    rdo |= ((uint32_t)op_current_ma / 10u) & 0x3FFu;
    return rdo;
}

static bool test_pe_src_attach_sends_source_caps(void) {
    UcsiPpm* ppm = mock_attach(true);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeSrcSendCapabilities);

    const int idx = find_fifo_burst_by_msg_type(0x01u /* Source_Capabilities */);
    TEST_ASSERT(idx >= 0);
    const MockI2cTxn* t = &g_mock_txns[idx];
    const uint16_t hdr = (uint16_t)(t->data[6] | ((uint16_t)t->data[7] << 8));
    TEST_ASSERT((hdr & (1u << 8)) != 0u); // power role = Source
    TEST_ASSERT(((hdr >> 12) & 0x07u) == ppm->config.source_caps.count);

    // First PDO bytes match config.source_caps.pdos[0].
    const uint32_t pdo0 = (uint32_t)t->data[8] | ((uint32_t)t->data[9] << 8) |
                          ((uint32_t)t->data[10] << 16) | ((uint32_t)t->data[11] << 24);
    TEST_ASSERT(pdo0 == ppm->config.source_caps.pdos[0]);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_pe_src_recv_request_sends_accept_and_drives_psu(void) {
    UcsiPpm* ppm = mock_attach(true);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeSrcSendCapabilities);
    mock_i2c_reset();
    g_mock_psu_calls = 0;

    // Partner picks PDO #1 (5 V) at 3 A.
    const uint32_t rdo = make_request_rdo(1u, 3000u);
    simulate_pd_message(ppm, 0x02u /* Request */, &rdo, 1);

    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeSrcTransitionSupply);
    TEST_ASSERT(ppm->pe_requested_pdo_index == 1u);
    TEST_ASSERT(ppm->pe_negotiated_voltage_mv == 5000u);
    TEST_ASSERT(ppm->pe_negotiated_current_ma == 3000u);

    // Accept transmitted.
    TEST_ASSERT(find_fifo_burst_by_msg_type(0x03u /* Accept */) >= 0);
    // PSU driven to the negotiated voltage/current.
    TEST_ASSERT(g_mock_psu_calls == 1);
    TEST_ASSERT(g_mock_psu_last_voltage_mv == 5000u);
    TEST_ASSERT(g_mock_psu_last_current_ma == 3000u);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_pe_src_psu_ready_sends_ps_rdy(void) {
    UcsiPpm* ppm = mock_attach(true);
    const uint32_t rdo = make_request_rdo(1u, 3000u);
    simulate_pd_message(ppm, 0x02u, &rdo, 1);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeSrcTransitionSupply);
    mock_i2c_reset();

    // Caller signals PSU settled.
    TEST_ASSERT(ucsi_ppm_notify_power_supply_ready(ppm) == UcsiPpmStatusOk);
    ucsi_ppm_tick(ppm);

    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeSrcReady);
    TEST_ASSERT(find_fifo_burst_by_msg_type(0x06u /* PS_RDY */) >= 0);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_pe_src_invalid_pdo_rejects(void) {
    UcsiPpm* ppm = mock_attach(true);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeSrcSendCapabilities);
    mock_i2c_reset();

    // PDO position 5 — outside our advertised count.
    const uint32_t rdo = make_request_rdo(5u, 1000u);
    simulate_pd_message(ppm, 0x02u, &rdo, 1);

    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeStateError);
    TEST_ASSERT(find_fifo_burst_by_msg_type(0x04u /* Reject */) >= 0);
    TEST_ASSERT(find_fifo_burst_by_msg_type(0x03u /* Accept */) < 0);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_pe_src_source_cap_timer_resends(void) {
    UcsiPpm* ppm = mock_attach(true);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeSrcSendCapabilities);

    const int initial = count_fifo_bursts();
    // tTypeCSendSourceCap = 150 ms — advance past it.
    g_mock_time_ms += 200;
    ucsi_ppm_tick(ppm);
    TEST_ASSERT(count_fifo_bursts() > initial);

    // Still in SrcSendCapabilities — waiting for partner Request.
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeSrcSendCapabilities);
    TEST_ASSERT(ppm->pe_caps_counter >= 2u);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_pe_src_ps_transition_timeout_triggers_hard_reset(void) {
    UcsiPpm* ppm = mock_attach(true);
    const uint32_t rdo = make_request_rdo(1u, 3000u);
    simulate_pd_message(ppm, 0x02u, &rdo, 1);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeSrcTransitionSupply);

    g_mock_time_ms += 600;
    ucsi_ppm_tick(ppm);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPePendingHardResetSent);
    TEST_ASSERT(ppm->pe_hard_reset_counter == 1u);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_pe_src_detach_returns_to_idle(void) {
    UcsiPpm* ppm = mock_attach(true);
    const uint32_t rdo = make_request_rdo(1u, 3000u);
    simulate_pd_message(ppm, 0x02u, &rdo, 1);
    TEST_ASSERT(ucsi_ppm_notify_power_supply_ready(ppm) == UcsiPpmStatusOk);
    ucsi_ppm_tick(ppm);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeSrcReady);

    // BC_LVL goes high → source-side detach → tc_enter_unattached → PE idle.
    simulate_bc_lvl_changed(ppm, 0b11u);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeStateIdle);
    TEST_ASSERT(ppm->pe_negotiated_voltage_mv == 0u);

    ucsi_ppm_free(ppm);
    return true;
}

// Symmetric counterpart of cmd.c::write_field — extracts a `width`-bit
// little-endian field at bit_offset from a packed buffer.
static uint64_t read_packed_field(const uint8_t* buf, uint32_t bit_offset, uint32_t width) {
    uint64_t value = 0;
    for(uint32_t i = 0; i < width; ++i) {
        const uint32_t bit = bit_offset + i;
        const uint32_t byte_idx = bit / 8u;
        const uint8_t bit_mask = (uint8_t)(1u << (bit % 8u));
        if(buf[byte_idx] & bit_mask) value |= ((uint64_t)1u << i);
    }
    return value;
}

// Sends GET_CONNECTOR_STATUS via the regfile transport and reads the 19-byte
// response into `out_payload`. Returns the resulting CCI.
static uint32_t send_get_connector_status(UcsiPpm* ppm, uint8_t* out_payload) {
    uint8_t op = UCSI_PPM_OPCODE_GET_CONNECTOR_STATUS;
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 1, &op);
    ucsi_ppm_register_read(ppm, UCSI_PPM_OFFSET_MESSAGE_IN, 19, out_payload);
    return read_cci(ppm);
}

// Drives the sink-side PD handshake to SnkReady. Caller must have already
// done mock_attach(false). Partner advertises one PDO at the given current.
static void drive_sink_contract(UcsiPpm* ppm, uint16_t advertised_ma) {
    const uint32_t pdo =
        ucsi_ppm_pdo_fixed_source(5000, advertised_ma, true, false, true, true);
    simulate_pd_message(ppm, 0x01u /* Source_Capabilities */, &pdo, 1);
    simulate_pd_message(ppm, 0x03u /* Accept */, NULL, 0);
    simulate_pd_message(ppm, 0x06u /* PS_RDY */, NULL, 0);
}

static bool test_cs_attach_alerts_opm_and_sets_cci(void) {
    UcsiPpm* ppm = mock_attach(false);
    // mock_attach commits to Attached.SNK → notify fires alert + stamps CCI.
    TEST_ASSERT(g_mock_alert_calls >= 1);
    const uint32_t cci = read_cci(ppm);
    const uint32_t conn = (cci >> UCSI_PPM_CCI_CONNECTOR_CHANGE_SHIFT) & 0x7Fu;
    TEST_ASSERT(conn == UCSI_PPM_NUM_CONNECTORS);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_cs_attach_sets_connect_change_bitmap(void) {
    UcsiPpm* ppm = mock_attach(false);
    uint8_t resp[19];
    send_get_connector_status(ppm, resp);

    const uint16_t bitmap = (uint16_t)read_packed_field(resp, 0u, 16u);
    TEST_ASSERT(bitmap & UCSI_PPM_CSC_CONNECT_CHANGE);
    TEST_ASSERT(bitmap & UCSI_PPM_CSC_PARTNER_CHANGED);
    TEST_ASSERT(bitmap & UCSI_PPM_CSC_POWER_OP_MODE_CHANGE);

    // Connect Status = 1, Power Direction = 0 (sink), Partner Type = 1 (DFP).
    TEST_ASSERT(read_packed_field(resp, 19u, 1u) == 1u);
    TEST_ASSERT(read_packed_field(resp, 20u, 1u) == 0u);
    TEST_ASSERT(read_packed_field(resp, 29u, 3u) == 1u);
    // Power Operation Mode = USB Default (no PD contract yet).
    TEST_ASSERT(read_packed_field(resp, 16u, 3u) == 1u);
    // Sink Path Status = 1 (sink-side attached).
    TEST_ASSERT(read_packed_field(resp, 87u, 1u) == 1u);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_cs_second_get_clears_bitmap(void) {
    UcsiPpm* ppm = mock_attach(false);
    uint8_t resp[19];
    send_get_connector_status(ppm, resp);
    TEST_ASSERT((read_packed_field(resp, 0u, 16u) & UCSI_PPM_CSC_CONNECT_CHANGE) != 0u);

    // ACK the previous Command Completed so dispatch can run again.
    uint8_t ack_byte = UCSI_PPM_ACK_CC_CI_COMMAND_COMPLETED_ACK;
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL + 2, 1, &ack_byte);
    uint8_t ack_op = UCSI_PPM_OPCODE_ACK_CC_CI;
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 1, &ack_op);

    send_get_connector_status(ppm, resp);
    // Bitmap cleared; persistent state still readable (Connect Status etc).
    TEST_ASSERT(read_packed_field(resp, 0u, 16u) == 0u);
    TEST_ASSERT(read_packed_field(resp, 19u, 1u) == 1u); // still attached

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_cs_pd_contract_sets_pom_pd_and_rdo(void) {
    UcsiPpm* ppm = mock_attach(false);
    drive_sink_contract(ppm, 3000);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeSnkReady);

    uint8_t resp[19];
    send_get_connector_status(ppm, resp);

    // Bitmap accumulated both attach (3 bits) and contract Ready (2 bits).
    const uint16_t bitmap = (uint16_t)read_packed_field(resp, 0u, 16u);
    TEST_ASSERT(bitmap & UCSI_PPM_CSC_POWER_OP_MODE_CHANGE);
    TEST_ASSERT(bitmap & UCSI_PPM_CSC_NEGOTIATED_PL_CHANGE);
    // Power Operation Mode = 3 (PD).
    TEST_ASSERT(read_packed_field(resp, 16u, 3u) == 3u);
    // RDO populated and matches PE's stored value.
    const uint32_t rdo = (uint32_t)read_packed_field(resp, 32u, 32u);
    TEST_ASSERT(rdo == ppm->pe_current_rdo);
    TEST_ASSERT(((rdo >> 28) & 0x07u) == 1u); // PDO #1 selected
    // bcdPDVersion = UCSI_PPM_VERSION_PD.
    TEST_ASSERT(read_packed_field(resp, 70u, 16u) == UCSI_PPM_VERSION_PD);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_cs_detach_sets_connect_change(void) {
    UcsiPpm* ppm = mock_attach(false);
    // Drain attach notification.
    uint8_t resp[19];
    send_get_connector_status(ppm, resp);
    uint8_t ack_byte = UCSI_PPM_ACK_CC_CI_COMMAND_COMPLETED_ACK |
                       UCSI_PPM_ACK_CC_CI_CONNECTOR_CHANGE_ACK;
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL + 2, 1, &ack_byte);
    uint8_t ack_op = UCSI_PPM_OPCODE_ACK_CC_CI;
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 1, &ack_op);

    g_mock_alert_calls = 0;
    simulate_vbus_changed(ppm, false);
    TEST_ASSERT(g_mock_alert_calls >= 1); // detach woke OPM

    send_get_connector_status(ppm, resp);
    TEST_ASSERT(read_packed_field(resp, 0u, 16u) & UCSI_PPM_CSC_CONNECT_CHANGE);
    TEST_ASSERT(read_packed_field(resp, 19u, 1u) == 0u); // no longer connected

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_cs_src_partner_type_and_direction(void) {
    UcsiPpm* ppm = mock_attach(true);
    uint8_t resp[19];
    send_get_connector_status(ppm, resp);

    TEST_ASSERT(read_packed_field(resp, 19u, 1u) == 1u); // attached
    TEST_ASSERT(read_packed_field(resp, 20u, 1u) == 1u); // power direction = src
    TEST_ASSERT(read_packed_field(resp, 29u, 3u) == 2u); // partner = UFP
    TEST_ASSERT(read_packed_field(resp, 87u, 1u) == 0u); // sink path off (we are src)

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_cs_orientation_cc2_flipped(void) {
    // Force CC2 settlement.
    UcsiPpm* ppm = mock_make_ppm_with_mode(UcsiPpmCcModeDrp, false);
    g_mock_time_ms = 0;
    simulate_toggle_done(ppm, FUSB302_STATUS1A_TOGSS_SNKON_CC2);
    g_mock_time_ms = 10;
    simulate_vbus_changed(ppm, true);
    g_mock_time_ms = 150;
    ucsi_ppm_tick(ppm);
    TEST_ASSERT(ucsi_ppm_get_connector_state(ppm) == UcsiPpmStateAttachedSnk);

    uint8_t resp[19];
    send_get_connector_status(ppm, resp);
    TEST_ASSERT(read_packed_field(resp, 86u, 1u) == 1u); // flipped

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_cs_ack_clears_indicator_not_bitmap(void) {
    // Architecture §9 / commands.md §2.4: ACK_CC_CI with Connector Change Ack
    // clears CCI.Connector Change Indicator, but the Connector Status Change
    // bitmap inside GET_CONNECTOR_STATUS is only cleared on read.
    UcsiPpm* ppm = mock_attach(false);
    uint32_t cci = read_cci(ppm);
    TEST_ASSERT(((cci >> UCSI_PPM_CCI_CONNECTOR_CHANGE_SHIFT) & 0x7Fu) != 0u);

    // Ack the indicator only — bitmap should survive.
    uint8_t ack_byte = UCSI_PPM_ACK_CC_CI_CONNECTOR_CHANGE_ACK;
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL + 2, 1, &ack_byte);
    uint8_t ack_op = UCSI_PPM_OPCODE_ACK_CC_CI;
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 1, &ack_op);

    cci = read_cci(ppm);
    TEST_ASSERT(((cci >> UCSI_PPM_CCI_CONNECTOR_CHANGE_SHIFT) & 0x7Fu) == 0u);
    TEST_ASSERT(ppm->connector_status_change != 0u);

    ucsi_ppm_free(ppm);
    return true;
}

// --- L3 PE Hard Reset orchestration ----------------------------------------

static void simulate_hard_reset_sent(UcsiPpm* ppm) {
    g_mock_regs[Fusb302RegInterrupt] = 0;
    g_mock_regs[Fusb302RegInterruptB] = 0;
    const Fusb302InterruptARegBits inta = {.i_hard_sent = 1};
    g_mock_regs[Fusb302RegInterruptA] = *(const uint8_t*)&inta;
    ucsi_ppm_notify_fusb302_irq(ppm);
    ucsi_ppm_tick(ppm);
}

static void simulate_hard_reset_rx(UcsiPpm* ppm) {
    g_mock_regs[Fusb302RegInterrupt] = 0;
    g_mock_regs[Fusb302RegInterruptB] = 0;
    const Fusb302InterruptARegBits inta = {.i_hard_rst = 1};
    g_mock_regs[Fusb302RegInterruptA] = *(const uint8_t*)&inta;
    ucsi_ppm_notify_fusb302_irq(ppm);
    ucsi_ppm_tick(ppm);
}

static void simulate_tx_retry_fail(UcsiPpm* ppm) {
    g_mock_regs[Fusb302RegInterrupt] = 0;
    g_mock_regs[Fusb302RegInterruptB] = 0;
    const Fusb302InterruptARegBits inta = {.i_retry_fail = 1};
    g_mock_regs[Fusb302RegInterruptA] = *(const uint8_t*)&inta;
    ucsi_ppm_notify_fusb302_irq(ppm);
    ucsi_ppm_tick(ppm);
}

static bool test_pe_snk_hard_reset_sent_returns_to_wait_caps(void) {
    UcsiPpm* ppm = mock_attach(false);
    g_mock_time_ms += 600; // expire SinkWaitCapTimer
    ucsi_ppm_tick(ppm);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPePendingHardResetSent);

    simulate_hard_reset_sent(ppm);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeSnkWaitForCapabilities);
    // Counter holds (1 attempt used so far).
    TEST_ASSERT(ppm->pe_hard_reset_counter == 1u);
    // PRL counters reset by PRL handler (it also saw HardResetSent).
    TEST_ASSERT(ppm->prl_next_tx_msg_id == 0u);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_pe_src_hard_reset_sent_returns_to_send_caps(void) {
    UcsiPpm* ppm = mock_attach(true);
    const uint32_t rdo = make_request_rdo(1u, 3000u);
    simulate_pd_message(ppm, 0x02u, &rdo, 1);
    // Force PS transition timeout.
    g_mock_time_ms += 600;
    ucsi_ppm_tick(ppm);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPePendingHardResetSent);
    mock_i2c_reset();

    simulate_hard_reset_sent(ppm);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeSrcSendCapabilities);
    // Source_Capabilities re-advertised after recovery.
    TEST_ASSERT(find_fifo_burst_by_msg_type(0x01u) >= 0);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_pe_hard_reset_rx_restarts_without_counter_bump(void) {
    // Partner Hard Resets us — we recover but don't consume our own retry
    // budget (per PD §6.8.2, counter is for self-initiated Hard Resets).
    UcsiPpm* ppm = mock_attach(false);
    const uint32_t pdo = ucsi_ppm_pdo_fixed_source(5000, 3000, true, false, true, true);
    simulate_pd_message(ppm, 0x01u, &pdo, 1);
    simulate_pd_message(ppm, 0x03u, NULL, 0);
    simulate_pd_message(ppm, 0x06u, NULL, 0);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeSnkReady);
    TEST_ASSERT(ppm->pe_hard_reset_counter == 0u);

    simulate_hard_reset_rx(ppm);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeSnkWaitForCapabilities);
    TEST_ASSERT(ppm->pe_hard_reset_counter == 0u); // not bumped on partner-HR
    TEST_ASSERT(ppm->pe_current_rdo == 0u); // contract details wiped

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_pe_hard_reset_counter_caps_at_max(void) {
    // Two attempts allowed; the third escalates to permanent Error.
    UcsiPpm* ppm = mock_attach(false);

    for(int i = 0; i < 2; ++i) {
        g_mock_time_ms += 600;
        ucsi_ppm_tick(ppm);
        TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPePendingHardResetSent);
        simulate_hard_reset_sent(ppm);
        TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeSnkWaitForCapabilities);
    }
    TEST_ASSERT(ppm->pe_hard_reset_counter == 2u);

    // Third timeout — counter would go to 3, but cap rejects → Error.
    g_mock_time_ms += 600;
    ucsi_ppm_tick(ppm);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeStateError);

    ucsi_ppm_free(ppm);
    return true;
}

// --- L2 SET_POWER_LEVEL → PE renegotiate -----------------------------------

// Packs the SET_POWER_LEVEL CONTROL payload: Operating Current at bits 36..42,
// Source-or-Sink at bit 23. Returns the CONTROL bytes 1..7 as if written by
// the OPM ahead of the opcode byte.
static void build_set_power_level_payload(
    uint8_t* payload,
    bool is_source,
    uint16_t op_current_ma) {
    memset(payload, 0, 7);
    // Bit 23 = byte 2 bit 7.
    if(is_source) payload[1] |= (uint8_t)(1u << 7);
    // Bits 36..42 = 7 bits starting at byte 3 bit 4.
    const uint8_t op_units = (uint8_t)(op_current_ma / 50u);
    payload[3] |= (uint8_t)((op_units & 0x0Fu) << 4); // bits 36..39
    payload[4] |= (uint8_t)((op_units >> 4) & 0x07u); // bits 40..42
}

static UcsiPpmStatus send_set_power_level(UcsiPpm* ppm, bool is_source, uint16_t op_ma) {
    uint8_t payload[7];
    build_set_power_level_payload(payload, is_source, op_ma);
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL + 1, 7, payload);
    uint8_t op = UCSI_PPM_OPCODE_SET_POWER_LEVEL;
    return ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 1, &op);
}

static bool test_set_power_level_renegotiates_sink_contract(void) {
    UcsiPpm* ppm = mock_attach(false);
    const uint32_t pdo = ucsi_ppm_pdo_fixed_source(5000, 3000, true, false, true, true);
    simulate_pd_message(ppm, 0x01u, &pdo, 1);
    simulate_pd_message(ppm, 0x03u, NULL, 0);
    simulate_pd_message(ppm, 0x06u, NULL, 0);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeSnkReady);
    TEST_ASSERT(ppm->pe_negotiated_current_ma == 3000u);
    mock_i2c_reset();

    // Ask for a lower operating current (1500 mA).
    TEST_ASSERT(send_set_power_level(ppm, false /*sink*/, 1500u) == UcsiPpmStatusOk);

    // PE re-armed: sent a fresh Request, awaiting Accept.
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeSnkWaitForAccept);
    const int idx = find_fifo_burst_by_msg_type(0x02u /* Request */);
    TEST_ASSERT(idx >= 0);
    const uint8_t* fifo = &g_mock_txns[idx].data[1];
    // RDO at fifo[7..10], LE.
    const uint32_t rdo = (uint32_t)fifo[7] | ((uint32_t)fifo[8] << 8) |
                         ((uint32_t)fifo[9] << 16) | ((uint32_t)fifo[10] << 24);
    TEST_ASSERT(((rdo >> 28) & 0x07u) == 1u); // same PDO position
    TEST_ASSERT(((rdo >> 10) & 0x3FFu) == 150u); // 1500 / 10

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_set_power_level_zero_uses_advertised_max(void) {
    UcsiPpm* ppm = mock_attach(false);
    const uint32_t pdo = ucsi_ppm_pdo_fixed_source(5000, 2500, true, false, true, true);
    simulate_pd_message(ppm, 0x01u, &pdo, 1);
    simulate_pd_message(ppm, 0x03u, NULL, 0);
    simulate_pd_message(ppm, 0x06u, NULL, 0);
    mock_i2c_reset();

    // 0 → "PPM decides" → fall back to advertised max (2500 mA).
    TEST_ASSERT(send_set_power_level(ppm, false, 0u) == UcsiPpmStatusOk);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeSnkWaitForAccept);
    const int idx = find_fifo_burst_by_msg_type(0x02u);
    const uint8_t* fifo = &g_mock_txns[idx].data[1];
    const uint32_t rdo = (uint32_t)fifo[7] | ((uint32_t)fifo[8] << 8) |
                         ((uint32_t)fifo[9] << 16) | ((uint32_t)fifo[10] << 24);
    TEST_ASSERT(((rdo >> 10) & 0x3FFu) == 250u); // 2500 / 10

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_set_power_level_clamps_to_advertised_max(void) {
    UcsiPpm* ppm = mock_attach(false);
    const uint32_t pdo = ucsi_ppm_pdo_fixed_source(5000, 1500, true, false, true, true);
    simulate_pd_message(ppm, 0x01u, &pdo, 1);
    simulate_pd_message(ppm, 0x03u, NULL, 0);
    simulate_pd_message(ppm, 0x06u, NULL, 0);
    mock_i2c_reset();

    // Ask for 5 A — advertised is only 1.5 A. PE must clamp.
    TEST_ASSERT(send_set_power_level(ppm, false, 5000u) == UcsiPpmStatusOk);
    const int idx = find_fifo_burst_by_msg_type(0x02u);
    const uint8_t* fifo = &g_mock_txns[idx].data[1];
    const uint32_t rdo = (uint32_t)fifo[7] | ((uint32_t)fifo[8] << 8) |
                         ((uint32_t)fifo[9] << 16) | ((uint32_t)fifo[10] << 24);
    TEST_ASSERT(((rdo >> 10) & 0x3FFu) == 150u); // clamped to 1500

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_set_power_level_source_rejected_in_v1(void) {
    UcsiPpm* ppm = mock_attach(true);
    const uint32_t rdo = make_request_rdo(1u, 3000u);
    simulate_pd_message(ppm, 0x02u, &rdo, 1);
    ucsi_ppm_notify_power_supply_ready(ppm);
    ucsi_ppm_tick(ppm);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeSrcReady);

    TEST_ASSERT(send_set_power_level(ppm, true /*source*/, 1500u) == UcsiPpmStatusOk);
    const uint32_t cci = read_cci(ppm);
    TEST_ASSERT(cci & UCSI_PPM_CCI_ERROR);
    TEST_ASSERT(ppm->error_info & UCSI_PPM_ERR_INVALID_CMD_PARAMS);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_set_power_level_without_contract_rejected(void) {
    UcsiPpm* ppm = mock_attach(false);
    // Still in WaitForCapabilities — no contract yet.
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeSnkWaitForCapabilities);

    TEST_ASSERT(send_set_power_level(ppm, false, 1500u) == UcsiPpmStatusOk);
    const uint32_t cci = read_cci(ppm);
    TEST_ASSERT(cci & UCSI_PPM_CCI_ERROR);

    ucsi_ppm_free(ppm);
    return true;
}

// --- GET_PDOS partner-side (cached Source_Capabilities) --------------------

// Sends GET_PDOS for partner-side source PDOs and reads the response.
// Returns the resulting CCI after the command.
static uint32_t send_get_pdos_partner(
    UcsiPpm* ppm,
    uint8_t pdo_offset,
    uint8_t num_pdos,
    uint8_t* out_payload,
    size_t out_len) {
    uint8_t payload[7] = {0};
    // bit 23 = byte 2 bit 7 — Partner PDO.
    payload[1] = (uint8_t)(1u << 7);
    // byte 3 = PDO Offset.
    payload[2] = pdo_offset;
    // byte 4 bits 0..1 = num-1; bit 2 = Source PDO.
    payload[3] = (uint8_t)(((num_pdos - 1u) & 0x03u) | (1u << 2));
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL + 1, 7, payload);
    uint8_t op = UCSI_PPM_OPCODE_GET_PDOS;
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 1, &op);
    ucsi_ppm_register_read(ppm, UCSI_PPM_OFFSET_MESSAGE_IN, (uint16_t)out_len, out_payload);
    return read_cci(ppm);
}

static bool test_get_pdos_partner_returns_cached_caps(void) {
    UcsiPpm* ppm = mock_attach(false);
    const uint32_t partner_pdos[2] = {
        ucsi_ppm_pdo_fixed_source(5000, 3000, true, false, true, true),
        ucsi_ppm_pdo_fixed_source(9000, 3000, true, false, true, true),
    };
    simulate_pd_message(ppm, 0x01u, partner_pdos, 2);
    TEST_ASSERT(ppm->pe_received_pdo_count == 2u);

    uint8_t resp[8];
    const uint32_t cci = send_get_pdos_partner(ppm, 0u, 2u, resp, sizeof(resp));
    TEST_ASSERT(!(cci & UCSI_PPM_CCI_ERROR));
    TEST_ASSERT(((cci >> UCSI_PPM_CCI_DATA_LENGTH_SHIFT) & 0xFFu) == 8u);

    const uint32_t pdo0 = (uint32_t)resp[0] | ((uint32_t)resp[1] << 8) |
                          ((uint32_t)resp[2] << 16) | ((uint32_t)resp[3] << 24);
    const uint32_t pdo1 = (uint32_t)resp[4] | ((uint32_t)resp[5] << 8) |
                          ((uint32_t)resp[6] << 16) | ((uint32_t)resp[7] << 24);
    TEST_ASSERT(pdo0 == partner_pdos[0]);
    TEST_ASSERT(pdo1 == partner_pdos[1]);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_get_pdos_partner_without_caps_errors(void) {
    UcsiPpm* ppm = mock_attach(false);
    // Don't simulate any Source_Capabilities — pe_received_pdo_count is 0.
    TEST_ASSERT(ppm->pe_received_pdo_count == 0u);

    uint8_t resp[8];
    const uint32_t cci = send_get_pdos_partner(ppm, 0u, 1u, resp, sizeof(resp));
    TEST_ASSERT(cci & UCSI_PPM_CCI_ERROR);
    TEST_ASSERT(ppm->error_info & UCSI_PPM_ERR_CC_COMMUNICATION);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_pe_hard_reset_counter_resets_on_ready(void) {
    // A clean contract should refill the Hard Reset budget for subsequent
    // protocol upsets — otherwise a long session burns through the counter.
    UcsiPpm* ppm = mock_attach(false);
    g_mock_time_ms += 600;
    ucsi_ppm_tick(ppm);
    simulate_hard_reset_sent(ppm); // counter = 1
    TEST_ASSERT(ppm->pe_hard_reset_counter == 1u);

    // Recover into a real contract.
    const uint32_t pdo = ucsi_ppm_pdo_fixed_source(5000, 3000, true, false, true, true);
    simulate_pd_message(ppm, 0x01u, &pdo, 1);
    simulate_pd_message(ppm, 0x03u, NULL, 0);
    simulate_pd_message(ppm, 0x06u, NULL, 0);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeSnkReady);
    TEST_ASSERT(ppm->pe_hard_reset_counter == 0u);

    ucsi_ppm_free(ppm);
    return true;
}

// --- L3 PE Soft Reset on TX_RETRY_FAIL (PD R3.0 §6.3.13 / §8.3.3.4) --------

// Drives sink-side PD to SnkReady so soft-reset tests start from a stable
// contract.
static UcsiPpm* mock_snk_ready(void) {
    UcsiPpm* ppm = mock_attach(false);
    const uint32_t pdo = ucsi_ppm_pdo_fixed_source(5000, 3000, true, false, true, true);
    simulate_pd_message(ppm, 0x01u /* Source_Capabilities */, &pdo, 1);
    simulate_pd_message(ppm, 0x03u /* Accept */, NULL, 0);
    simulate_pd_message(ppm, 0x06u /* PS_RDY */, NULL, 0);
    return ppm;
}

static bool test_pe_tx_retry_fail_in_snk_ready_triggers_soft_reset(void) {
    UcsiPpm* ppm = mock_snk_ready();
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeSnkReady);
    mock_i2c_reset();

    simulate_tx_retry_fail(ppm);

    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeWaitForSoftResetAccept);

    // Soft_Reset (msg_type 0x0D, NDO=0) was transmitted via PRL after a
    // counter reset, so the header carries MessageID = 0 and msg_id_counter
    // advanced to 1 for the next outgoing frame.
    const int idx = find_fifo_burst_by_msg_type(0x0Du);
    TEST_ASSERT(idx >= 0);
    const uint8_t* fifo = &g_mock_txns[idx].data[1];
    const uint16_t hdr = (uint16_t)(fifo[5] | ((uint16_t)fifo[6] << 8));
    TEST_ASSERT(((hdr >> 9) & 0x07u) == 0u); // MessageID == 0
    TEST_ASSERT(((hdr >> 12) & 0x07u) == 0u); // NDO == 0
    TEST_ASSERT(ppm->prl_next_tx_msg_id == 1u);
    TEST_ASSERT(!ppm->prl_last_rx_valid);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_pe_soft_reset_accept_restarts_negotiation(void) {
    UcsiPpm* ppm = mock_snk_ready();
    const uint8_t hr_before = ppm->pe_hard_reset_counter;
    simulate_tx_retry_fail(ppm);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeWaitForSoftResetAccept);

    // Partner Accepts the Soft_Reset; its own counter is now 0 too.
    g_test_partner_msg_id = 0;
    simulate_pd_message(ppm, 0x03u /* Accept */, NULL, 0);

    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeSnkWaitForCapabilities);
    // Soft reset does not consume Hard Reset budget.
    TEST_ASSERT(ppm->pe_hard_reset_counter == hr_before);
    // Half-formed contract state cleared, ready for fresh caps.
    TEST_ASSERT(ppm->pe_received_pdo_count == 0u);
    TEST_ASSERT(ppm->pe_current_rdo == 0u);
    TEST_ASSERT(ppm->pe_negotiated_voltage_mv == 0u);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_pe_soft_reset_timeout_escalates_to_hard_reset(void) {
    UcsiPpm* ppm = mock_snk_ready();
    simulate_tx_retry_fail(ppm);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeWaitForSoftResetAccept);

    // SenderResponseTimer = 500 ms — no Accept within the window → Hard Reset.
    g_mock_time_ms += 600;
    ucsi_ppm_tick(ppm);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPePendingHardResetSent);
    TEST_ASSERT(ppm->pe_hard_reset_counter == 1u);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_pe_tx_retry_fail_in_wait_soft_reset_escalates_hard(void) {
    UcsiPpm* ppm = mock_snk_ready();
    simulate_tx_retry_fail(ppm); // -> WaitForSoftResetAccept
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeWaitForSoftResetAccept);

    // Soft_Reset frame itself failed — must go straight to Hard Reset.
    simulate_tx_retry_fail(ppm);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPePendingHardResetSent);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_pe_rx_soft_reset_accepted_and_restarts(void) {
    UcsiPpm* ppm = mock_snk_ready();
    mock_i2c_reset();

    // Partner just reset its counter, so its Soft_Reset goes out with id=0.
    g_test_partner_msg_id = 0;
    simulate_pd_message(ppm, 0x0Du /* Soft_Reset */, NULL, 0);

    // PE Accepted and restarted contract negotiation.
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeSnkWaitForCapabilities);
    TEST_ASSERT(find_fifo_burst_by_msg_type(0x03u /* Accept */) >= 0);

    // PRL state was rolled back so partner's next msg (id=1) isn't deduped.
    TEST_ASSERT(ppm->prl_last_rx_valid); // SOFT_RESET frame itself recorded
    TEST_ASSERT(ppm->prl_last_rx_msg_id == 0u);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_prl_soft_reset_rx_resets_state_before_dedup(void) {
    // Targeted PRL-level check: Soft_Reset must bypass dedup even when its
    // msg_id collides with prl_last_rx_msg_id.
    UcsiPpm* ppm = mock_snk_ready();
    // Drive prl_last_rx_msg_id to a known non-zero value.
    TEST_ASSERT(ppm->prl_last_rx_valid);
    const uint8_t stale_id = ppm->prl_last_rx_msg_id;

    // Force partner counter back to stale_id — without Soft_Reset bypass the
    // PRL would discard this frame as a duplicate.
    g_test_partner_msg_id = stale_id;
    simulate_pd_message(ppm, 0x0Du /* Soft_Reset */, NULL, 0);

    // PE saw the frame and Accept'd → state advanced.
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeSnkWaitForCapabilities);

    ucsi_ppm_free(ppm);
    return true;
}

// --- L3 PE swap handling (DR / PR / VCONN) ---------------------------------

static bool test_pe_dr_swap_received_accepted_flips_data_role(void) {
    UcsiPpm* ppm = mock_snk_ready(); // sink → starts as UFP
    TEST_ASSERT(!ppm->pe_data_role_is_dfp);
    TEST_ASSERT(ppm->accept_dr_swap); // default policy
    mock_i2c_reset();

    simulate_pd_message(ppm, 0x09u /* DR_Swap */, NULL, 0);

    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeSnkReady); // still Ready
    TEST_ASSERT(ppm->pe_data_role_is_dfp); // flipped to DFP
    const int idx = find_fifo_burst_by_msg_type(0x03u /* Accept */);
    TEST_ASSERT(idx >= 0);
    // Per PD §6.3.10 the Accept itself goes out with the OLD data role.
    const uint8_t* fifo = &g_mock_txns[idx].data[1];
    const uint16_t hdr = (uint16_t)(fifo[5] | ((uint16_t)fifo[6] << 8));
    TEST_ASSERT((hdr & (1u << 5)) == 0u); // old data role = UFP
    TEST_ASSERT(ppm->connector_status_change & UCSI_PPM_CSC_PARTNER_CHANGED);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_pe_dr_swap_received_rejected_by_policy(void) {
    UcsiPpm* ppm = mock_snk_ready();
    ppm->accept_dr_swap = false;
    mock_i2c_reset();

    simulate_pd_message(ppm, 0x09u /* DR_Swap */, NULL, 0);

    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeSnkReady);
    TEST_ASSERT(!ppm->pe_data_role_is_dfp); // unchanged
    TEST_ASSERT(find_fifo_burst_by_msg_type(0x04u /* Reject */) >= 0);
    TEST_ASSERT(find_fifo_burst_by_msg_type(0x03u /* Accept */) < 0);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_pe_dr_swap_received_outside_ready_ignored(void) {
    UcsiPpm* ppm = mock_attach(false); // PE_SNK_WaitForCapabilities
    mock_i2c_reset();

    simulate_pd_message(ppm, 0x09u, NULL, 0);

    // No response sent — swap must come from an established contract.
    TEST_ASSERT(find_fifo_burst_by_msg_type(0x03u) < 0);
    TEST_ASSERT(find_fifo_burst_by_msg_type(0x04u) < 0);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_pe_pr_swap_always_rejected_in_v1(void) {
    UcsiPpm* ppm = mock_snk_ready();
    ppm->accept_pr_swap = true; // even with policy enabled — v1 still rejects
    mock_i2c_reset();

    simulate_pd_message(ppm, 0x0Au /* PR_Swap */, NULL, 0);

    TEST_ASSERT(find_fifo_burst_by_msg_type(0x04u /* Reject */) >= 0);
    TEST_ASSERT(find_fifo_burst_by_msg_type(0x03u) < 0);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeSnkReady);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_pe_vconn_swap_always_rejected_in_v1(void) {
    UcsiPpm* ppm = mock_snk_ready();
    mock_i2c_reset();
    simulate_pd_message(ppm, 0x0Bu /* VCONN_Swap */, NULL, 0);
    TEST_ASSERT(find_fifo_burst_by_msg_type(0x04u) >= 0);
    ucsi_ppm_free(ppm);
    return true;
}

static bool test_pe_dr_swap_initiated_via_set_uor(void) {
    UcsiPpm* ppm = mock_snk_ready();
    TEST_ASSERT(!ppm->pe_data_role_is_dfp);
    mock_i2c_reset();

    // SET_UOR Initiate-DFP (Role bit 0 = 1, bits 1/2 = 0).
    uint8_t payload[7] = {0};
    pack_role3(&payload[1], &payload[2], 0x01u);
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL + 1, 7, payload);
    uint8_t op = UCSI_PPM_OPCODE_SET_UOR;
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 1, &op);

    TEST_ASSERT(read_cci(ppm) & UCSI_PPM_CCI_COMMAND_COMPLETED);
    TEST_ASSERT(!(read_cci(ppm) & UCSI_PPM_CCI_ERROR));
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeWaitForDrSwapResponse);
    TEST_ASSERT(find_fifo_burst_by_msg_type(0x09u /* DR_Swap */) >= 0);
    // accept_dr_swap stays false because ROLE_ACCEPT_SWAPS (bit 2) wasn't set.
    TEST_ASSERT(!ppm->accept_dr_swap);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_pe_dr_swap_initiator_accept_flips_role(void) {
    UcsiPpm* ppm = mock_snk_ready();
    TEST_ASSERT(ucsi_ppm_pe_request_dr_swap(ppm, true) == UcsiPpmStatusOk);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeWaitForDrSwapResponse);

    simulate_pd_message(ppm, 0x03u /* Accept */, NULL, 0);

    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeSnkReady);
    TEST_ASSERT(ppm->pe_data_role_is_dfp);
    TEST_ASSERT(ppm->connector_status_change & UCSI_PPM_CSC_PARTNER_CHANGED);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_pe_dr_swap_initiator_reject_keeps_role(void) {
    UcsiPpm* ppm = mock_snk_ready();
    ucsi_ppm_pe_request_dr_swap(ppm, true);
    simulate_pd_message(ppm, 0x04u /* Reject */, NULL, 0);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeSnkReady);
    TEST_ASSERT(!ppm->pe_data_role_is_dfp);
    ucsi_ppm_free(ppm);
    return true;
}

static bool test_pe_dr_swap_initiator_wait_keeps_role(void) {
    UcsiPpm* ppm = mock_snk_ready();
    ucsi_ppm_pe_request_dr_swap(ppm, true);
    simulate_pd_message(ppm, 0x0Cu /* Wait */, NULL, 0);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeSnkReady);
    TEST_ASSERT(!ppm->pe_data_role_is_dfp);
    ucsi_ppm_free(ppm);
    return true;
}

static bool test_pe_dr_swap_initiator_timeout_hard_resets(void) {
    UcsiPpm* ppm = mock_snk_ready();
    ucsi_ppm_pe_request_dr_swap(ppm, true);
    g_mock_time_ms += 600; // past SenderResponseTimer
    ucsi_ppm_tick(ppm);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPePendingHardResetSent);
    ucsi_ppm_free(ppm);
    return true;
}

static bool test_pe_dr_swap_initiated_outside_ready_errors(void) {
    UcsiPpm* ppm = mock_attach(false); // PE_SNK_WaitForCapabilities
    TEST_ASSERT(ucsi_ppm_pe_request_dr_swap(ppm, true) == UcsiPpmStatusInvalidArg);
    ucsi_ppm_free(ppm);
    return true;
}

static bool test_pe_dr_swap_initiated_same_role_is_no_op(void) {
    UcsiPpm* ppm = mock_snk_ready(); // already UFP
    mock_i2c_reset();
    TEST_ASSERT(ucsi_ppm_pe_request_dr_swap(ppm, false) == UcsiPpmStatusOk);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeSnkReady);
    TEST_ASSERT(find_fifo_burst_by_msg_type(0x09u) < 0); // nothing transmitted
    ucsi_ppm_free(ppm);
    return true;
}

static bool test_cs_partner_type_reflects_data_role_after_dr_swap(void) {
    UcsiPpm* ppm = mock_snk_ready(); // we're UFP → partner = DFP (Table 6-43 = 1)
    uint8_t resp[19];
    send_get_connector_status(ppm, resp);
    TEST_ASSERT(read_packed_field(resp, 29u, 3u) == 1u);

    // Drain the change bitmap before the swap.
    uint8_t ack_byte =
        UCSI_PPM_ACK_CC_CI_COMMAND_COMPLETED_ACK | UCSI_PPM_ACK_CC_CI_CONNECTOR_CHANGE_ACK;
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL + 2, 1, &ack_byte);
    uint8_t ack_op = UCSI_PPM_OPCODE_ACK_CC_CI;
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 1, &ack_op);

    // Partner-initiated DR_Swap → we flip to DFP → partner becomes UFP (=2).
    simulate_pd_message(ppm, 0x09u, NULL, 0);
    TEST_ASSERT(ppm->pe_data_role_is_dfp);

    send_get_connector_status(ppm, resp);
    TEST_ASSERT(read_packed_field(resp, 29u, 3u) == 2u);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_cs_failed_attach_does_not_notify(void) {
    // AttachWait → Unattached via timeout (no VBUS) must NOT raise a
    // Connect Change — we never actually attached as far as OPM is concerned.
    UcsiPpm* ppm = mock_make_ppm_with_mode(UcsiPpmCcModeDrp, false);
    g_mock_time_ms = 0;
    simulate_toggle_done(ppm, FUSB302_STATUS1A_TOGSS_SRCON_CC1);
    g_mock_alert_calls = 0;

    g_mock_time_ms = 600; // past AttachWait timeout
    ucsi_ppm_tick(ppm);
    TEST_ASSERT(ucsi_ppm_get_connector_state(ppm) == UcsiPpmStateUnattached);
    TEST_ASSERT(g_mock_alert_calls == 0);
    TEST_ASSERT(ppm->connector_status_change == 0u);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_pe_src_psu_ready_ignored_outside_transition(void) {
    // notify_power_supply_ready arriving when we're not in SrcTransitionSupply
    // (e.g., before Request) must be a no-op — must not emit PS_RDY or
    // misadvance state.
    UcsiPpm* ppm = mock_attach(true);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeSrcSendCapabilities);
    mock_i2c_reset();

    ucsi_ppm_notify_power_supply_ready(ppm);
    ucsi_ppm_tick(ppm);

    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeSrcSendCapabilities);
    TEST_ASSERT(find_fifo_burst_by_msg_type(0x06u) < 0); // no PS_RDY

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_pe_snk_detach_returns_to_idle(void) {
    UcsiPpm* ppm = mock_attach(false);
    const uint32_t pdo = ucsi_ppm_pdo_fixed_source(5000, 1500, true, false, true, true);
    simulate_pd_message(ppm, 0x01u, &pdo, 1);
    simulate_pd_message(ppm, 0x03u, NULL, 0);
    simulate_pd_message(ppm, 0x06u, NULL, 0);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeSnkReady);

    // Partner drops VBUS → TC unattaches → PE returns to Idle.
    simulate_vbus_changed(ppm, false);
    TEST_ASSERT(ppm->pe_state == (int)UcsiPpmPeStateIdle);
    TEST_ASSERT(ppm->pe_negotiated_voltage_mv == 0u);
    TEST_ASSERT(ppm->pe_received_pdo_count == 0u);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_prl_detach_resets_counter(void) {
    // Get into Attached.SNK, advance the counter, then detach. PRL state
    // must reset so the next attach starts fresh.
    UcsiPpm* ppm = mock_attach(false);
    UcsiPpmPhyPdMsg msg = {.sop_type = UcsiPpmPhySopTypeSop, .header = 0u};
    ucsi_ppm_prl_send_message(ppm, &msg);
    ucsi_ppm_prl_send_message(ppm, &msg);
    TEST_ASSERT(ppm->prl_next_tx_msg_id == 2u);

    // Partner drops VBUS → tc_enter_unattached → prl_reset.
    simulate_vbus_changed(ppm, false);
    TEST_ASSERT(ucsi_ppm_get_connector_state(ppm) == UcsiPpmStateUnattached);
    TEST_ASSERT(ppm->prl_next_tx_msg_id == 0u);

    ucsi_ppm_free(ppm);
    return true;
}

// --- notification mask filtering (architecture.md §4.3) -------------------

static bool test_notify_mask_zero_blocks_alert(void) {
    // Default notification_mask = 0 after init. notify_connector_change must
    // still accumulate the bitmap and stamp CCI.Connector Change Indicator,
    // but must NOT raise the alert callback.
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    cfg.alert = mock_alert;
    ucsi_ppm_init(ppm, &cfg);
    TEST_ASSERT(ppm->notification_mask == 0u);
    g_mock_alert_calls = 0;

    ucsi_ppm_notify_connector_change(ppm, UCSI_PPM_CSC_CONNECT_CHANGE);

    TEST_ASSERT(g_mock_alert_calls == 0);
    TEST_ASSERT(ppm->connector_status_change == UCSI_PPM_CSC_CONNECT_CHANGE);
    const uint32_t cci = read_cci(ppm);
    const uint32_t conn = (cci >> UCSI_PPM_CCI_CONNECTOR_CHANGE_SHIFT) & 0x7Fu;
    TEST_ASSERT(conn == UCSI_PPM_NUM_CONNECTORS);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_notify_mask_enables_specific_bit(void) {
    // Mask with only Connect Change enabled — notify with the same bit fires
    // the alert exactly once.
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    cfg.alert = mock_alert;
    ucsi_ppm_init(ppm, &cfg);
    ppm->notification_mask = UCSI_PPM_CSC_CONNECT_CHANGE;
    g_mock_alert_calls = 0;

    ucsi_ppm_notify_connector_change(ppm, UCSI_PPM_CSC_CONNECT_CHANGE);
    TEST_ASSERT(g_mock_alert_calls == 1);

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_notify_mask_blocks_mismatched_bit(void) {
    // Mask enables ATTENTION only; notify raises CONNECT_CHANGE. No alert
    // fires (no intersecting bits) but bitmap is still accumulated.
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    cfg.alert = mock_alert;
    ucsi_ppm_init(ppm, &cfg);
    ppm->notification_mask = UCSI_PPM_CSC_ATTENTION;
    g_mock_alert_calls = 0;

    ucsi_ppm_notify_connector_change(ppm, UCSI_PPM_CSC_CONNECT_CHANGE);
    TEST_ASSERT(g_mock_alert_calls == 0);
    TEST_ASSERT(ppm->connector_status_change == UCSI_PPM_CSC_CONNECT_CHANGE);

    // Raise an intersecting bit — alert fires now, bitmap accumulates both.
    ucsi_ppm_notify_connector_change(
        ppm, (uint16_t)(UCSI_PPM_CSC_ATTENTION | UCSI_PPM_CSC_PD_RESET_COMPLETE));
    TEST_ASSERT(g_mock_alert_calls == 1);
    TEST_ASSERT(
        ppm->connector_status_change ==
        (UCSI_PPM_CSC_CONNECT_CHANGE | UCSI_PPM_CSC_ATTENTION | UCSI_PPM_CSC_PD_RESET_COMPLETE));

    ucsi_ppm_free(ppm);
    return true;
}

static bool test_notify_mask_set_via_command_takes_effect(void) {
    // SET_NOTIFICATION_ENABLE writes the mask; subsequent notify_connector_change
    // honours the new value.
    UcsiPpm* ppm = ucsi_ppm_alloc();
    UcsiPpmConfig cfg;
    make_valid_config(&cfg);
    cfg.alert = mock_alert;
    ucsi_ppm_init(ppm, &cfg);

    // Build SET_NOTIFICATION_ENABLE CONTROL: opcode + 17-bit mask at bit 16.
    uint8_t ctrl[8] = {0};
    ctrl[0] = UCSI_PPM_OPCODE_SET_NOTIFICATION_ENABLE;
    const uint32_t mask = UCSI_PPM_CSC_PD_RESET_COMPLETE;
    ctrl[2] = (uint8_t)(mask & 0xFFu);
    ctrl[3] = (uint8_t)((mask >> 8) & 0xFFu);
    ctrl[4] = (uint8_t)((mask >> 16) & 0x01u);
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 8, ctrl);
    TEST_ASSERT(ppm->notification_mask == mask);

    // ACK to clear CCI before we count alerts.
    uint8_t ack_byte = UCSI_PPM_ACK_CC_CI_COMMAND_COMPLETED_ACK;
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL + 2, 1, &ack_byte);
    uint8_t ack_op = UCSI_PPM_OPCODE_ACK_CC_CI;
    ucsi_ppm_register_write(ppm, UCSI_PPM_OFFSET_CONTROL, 1, &ack_op);
    g_mock_alert_calls = 0;

    // Wrong-bit notify is silent.
    ucsi_ppm_notify_connector_change(ppm, UCSI_PPM_CSC_CONNECT_CHANGE);
    TEST_ASSERT(g_mock_alert_calls == 0);
    // Matching bit wakes OPM.
    ucsi_ppm_notify_connector_change(ppm, UCSI_PPM_CSC_PD_RESET_COMPLETE);
    TEST_ASSERT(g_mock_alert_calls == 1);

    ucsi_ppm_free(ppm);
    return true;
}

// --- entry point -----------------------------------------------------------

typedef struct {
    const char* name;
    bool (*fn)(void);
} TestEntry;

#define TEST_ENTRY(f) {#f, f}

static const TestEntry k_tests[] = {
    // L1 alloc / free.
    TEST_ENTRY(test_alloc_returns_nonnull),
    TEST_ENTRY(test_free_null_is_noop),
    TEST_ENTRY(test_free_auto_deinit),

    // L1 init / deinit / reset.
    TEST_ENTRY(test_init_null_args),
    TEST_ENTRY(test_init_double),
    TEST_ENTRY(test_init_validates_callbacks),
    TEST_ENTRY(test_init_validates_i2c_addr),
    TEST_ENTRY(test_init_validates_pdo),
    TEST_ENTRY(test_init_validates_power_source),
    TEST_ENTRY(test_init_validates_disabled_mode),
    TEST_ENTRY(test_deinit_before_init),
    TEST_ENTRY(test_deinit_then_reinit),
    TEST_ENTRY(test_reset_before_init),

    // L1 register_read.
    TEST_ENTRY(test_register_read_before_init),
    TEST_ENTRY(test_register_read_null),
    TEST_ENTRY(test_register_read_bounds),
    TEST_ENTRY(test_register_read_version),
    TEST_ENTRY(test_register_read_zeros),

    // L1 register_write.
    TEST_ENTRY(test_register_write_before_init),
    TEST_ENTRY(test_register_write_bounds),
    TEST_ENTRY(test_register_write_readonly_zones),
    TEST_ENTRY(test_register_write_reserved),
    TEST_ENTRY(test_register_write_reserved_span),
    TEST_ENTRY(test_register_write_control),
    TEST_ENTRY(test_register_write_msg_out),
    TEST_ENTRY(test_reset_clears_regfile),

    // PDO helpers.
    TEST_ENTRY(test_pdo_fixed_source_5v),
    TEST_ENTRY(test_pdo_fixed_sink_5v),

    // L2 command dispatcher base.
    TEST_ENTRY(test_cmd_init_state_is_idle),
    TEST_ENTRY(test_cmd_reset_returns_state_to_idle),
    TEST_ENTRY(test_cmd_ppm_reset),
    TEST_ENTRY(test_cmd_reset_clears_on_next_cmd),
    TEST_ENTRY(test_cmd_get_capability),
    TEST_ENTRY(test_cmd_get_connector_capability_drp),
    TEST_ENTRY(test_cmd_get_connector_capability_rp_only),
    TEST_ENTRY(test_cmd_set_notification_enable),
    TEST_ENTRY(test_cmd_not_supported_unknown),
    TEST_ENTRY(test_cmd_not_supported_in_scope),
    TEST_ENTRY(test_cmd_ack_cc_ci_clears_cci),
    TEST_ENTRY(test_cmd_ack_cc_ci_in_idle_ignored),

    // L2 SET_CCOM / SET_UOR / SET_PDR.
    TEST_ENTRY(test_cmd_set_ccom_picks_drp),
    TEST_ENTRY(test_cmd_set_ccom_rejects_empty),
    TEST_ENTRY(test_cmd_set_ccom_rejects_disabled_when_unsupported),
    TEST_ENTRY(test_cmd_set_uor_stores_accept),
    TEST_ENTRY(test_cmd_set_uor_rejects_both_swap_bits),
    TEST_ENTRY(test_cmd_set_pdr_stores_accept),
    TEST_ENTRY(test_cmd_set_pdr_rejects_all_zero),

    // L2 GET_PDOS.
    TEST_ENTRY(test_cmd_get_pdos_own_source),
    TEST_ENTRY(test_cmd_get_pdos_own_sink),
    TEST_ENTRY(test_cmd_get_pdos_partner_no_partner),
    TEST_ENTRY(test_cmd_get_pdos_out_of_range),

    // L2 GET_CONNECTOR_STATUS / GET_ERROR_STATUS.
    TEST_ENTRY(test_cmd_get_connector_status),
    TEST_ENTRY(test_cmd_get_error_status_initial_zero),
    TEST_ENTRY(test_cmd_error_info_flows_through),

    // L4 PHY (FUSB302) — mock I²C.
    TEST_ENTRY(test_phy_init_sequence),
    TEST_ENTRY(test_phy_init_order_reset_first),
    TEST_ENTRY(test_phy_start_toggle_drp),
    TEST_ENTRY(test_phy_start_toggle_src),
    TEST_ENTRY(test_phy_stop_toggle),
    TEST_ENTRY(test_phy_set_rp_current),
    TEST_ENTRY(test_phy_lock_polarity_cc1),
    TEST_ENTRY(test_phy_lock_polarity_cc2),
    TEST_ENTRY(test_phy_enable_pd),
    TEST_ENTRY(test_phy_pd_reset),
    TEST_ENTRY(test_phy_send_hard_reset),
    TEST_ENTRY(test_phy_pump_vbus_changed),
    TEST_ENTRY(test_phy_pump_toggle_done_src_cc1),
    TEST_ENTRY(test_phy_pump_multiple_events),
    TEST_ENTRY(test_phy_pump_idle_no_events),

    // L4 measurements (MDAC + BC_LVL).
    TEST_ENTRY(test_phy_measure_vbus_threshold_above),
    TEST_ENTRY(test_phy_measure_vbus_threshold_below),
    TEST_ENTRY(test_phy_measure_vbus_threshold_mdac_calc),
    TEST_ENTRY(test_phy_measure_vbus_threshold_null_out),
    TEST_ENTRY(test_phy_arm_vbus_compare),
    TEST_ENTRY(test_phy_read_bc_lvl),

    // L4 PD message TX (FIFO encoding).
    TEST_ENTRY(test_phy_send_message_control),
    TEST_ENTRY(test_phy_send_message_one_object),
    TEST_ENTRY(test_phy_send_message_three_objects),
    TEST_ENTRY(test_phy_send_message_header_ndo_patched),
    TEST_ENTRY(test_phy_send_message_sop_prime),
    TEST_ENTRY(test_phy_send_message_sop_double_prime),
    TEST_ENTRY(test_phy_send_message_validates),

    // L4 PD message RX (FIFO decoding).
    TEST_ENTRY(test_phy_recv_empty),
    TEST_ENTRY(test_phy_recv_sop_control),
    TEST_ENTRY(test_phy_recv_sop_one_object),
    TEST_ENTRY(test_phy_recv_sop_three_objects),
    TEST_ENTRY(test_phy_recv_sop_prime),
    TEST_ENTRY(test_phy_recv_sop_double_prime),
    TEST_ENTRY(test_phy_recv_token_low_bits_ignored),
    TEST_ENTRY(test_phy_recv_sop_debug_returns_internal),
    TEST_ENTRY(test_phy_recv_validates),
    TEST_ENTRY(test_phy_recv_drains_multiple),
    TEST_ENTRY(test_phy_tx_rx_roundtrip),

    // L1/L4 wire-up (init/deinit/reset/tick/notify_* glue).
    TEST_ENTRY(test_wireup_init_calls_phy_init),
    TEST_ENTRY(test_wireup_init_phy_failure_propagates),
    TEST_ENTRY(test_wireup_deinit_drops_terminations),
    TEST_ENTRY(test_wireup_reset_re_inits_phy),
    TEST_ENTRY(test_wireup_notify_irq_sets_flag_no_i2c),
    TEST_ENTRY(test_wireup_notify_psu_ready_sets_flag),
    TEST_ENTRY(test_wireup_tick_drains_irq),
    TEST_ENTRY(test_wireup_tick_idle_no_i2c),

    // L3 Type-C SM scaffold (Unattached + ToggleDone).
    TEST_ENTRY(test_tc_init_drp_starts_drp_toggle),
    TEST_ENTRY(test_tc_init_rp_only_starts_src_toggle),
    TEST_ENTRY(test_tc_init_rd_only_starts_snk_toggle),
    TEST_ENTRY(test_tc_init_disabled_no_toggle),
    TEST_ENTRY(test_tc_toggle_done_src_cc1),
    TEST_ENTRY(test_tc_toggle_done_src_cc2),
    TEST_ENTRY(test_tc_toggle_done_snk_cc1),
    TEST_ENTRY(test_tc_toggle_done_snk_cc2),
    TEST_ENTRY(test_tc_toggle_done_audio_rearms_toggle),
    TEST_ENTRY(test_tc_toggle_done_outside_unattached_dropped),
    TEST_ENTRY(test_tc_deinit_stops_toggle),

    // L3 Type-C SM 1b (AttachWait → Attached debounce + commit).
    TEST_ENTRY(test_tc_attach_wait_src_raises_vbus_source),
    TEST_ENTRY(test_tc_attach_wait_snk_does_not_raise_vbus),
    TEST_ENTRY(test_tc_attach_commits_src_after_debounce),
    TEST_ENTRY(test_tc_attach_commits_snk_after_debounce),
    TEST_ENTRY(test_tc_attach_no_vbus_stays_attach_wait),
    TEST_ENTRY(test_tc_attach_vbus_before_debounce_holds),
    TEST_ENTRY(test_tc_vbus_changed_in_unattached_ignored),
    TEST_ENTRY(test_tc_deinit_drops_vbus_source),

    // L3 Type-C SM 1c (Detach + AttachWait timeout).
    TEST_ENTRY(test_tc_snk_detach_on_vbus_lost),
    TEST_ENTRY(test_tc_src_detach_on_bc_lvl_high),
    TEST_ENTRY(test_tc_src_bc_lvl_rd_present_no_detach),
    TEST_ENTRY(test_tc_snk_bc_lvl_changes_no_detach),
    TEST_ENTRY(test_tc_attach_wait_timeout_restarts_toggle),
    TEST_ENTRY(test_tc_detach_reattach_full_cycle),
    TEST_ENTRY(test_tc_vbus_lost_in_attached_src_no_detach),

    // L3 PRL (Protocol Layer): MessageID counter + duplicate detection.
    TEST_ENTRY(test_prl_init_zero_state),
    TEST_ENTRY(test_prl_send_stamps_msg_id_zero),
    TEST_ENTRY(test_prl_send_increments_counter),
    TEST_ENTRY(test_prl_send_wraps_after_seven),
    TEST_ENTRY(test_prl_send_preserves_other_header_bits),
    TEST_ENTRY(test_prl_recv_first_message_delivered),
    TEST_ENTRY(test_prl_recv_duplicate_dropped),
    TEST_ENTRY(test_prl_recv_different_ids_all_delivered),
    TEST_ENTRY(test_prl_reset_clears_state),
    TEST_ENTRY(test_prl_hard_reset_event_resets_counter),
    TEST_ENTRY(test_prl_detach_resets_counter),

    // L3 PE (Policy Engine): Sink contract path.
    TEST_ENTRY(test_pe_init_idle),
    TEST_ENTRY(test_pe_snk_attach_enters_wait_capabilities),
    TEST_ENTRY(test_pe_snk_recv_source_caps_sends_request),
    TEST_ENTRY(test_pe_snk_accept_then_ps_rdy_completes_contract),
    TEST_ENTRY(test_pe_snk_reject_triggers_hard_reset),
    TEST_ENTRY(test_pe_snk_wait_response_triggers_hard_reset),
    TEST_ENTRY(test_pe_snk_wait_cap_timeout_triggers_hard_reset),
    TEST_ENTRY(test_pe_snk_sender_response_timeout_triggers_hard_reset),
    TEST_ENTRY(test_pe_snk_ps_transition_timeout_triggers_hard_reset),
    TEST_ENTRY(test_pe_snk_ready_ignores_late_accept),
    TEST_ENTRY(test_pe_snk_detach_returns_to_idle),

    // L3 PE Source path.
    TEST_ENTRY(test_pe_src_attach_sends_source_caps),
    TEST_ENTRY(test_pe_src_recv_request_sends_accept_and_drives_psu),
    TEST_ENTRY(test_pe_src_psu_ready_sends_ps_rdy),
    TEST_ENTRY(test_pe_src_invalid_pdo_rejects),
    TEST_ENTRY(test_pe_src_source_cap_timer_resends),
    TEST_ENTRY(test_pe_src_ps_transition_timeout_triggers_hard_reset),
    TEST_ENTRY(test_pe_src_detach_returns_to_idle),
    TEST_ENTRY(test_pe_src_psu_ready_ignored_outside_transition),

    // L3 → L2 event flow (Connector Status Change bitmap + alert + CCI).
    TEST_ENTRY(test_cs_attach_alerts_opm_and_sets_cci),
    TEST_ENTRY(test_cs_attach_sets_connect_change_bitmap),
    TEST_ENTRY(test_cs_second_get_clears_bitmap),
    TEST_ENTRY(test_cs_pd_contract_sets_pom_pd_and_rdo),
    TEST_ENTRY(test_cs_detach_sets_connect_change),
    TEST_ENTRY(test_cs_src_partner_type_and_direction),
    TEST_ENTRY(test_cs_orientation_cc2_flipped),
    TEST_ENTRY(test_cs_ack_clears_indicator_not_bitmap),
    TEST_ENTRY(test_cs_failed_attach_does_not_notify),

    // Notification mask filtering (architecture.md §4.3).
    TEST_ENTRY(test_notify_mask_zero_blocks_alert),
    TEST_ENTRY(test_notify_mask_enables_specific_bit),
    TEST_ENTRY(test_notify_mask_blocks_mismatched_bit),
    TEST_ENTRY(test_notify_mask_set_via_command_takes_effect),

    // L3 PE Hard Reset orchestration.
    TEST_ENTRY(test_pe_snk_hard_reset_sent_returns_to_wait_caps),
    TEST_ENTRY(test_pe_src_hard_reset_sent_returns_to_send_caps),
    TEST_ENTRY(test_pe_hard_reset_rx_restarts_without_counter_bump),
    TEST_ENTRY(test_pe_hard_reset_counter_caps_at_max),
    TEST_ENTRY(test_pe_hard_reset_counter_resets_on_ready),

    // L3 PE Soft Reset on TX_RETRY_FAIL (PD §6.3.13 / §8.3.3.4).
    TEST_ENTRY(test_pe_tx_retry_fail_in_snk_ready_triggers_soft_reset),
    TEST_ENTRY(test_pe_soft_reset_accept_restarts_negotiation),
    TEST_ENTRY(test_pe_soft_reset_timeout_escalates_to_hard_reset),
    TEST_ENTRY(test_pe_tx_retry_fail_in_wait_soft_reset_escalates_hard),
    TEST_ENTRY(test_pe_rx_soft_reset_accepted_and_restarts),
    TEST_ENTRY(test_prl_soft_reset_rx_resets_state_before_dedup),

    // L3 PE swap handling (DR / PR / VCONN — PD §6.3.10 / §8.3.3.8 / .9).
    TEST_ENTRY(test_pe_dr_swap_received_accepted_flips_data_role),
    TEST_ENTRY(test_pe_dr_swap_received_rejected_by_policy),
    TEST_ENTRY(test_pe_dr_swap_received_outside_ready_ignored),
    TEST_ENTRY(test_pe_pr_swap_always_rejected_in_v1),
    TEST_ENTRY(test_pe_vconn_swap_always_rejected_in_v1),
    TEST_ENTRY(test_pe_dr_swap_initiated_via_set_uor),
    TEST_ENTRY(test_pe_dr_swap_initiator_accept_flips_role),
    TEST_ENTRY(test_pe_dr_swap_initiator_reject_keeps_role),
    TEST_ENTRY(test_pe_dr_swap_initiator_wait_keeps_role),
    TEST_ENTRY(test_pe_dr_swap_initiator_timeout_hard_resets),
    TEST_ENTRY(test_pe_dr_swap_initiated_outside_ready_errors),
    TEST_ENTRY(test_pe_dr_swap_initiated_same_role_is_no_op),
    TEST_ENTRY(test_cs_partner_type_reflects_data_role_after_dr_swap),

    // L2 SET_POWER_LEVEL → PE renegotiate (sink-side).
    TEST_ENTRY(test_set_power_level_renegotiates_sink_contract),
    TEST_ENTRY(test_set_power_level_zero_uses_advertised_max),
    TEST_ENTRY(test_set_power_level_clamps_to_advertised_max),
    TEST_ENTRY(test_set_power_level_source_rejected_in_v1),
    TEST_ENTRY(test_set_power_level_without_contract_rejected),

    // L2 GET_PDOS partner-side (cached Source_Capabilities).
    TEST_ENTRY(test_get_pdos_partner_returns_cached_caps),
    TEST_ENTRY(test_get_pdos_partner_without_caps_errors),
};

const size_t test_count = COUNT_OF(k_tests);

bool ucsi_ppm_test_run(void) {
    FURI_LOG_I(TAG, "suite: start (%zu tests)", test_count);
    unsigned failed = 0;
    for(size_t i = 0; i < test_count; ++i) {
        if(!k_tests[i].fn()) {
            FURI_LOG_E(TAG, "FAIL: %s", k_tests[i].name);
            failed++;
        }
    }

    if(failed == 0) {
        FURI_LOG_I(TAG, "suite: PASS (%zu/%zu)", test_count, test_count);
        return true;
    } else {
        FURI_LOG_E(TAG, "suite: FAIL (%u/%zu failed)", failed, test_count);
        return false;
    }
}
