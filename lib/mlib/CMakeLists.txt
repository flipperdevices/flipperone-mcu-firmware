project("mlib")

add_library(${PROJECT_NAME} INTERFACE)

set(INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/mlib
)

target_include_directories(${PROJECT_NAME} INTERFACE ${INCLUDES})