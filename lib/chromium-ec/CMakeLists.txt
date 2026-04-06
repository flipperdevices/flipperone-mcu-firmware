project("chromium-ec")

set(INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set(PRIVATE_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/glue
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/driver
)

set(SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/fusb302.c
)

add_library(${PROJECT_NAME} STATIC
    ${SOURCES}
)

target_include_directories(${PROJECT_NAME} 
    PUBLIC ${INCLUDES}
    PRIVATE ${PRIVATE_INCLUDES}
)

target_link_libraries(${PROJECT_NAME}
    mlib
    cmsis_core
)
