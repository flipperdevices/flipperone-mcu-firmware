#pragma once
#include <stdbool.h>

typedef struct {
    const char* name;
    bool (*fn)(void);
} TestEntry;

#define TEST_ENTRY(f) {#f, f}

#define TEST_ASSERT(cond)                                                 \
    do {                                                                  \
        if(!(cond)) {                                                     \
            FURI_LOG_E(TAG, "FAIL %s:%d: %s", __func__, __LINE__, #cond); \
            return false;                                                 \
        }                                                                 \
    } while(0)

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif
