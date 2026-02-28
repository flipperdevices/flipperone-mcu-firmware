#include <status_lights/status_lights_notification.h>

// all leds off

const StatusLightsNotificationItem notification_all_leds_off_items[] = {
    {.status_lights_type = StatusLightsTypeLineAllOff},
};

const StatusLightsNotification notification_all_leds_off = {
    .notifications = notification_all_leds_off_items,
    .notification_count = COUNT_OF(notification_all_leds_off_items),
};

// power red

const StatusLightsNotificationItem notification_power_red_items[] = {
    {.status_lights_type = StatusLightsTypePower, .color = STATUS_LIGHTS_COLOR_RED},
};

const StatusLightsNotification notification_power_red = {
    .notifications = notification_power_red_items,
    .notification_count = COUNT_OF(notification_power_red_items),
};

// all leds on

const StatusLightsNotificationItem notification_all_leds_on_items[] = {
    {.status_lights_type = StatusLightsTypeNet, .color = STATUS_LIGHTS_COLOR_BLUE},
    {.status_lights_type = StatusLightsTypeWiFi, .color = STATUS_LIGHTS_COLOR_BLUE},
    {.status_lights_type = StatusLightsTypeEth2, .color = STATUS_LIGHTS_COLOR_BLUE},
    {.status_lights_type = StatusLightsTypeEth1, .color = STATUS_LIGHTS_COLOR_BLUE},
    {.status_lights_type = StatusLightsTypePower, .color = STATUS_LIGHTS_COLOR_GREEN},
    {.status_lights_type = StatusLightsTypeBatteryOutline, .color = STATUS_LIGHTS_COLOR_GREEN},
    {.status_lights_type = StatusLightsTypeBatteryWatt1, .color = STATUS_LIGHTS_COLOR_RED},
    {.status_lights_type = StatusLightsTypeBatteryWatt2, .color = STATUS_LIGHTS_COLOR_RED},
    {.status_lights_type = StatusLightsTypeBatteryWatt3, .color = STATUS_LIGHTS_COLOR_YELLOW},
    {.status_lights_type = StatusLightsTypeBatteryWatt4, .color = STATUS_LIGHTS_COLOR_GREEN},
    {.status_lights_type = StatusLightsTypeUsbCharging, .color = STATUS_LIGHTS_COLOR_RED},
    {.status_lights_type = StatusLightsTypeUsbWatt1, .color = STATUS_LIGHTS_COLOR_RED},
    {.status_lights_type = StatusLightsTypeUsbWatt2, .color = STATUS_LIGHTS_COLOR_RED},
    {.status_lights_type = StatusLightsTypeUsbWatt3, .color = STATUS_LIGHTS_COLOR_YELLOW},
    {.status_lights_type = StatusLightsTypeUsbWatt4, .color = STATUS_LIGHTS_COLOR_GREEN},
    {.status_lights_type = StatusLightsTypeBatteryCenter, .color = STATUS_LIGHTS_COLOR_GREEN},
};

const StatusLightsNotification notification_all_leds_on = {
    .notifications = notification_all_leds_on_items,
    .notification_count = COUNT_OF(notification_all_leds_on_items),
};

// all leds white

const StatusLightsNotificationItem notification_all_leds_white_items[] = {
    {.status_lights_type = StatusLightsTypeNet, .color = STATUS_LIGHTS_COLOR_WHITE},
    {.status_lights_type = StatusLightsTypeWiFi, .color = STATUS_LIGHTS_COLOR_WHITE},
    {.status_lights_type = StatusLightsTypeEth2, .color = STATUS_LIGHTS_COLOR_WHITE},
    {.status_lights_type = StatusLightsTypeEth1, .color = STATUS_LIGHTS_COLOR_WHITE},
    {.status_lights_type = StatusLightsTypePower, .color = STATUS_LIGHTS_COLOR_WHITE},
    {.status_lights_type = StatusLightsTypeBatteryOutline, .color = STATUS_LIGHTS_COLOR_WHITE},
    {.status_lights_type = StatusLightsTypeBatteryWatt1, .color = STATUS_LIGHTS_COLOR_WHITE},
    {.status_lights_type = StatusLightsTypeBatteryWatt2, .color = STATUS_LIGHTS_COLOR_WHITE},
    {.status_lights_type = StatusLightsTypeBatteryWatt3, .color = STATUS_LIGHTS_COLOR_WHITE},
    {.status_lights_type = StatusLightsTypeBatteryWatt4, .color = STATUS_LIGHTS_COLOR_WHITE},
    {.status_lights_type = StatusLightsTypeUsbCharging, .color = STATUS_LIGHTS_COLOR_WHITE},
    {.status_lights_type = StatusLightsTypeUsbWatt1, .color = STATUS_LIGHTS_COLOR_WHITE},
    {.status_lights_type = StatusLightsTypeUsbWatt2, .color = STATUS_LIGHTS_COLOR_WHITE},
    {.status_lights_type = StatusLightsTypeUsbWatt3, .color = STATUS_LIGHTS_COLOR_WHITE},
    {.status_lights_type = StatusLightsTypeUsbWatt4, .color = STATUS_LIGHTS_COLOR_WHITE},
    {.status_lights_type = StatusLightsTypeBatteryCenter, .color = STATUS_LIGHTS_COLOR_WHITE},
};

const StatusLightsNotification notification_all_leds_white = {
    .notifications = notification_all_leds_white_items,
    .notification_count = COUNT_OF(notification_all_leds_white_items),
};

// functions

void status_lights_notification_send(const StatusLightsNotification* notifications) {
    StatusLights* status_lights = furi_record_open(RECORD_STATUS_LIGHTS);
    status_lights_set_color_batch(status_lights, notifications);
    furi_record_close(RECORD_STATUS_LIGHTS);
}
