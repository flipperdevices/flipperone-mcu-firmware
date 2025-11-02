#pragma once

void flipper_init(void);

// Weak symbol to be implemented by target-specific code
void flipper_init_services(void);
