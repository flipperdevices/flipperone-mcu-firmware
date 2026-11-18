"""Architecture test: the package keeps its layer direction.

    packets   -> stdlib, constants, errors only
    transport -> stdlib, constants, errors, transport.*
    logic     -> packets, transport, constants, errors, logic.*
    cli       -> anything

Run with pytest, or directly: python3 scripts/agent_tools/tests/test_layers.py
"""

import ast
import os
import sys
from pathlib import Path

PACKAGE = Path(__file__).resolve().parents[1]

ALLOWED = {
    "packets": {"constants", "errors", "packets"},
    "transport": {"constants", "errors", "transport"},
    "logic": {"constants", "errors", "packets", "transport", "logic"},
    "cli": {"constants", "errors", "packets", "transport", "logic", "cli"},
}


def imported_modules(path):
    """Internal agent_tools modules (dotted, relative to the package) imported by a file."""
    tree = ast.parse(path.read_text(), filename=str(path))
    package_parts = list(path.relative_to(PACKAGE).parts[:-1])
    found = set()

    def resolve(module, level):
        if level == 0:
            parts = (module or "").split(".")
            return ".".join(parts[1:]) if parts[0] == "agent_tools" else None
        base = package_parts
        for _ in range(level - 1):
            base = base[:-1]
        return base, module

    for node in ast.walk(tree):
        if isinstance(node, ast.ImportFrom):
            target = resolve(node.module, node.level or 0)
            if target is None:
                continue
            if isinstance(target, tuple):
                base, module = target
                if module:
                    found.add(".".join([*base, *module.split(".")]) if base else module)
                else:
                    for alias in node.names:
                        found.add(".".join([*base, alias.name]) if base else alias.name)
            elif target:
                found.add(target)
        elif isinstance(node, ast.Import):
            for alias in node.names:
                if alias.name.startswith("agent_tools."):
                    found.add(alias.name[len("agent_tools.") :])
    return found


def test_layer_direction():
    violations = []
    for path in PACKAGE.rglob("*.py"):
        parts = path.relative_to(PACKAGE).parts
        if parts[0] in {"tests", "__pycache__"} or len(parts) == 1:
            continue  # tests and package-root modules (constants, errors, f1)
        layer = parts[0]
        if layer not in ALLOWED:
            violations.append(f"{path.relative_to(PACKAGE)}: unknown layer '{layer}'")
            continue
        for imported in imported_modules(path):
            top = imported.split(".")[0]
            if top not in ALLOWED[layer]:
                violations.append(f"{path.relative_to(PACKAGE)}: {layer} may not import {imported}")
    assert not violations, "\n".join(violations)


def test_package_imports():
    import agent_tools.cli.main  # noqa: F401
    import agent_tools.logic.console  # noqa: F401
    import agent_tools.logic.rpc  # noqa: F401
    import agent_tools.logic.swd  # noqa: F401
    import agent_tools.packets.picobin  # noqa: F401
    import agent_tools.packets.rpc  # noqa: F401
    import agent_tools.transport.ports  # noqa: F401


if __name__ == "__main__":
    sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
    test_layer_direction()
    test_package_imports()
    print("layer checks passed")
