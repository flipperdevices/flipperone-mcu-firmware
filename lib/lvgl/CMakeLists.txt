project("lvgl_lib")

add_library(${PROJECT_NAME} INTERFACE)

set(INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_sources(${PROJECT_NAME} INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/fonts/lv_font_tiny5_8.c
    ${CMAKE_CURRENT_SOURCE_DIR}/fonts/lv_font_tiny_6.c
    ${CMAKE_CURRENT_SOURCE_DIR}/fonts/lv_font_somybmp_7.c
    ${CMAKE_CURRENT_SOURCE_DIR}/fonts/lv_font_cubic_12.c
    ${CMAKE_CURRENT_SOURCE_DIR}/fonts/lv_font_ark_numerals_small_10.c
    ${CMAKE_CURRENT_SOURCE_DIR}/fonts/lv_font_ark_numerals_regular_10.c
    ${CMAKE_CURRENT_SOURCE_DIR}/fonts/lv_font_ark_numerals_condensed_10.c
)

target_compile_options(${PROJECT_NAME} INTERFACE
)

target_include_directories(${PROJECT_NAME} INTERFACE ${INCLUDES})

set(CONFIG_LV_BUILD_DEMOS OFF) # for demos
set(CONFIG_LV_BUILD_EXAMPLES OFF) # for examples
set(CONFIG_LV_USE_THORVG_INTERNAL OFF) # for ThorVG
set(LV_BUILD_CONF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h CACHE STRING "" FORCE)

add_subdirectory(lvgl)

target_link_libraries(${PROJECT_NAME} INTERFACE
    lvgl
)
