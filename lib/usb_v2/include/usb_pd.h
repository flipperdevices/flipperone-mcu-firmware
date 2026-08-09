/**
 * @file usb_pd.h
 * @brief Self-contained USB-C PD / UCSI PPM service built on top of the
 *        UcsiPpm core (lib/usb).
 *
 * Unlike the bare UcsiPpm core, this library owns its own runtime:
 * - a dedicated worker thread running a FuriEventLoop;
 * - the FUSB302 I2C traffic (bus acquire/release included);
 * - the FUSB302 interrupt (attached through the BSP expander);
 * - all state machine timers.
 *
 * The runtime is event-driven, not polled: the worker sleeps until either
 * a FUSB302 interrupt / UCSI write / power-ready notification arrives, or
 * the next state-machine deadline reported by the core
 * (ucsi_ppm_next_timeout_ms) expires — a one-shot timer is re-armed to the
 * exact deadline after every piece of activity. With nothing attached and
 * no deadlines armed the thread does not wake at all (except the optional
 * lost-IRQ poll safety net).
 *
 * The only things left to the integrator are policy (PDO lists, CC mode)
 * and the power path (VBUS source enable / supply voltage programming),
 * because those belong to the product, not to the PD stack.
 *
 * OPM-facing UCSI channel
 * -----------------------
 * usb_pd_ucsi_read() / usb_pd_ucsi_write() implement the UCSI register
 * interface (VERSION / CCI / CONTROL / MESSAGE_IN / MESSAGE_OUT) for an
 * external OPM (e.g. the host CPU behind i2c_intercom). Both calls are
 * ISR-safe and accept any granularity — single bytes included:
 * - the whole 528-byte space is readable, served from an image of the
 *   register file kept coherent by the worker thread;
 * - writes are accepted for CONTROL and MESSAGE_OUT (the OPM-owned fields)
 *   and staged in that same image; the command is handed to the worker only
 *   when the last byte of CONTROL has been written (the UCSI doorbell), so
 *   partially written commands never dispatch.
 * When the PPM raises a UCSI alert (command completed / connector change),
 * the `ucsi_alert` callback fires from the worker thread AFTER the image
 * has been refreshed, so the OPM glue can immediately serve CCI reads.
 *
 * @warning Do not run this library together with another FUSB302 user
 *          (e.g. the legacy `pd` service) — they will race on the chip.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <furi.h>
#include <furi_hal_i2c_types.h>

#include <ucsi_ppm.h>
#include <ucsi_ppm_config.h>

#ifdef __cplusplus
extern "C" {
#endif

/* UCSI 3.0 Table 4-1 register file layout — exported for OPM glue code. */
#define USB_PD_UCSI_OFFSET_VERSION     0u
#define USB_PD_UCSI_SIZE_VERSION       3u
#define USB_PD_UCSI_OFFSET_CCI         4u
#define USB_PD_UCSI_SIZE_CCI           4u
#define USB_PD_UCSI_OFFSET_CONTROL     8u
#define USB_PD_UCSI_SIZE_CONTROL       8u
#define USB_PD_UCSI_OFFSET_MESSAGE_IN  16u
#define USB_PD_UCSI_SIZE_MESSAGE_IN    255u
#define USB_PD_UCSI_OFFSET_MESSAGE_OUT 272u
#define USB_PD_UCSI_SIZE_MESSAGE_OUT   255u
#define USB_PD_UCSI_REGFILE_SIZE       528u

typedef struct UsbPd UsbPd;

/** Events published on the pubsub returned by usb_pd_get_pubsub().
 * Callbacks run on the UsbPd worker thread — keep them short. */
typedef enum {
    UsbPdEventTypeConnectorStateChanged,
    UsbPdEventTypeContractChanged,
} UsbPdEventType;

typedef struct {
    UsbPdEventType type;
    UcsiPpmConnectorState connector_state;
    UcsiPpmContractInfo contract;
} UsbPdEvent;

/** Enable/disable sourcing VBUS (e.g. bq25792 OTG on/off).
 * Called from the worker thread. */
typedef void (*UsbPdVbusSourceFn)(void* context, bool enable);

/** Program the source supply to the requested voltage/current.
 * Called from the worker thread. Return false on failure — the PE will
 * treat it as a hard error on the contract.
 *
 * If `power_supply_ready_async` is false (default), the library assumes the
 * rail settles synchronously and signals PS_RDY readiness right away.
 * If true, the integrator must call usb_pd_notify_power_supply_ready()
 * once the rail has actually settled. */
typedef bool (*UsbPdPowerSupplySetFn)(void* context, uint16_t voltage_mv, uint16_t current_limit_ma);

/** Whether the device currently has a power source other than VBUS
 * (battery, DC jack). Optional; NULL means "no". Worker thread context. */
typedef bool (*UsbPdHasAltPowerFn)(void* context);

/** UCSI alert towards the OPM: CCI has news (command completed or connector
 * change). Fired from the worker thread after the register file image is
 * refreshed — typically used to raise the intercom interrupt line. */
typedef void (*UsbPdUcsiAlertFn)(void* context);

