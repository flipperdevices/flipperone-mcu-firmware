"""Subprocess runner: the single place where external programs are started."""

import subprocess


def run(args, timeout=60.0):
    """Run a program, captured; returns subprocess.CompletedProcess with text output."""
    return subprocess.run(args, capture_output=True, text=True, timeout=timeout)
