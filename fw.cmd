@echo off
rem Command-line build for Windows (Linux/macOS: fw). See docs/building.md.
rem
rem   fw.cmd [build]   configure on the first run, then build -> build\flipperone-mcu-firmware.uf2
rem   fw.cmd clean     remove the build directory
rem   fw.cmd flash     build, then write the partition table and firmware over SWD (openocd)
rem   fw.cmd setup     fetch the Pico SDK and the ARM toolchain without building
rem
rem Variables go make-style after the command, e.g. `fw.cmd build FW_TARGET=f1`, or are set
rem beforehand with `set`. FW_TARGET reconfigures the build directory for another board.
rem
rem The Pico SDK and the toolchain live in %USERPROFILE%\.pico-sdk, the directory the
rem Raspberry Pi Pico VS Code extension uses, so one copy serves both. Versions come
rem from CMakeLists.txt. Needs Windows 10 or later for the bundled curl.exe and tar.exe.
setlocal
cd /d "%~dp0"
for /f "usebackq eol=# tokens=1,* delims==" %%a in ("fw.cfg") do set "%%a=%%b"

rem One command plus NAME=value overrides, in any order. %* is split by hand because cmd
rem treats '=' as a separator when it fills %1, %2, ...
set "COMMAND="
set "ARGS=%*"
:parse_args
if not defined ARGS goto args_done
for /f "tokens=1,*" %%a in ("%ARGS%") do (
    set "ARG=%%a"
    set "ARGS=%%b"
)
call :parse_arg "%ARG%" || goto usage
goto parse_args
:args_done
if not defined COMMAND set "COMMAND=build"

set "PICO_HOME=%USERPROFILE%\.pico-sdk"
for /f "tokens=3 delims=() " %%v in ('findstr /b /c:"set(sdkVersion" CMakeLists.txt') do set "SDK_VERSION=%%v"
for /f "tokens=3 delims=() " %%v in ('findstr /b /c:"set(toolchainVersion" CMakeLists.txt') do set "TOOLCHAIN_VERSION=%%v"
if not defined SDK_VERSION goto no_versions
if not defined TOOLCHAIN_VERSION goto no_versions
rem 14_2_Rel1 -> 14.2.rel1
set "ARM_RELEASE=%TOOLCHAIN_VERSION:_=.%"
set "ARM_RELEASE=%ARM_RELEASE:Rel=rel%"

set "DEFAULT_SDK=%PICO_HOME%\sdk\%SDK_VERSION%"
set "DEFAULT_TOOLCHAIN=%PICO_HOME%\toolchain\%TOOLCHAIN_VERSION%"
if not defined PICO_SDK_PATH set "PICO_SDK_PATH=%DEFAULT_SDK%"
if not defined PICO_TOOLCHAIN_PATH set "PICO_TOOLCHAIN_PATH=%DEFAULT_TOOLCHAIN%"

if /i "%COMMAND%"=="build" goto run
if /i "%COMMAND%"=="clean" goto run
if /i "%COMMAND%"=="flash" goto run
if /i "%COMMAND%"=="setup" goto run
:usage
echo usage: fw.cmd [build^|clean^|flash^|setup] [NAME=value ...] >&2
exit /b 2

:no_versions
echo error: sdkVersion/toolchainVersion not found in CMakeLists.txt >&2
exit /b 1

:run
call :%COMMAND%
exit /b

:parse_arg
for /f "tokens=1,* delims==" %%a in ("%~1") do (
    if "%%b"=="" (
        if defined COMMAND exit /b 1
        set "COMMAND=%%a"
    ) else (
        set "%%a=%%b"
    )
)
exit /b 0

:build
call :check_host_tools || exit /b 1
call :ensure_sdk || exit /b 1
call :ensure_toolchain || exit /b 1
set "TARGET=%FW_TARGET%"
if not defined TARGET set "TARGET=%DEFAULT_FW_TARGET%"
set "RECONFIGURE="
if not exist build\CMakeCache.txt set "RECONFIGURE=1"
if defined FW_TARGET set "RECONFIGURE=1"
if defined RECONFIGURE (
    "%CMAKE%" -S . -B build -G Ninja -DCMAKE_MAKE_PROGRAM="%NINJA%" -DFW_TARGET=%TARGET% || exit /b 1
)
"%CMAKE%" --build build
exit /b

:clean
if not exist build (
    echo Nothing to clean: %CD%\build does not exist
    exit /b 0
)
echo Removing %CD%\build
rmdir /s /q build
if exist build (
    echo error: could not remove %CD%\build >&2
    exit /b 1
)
exit /b 0

:flash
call :build || exit /b 1
set "OPENOCD_SCRIPTS="
if not defined OPENOCD (
    call :find_tool OPENOCD openocd openocd openocd.exe
    for /d %%d in ("%PICO_HOME%\openocd\*") do if exist "%%d\openocd.exe" set "OPENOCD_SCRIPTS=%%d\scripts"
)
if not defined OPENOCD (
    echo error: openocd not found. RP2350 needs the Raspberry Pi build: https://github.com/raspberrypi/openocd >&2
    exit /b 1
)
if defined OPENOCD_SCRIPTS (
    "%OPENOCD%" -s "%OPENOCD_SCRIPTS%" -f %OPENOCD_INTERFACE% -f %OPENOCD_TARGET% -f targets/flash.tcl
) else (
    "%OPENOCD%" -f %OPENOCD_INTERFACE% -f %OPENOCD_TARGET% -f targets/flash.tcl
)
exit /b

