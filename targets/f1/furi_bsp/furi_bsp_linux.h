#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Reset the system via the expander. This is a hard reset and will not return.
 */
void furi_bsp_linux_reset(void);

/** Check if Linux booting is allowed. 
 * This checks if the expander is set to allow Linux booting.
 * @return true if Linux booting is allowed, false otherwise.
*/
bool furi_bsp_linux_is_load(void);

/** Start Linux booting via the expander. */
void furi_bsp_linux_start(void);

/** Set the expander to mask ROM mode. */
void furi_bsp_linux_maskrom(void);

#ifdef __cplusplus
}
#endif
