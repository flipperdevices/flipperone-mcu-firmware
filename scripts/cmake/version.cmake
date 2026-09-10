# Build-time firmware version metadata.
#
# lib/corelibs/lib/version/version.c includes "version.inc.h" and expects it to
# define GIT_COMMIT, GIT_BRANCH, VERSION, BUILD_DIRTY, GIT_ORIGIN, BUILD_DATE,
# TARGET and FIRMWARE_ORIGIN. corelibs ships its own CMakeLists.txt for this
# that computes the values with execute_process() at configure time — which
# means they only move on a reconfigure, and BUILD_DIRTY reflects whatever the
# tree looked like when cmake last ran. This replaces it with a target that
# regenerates the header on every build; version_inc.cmake only touches the
# file when the content changes, so version.c is recompiled exactly when the
# metadata does. Ninja's restat takes care of not propagating an unchanged
# header further.
#
# Expects ${PROJECT_NAME}, FIRMWARE_TARGET and FIRMWARE_ORIGIN to be defined.

include_guard(GLOBAL)

set(VERSION_INC_DIR "${CMAKE_BINARY_DIR}/generated/version")
set(VERSION_INC_H "${VERSION_INC_DIR}/version.inc.h")

# No OUTPUT, so the target is never up to date and runs on every build
add_custom_target(version_inc
    COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_DIR=${CMAKE_SOURCE_DIR}"
        "-DOUTPUT=${VERSION_INC_H}"
        "-DFIRMWARE_TARGET=${FIRMWARE_TARGET}"
        "-DFIRMWARE_ORIGIN=${FIRMWARE_ORIGIN}"
        -P "${CMAKE_CURRENT_LIST_DIR}/version_inc.cmake"
    BYPRODUCTS "${VERSION_INC_H}"
    COMMENT "Updating version.inc.h"
    VERBATIM
)
add_dependencies(${PROJECT_NAME} version_inc)

# Only version.c sees the generated directory, and it is ordered after the
# generator so the very first build finds the header too.
set(_version_c "${CMAKE_SOURCE_DIR}/lib/corelibs/lib/version/version.c")
set_source_files_properties("${_version_c}" PROPERTIES
    INCLUDE_DIRECTORIES "${VERSION_INC_DIR}"
    OBJECT_DEPENDS "${VERSION_INC_H}"
)