:setup
call :check_host_tools || exit /b 1
call :ensure_sdk || exit /b 1
call :ensure_toolchain || exit /b 1
echo Pico SDK:  %PICO_SDK_PATH%
echo Toolchain: %PICO_TOOLCHAIN_PATH%
exit /b 0

rem find_tool <variable> <name> <.pico-sdk subdirectory> <path inside the version directory>
rem The extension's copy first, so command-line and VS Code builds share one CMake cache.
rem Leaves the variable undefined when the tool is not installed.
:find_tool
set "%~1="
for /d %%d in ("%PICO_HOME%\%~3\*") do if exist "%%d\%~4" set "%~1=%%d\%~4"
if defined %~1 exit /b 0
for /f "delims=" %%p in ('where %~2 2^>nul') do if not defined %~1 set "%~1=%%p"
exit /b 0

rem Everything the build needs from the system, reported in one go before anything is fetched.
:check_host_tools
call :find_tool CMAKE cmake cmake bin\cmake.exe
call :find_tool NINJA ninja ninja ninja.exe
set "MISSING="
if not defined CMAKE set "MISSING=%MISSING% cmake"
if not defined NINJA set "MISSING=%MISSING% ninja"
where /q git || set "MISSING=%MISSING% git"
where /q py || set "MISSING=%MISSING% python"
where /q curl.exe || set "MISSING=%MISSING% curl"
where /q tar.exe || set "MISSING=%MISSING% tar"
if not defined MISSING exit /b 0
echo error: missing:%MISSING% >&2
echo   Git for Windows and Python 3 with the py launcher are required. CMake and Ninja come with the Raspberry Pi Pico VS Code extension, or go on PATH. curl and tar ship with Windows 10 and later. >&2
exit /b 1

:ensure_sdk
if exist "%PICO_SDK_PATH%\pico_sdk_init.cmake" exit /b 0
if /i not "%PICO_SDK_PATH%"=="%DEFAULT_SDK%" (
    echo error: PICO_SDK_PATH=%PICO_SDK_PATH% does not contain the Pico SDK >&2
    exit /b 1
)
echo Fetching Pico SDK %SDK_VERSION% into %PICO_SDK_PATH%
if exist "%PICO_SDK_PATH%.tmp" rmdir /s /q "%PICO_SDK_PATH%.tmp"
if not exist "%PICO_HOME%\sdk" mkdir "%PICO_HOME%\sdk"
git -c advice.detachedHead=false clone --depth 1 --branch %SDK_VERSION% %PICO_SDK_GIT_URL% "%PICO_SDK_PATH%.tmp" || exit /b 1
git -C "%PICO_SDK_PATH%.tmp" submodule update --init --depth 1 || exit /b 1
move "%PICO_SDK_PATH%.tmp" "%PICO_SDK_PATH%" >nul || exit /b 1
if not exist "%PICO_SDK_PATH%\pico_sdk_init.cmake" (
    echo error: %PICO_SDK_PATH% does not look like the Pico SDK >&2
    exit /b 1
)
exit /b 0

:ensure_toolchain
if exist "%PICO_TOOLCHAIN_PATH%\bin\arm-none-eabi-gcc.exe" exit /b 0
if /i not "%PICO_TOOLCHAIN_PATH%"=="%DEFAULT_TOOLCHAIN%" (
    echo error: PICO_TOOLCHAIN_PATH=%PICO_TOOLCHAIN_PATH% has no bin\arm-none-eabi-gcc.exe >&2
    exit /b 1
)
set "TC_NAME=arm-gnu-toolchain-%ARM_RELEASE%-mingw-w64-x86_64-arm-none-eabi"
set "TC_TMP=%PICO_TOOLCHAIN_PATH%.tmp"
echo Fetching %TC_NAME% into %PICO_TOOLCHAIN_PATH%
if exist "%TC_TMP%" rmdir /s /q "%TC_TMP%"
mkdir "%TC_TMP%" || exit /b 1
curl.exe -fL --retry 3 -o "%TC_TMP%\%TC_NAME%.zip" "%ARM_TOOLCHAIN_BASE_URL%/%ARM_RELEASE%/binrel/%TC_NAME%.zip" || exit /b 1
tar.exe -xf "%TC_TMP%\%TC_NAME%.zip" -C "%TC_TMP%" || exit /b 1
del "%TC_TMP%\%TC_NAME%.zip"
for /d %%d in ("%TC_TMP%\arm-gnu-toolchain-*") do move "%%d" "%PICO_TOOLCHAIN_PATH%" >nul || exit /b 1
if not exist "%PICO_TOOLCHAIN_PATH%\bin" move "%TC_TMP%" "%PICO_TOOLCHAIN_PATH%" >nul || exit /b 1
if exist "%TC_TMP%" rmdir /s /q "%TC_TMP%"
if not exist "%PICO_TOOLCHAIN_PATH%\bin\arm-none-eabi-gcc.exe" (
    echo error: unexpected toolchain archive layout in %PICO_TOOLCHAIN_PATH% >&2
    exit /b 1
)
exit /b 0
