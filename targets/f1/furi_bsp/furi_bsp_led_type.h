#pragma once

typedef enum {
    FuriBspLedTypeNet,
    FuriBspLedTypeWiFi,
    FuriBspLedTypeEth2,
    FuriBspLedTypeEth1,

    FuriBspLedTypePower,
    FuriBspLedTypeBatteryOutline,
    FuriBspLedTypeBatteryWatt1,
    FuriBspLedTypeBatteryWatt2,
    FuriBspLedTypeBatteryWatt3,
    FuriBspLedTypeBatteryWatt4,

    FuriBspLedTypeUsbCharging,
    FuriBspLedTypeUsbWatt1,
    FuriBspLedTypeUsbWatt2,
    FuriBspLedTypeUsbWatt3,
    FuriBspLedTypeUsbWatt4,
    FuriBspLedTypeBatteryCenter,

    // special types
    FuriBspLedTypeAllOff, // special type to turn all LEDs off

    FuriBspLedTypeBatteryOutline1, // additional battery outline LEDs 1
    FuriBspLedTypeBatteryOutline2, // additional battery outline LEDs 2

} FuriBspLedType;
