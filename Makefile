# Command-line build for the Flipper One MCU firmware (Linux, macOS).
#
# A thin wrapper around the CMake project in this directory. It drives the same
# build directory as VS Code (build/), so the two can be used interchangeably.
#
#   make                  configure on first run, then build
#   make FW_TARGET=f2     reconfigure the build directory for another board
#   make flash            partition table + firmware over SWD (openocd)
#   make load             firmware over USB (picotool, board in BOOTSEL mode)
#   make help             every target and the resolved settings
#
# ARM GCC and the Pico SDK are taken from ~/.pico-sdk when the VS Code extension
# has installed the versions CMakeLists.txt asks for. Otherwise the versions CI
# builds with are fetched into TOOLCHAIN_DIR on first use. Set PICO_SDK_PATH and
# PICO_TOOLCHAIN_PATH to use your own copies instead.

BUILD_DIR     ?= build
TOOLCHAIN_DIR ?= $(or $(XDG_CACHE_HOME),$(HOME)/.cache)/flipperone-mcu-firmware
JOBS          ?= $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
CMAKE_ARGS    ?=

# The xpack build of GCC, the same one CI uses (.github/workflows/build.yml).
ARM_GCC_VERSION ?= 14.2.1-1.1

# `override`: values given on the command line are otherwise read-only.
override TOOLCHAIN_DIR := $(abspath $(TOOLCHAIN_DIR))

# ---------------------------------------------------------------------------
# Versions. The Pico SDK, toolchain and picotool versions are the ones pinned in
# the VS Code extension block of CMakeLists.txt, so there is one place to bump.
# ---------------------------------------------------------------------------
# A literal parenthesis would end the $(shell ...) for make, hence the variables.
lp := (
rp := )
cmake_setting = $(shell sed -n 's/^set$(lp)$(1) \(.*\)$(rp)$$/\1/p' CMakeLists.txt)
PICO_SDK_VERSION       := $(call cmake_setting,sdkVersion)
PICO_TOOLCHAIN_VERSION := $(call cmake_setting,toolchainVersion)
PICOTOOL_VERSION       := $(call cmake_setting,picotoolVersion)
ifeq ($(PICO_SDK_VERSION),)
$(error Could not read set(sdkVersion ...) from CMakeLists.txt; run make from the repository root)
endif

# ---------------------------------------------------------------------------
# Build settings. The build directory remembers the target and build type, like
# the VS Code "Select Target" task: a plain `make` keeps whatever was configured
# last, and passing a different value reconfigures (a full rebuild).
# ---------------------------------------------------------------------------
CMAKE_CACHE := $(BUILD_DIR)/CMakeCache.txt
cache_entry = $(shell sed -n 's/^$(1):[A-Z]*=//p' $(CMAKE_CACHE) 2>/dev/null)
CACHED_FW_TARGET  := $(call cache_entry,FW_TARGET)
CACHED_BUILD_TYPE := $(call cache_entry,CMAKE_BUILD_TYPE)

# Board built when the build directory does not say otherwise. CMake on its own,
# and so VS Code, defaults to f1 (see CMakeLists.txt).
FW_TARGET  ?= $(if $(CACHED_FW_TARGET),$(CACHED_FW_TARGET),f2)
BUILD_TYPE ?= $(if $(CACHED_BUILD_TYPE),$(CACHED_BUILD_TYPE),Release)
FW_TARGET  := $(FW_TARGET)
BUILD_TYPE := $(BUILD_TYPE)

ifneq ($(FW_TARGET)/$(BUILD_TYPE),$(CACHED_FW_TARGET)/$(CACHED_BUILD_TYPE))
NEED_CONFIGURE := configure
endif

# ---------------------------------------------------------------------------
# Host tools: PATH first, then what the VS Code extension installed.
# ---------------------------------------------------------------------------
CMAKE := $(or $(shell command -v cmake 2>/dev/null),$(firstword $(wildcard $(HOME)/.pico-sdk/cmake/*/bin/cmake)))
NINJA := $(or $(shell command -v ninja 2>/dev/null),$(firstword $(wildcard $(HOME)/.pico-sdk/ninja/*/ninja)))
ifneq ($(NINJA),)
GENERATOR      := Ninja
GENERATOR_ARGS := -G Ninja -DCMAKE_MAKE_PROGRAM=$(NINJA)
else
GENERATOR      := Unix Makefiles
GENERATOR_ARGS := -G "Unix Makefiles"
endif

