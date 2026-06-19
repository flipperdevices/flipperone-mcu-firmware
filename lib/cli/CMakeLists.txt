project("cli")

add_library(${PROJECT_NAME} INTERFACE)

set(INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/shell
)

# Expand source file globs into a list before passing to target_sources
file(GLOB CLI_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/shell/*.c"
)

target_sources(${PROJECT_NAME} INTERFACE ${CLI_SOURCES})

target_include_directories(${PROJECT_NAME} INTERFACE ${INCLUDES})




