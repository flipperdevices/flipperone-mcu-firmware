#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void flipper_init(void);

// Weak symbol to be implemented by target-specific code
void flipper_init_services(void);

#ifdef __cplusplus
}
#endif
