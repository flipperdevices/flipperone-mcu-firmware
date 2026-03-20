/* Configuration for Flipper One */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

/* USB PD config */
#define CONFIG_USB_PD_TCPMV1
#define CONFIG_USB_PD_PORT_MAX_COUNT 1
#define CONFIG_USB_PD_TCPM_FUSB302
#define CONFIG_USB_PD_TCPM_TCPCI


/* Optional features */
#define CONFIG_USB_PID 0x5036
#define VPD_HW_VERSION 0x0001
#define VPD_FW_VERSION 0x0001

/* USB bcdDevice */
#define USB_BCD_DEVICE 0

/* Vbus impedance in milliohms */
#define VPD_VBUS_IMPEDANCE 65

/* GND impedance in milliohms */
#define VPD_GND_IMPEDANCE 33

/* Define typical operating power and max power */
#define PD_OPERATING_POWER_MW 15000
#define PD_MAX_POWER_MW       45000
#define PD_MAX_CURRENT_MA     3000
#define PD_MAX_VOLTAGE_MV	20000

#endif /* __CROS_EC_BOARD_H */