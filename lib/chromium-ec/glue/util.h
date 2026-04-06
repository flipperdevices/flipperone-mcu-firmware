#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DIV_ROUND_NEAREST
#define DIV_ROUND_NEAREST(x, y) (((x) + ((y) / 2)) / (y))
#endif

#ifdef __cplusplus
}
#endif
