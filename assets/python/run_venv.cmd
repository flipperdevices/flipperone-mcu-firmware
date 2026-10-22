@echo off
pushd "%~dp0"

set FRESH_VENV=
if not exist ".venv\Scripts\activate.bat" (
    echo Creating virtual environment...
    py -m venv .venv
    set FRESH_VENV=1
)

call ".venv\Scripts\activate.bat"

if defined FRESH_VENV (
    echo Installing requirements...
    pip install -r requirements.txt
)

py %*

popd
