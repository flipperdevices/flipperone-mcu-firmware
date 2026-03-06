@echo off
pushd "%~dp0"

if not exist ".venv\Scripts\activate.bat" (
    echo Creating virtual environment...
    py -m venv .venv
)

call ".venv\Scripts\activate.bat"

py -c "import png" 2>NUL
if errorlevel 1 (
    echo Installing pypng...
    pip install pypng
)

py -c "import lz4" 2>NUL
if errorlevel 1 (
    echo Installing lz4...
    pip install lz4
)

py -c "import PIL" 2>NUL
if errorlevel 1 (
    echo Installing Pillow...
    pip install pillow
)

py %*

popd