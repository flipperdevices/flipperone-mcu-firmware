#include "ucsi_ppm_test.h"

#include "ucsi_ppm.h"
#include "ucsi_ppm_i.h"

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

static UcsiPpmStatus stub_i2c_read(void* ctx, uint8_t addr, uint8_t* data, size_t len) {
    (void)ctx;
    (void)addr;
    (void)data;
    (void)len;
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

// --- entry point -----------------------------------------------------------

bool ucsi_ppm_test_run(void) {
    FURI_LOG_I(TAG, "L1 suite: start");
    bool ok = true;

    ok = test_alloc_returns_nonnull() && ok;
    ok = test_free_null_is_noop() && ok;
    ok = test_free_auto_deinit() && ok;

    ok = test_init_null_args() && ok;
    ok = test_init_double() && ok;
    ok = test_init_validates_callbacks() && ok;
    ok = test_init_validates_i2c_addr() && ok;
    ok = test_init_validates_pdo() && ok;
    ok = test_init_validates_power_source() && ok;
    ok = test_init_validates_disabled_mode() && ok;

    ok = test_deinit_before_init() && ok;
    ok = test_deinit_then_reinit() && ok;
    ok = test_reset_before_init() && ok;

    ok = test_register_read_before_init() && ok;
    ok = test_register_read_null() && ok;
    ok = test_register_read_bounds() && ok;
    ok = test_register_read_version() && ok;
    ok = test_register_read_zeros() && ok;

    ok = test_register_write_before_init() && ok;
    ok = test_register_write_bounds() && ok;
    ok = test_register_write_readonly_zones() && ok;
    ok = test_register_write_reserved() && ok;
    ok = test_register_write_reserved_span() && ok;
    ok = test_register_write_control() && ok;
    ok = test_register_write_msg_out() && ok;

    ok = test_reset_clears_regfile() && ok;

    ok = test_pdo_fixed_source_5v() && ok;
    ok = test_pdo_fixed_sink_5v() && ok;

    if(ok) {
        FURI_LOG_I(TAG, "L1 suite: PASS");
    } else {
        FURI_LOG_E(TAG, "L1 suite: FAIL");
    }
    return ok;
}
