#pragma once

#include <status_lights/status_lights.h>

void status_lights_notification_send(const StatusLightsNotification** notifications);

extern const StatusLightsNotification* notification_all_leds_off[];
extern const StatusLightsNotification* notification_power_red[];
extern const StatusLightsNotification* notification_all_leds_on[];
extern const StatusLightsNotification* notification_all_leds_white[];
