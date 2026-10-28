#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Runs the full L1 (transport + regfile) unit-test suite. Returns true if all
// tests pass. Failures are logged via FURI_LOG_E with module name, file, line,
// and the failing assertion. Must be called from a task context — the test
// suite allocates/frees UcsiPpm instances through the project allocator.
bool ucsi_ppm_test_run(void);

#ifdef __cplusplus
}
#endif
