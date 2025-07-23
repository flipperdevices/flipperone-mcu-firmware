project("ws2812")

add_library(${PROJECT_NAME} INTERFACE)

pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)

set(INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_sources(${PROJECT_NAME} INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/ws2812.c
)

target_link_libraries(${PROJECT_NAME} INTERFACE
        hardware_pio
        )

target_include_directories(${PROJECT_NAME} INTERFACE ${INCLUDES})




