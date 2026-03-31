# Git version info — applied as compile definitions to version.c only

execute_process(
    COMMAND git -C ${CMAKE_SOURCE_DIR} rev-parse --short HEAD
    OUTPUT_VARIABLE GIT_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT GIT_COMMIT)
    set(GIT_COMMIT "unknown")
endif()

execute_process(
    COMMAND git -C ${CMAKE_SOURCE_DIR} rev-parse --abbrev-ref HEAD
    OUTPUT_VARIABLE GIT_BRANCH OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT GIT_BRANCH)
    set(GIT_BRANCH "unknown")
endif()

execute_process(
    COMMAND git -C ${CMAKE_SOURCE_DIR} describe --tags --abbrev=0
    OUTPUT_VARIABLE GIT_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT GIT_VERSION)
    execute_process(
        COMMAND git -C ${CMAKE_SOURCE_DIR} rev-list --count HEAD
        OUTPUT_VARIABLE GIT_COMMIT_COUNT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(GIT_COMMIT_COUNT)
        set(GIT_VERSION "r${GIT_COMMIT_COUNT}")
    else()
        set(GIT_VERSION "unknown")
    endif()
endif()

execute_process(
    COMMAND git -C ${CMAKE_SOURCE_DIR} status --porcelain
    OUTPUT_VARIABLE GIT_DIRTY_CHECK OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(GIT_DIRTY_CHECK)
    set(BUILD_DIRTY 1)
else()
    set(BUILD_DIRTY 0)
endif()

execute_process(
    COMMAND git -C ${CMAKE_SOURCE_DIR} remote get-url origin
    OUTPUT_VARIABLE GIT_ORIGIN OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT GIT_ORIGIN)
    set(GIT_ORIGIN "unknown")
endif()

string(TIMESTAMP BUILD_DATE "%Y-%m-%d")

set(VERSION_COMPILE_DEFS
    "GIT_COMMIT=\"${GIT_COMMIT}\""
    "GIT_BRANCH=\"${GIT_BRANCH}\""
    "VERSION=\"${GIT_VERSION}\""
    "BUILD_DIRTY=${BUILD_DIRTY}"
    "GIT_ORIGIN=\"${GIT_ORIGIN}\""
    "BUILD_DATE=\"${BUILD_DATE}\""
    "TARGET=${FIRMWARE_TARGET}"
    "FIRMWARE_ORIGIN=\"${FIRMWARE_ORIGIN}\""
)

target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/version.c)
set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/version.c PROPERTIES
    COMPILE_DEFINITIONS "${VERSION_COMPILE_DEFS}")