ifeq ($(filter help,$(MAKECMDGOALS)),)
ifeq ($(CMAKE),)
$(error cmake not found. Ubuntu: sudo apt install cmake ninja-build   macOS: brew install cmake ninja)
endif
endif

# ---------------------------------------------------------------------------
# Pico SDK and ARM GCC.
# ---------------------------------------------------------------------------
VSCODE_SDK      := $(HOME)/.pico-sdk/sdk/$(PICO_SDK_VERSION)
VSCODE_GCC      := $(HOME)/.pico-sdk/toolchain/$(PICO_TOOLCHAIN_VERSION)
VSCODE_PICOTOOL := $(HOME)/.pico-sdk/picotool/$(PICOTOOL_VERSION)/picotool
FETCHED_SDK     := $(TOOLCHAIN_DIR)/pico-sdk-$(PICO_SDK_VERSION)
FETCHED_GCC     := $(TOOLCHAIN_DIR)/xpack-arm-none-eabi-gcc-$(ARM_GCC_VERSION)

# The VS Code integrated terminal exports both variables pointing into
# ~/.pico-sdk whether or not the extension has installed that version yet. A
# value from the environment that points nowhere is ignored with a warning; one
# given on the command line is passed through as is.
ifeq ($(origin PICO_SDK_PATH),environment)
ifeq ($(wildcard $(PICO_SDK_PATH)/pico_sdk_init.cmake),)
$(warning PICO_SDK_PATH=$(PICO_SDK_PATH) from the environment is not a Pico SDK checkout, ignoring it)
PICO_SDK_PATH :=
endif
endif
ifeq ($(origin PICO_TOOLCHAIN_PATH),environment)
ifeq ($(wildcard $(PICO_TOOLCHAIN_PATH)/bin/arm-none-eabi-gcc),)
$(warning PICO_TOOLCHAIN_PATH=$(PICO_TOOLCHAIN_PATH) from the environment has no bin/arm-none-eabi-gcc, ignoring it)
PICO_TOOLCHAIN_PATH :=
endif
endif

ifeq ($(PICO_SDK_PATH),)
PICO_SDK_PATH := $(if $(wildcard $(VSCODE_SDK)/pico_sdk_init.cmake),$(VSCODE_SDK),$(FETCHED_SDK))
endif
ifeq ($(PICO_TOOLCHAIN_PATH),)
PICO_TOOLCHAIN_PATH := $(if $(wildcard $(VSCODE_GCC)/bin/arm-none-eabi-gcc),$(VSCODE_GCC),$(FETCHED_GCC))
endif
override PICO_SDK_PATH       := $(abspath $(PICO_SDK_PATH))
override PICO_TOOLCHAIN_PATH := $(abspath $(PICO_TOOLCHAIN_PATH))

# CMakeLists.txt skips the VS Code hook when PICO_SDK_PATH is in the environment,
# so what is resolved here is what the build uses. Exported for every recipe, not
# only `configure`: a build that re-runs CMake by itself must see the same values.
export PICO_SDK_PATH PICO_TOOLCHAIN_PATH
# Where the SDK builds picotool when no matching one is installed. Outside the
# build directory so that `make distclean` does not throw it away.
export PICOTOOL_FETCH_FROM_GIT_PATH := $(TOOLCHAIN_DIR)/picotool-$(PICO_SDK_VERSION)

PICOTOOL_DIR_ARGS := $(if $(wildcard $(VSCODE_PICOTOOL)/picotoolConfig.cmake),-Dpicotool_DIR=$(VSCODE_PICOTOOL))

# A build directory configured against another SDK copy (by VS Code, or before
# the extension installed its own) keeps using it: the compiler is cached too,
# and CMake cannot switch it in place. Say so instead of silently diverging.
CACHED_SDK := $(call cache_entry,PICO_SDK_PATH)
ifneq ($(CACHED_SDK),)
ifneq ($(realpath $(CACHED_SDK)),$(realpath $(PICO_SDK_PATH)))
$(info note: $(BUILD_DIR) was configured with the Pico SDK at $(CACHED_SDK); run `make distclean` to switch to $(PICO_SDK_PATH))
endif
endif

