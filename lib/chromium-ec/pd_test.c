#include "pd_test.h"
#include "config.h"
#include "driver/tcpm/tcpm.h"
#include "usb_pd_tcpm.h"
#include "source/driver/fusb302.h"

const struct tcpc_config_t tcpc_config[CONFIG_USB_PD_PORT_MAX_COUNT] = {
    [0] =
        {
            .i2c_info =
                {
                    .port = 0,
                    .addr_flags = FUSB302_I2C_ADDR_FLAGS,
                },
            .drv = &fusb302_tcpm_drv,
        },
};

void pd_test(void) {
    tcpm_init(0);
}
