@echo off
REM Change working dir to script directory
pushd "%~dp0"

REM create virtual environment if missing
if not exist ".venv\Scripts\activate.bat" (
    echo Creating virtual environment...
    python -m venv .venv
)

REM activate the venv
call ".venv\Scripts\activate.bat"

REM install pypng if not present
python -c "import png" 2>NUL
if errorlevel 1 (
    echo Installing pypng...
    ".venv\Scripts\pip.exe" install pypng
)

REM install lz4 if not present
python -c "import lz4" 2>NUL
if errorlevel 1 (
    echo Installing lz4...
    ".venv\Scripts\pip.exe" install lz4
)

REM run the script with forwarded arguments
python ..\..\lib\lvgl\lvgl\scripts\LVGLImage.py %*

popd