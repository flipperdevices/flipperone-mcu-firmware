#pragma once

typedef enum {
    HeadphonesStatusNone = 0,
    HeadphonesStatusDisconnected = (1 << 0),
    HeadphonesStatusConnected = (1 << 1),
    HeadphonesStatusMicrophoneConnected = (1 << 2),
    HeadphonesStatusKeyPressedA = (1 << 3),
    HeadphonesStatusKeyPressedB = (1 << 4),
    HeadphonesStatusKeyPressedC = (1 << 5),
    HeadphonesStatusKeyPressedD = (1 << 6),
    HeadphonesStatusUnknown = 0xFFFF,
} HeadphonesStatus;
