# Version source of truth: exact tag on HEAD wins; otherwise the newest
# merged semver tag, with the commit distance since it carried in PATCH.
# MAJOR and MINOR never move between releases.
# Picoblock packing: MAJOR(8) | MINOR(8) | PATCH(16) across the 32-bit
# major:minor version item (major slot: MAJOR<<8|MINOR, minor slot: PATCH).
# The picobin must not be modified after the build (silicon B3 rejects it).

# Bootstrap only if the repo has no semver tags at all (the release process
# never edits these).
set(FW_IMAGE_MAJOR 0)
set(FW_IMAGE_MINOR 3)
set(FW_IMAGE_PATCH 0)

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
else()
    # Untagged: newest semver tag merged into HEAD + commit distance.
    execute_process(
        COMMAND git -C ${CMAKE_SOURCE_DIR} tag --merged HEAD --sort=-version:refname
        OUTPUT_VARIABLE FW_GIT_TAGS OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(FW_GIT_TAGS MATCHES "v?([0-9]+)\\.([0-9]+)\\.([0-9]+)")
        set(FW_GIT_LAST_TAG "${CMAKE_MATCH_0}")
        set(FW_IMAGE_MAJOR ${CMAKE_MATCH_1})
        set(FW_IMAGE_MINOR ${CMAKE_MATCH_2})
        execute_process(
            COMMAND git -C ${CMAKE_SOURCE_DIR} rev-list --count "${FW_GIT_LAST_TAG}..HEAD"
            OUTPUT_VARIABLE FW_GIT_DISTANCE OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        if(NOT FW_GIT_DISTANCE)
            set(FW_GIT_DISTANCE 0)
        endif()
        # Distance rides in PATCH; MAJOR/MINOR stay at the tag values.
        math(EXPR FW_IMAGE_PATCH "${CMAKE_MATCH_3} + ${FW_GIT_DISTANCE}")
    endif()
endif()

if(FW_IMAGE_MAJOR GREATER 255 OR FW_IMAGE_MINOR GREATER 255 OR FW_IMAGE_PATCH GREATER 65535)
    message(FATAL_ERROR
        "Firmware version ${FW_IMAGE_MAJOR}.${FW_IMAGE_MINOR}.${FW_IMAGE_PATCH} "
        "does not fit the picobin packing (major/minor 0-255, patch 0-65535)")
endif()

set(FW_VERSION_STRING "${FW_IMAGE_MAJOR}.${FW_IMAGE_MINOR}.${FW_IMAGE_PATCH}-${GIT_COMMIT}")
if(BUILD_DIRTY)
    string(APPEND FW_VERSION_STRING "-dirty")
endif()
if(DEFINED ENV{DIST_SUFFIX} AND NOT "$ENV{DIST_SUFFIX}" STREQUAL "")
    set(FW_VERSION_STRING "$ENV{DIST_SUFFIX}")
endif()

# Version word (MAJOR<<8|MINOR) in the major slot; the minor slot is the
# boot counter/rollback word (B3 rejects nonzero), so PATCH stays in the
# build string.
math(EXPR FW_IMAGE_VERSION_WORD "((${FW_IMAGE_MAJOR} & 0xFF) << 8) | (${FW_IMAGE_MINOR} & 0xFF)")
pico_set_binary_version(${PROJECT_NAME} MAJOR ${FW_IMAGE_VERSION_WORD} MINOR 0)
pico_set_program_version(${PROJECT_NAME} "${FW_IMAGE_MAJOR}.${FW_IMAGE_MINOR}.${FW_IMAGE_PATCH}")

math(EXPR VERSION_PACKED "(${FW_IMAGE_MAJOR} << 24) | (${FW_IMAGE_MINOR} << 16) | ${FW_IMAGE_PATCH}")
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

# Attach version.c directly: no submodule changes needed.
target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/lib/corelibs/lib/version/version.c)
set_source_files_properties(${CMAKE_SOURCE_DIR}/lib/corelibs/lib/version/version.c PROPERTIES
    COMPILE_DEFINITIONS "${FW_VERSION_COMPILE_DEFS}")
