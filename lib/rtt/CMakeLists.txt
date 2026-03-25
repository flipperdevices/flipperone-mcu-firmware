project("rtt")

add_library(${PROJECT_NAME} STATIC
   ${CMAKE_CURRENT_SOURCE_DIR}/segger_rtt/RTT/SEGGER_RTT.c
)

set(INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/segger_rtt/RTT
    ${CMAKE_CURRENT_SOURCE_DIR}/segger_rtt/Config
)

target_include_directories(${PROJECT_NAME} PUBLIC ${INCLUDES})