typedef struct {
    /* --- Hardware --- */
    const FuriHalI2cBusHandle* i2c_bus; /**< NULL — main bus. */
    uint8_t fusb302_i2c_addr; /**< 0 — 0x22. */
    bool use_phy_irq; /**< Attach the FUSB302 IRQ via the BSP expander. */

    /* --- Timing --- */
    /** Lost-IRQ safety net: additionally poll FUSB302 interrupt registers
     * this often. 0 disables polling (pure IRQ-driven). If use_phy_irq is
     * false this becomes the only PHY event source and must be non-zero. */
    uint32_t phy_poll_period_ms;
    uint32_t thread_stack_size; /**< 0 — 2048 bytes. */

    /* --- Power path (product-specific, hence callbacks) --- */
    void* callback_context;
    UsbPdVbusSourceFn vbus_source_set; /**< Required. */
    UsbPdPowerSupplySetFn power_supply_set; /**< Required. */
    UsbPdHasAltPowerFn has_alt_power; /**< Optional. */
    bool power_supply_ready_async; /**< See UsbPdPowerSupplySetFn. */

    /* --- OPM link --- */
    UsbPdUcsiAlertFn ucsi_alert; /**< Optional. */

    /* --- Type-C / PD policy --- */
    UcsiPpmCcOperationMode cc_operation_mode;
    UcsiPpmDrpFirstRole drp_advertise_first;
    UcsiPpmRpCurrent source_rp_current;
    UcsiPpmPdoList source_caps; /**< First PDO must be Fixed 5 V. */
    UcsiPpmPdoList sink_caps; /**< First PDO must be Fixed 5 V. */

    /* --- UCSI GET_CAPABILITY feature flags --- */
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
} UsbPdConfig;

/** Fill a config with sane device defaults: DRP with 1.5 A Rp, single
 * 5 V / 3 A source and sink PDO, USB PD supported, VBUS-powered, USB2
 * capable, IRQ-driven with a 250 ms poll safety net. The caller still has
 * to provide the power path callbacks. */
void usb_pd_config_init_default(UsbPdConfig* config);

/** Allocate and start the PD service: spawns the worker thread, brings up
 * the FUSB302 over I2C, attaches the interrupt and starts the timers.
 * Blocks until the worker finished initialization.
 *
 * @return instance, or NULL if the FUSB302 bring-up failed (I2C errors,
 *         invalid policy config). */
UsbPd* usb_pd_alloc(const UsbPdConfig* config);

/** Stop the worker thread and release everything. Blocks until the thread
 * has joined. Safe to call with an attached partner (the chip is deinited
 * into a detached state). */
void usb_pd_free(UsbPd* instance);

/** Pubsub delivering UsbPdEvent. Callbacks run on the worker thread. */
FuriPubSub* usb_pd_get_pubsub(UsbPd* instance);

/** Thread-safe connector state snapshot. */
UcsiPpmConnectorState usb_pd_get_connector_state(UsbPd* instance);

/** Thread-safe contract snapshot.
 * @return true if an explicit PD contract is in place. */
bool usb_pd_get_contract(UsbPd* instance, UcsiPpmContractInfo* out);

/** Read from the UCSI register file. The whole 528-byte space is readable:
 * PPM-owned fields (VERSION / CCI / MESSAGE_IN) come from the core, and the
 * OPM-owned ones (CONTROL / MESSAGE_OUT) read back what the OPM wrote.
 * ISR-safe, non-blocking, any granularity — served from an image maintained
 * by the worker thread.
 * @return false if [offset, offset+length) is outside the register file. */
bool usb_pd_ucsi_read(UsbPd* instance, uint16_t offset, uint16_t length, uint8_t* data);

/** Store an OPM write to the UCSI register file. ISR-safe, non-blocking, any
 * granularity — writing register space byte by byte is fine: data is staged
 * into the register file image, and the command is dispatched on the worker
 * thread only once the LAST byte of CONTROL (offset 15) has been written —
 * that write is the UCSI doorbell. Per the UCSI flow the OPM must not start
 * a new command until CCI reports the previous one completed.
 * @return false unless [offset, offset+length) lies entirely within CONTROL
 *         or MESSAGE_OUT — the only OPM-writable fields. */
bool usb_pd_ucsi_write(UsbPd* instance, uint16_t offset, uint16_t length, const uint8_t* data);

/** Reset the PPM: drop any contract, re-init the FUSB302 and return the
 * register file to its post-boot state. Non-blocking — the reset runs on the
 * worker thread; watch CCI or the pubsub for completion.
 *
 * This is the out-of-band equivalent of the OPM issuing PPM_RESET, and going
 * through it rather than writing the opcode into CONTROL keeps local resets
 * from colliding with a host command already in flight. */
void usb_pd_reset(UsbPd* instance);

/** Signal that the source supply rail has settled after power_supply_set().
 * Only needed when power_supply_ready_async is true. ISR-safe. */
void usb_pd_notify_power_supply_ready(UsbPd* instance);

#ifdef __cplusplus
}
#endif