TOOLCHAIN_DEPS :=
ifeq ($(PICO_SDK_PATH),$(FETCHED_SDK))
TOOLCHAIN_DEPS += $(FETCHED_SDK)/pico_sdk_init.cmake
endif
ifeq ($(PICO_TOOLCHAIN_PATH),$(FETCHED_GCC))
TOOLCHAIN_DEPS += $(FETCHED_GCC)/bin/arm-none-eabi-gcc
endif

# xpack publishes linux/darwin builds for x64 and arm64.
UNAME_S    := $(shell uname -s)
UNAME_M    := $(shell uname -m)
XPACK_OS   := $(if $(filter Darwin,$(UNAME_S)),darwin,$(if $(filter Linux,$(UNAME_S)),linux))
XPACK_ARCH := $(if $(filter x86_64 amd64,$(UNAME_M)),x64,$(if $(filter aarch64 arm64,$(UNAME_M)),arm64))
XPACK_GCC_TARBALL := xpack-arm-none-eabi-gcc-$(ARM_GCC_VERSION)-$(XPACK_OS)-$(XPACK_ARCH).tar.gz
XPACK_GCC_URL     := https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/download/v$(ARM_GCC_VERSION)/$(XPACK_GCC_TARBALL)

# ---------------------------------------------------------------------------
# Flashing tools. The openocd the VS Code extension ships knows the RP2350; a
# distribution openocd 0.12 does not, so the former is preferred when present.
# ---------------------------------------------------------------------------
VSCODE_OPENOCD := $(firstword $(wildcard $(HOME)/.pico-sdk/openocd/*/openocd))
ifeq ($(origin OPENOCD),undefined)
ifneq ($(VSCODE_OPENOCD),)
OPENOCD      := $(VSCODE_OPENOCD)
OPENOCD_ARGS := -s $(dir $(VSCODE_OPENOCD))scripts
else
OPENOCD      := $(shell command -v openocd 2>/dev/null)
endif
endif
OPENOCD_ARGS ?=

PICOTOOL ?= $(or $(wildcard $(VSCODE_PICOTOOL)/picotool),$(shell command -v picotool 2>/dev/null))

FIRMWARE := $(BUILD_DIR)/flipperone-mcu-firmware

# ---------------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------------
.DEFAULT_GOAL := all
.PHONY: all firmware configure toolchain flash load load-partition-table clean distclean help

all: firmware

firmware: $(NEED_CONFIGURE)
	$(CMAKE) --build $(BUILD_DIR) --parallel $(JOBS)

# The generator is fixed on the first configure; later reconfigures keep it.
configure: $(TOOLCHAIN_DEPS)
	$(CMAKE) -S . -B $(BUILD_DIR) $(if $(wildcard $(CMAKE_CACHE)),,$(GENERATOR_ARGS)) \
	    -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DFW_TARGET=$(FW_TARGET) $(PICOTOOL_DIR_ARGS) $(CMAKE_ARGS)

toolchain: $(TOOLCHAIN_DEPS)
	@echo "Pico SDK: $(PICO_SDK_PATH)"
	@echo "ARM GCC:  $(PICO_TOOLCHAIN_PATH)"

# flash.tcl writes the partition table, the firmware into slot A and invalidates
# slot B. Paths in it are relative to the current directory, hence BUILD_DIR.
flash: firmware
	$(if $(OPENOCD),,$(error openocd not found. RP2350 needs the Raspberry Pi build (https://github.com/raspberrypi/openocd) or the one the VS Code extension installs; or set OPENOCD=/path/to/openocd))
	$(OPENOCD) $(OPENOCD_ARGS) -f interface/cmsis-dap.cfg -f target/rp2350.cfg \
	    -c "set BUILD_DIR $(BUILD_DIR)" -f targets/flash.tcl

load: firmware
	$(if $(PICOTOOL),,$(error picotool not found. macOS: brew install picotool; Linux: https://github.com/raspberrypi/picotool; or set PICOTOOL=/path/to/picotool))
	$(PICOTOOL) load $(FIRMWARE).uf2 -fx

load-partition-table: firmware
	$(if $(PICOTOOL),,$(error picotool not found. macOS: brew install picotool; Linux: https://github.com/raspberrypi/picotool; or set PICOTOOL=/path/to/picotool))
	$(PICOTOOL) load $(BUILD_DIR)/partition_table.uf2 -fvx

clean:
	$(if $(wildcard $(CMAKE_CACHE)),$(CMAKE) --build $(BUILD_DIR) --target clean,@echo "$(BUILD_DIR) is not configured, nothing to clean")

distclean:
	rm -rf $(BUILD_DIR)

# ---------------------------------------------------------------------------
# Fetching. Each rule unpacks into a temporary name and renames at the end, so
# an interrupted download never leaves something that looks complete behind.
# The version is part of the directory name: bumping it fetches anew.
# ---------------------------------------------------------------------------
$(FETCHED_SDK)/pico_sdk_init.cmake:
	@echo "Fetching Pico SDK $(PICO_SDK_VERSION) into $(FETCHED_SDK)"
	@mkdir -p $(TOOLCHAIN_DIR) && rm -rf $(FETCHED_SDK) $(FETCHED_SDK).tmp
	git clone --depth 1 --branch $(PICO_SDK_VERSION) https://github.com/raspberrypi/pico-sdk.git $(FETCHED_SDK).tmp
	git -C $(FETCHED_SDK).tmp submodule update --init --depth 1
	@mv $(FETCHED_SDK).tmp $(FETCHED_SDK)

$(FETCHED_GCC)/bin/arm-none-eabi-gcc:
	$(if $(and $(XPACK_OS),$(XPACK_ARCH)),,$(error No xpack ARM GCC build for $(UNAME_S)/$(UNAME_M); install arm-none-eabi-gcc yourself and set PICO_TOOLCHAIN_PATH))
	@echo "Fetching ARM GCC $(ARM_GCC_VERSION) ($(XPACK_OS)-$(XPACK_ARCH)) into $(FETCHED_GCC)"
	@mkdir -p $(TOOLCHAIN_DIR) && rm -rf $(FETCHED_GCC) $(FETCHED_GCC).tmp
	curl -fL --retry 3 -o $(FETCHED_GCC).tar.gz $(XPACK_GCC_URL)
	@mkdir -p $(FETCHED_GCC).tmp && tar -xzf $(FETCHED_GCC).tar.gz -C $(FETCHED_GCC).tmp
	@mv $(FETCHED_GCC).tmp/xpack-arm-none-eabi-gcc-$(ARM_GCC_VERSION) $(FETCHED_GCC)
	@rm -rf $(FETCHED_GCC).tmp $(FETCHED_GCC).tar.gz

# ---------------------------------------------------------------------------
# Help
# ---------------------------------------------------------------------------
define HELP
Flipper One MCU firmware

  make [FW_TARGET=f1|f2] [BUILD_TYPE=Release|Debug]
                           Configure on first run, then build.
                           Output: $(FIRMWARE).uf2 and $(BUILD_DIR)/partition_table.uf2
  make configure           Re-run the CMake configure step
  make toolchain           Fetch ARM GCC and the Pico SDK without building
  make flash               Partition table + firmware over SWD (openocd, CMSIS-DAP probe)
  make load                Firmware over USB (picotool, board in BOOTSEL mode)
  make load-partition-table
                           Partition table over USB (picotool)
  make clean               Remove build outputs, keep the configuration
  make distclean           Remove the build directory

The build directory remembers FW_TARGET and BUILD_TYPE; pass a different value to
reconfigure. Set PICO_SDK_PATH / PICO_TOOLCHAIN_PATH to use your own SDK or GCC,
CMAKE_ARGS to pass extra options to CMake, JOBS to limit parallelism.

Resolved settings
  FW_TARGET            $(FW_TARGET)$(if $(CACHED_FW_TARGET), (from $(CMAKE_CACHE)))
  BUILD_TYPE           $(BUILD_TYPE)
  BUILD_DIR            $(BUILD_DIR)
  PICO_SDK_PATH        $(PICO_SDK_PATH)
  PICO_TOOLCHAIN_PATH  $(PICO_TOOLCHAIN_PATH)
  TOOLCHAIN_DIR        $(TOOLCHAIN_DIR)  (fetched SDK, GCC and picotool go here)
  CMAKE                $(if $(CMAKE),$(CMAKE),not found)  ($(GENERATOR))
  OPENOCD              $(if $(OPENOCD),$(OPENOCD),not found)
  PICOTOOL             $(if $(PICOTOOL),$(PICOTOOL),not found)
endef
export HELP

help:
	@echo "$$HELP"
