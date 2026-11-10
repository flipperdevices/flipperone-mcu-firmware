# Firmware version - single source of truth.
#
# The numeric version packed into the picobin VERSION item (seen by the bootrom
# and by the host over I2C) and the human-readable version string (CLI, desktop,
# device info) are both derived here, in this order:
#
#   1. HEAD carries an exact tag X.Y.Z (git describe --tags --exact-match HEAD):
#      the tag wins - both the packed numbers and the string come from it.
#   2. Otherwise the FW_IMAGE_* fallback below is used (dev/nightly builds).
#   3. DIST_SUFFIX (CI) overrides the display string only, never the numbers.
#
# The string always carries the commit hash and the dirty flag so no two builds
# are ever indistinguishable. Everything is computed at configure time: the
# bootrom on silicon B3 rejects images whose picobin was modified after the
# build, so nothing here may be changed at runtime or at flash time.

# Dev fallback when HEAD is not tagged. Bump locally to make a dev build
# "newer" than the installed one for normal-boot partition selection
# (major 0-15, minor 0-255, patch 0-15 - the picobin packing limits).
set(FW_IMAGE_MAJOR 0)
set(FW_IMAGE_MINOR 3)
set(FW_IMAGE_PATCH 0)

# --- git facts ---------------------------------------------------------------

execute_process(
    COMMAND git -C ${CMAKE_SOURCE_DIR} rev-parse --short HEAD
    OUTPUT_VARIABLE GIT_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(DEFINED ENV{COMMIT_SHA} AND NOT "$ENV{COMMIT_SHA}" STREQUAL "")
    set(GIT_COMMIT "$ENV{COMMIT_SHA}")
endif()
if(NOT GIT_COMMIT)
    set(GIT_COMMIT "unknown")
endif()

execute_process(
    COMMAND git -C ${CMAKE_SOURCE_DIR} rev-parse --abbrev-ref HEAD
    OUTPUT_VARIABLE GIT_BRANCH OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(DEFINED ENV{BRANCH_NAME} AND NOT "$ENV{BRANCH_NAME}" STREQUAL "")
    set(GIT_BRANCH "$ENV{BRANCH_NAME}")
endif()
if(NOT GIT_BRANCH)
    set(GIT_BRANCH "unknown")
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

# --- tag precedence ----------------------------------------------------------

execute_process(
    COMMAND git -C ${CMAKE_SOURCE_DIR} describe --tags --exact-match HEAD
    OUTPUT_VARIABLE FW_GIT_EXACT_TAG OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)

if(FW_GIT_EXACT_TAG)
    if(NOT FW_GIT_EXACT_TAG MATCHES "^v?([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
        message(FATAL_ERROR "Version tag '${FW_GIT_EXACT_TAG}' is not X.Y.Z")
    endif()
    set(FW_IMAGE_MAJOR ${CMAKE_MATCH_1})
    set(FW_IMAGE_MINOR ${CMAKE_MATCH_2})
    set(FW_IMAGE_PATCH ${CMAKE_MATCH_3})
endif()

if(FW_IMAGE_MAJOR GREATER 15 OR FW_IMAGE_MINOR GREATER 255 OR FW_IMAGE_PATCH GREATER 15)
    message(FATAL_ERROR
        "Firmware version ${FW_IMAGE_MAJOR}.${FW_IMAGE_MINOR}.${FW_IMAGE_PATCH} "
        "does not fit the picobin packing (major 0-15, minor 0-255, patch 0-15)")
endif()

# --- version string: X.Y.Z-<hash>[-dirty], DIST_SUFFIX overrides -------------

set(FW_VERSION_STRING "${FW_IMAGE_MAJOR}.${FW_IMAGE_MINOR}.${FW_IMAGE_PATCH}-${GIT_COMMIT}")
if(BUILD_DIRTY)
    string(APPEND FW_VERSION_STRING "-dirty")
endif()
if(DEFINED ENV{DIST_SUFFIX} AND NOT "$ENV{DIST_SUFFIX}" STREQUAL "")
    set(FW_VERSION_STRING "$ENV{DIST_SUFFIX}")
endif()

# --- packed picobin version ---------------------------------------------------

math(EXPR VERSION_PACKED "(${FW_IMAGE_MAJOR} & 0x0F) << 12 | (${FW_IMAGE_MINOR} & 0xFF) << 4 | (${FW_IMAGE_PATCH} & 0x0F)")
# The picobin MINOR word is the updater's boot counter: it stays 0 forever.
# Bumping it after the build makes the image unbootable on silicon B3.
pico_set_binary_version(${PROJECT_NAME} MAJOR ${VERSION_PACKED} MINOR 0)
pico_set_program_version(${PROJECT_NAME} "${FW_IMAGE_MAJOR}.${FW_IMAGE_MINOR}")

math(EXPR VERSION_PACKED_HEX "${VERSION_PACKED}" OUTPUT_FORMAT HEXADECIMAL)
message(STATUS "Firmware version: ${FW_VERSION_STRING} (packed ${VERSION_PACKED_HEX})")

# Compile definitions consumed by lib/corelibs/lib/version/version.c
set(FW_VERSION_COMPILE_DEFS
    "GIT_COMMIT=\"${GIT_COMMIT}\""
    "GIT_BRANCH=\"${GIT_BRANCH}\""
    "VERSION=\"${FW_VERSION_STRING}\""
    "BUILD_DIRTY=${BUILD_DIRTY}"
    "GIT_ORIGIN=\"${GIT_ORIGIN}\""
    "BUILD_DATE=\"${BUILD_DATE}\""
    "TARGET=${FIRMWARE_TARGET}"
    "FIRMWARE_ORIGIN=\"${FIRMWARE_ORIGIN}\""
)

# version.c lives in the lib/corelibs submodule; attach it directly from here so
# the version refactor does not require committing changes inside the submodule.
target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/lib/corelibs/lib/version/version.c)
set_source_files_properties(${CMAKE_SOURCE_DIR}/lib/corelibs/lib/version/version.c PROPERTIES
    COMPILE_DEFINITIONS "${FW_VERSION_COMPILE_DEFS}")
