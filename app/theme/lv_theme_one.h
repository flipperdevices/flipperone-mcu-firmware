/**
 * @file lv_theme_one.h
 *
 */

#ifndef LV_THEME_ONE_H
#define LV_THEME_ONE_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "src/themes/lv_theme.h"
#include "src/display/lv_display.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Initialize the theme
 * @param disp pointer to display
 * @return a pointer to reference this theme later
 */
lv_theme_t* lv_theme_one_init(lv_display_t* disp);

/**
* Check if the theme is initialized
* @return true if default theme is initialized, false otherwise
*/
bool lv_theme_one_is_inited(void);

/**
 * Get simple theme
 * @return a pointer to simple theme, or NULL if this is not initialized
 */
lv_theme_t* lv_theme_one_get(void);

/**
 * Deinitialize the simple theme
 */
void lv_theme_one_deinit(void);

/**********************
 *      MACROS
 **********************/

#endif

#ifdef __cplusplus
} /*extern "C"*/
#endif
