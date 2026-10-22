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
    FuriBspLedTypeAllOff,
} FuriBspLedType;
