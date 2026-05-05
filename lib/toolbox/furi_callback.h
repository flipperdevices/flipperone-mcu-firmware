#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*FuriCallback)(void* context);

typedef struct {
    FuriCallback callback;
    void* context;
} FuriCallbackWithContext;

#ifdef __cplusplus
}
#endif
