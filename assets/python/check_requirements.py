"""Exit 0 when this interpreter already has every package of a requirements file,
at exactly the pinned version; otherwise say what is missing and exit 1.

    python3 check_requirements.py requirements.txt

Only `name==version` lines are understood. assets/CMakeLists.txt uses this to run
the generators with the toolchain bundle's Python as is, instead of building a venv.
"""

import re
import sys
from importlib import metadata


def main(path):
    with open(path, encoding="utf-8") as f:
        lines = [raw.split("#", 1)[0].strip() for raw in f]
    for line in filter(None, lines):
        match = re.fullmatch(r"([A-Za-z0-9._-]+)==(\S+)", line)
        if not match:
            print(f"unsupported requirement line: {line}")
            return 1
        name, version = match.groups()
        try:
            installed = metadata.version(name)
        except metadata.PackageNotFoundError:
            print(f"{name} is not installed")
            return 1
        if installed != version:
            print(f"{name} is {installed}, {version} is pinned")
            return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
