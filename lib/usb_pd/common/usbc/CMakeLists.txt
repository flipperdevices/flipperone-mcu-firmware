# CMake conversion of lib/usb_pd/common/usbc/build.mk
# This file defines an object library `usb_pd_usbc` with the same
# conditional-source behaviour as the original build.mk.

# Declare options corresponding to Kconfig-like CONFIG_* variables used by the
# original Makefile. Set these on the CMake command-line if you want them enabled,
# e.g. -DCONFIG_USB_PD_TCPMV2=ON
option(CONFIG_USB_PD_TCPMV2 "Enable USB PD TCPMv2" OFF)
option(CONFIG_USB_TYPEC_SM "Enable Type-C state machines" OFF)
option(CONFIG_USB_PRL_SM "Enable protocol state machine" OFF)
option(CONFIG_USB_PE_SM "Enable Policy Engine state machines" OFF)
option(CONFIG_USB_VPD "Enable VPD" OFF)
option(CONFIG_USB_CTVPD "Enable CTVPD" OFF)
option(CONFIG_USB_DRP_ACC_TRYSRC "Enable DRP accessory try-src" OFF)
option(CONFIG_USB_PD_TBT_COMPAT_MODE "Enable TBT compatibility mode" OFF)
option(CONFIG_USB_PD_USB4 "Enable USB4 mode" OFF)
option(CONFIG_CMD_PD "Enable PD command" OFF)
option(CONFIG_USB_PD_HOST_CMD "Enable PD host commands" OFF)
option(CONFIG_TEST_USB_PE_SM "Test: USB PE SM" OFF)
option(CONFIG_TEST_SM "Test: SM" OFF)

# Collect sources following the logic of build.mk
set(usbc_sources)

if(CONFIG_USB_PD_TCPMV2)
    list(APPEND usbc_sources
        usb_sm.c
        usbc_task.c
    )

    if(CONFIG_USB_TYPEC_SM)
        if(CONFIG_USB_VPD)
            list(APPEND usbc_sources usb_tc_vpd_sm.c)
        endif()
        if(CONFIG_USB_CTVPD)
            list(APPEND usbc_sources usb_tc_ctvpd_sm.c)
        endif()
        if(CONFIG_USB_DRP_ACC_TRYSRC)
            list(APPEND usbc_sources usb_tc_drp_acc_trysrc_sm.c)
        endif()
    endif()

    if(CONFIG_USB_PRL_SM)
        list(APPEND usbc_sources usb_prl_sm.c)
    endif()

    if(CONFIG_USB_PE_SM)
        if(CONFIG_USB_VPD)
            list(APPEND usbc_sources usb_pe_ctvpd_sm.c)
        endif()
        if(CONFIG_USB_CTVPD)
            list(APPEND usbc_sources usb_pe_ctvpd_sm.c)
        endif()
        if(CONFIG_USB_DRP_ACC_TRYSRC)
            list(APPEND usbc_sources
                usb_pe_drp_sm.c
                usb_pd_dpm.c
                dp_alt_mode.c
            )
        endif()
        if(CONFIG_USB_PD_TBT_COMPAT_MODE)
            list(APPEND usbc_sources tbt_alt_mode.c)
        endif()
        if(CONFIG_USB_PD_USB4)
            list(APPEND usbc_sources usb_mode.c)
        endif()
        if(CONFIG_CMD_PD)
            list(APPEND usbc_sources usb_pd_console.c)
        endif()
        if(CONFIG_USB_PD_HOST_CMD)
            list(APPEND usbc_sources usb_pd_host.c)
        endif()
    endif()
endif()

# For testing (outside main CONFIG_USB_PD_TCPMV2 block in original Makefile)
if(CONFIG_TEST_USB_PE_SM)
    list(APPEND usbc_sources usb_pe_drp_sm.c)
endif()
if(CONFIG_TEST_SM)
    list(APPEND usbc_sources usb_sm.c)
endif()

# Create an object library so parent targets can consume the objects with
# $<TARGET_OBJECTS:usb_pd_usbc>
if(NOT "${usbc_sources}" STREQUAL "")
    add_library(usb_pd_usbc OBJECT)
    target_sources(usb_pd_usbc PRIVATE ${usbc_sources})
    target_include_directories(usb_pd_usbc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

    # Provide a convenience alias for linking
    add_library(usbc::usb_pd_usbc ALIAS usb_pd_usbc)
endif()

# Notes for integration:
# - Top-level CMake currently globbed sources from lib/usb_pd/common/usbc; to
#   use this conditional CMakeLists instead, remove that glob entry and add:
#       add_subdirectory(lib/usb_pd/common/usbc)
#       target_link_libraries(${PROJECT_NAME} PRIVATE usb_pd_usbc)
#   or append the object files directly to the executable's sources:
#       target_sources(${PROJECT_NAME} PRIVATE $<TARGET_OBJECTS:usb_pd_usbc>)
# - Enable CONFIG_* options via CMake cache (e.g. -DCONFIG_USB_PD_TCPMV2=ON)
#   so the correct sources are compiled in.
