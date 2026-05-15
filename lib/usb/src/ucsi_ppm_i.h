#pragma once

#include "ucsi_ppm.h"

#ifdef __cplusplus
extern "C" {
#endif

// UCSI 3.0 Table 4-1: register file layout. Total size 528 bytes.
#define UCSI_PPM_REGFILE_SIZE 528u

#define UCSI_PPM_OFFSET_VERSION     0u
#define UCSI_PPM_SIZE_VERSION       3u
#define UCSI_PPM_OFFSET_RESERVED1   3u
#define UCSI_PPM_SIZE_RESERVED1     1u
#define UCSI_PPM_OFFSET_CCI         4u
#define UCSI_PPM_SIZE_CCI           4u
#define UCSI_PPM_OFFSET_CONTROL     8u
#define UCSI_PPM_SIZE_CONTROL       8u
#define UCSI_PPM_OFFSET_MESSAGE_IN  16u
#define UCSI_PPM_SIZE_MESSAGE_IN    255u
#define UCSI_PPM_OFFSET_RESERVED2   271u
#define UCSI_PPM_SIZE_RESERVED2     1u
#define UCSI_PPM_OFFSET_MESSAGE_OUT 272u
#define UCSI_PPM_SIZE_MESSAGE_OUT   255u
#define UCSI_PPM_OFFSET_RESERVED3   527u
#define UCSI_PPM_SIZE_RESERVED3     1u

// CONTROL[0] is the Command opcode byte. A non-zero write to this byte
// triggers command processing (architecture.md §2, api.md §6).
#define UCSI_PPM_OFFSET_CONTROL_COMMAND UCSI_PPM_OFFSET_CONTROL

typedef enum {
    UcsiPpmLifecycleAllocated,
    UcsiPpmLifecycleInitialized,
} UcsiPpmLifecycle;

struct UcsiPpm {
    UcsiPpmLifecycle lifecycle;
    UcsiPpmConfig config;
    uint8_t regfile[UCSI_PPM_REGFILE_SIZE];
};

#ifdef __cplusplus
}
#endif
