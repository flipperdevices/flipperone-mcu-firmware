#pragma once
#include <drivers/headphones/headphones_i.h>

#define RECORD_HEADPHONES "headphones"
typedef struct Headphones Headphones;

typedef struct {
    HeadphonesStatus hp_status;
} HeadphonesEvent;

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif
