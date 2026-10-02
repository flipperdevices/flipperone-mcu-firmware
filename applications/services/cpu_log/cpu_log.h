#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RECORD_CPU_LOG "CpuLog"

typedef struct CpuLogSrv CpuLogSrv;

typedef struct {
    void (*callback)(const uint8_t* data, size_t size, void* context);
    void* context;
} CpuLogHandler;

void cpu_log_add_handler(CpuLogSrv* app, CpuLogHandler* handler);

void cpu_log_remove_handler(CpuLogSrv* app, CpuLogHandler* handler);

void cpu_log_clear(CpuLogSrv* app);
