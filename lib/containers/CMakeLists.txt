project("containers")

add_library(${PROJECT_NAME} INTERFACE)

set(INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Expand source file globs into a list before passing to target_sources
file(GLOB CONTAINERS_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
)

target_sources(${PROJECT_NAME} INTERFACE ${CONTAINERS_SOURCES})

target_include_directories(${PROJECT_NAME} INTERFACE ${INCLUDES})




