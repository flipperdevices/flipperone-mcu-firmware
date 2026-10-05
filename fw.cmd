@echo off
rem Command-line build for Windows (Linux/macOS: fw). See docs/building.md.
rem
rem   fw.cmd [build]   configure on the first run, then build -> build\flipperone-mcu-firmware.uf2
rem   fw.cmd clean     remove the build directory
rem   fw.cmd flash     build, then write the partition table and firmware over SWD (openocd)
rem   fw.cmd setup     fetch everything the build needs without building
rem
rem Variables go make-style after the command, e.g. `fw.cmd build FW_TARGET=f1`, or are set
rem beforehand with `set`. FW_TARGET reconfigures the build directory for another board.
rem
rem Everything but Git lives in %USERPROFILE%\.pico-sdk, laid out as the Raspberry Pi Pico
rem VS Code extension does it, so one copy serves both: CMake, Ninja, Python, the Pico SDK,
rem the ARM toolchain and prebuilt pioasm/picotool are fetched there when missing. Nothing is
rem added to the user's PATH; this script locates the tools itself, so no new terminal is
rem needed after a fetch. Versions come from CMakeLists.txt and fw.cfg. Needs Windows 10 or
rem later for the bundled curl.exe and tar.exe.
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
set "DOWNLOADS=%TEMP%\flipperone-mcu-firmware"
for /f "tokens=3 delims=() " %%v in ('findstr /b /c:"set(sdkVersion" CMakeLists.txt') do set "SDK_VERSION=%%v"
for /f "tokens=3 delims=() " %%v in ('findstr /b /c:"set(toolchainVersion" CMakeLists.txt') do set "TOOLCHAIN_VERSION=%%v"
for /f "tokens=3 delims=() " %%v in ('findstr /b /c:"set(picotoolVersion" CMakeLists.txt') do set "PICOTOOL_VERSION=%%v"
if not defined SDK_VERSION goto no_versions
if not defined TOOLCHAIN_VERSION goto no_versions
if not defined PICOTOOL_VERSION goto no_versions
rem 14_2_Rel1 -> 14.2.rel1
set "ARM_RELEASE=%TOOLCHAIN_VERSION:_=.%"
set "ARM_RELEASE=%ARM_RELEASE:Rel=rel%"

set "DEFAULT_SDK=%PICO_HOME%\sdk\%SDK_VERSION%"
set "DEFAULT_TOOLCHAIN=%PICO_HOME%\toolchain\%TOOLCHAIN_VERSION%"
if not defined PICO_SDK_PATH set "PICO_SDK_PATH=%DEFAULT_SDK%"
if not defined PICO_TOOLCHAIN_PATH set "PICO_TOOLCHAIN_PATH=%DEFAULT_TOOLCHAIN%"
rem Where the VS Code extension's CMake hook expects pioasm and picotool
set "PIOASM_DIR=%PICO_HOME%\tools\%SDK_VERSION%\pioasm"
set "PICOTOOL_DIR=%PICO_HOME%\picotool\%PICOTOOL_VERSION%\picotool"

if /i "%COMMAND%"=="build" goto run
if /i "%COMMAND%"=="clean" goto run
if /i "%COMMAND%"=="flash" goto run
if /i "%COMMAND%"=="setup" goto run
:usage
echo usage: fw.cmd [build^|clean^|flash^|setup] [NAME=value ...] >&2
exit /b 2

