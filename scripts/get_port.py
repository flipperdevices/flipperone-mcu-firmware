#!/usr/bin/env python3
"""Print the Flipper One serial port for a role.

Implemented in scripts/agent_tools/transport/ports.py; this keeps the old path
working for scripts that call it.

    python3 scripts/get_port.py            # list all roles
    python3 scripts/get_port.py mcu        # print one device path
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from agent_tools.transport.ports import main  # noqa: E402

if __name__ == "__main__":
    sys.exit(main(sys.argv))
