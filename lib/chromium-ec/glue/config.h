#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define CONFIG_USB_PD_PORT_MAX_COUNT 1

#ifdef CONFIG_USB_PD_REV30
#define CONFIG_PD_RETRY_COUNT 2
#else
#define CONFIG_PD_RETRY_COUNT 3
#endif

enum gpio_signal {
    TODO // FIX ME,
};

#ifdef __cplusplus
}
#endif
