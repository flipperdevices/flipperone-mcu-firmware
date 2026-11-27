# Check the tools this configure uses against toolchain.json, the pinned
# flipperone-mcu-toolchain release. Include after project() and pico_sdk_init().
#
# - With the toolchain bundle (scripts/toolchain/fbtenv.sh, or the bundle's
#   cmake/initial-cache.cmake), the bundle's tag must be the pinned one, and the
#   compiler must come from it.
# - Without it, e.g. with the Raspberry Pi Pico VS Code extension, the Pico SDK and
#   GCC versions must match the pinned components. GCC 14.2.1 from Arm and from xPack
#   both pass. The two builds' C libraries are not byte-identical, so only the bundle
#   reproduces the binaries CI builds.
#
# -DFW_TOOLCHAIN_CHECK=WARN turns the errors into warnings, OFF skips the check.

set(FW_TOOLCHAIN_CHECK "ERROR" CACHE STRING "On a toolchain mismatch: ERROR, WARN or OFF")
set_property(CACHE FW_TOOLCHAIN_CHECK PROPERTY STRINGS ERROR WARN OFF)

function(_fw_toolchain_check)
    string(TOUPPER "${FW_TOOLCHAIN_CHECK}" mode)
    if(NOT mode MATCHES "^(ERROR|WARN|OFF)$")
        message(FATAL_ERROR "FW_TOOLCHAIN_CHECK is ${FW_TOOLCHAIN_CHECK}, not ERROR, WARN or OFF")
    endif()
    if(mode STREQUAL "OFF")
        return()
    endif()

    set(pin_file "${CMAKE_SOURCE_DIR}/toolchain.json")
    set_property(DIRECTORY "${CMAKE_SOURCE_DIR}" APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${pin_file}")
    file(READ "${pin_file}" pin)
    string(JSON tag GET "${pin}" toolchain)
    string(JSON sdk_version GET "${pin}" components pico-sdk version)
    string(JSON gcc_version GET "${pin}" components arm-none-eabi-gcc version)
    # xPack versions carry a build suffix: 14.2.1-1.1 is GCC 14.2.1
    string(REGEX REPLACE "-.*" "" gcc_version "${gcc_version}")

    set(problems "")
    set(hint "")

    # The Pico VS Code extension reads its versions from the top of CMakeLists.txt
    string(JSON picotool_version GET "${pin}" components picotool version)
    string(REGEX MATCH "^[0-9]+\\.[0-9]+" gcc_major_minor "${gcc_version}")
    string(REPLACE "_" "." vscode_gcc "${toolchainVersion}")
    string(REGEX MATCH "^[0-9]+\\.[0-9]+" vscode_gcc "${vscode_gcc}")
    if(NOT sdkVersion STREQUAL sdk_version OR NOT picotoolVersion STREQUAL picotool_version
       OR NOT vscode_gcc STREQUAL gcc_major_minor)
        list(APPEND problems
            "CMakeLists.txt: sdkVersion/toolchainVersion/picotoolVersion (${sdkVersion}, "
            "${toolchainVersion}, ${picotoolVersion}) do not match toolchain.json. Run "
            "'python3 scripts/toolchain/toolchain.py check'.\n")
    endif()

    get_filename_component(sdk_parent "${PICO_SDK_PATH}/.." ABSOLUTE)
    set(bundle_tag "")
    if(EXISTS "${sdk_parent}/toolchain.json")
        file(READ "${sdk_parent}/toolchain.json" bundle)
        string(JSON bundle_kind ERROR_VARIABLE err GET "${bundle}" kind)
        if(bundle_kind STREQUAL "flipperone-mcu-toolchain")
            string(JSON bundle_tag GET "${bundle}" tag)
        endif()
    endif()

    if(bundle_tag)
        if(NOT bundle_tag STREQUAL tag)
            list(APPEND problems
                "The toolchain bundle in ${sdk_parent} is ${bundle_tag}, this tree pins ${tag}.\n")
        endif()
        # Real paths: either side may have been reached through a symlink
        file(REAL_PATH "${sdk_parent}" bundle_real)
        file(REAL_PATH "${CMAKE_C_COMPILER}" compiler_real)
        cmake_path(IS_PREFIX bundle_real "${compiler_real}" NORMALIZE compiler_in_bundle)
        if(NOT compiler_in_bundle)
            list(APPEND problems
                "The compiler ${CMAKE_C_COMPILER} is not the one of the toolchain bundle in ${sdk_parent}.\n")
        endif()
        set(hint "Source scripts/toolchain/fbtenv.sh (fbtenv.ps1 on Windows) and configure a fresh build directory.")
        set(source "flipperone-mcu-toolchain ${bundle_tag}")
    else()
        if(DEFINED ENV{FLIPPERONE_TOOLCHAIN_ROOT})
            list(APPEND problems
                "fbtenv is set up, but the Pico SDK is ${PICO_SDK_PATH}, not the toolchain's.\n")
        endif()
        if(NOT PICO_SDK_VERSION_STRING STREQUAL sdk_version)
            list(APPEND problems "Pico SDK ${PICO_SDK_VERSION_STRING} in ${PICO_SDK_PATH}, toolchain.json pins ${sdk_version}.\n")
        endif()
        if(NOT CMAKE_C_COMPILER_VERSION VERSION_EQUAL gcc_version)
            list(APPEND problems "GCC ${CMAKE_C_COMPILER_VERSION} (${CMAKE_C_COMPILER}), toolchain.json pins ${gcc_version}.\n")
        endif()
        if(DEFINED ENV{FLIPPERONE_TOOLCHAIN_ROOT})
            set(hint "Configure a fresh build directory.")
        else()
            string(CONCAT hint "Use the pinned toolchain: source scripts/toolchain/fbtenv.sh "
                "(fbtenv.ps1 on Windows), or install the versions above.")
        endif()
        string(CONCAT source "Pico SDK ${PICO_SDK_VERSION_STRING}, GCC ${CMAKE_C_COMPILER_VERSION}, "
            "not the toolchain bundle, so the binaries may differ from CI's")
    endif()

    if(NOT problems)
        message(STATUS "Toolchain: ${source}")
        return()
    endif()
    string(JOIN "" problems ${problems})
    set(text "Toolchain mismatch:\n${problems}${hint}\n-DFW_TOOLCHAIN_CHECK=WARN builds anyway.")
    if(mode STREQUAL "WARN")
        message(WARNING "${text}")
    else()
        message(FATAL_ERROR "${text}")
    endif()
endfunction()

_fw_toolchain_check()