:no_versions
echo error: sdkVersion/toolchainVersion/picotoolVersion not found in CMakeLists.txt >&2
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
call :ensure_host_tools || exit /b 1
call :ensure_sdk || exit /b 1
call :ensure_toolchain || exit /b 1
call :ensure_sdk_tools || exit /b 1
set "TARGET=%FW_TARGET%"
if not defined TARGET set "TARGET=%DEFAULT_FW_TARGET%"
set "RECONFIGURE="
if not exist build\CMakeCache.txt set "RECONFIGURE=1"
if defined FW_TARGET set "RECONFIGURE=1"
if defined RECONFIGURE (
    "%CMAKE%" -S . -B build -G Ninja -DCMAKE_MAKE_PROGRAM="%NINJA%" -DFW_TARGET=%TARGET% -Dpioasm_DIR="%PIOASM_DIR%" -Dpicotool_DIR="%PICOTOOL_DIR%" || exit /b 1
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
call :ensure_host_tools || exit /b 1
call :ensure_sdk || exit /b 1
call :ensure_toolchain || exit /b 1
call :ensure_sdk_tools || exit /b 1
echo CMake:     %CMAKE%
echo Ninja:     %NINJA%
echo Python:    %PY%
echo Pico SDK:  %PICO_SDK_PATH%
echo Toolchain: %PICO_TOOLCHAIN_PATH%
echo pioasm:    %PIOASM_DIR%
echo picotool:  %PICOTOOL_DIR%
exit /b 0

rem ---------------------------------------------------------------------------
rem Host tools
rem ---------------------------------------------------------------------------

rem Git is the one thing that has to be installed by hand; curl and tar ship with Windows.
rem CMake, Ninja and Python are fetched into .pico-sdk when neither the extension nor PATH has them.
:ensure_host_tools
set "MISSING="
where /q git || set "MISSING=%MISSING% git"
where /q curl.exe || set "MISSING=%MISSING% curl"
where /q tar.exe || set "MISSING=%MISSING% tar"
if defined MISSING (
    echo error: missing:%MISSING%. Git for Windows is required; curl and tar ship with Windows 10 and later. >&2
    exit /b 1
)
call :find_tool CMAKE cmake cmake bin\cmake.exe
if not defined CMAKE (call :install_cmake || exit /b 1)
call :find_tool NINJA ninja ninja ninja.exe
if not defined NINJA (call :install_ninja || exit /b 1)
call :find_py
if not defined PY (call :install_python || exit /b 1)
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

rem assets\python\run_venv.cmd calls the py launcher. Look on PATH, then where the python.org
rem installer puts a per-user launcher, and make that visible to the build for this process only.
:find_py
set "PY="
for /f "delims=" %%p in ('where py 2^>nul') do if not defined PY set "PY=%%p"
if defined PY exit /b 0
if exist "%LOCALAPPDATA%\Programs\Python\Launcher\py.exe" (
    set "PY=%LOCALAPPDATA%\Programs\Python\Launcher\py.exe"
    set "PATH=%LOCALAPPDATA%\Programs\Python\Launcher;%PATH%"
)
exit /b 0

:install_cmake
set "CMAKE_DIR=%PICO_HOME%\cmake\v%WIN_CMAKE_VERSION%"
set "CMAKE_NAME=cmake-%WIN_CMAKE_VERSION%-windows-x86_64"
echo Fetching CMake %WIN_CMAKE_VERSION% into %CMAKE_DIR%
call :download "%CMAKE_RELEASES_URL%/v%WIN_CMAKE_VERSION%/%CMAKE_NAME%.zip" "%DOWNLOADS%\%CMAKE_NAME%.zip" || exit /b 1
call :unpack "%DOWNLOADS%\%CMAKE_NAME%.zip" "%CMAKE_DIR%" || exit /b 1
set "CMAKE=%CMAKE_DIR%\bin\cmake.exe"
if not exist "%CMAKE%" (
    echo error: cmake.exe not found in %CMAKE_DIR% >&2
    exit /b 1
)
exit /b 0

:install_ninja
set "NINJA_DIR=%PICO_HOME%\ninja\v%WIN_NINJA_VERSION%"
echo Fetching Ninja %WIN_NINJA_VERSION% into %NINJA_DIR%
call :download "%NINJA_RELEASES_URL%/v%WIN_NINJA_VERSION%/ninja-win.zip" "%DOWNLOADS%\ninja-%WIN_NINJA_VERSION%-win.zip" || exit /b 1
call :unpack "%DOWNLOADS%\ninja-%WIN_NINJA_VERSION%-win.zip" "%NINJA_DIR%" || exit /b 1
set "NINJA=%NINJA_DIR%\ninja.exe"
if not exist "%NINJA%" (
    echo error: ninja.exe not found in %NINJA_DIR% >&2
    exit /b 1
)
exit /b 0

rem Per-user python.org install: no administrator rights, no PATH or file association changes.
rem The py launcher lands in %LOCALAPPDATA%\Programs\Python\Launcher, where find_py looks.
:install_python
set "PY_DIR=%PICO_HOME%\python\%WIN_PYTHON_VERSION%"
set "PY_INSTALLER=%DOWNLOADS%\python-%WIN_PYTHON_VERSION%-amd64.exe"
echo Installing Python %WIN_PYTHON_VERSION% into %PY_DIR% (per user)
call :download "%PYTHON_RELEASES_URL%/%WIN_PYTHON_VERSION%/python-%WIN_PYTHON_VERSION%-amd64.exe" "%PY_INSTALLER%" || exit /b 1
"%PY_INSTALLER%" /quiet InstallAllUsers=0 TargetDir="%PY_DIR%" Include_launcher=1 InstallLauncherAllUsers=0 PrependPath=0 AssociateFiles=0 Shortcuts=0 Include_test=0 Include_doc=0 || (
    echo error: the Python installer failed >&2
    exit /b 1
)
del "%PY_INSTALLER%"
call :find_py
if not defined PY (
    echo error: Python is installed but the py launcher was not found >&2
    exit /b 1
)
exit /b 0

rem ---------------------------------------------------------------------------
rem Pico SDK, ARM toolchain, prebuilt pioasm and picotool
rem ---------------------------------------------------------------------------

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
echo Fetching %TC_NAME% into %PICO_TOOLCHAIN_PATH%
call :download "%ARM_TOOLCHAIN_BASE_URL%/%ARM_RELEASE%/binrel/%TC_NAME%.zip" "%DOWNLOADS%\%TC_NAME%.zip" || exit /b 1
call :unpack "%DOWNLOADS%\%TC_NAME%.zip" "%PICO_TOOLCHAIN_PATH%" || exit /b 1
if not exist "%PICO_TOOLCHAIN_PATH%\bin\arm-none-eabi-gcc.exe" (
    echo error: unexpected toolchain archive layout in %PICO_TOOLCHAIN_PATH% >&2
    exit /b 1
)
exit /b 0

rem Without these the SDK builds pioasm and picotool from source, which needs a host C++ compiler.
:ensure_sdk_tools
if not exist "%PIOASM_DIR%\pioasmConfig.cmake" (
    echo Fetching pioasm %SDK_VERSION% into %PIOASM_DIR%
    call :download "%PICO_SDK_TOOLS_URL%/%PICO_SDK_TOOLS_RELEASE%/pico-sdk-tools-%SDK_VERSION%-x64-win.zip" "%DOWNLOADS%\pico-sdk-tools-%SDK_VERSION%-x64-win.zip" || exit /b 1
    call :unpack "%DOWNLOADS%\pico-sdk-tools-%SDK_VERSION%-x64-win.zip" "%PIOASM_DIR%" || exit /b 1
)
if not exist "%PICOTOOL_DIR%\picotoolConfig.cmake" (
    echo Fetching picotool %PICOTOOL_VERSION% into %PICOTOOL_DIR%
    call :download "%PICO_SDK_TOOLS_URL%/%PICO_SDK_TOOLS_RELEASE%/picotool-%PICOTOOL_VERSION%-x64-win.zip" "%DOWNLOADS%\picotool-%PICOTOOL_VERSION%-x64-win.zip" || exit /b 1
    call :unpack "%DOWNLOADS%\picotool-%PICOTOOL_VERSION%-x64-win.zip" "%PICOTOOL_DIR%" || exit /b 1
)
exit /b 0

rem ---------------------------------------------------------------------------
rem Helpers
rem ---------------------------------------------------------------------------

rem download <url> <file>
:download
for %%f in ("%~2") do if not exist "%%~dpf" mkdir "%%~dpf"
curl.exe -fL --retry 3 -o "%~2" "%~1" || (
    echo error: download failed: %~1 >&2
    exit /b 1
)
exit /b 0

rem unpack <zip> <dest>: an archive with a single top-level folder becomes <dest> itself,
rem a flat archive lands inside <dest>. The archive is deleted afterwards.
:unpack
set "STAGE=%~2.tmp"
if exist "%STAGE%" rmdir /s /q "%STAGE%"
mkdir "%STAGE%" || exit /b 1
tar.exe -xf "%~1" -C "%STAGE%" || exit /b 1
del "%~1"
if exist "%~2" rmdir /s /q "%~2"
set "TOP="
set "TOP_COUNT=0"
for /f "delims=" %%e in ('dir /b "%STAGE%"') do (
    set /a TOP_COUNT+=1
    set "TOP=%%e"
)
if "%TOP_COUNT%"=="1" if exist "%STAGE%\%TOP%\" (
    move "%STAGE%\%TOP%" "%~2" >nul || exit /b 1
    rmdir /s /q "%STAGE%"
    exit /b 0
)
move "%STAGE%" "%~2" >nul || exit /b 1
exit /b 0
