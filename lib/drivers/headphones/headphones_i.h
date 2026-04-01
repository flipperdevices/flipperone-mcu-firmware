#pragma once

typedef enum {
    HeadphonesStatusNone = 0,
    HeadphonesStatusPresent = (1 << 0),
    HeadphonesStatusMicrophonePresent = (1 << 1),
    HeadphonesStatusKeyPressedA = (1 << 2),
    HeadphonesStatusKeyPressedB = (1 << 3),
    HeadphonesStatusKeyPressedC = (1 << 4),
    HeadphonesStatusKeyPressedD = (1 << 5),
    HeadphonesStatusUnknown = 0xFFFF,
} HeadphonesStatus;
