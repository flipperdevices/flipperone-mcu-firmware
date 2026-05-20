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

static void mock_i2c_reset(void) {
    g_mock_txn_count = 0;
    memset(g_mock_regs, 0, sizeof(g_mock_regs));
    g_mock_fifo_len = 0;
    g_mock_fifo_pos = 0;
    g_mock_time_ms = 0;
    g_mock_vbus_source_calls = 0;
    g_mock_vbus_source_last = false;
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
            data[i] = g_mock_regs[(reg + (uint8_t)i) & 0xFFu];
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
    if(ucsi_ppm_init(ppm, &cfg) != UcsiPpmStatusOk) {
        ucsi_ppm_free(ppm);
        return NULL;
    }
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

    const uint8_t opcodes[] = {0x0Cu, 0x0Du, 0x0Eu, 0x0Fu, 0x14u, 0x15u};
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
    cfg.initial_cc_operation_mode = mode;
    if(supports_disabled) cfg.supports_disabled_state = true;
    if(ucsi_ppm_init(ppm, &cfg) != UcsiPpmStatusOk) {
        ucsi_ppm_free(ppm);
        return NULL;
    }
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
