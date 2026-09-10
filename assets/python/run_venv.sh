#!/bin/bash
set -e

# Must be run from this directory (the build sets WORKING_DIRECTORY).
# Package list lives in requirements.txt; a freshly created venv is populated
# here, updates on requirements.txt changes are driven by the venv.stamp step
# in assets/CMakeLists.txt.
if [ ! -f ".venv/bin/activate" ]; then
    echo "Creating virtual environment..."
    rm -rf .venv
    python3 -m venv --copies .venv # --copies to fix macOS symlink issues
    FRESH_VENV=1
fi

source ./.venv/bin/activate

if [ -n "$FRESH_VENV" ]; then
    echo "Installing requirements..."
    pip install -r requirements.txt
fi

python3 "$@"
