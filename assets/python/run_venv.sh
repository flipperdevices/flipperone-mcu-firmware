#!/bin/bash

# check for virtual environment
if [ ! -d ".venv" ]; then
    echo "Creating virtual environment..."
    python3 -m venv .venv
fi

source ./.venv/bin/activate

# install pypng if not already installed
if ! python3 -c "import png" &> /dev/null; then
    echo "Installing pypng..."
    pip install pypng
fi  

# install lz4 if not already installed
if ! python3 -c "import lz4" &> /dev/null; then
    echo "Installing lz4..."
    pip install lz4
fi

# install Pillow if not already installed
if ! python3 -c "import PIL" &> /dev/null; then
    echo "Installing Pillow..."
    pip install pillow
fi

python3 $@