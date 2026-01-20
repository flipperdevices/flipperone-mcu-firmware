project("i2c_multi")

add_library(${PROJECT_NAME} INTERFACE)

pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/i2c_multi.pio)

set(INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_sources(${PROJECT_NAME} INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/i2c_multi.c
)

target_link_libraries(${PROJECT_NAME} INTERFACE
    hardware_pio
)

target_include_directories(${PROJECT_NAME} INTERFACE ${INCLUDES})




