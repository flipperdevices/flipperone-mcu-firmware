#!/usr/bin/env python3
"""Tools for an LLM agent to see and drive a Flipper One on the bench.

Every command is non-interactive, has a timeout, prints plain text and exits
non-zero with a one-line reason on failure. See AGENT_TOOLS.md next to this file.
"""

import os
import sys

if __package__ in (None, ""):
    # Run as a script: make the package importable from scripts/
    sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    from agent_tools.cli.main import main
else:
    from .cli.main import main

if __name__ == "__main__":
    sys.exit(main())
