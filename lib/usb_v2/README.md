# usb_v2 — self-hosted USB-C PD / UCSI service

OS-native shell around the `UcsiPpm` core from `lib/usb`. The core stays a
pure state machine; this library owns the runtime around it:

- a dedicated worker thread with a `FuriEventLoop`;
- FUSB302 I2C traffic (bus acquire/release included);
- the FUSB302 interrupt, attached through the BSP expander;
- all state machine timers;
- an ISR-safe UCSI register channel for an external OPM (host CPU behind
  `i2c_intercom`).

The runtime is fully event-driven, there is no periodic tick. The worker
sleeps until a FUSB302 interrupt, a UCSI write or a power-ready notification
arrives, or until the next state-machine deadline reported by the core via
`ucsi_ppm_next_timeout_ms()` (Type-C debounce, PE protocol timers) expires —
a one-shot timer is re-armed to the exact deadline after every piece of
activity. With nothing attached the thread does not wake at all, except for
the optional lost-IRQ poll safety net (`phy_poll_period_ms`, default 250 ms,
0 to disable).

The integrator provides only product policy (PDO lists, CC mode, UCSI
capability flags) and the power path (VBUS source on/off, supply
programming) — those depend on the board, not on the PD stack.

## Usage

```c
#include <usb_pd.h>

static void vbus_source_set(void* ctx, bool enable) {
    Power* power = ctx;
    if(enable) {
        power_bq25792_set_otg_params(power, 5000, 1500);
        power_bq25792_otg_enable(power, true);
    } else {
        power_bq25792_otg_enable(power, false);
    }
}

static bool power_supply_set(void* ctx, uint16_t mv, uint16_t ma) {
    Power* power = ctx;
    return power_bq25792_set_otg_params(power, mv, ma);
}

static void ucsi_alert(void* ctx) {
    // Raise the intercom interrupt line so the host reads CCI.
}

void example(void) {
    UsbPdConfig config;
    usb_pd_config_init_default(&config);

    config.callback_context = furi_record_open(RECORD_POWER);
    config.vbus_source_set = vbus_source_set;
    config.power_supply_set = power_supply_set;
    config.ucsi_alert = ucsi_alert;

    config.source_caps.pdos[0] = ucsi_ppm_pdo_fixed_source(5000, 3000, true, false, true, true);
    config.source_caps.pdos[1] = ucsi_ppm_pdo_fixed_source(9000, 3000, true, false, true, true);
    config.source_caps.count = 2;

    UsbPd* pd = usb_pd_alloc(&config); // spawns the thread, brings up FUSB302
    furi_check(pd);
}
```

## OPM (UCSI-over-intercom) glue

Both calls are ISR-safe and non-blocking, so they can be used directly from
the `i2c_intercom` slave callbacks. Write granularity does not matter — the
glue can forward every byte as it lands in the intercom register space:

```c
// Host reads VERSION / CCI / MESSAGE_IN:
usb_pd_ucsi_read(pd, USB_PD_UCSI_OFFSET_CCI, USB_PD_UCSI_SIZE_CCI, buf);

// Host writes MESSAGE_OUT and CONTROL in any chunks, e.g. byte by byte:
usb_pd_ucsi_write(pd, offset, 1, &byte);
```

Both directions share one 528-byte image of the register file (plain memory,
no queue — nothing to overflow), split by field ownership: the worker
refreshes the PPM-owned fields (VERSION, CCI, MESSAGE_IN) from the core, and
the OPM writes the OPM-owned ones (CONTROL, MESSAGE_OUT). The two sets never
overlap, so a read is a single memcpy over the whole space and MESSAGE_OUT
reads back exactly what the host wrote. Writes outside CONTROL / MESSAGE_OUT
are rejected, matching UCSI.

The command is dispatched only when the **last byte of CONTROL** (offset 15)
is written: that write is the UCSI doorbell, and it is the only one that
wakes the worker. Writing the opcode byte first therefore does not trigger a
half-written command. `ucsi_alert` fires after the image refresh, so CCI is
always coherent by the time the host reacts.

## Notes

- Do not run together with another FUSB302 user (the legacy `pd` service in
  `applications/services/pd`) — they will race on the chip registers, and
  the BSP allows only one attached FUSB302 callback at a time.
- The FUSB302 interrupt callback is attached on start and detached on
  `usb_pd_free()` (`furi_bsp_expander_main_detach_fusb302_callback` waits
  for an in-flight invocation), so alloc/free cycles are fully symmetric.
- If `power_supply_ready_async` is set, call
  `usb_pd_notify_power_supply_ready()` once the programmed rail settles;
  otherwise readiness is signaled synchronously after `power_supply_set`.
