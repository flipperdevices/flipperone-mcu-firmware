"""Agent tools for driving a Flipper One on the bench.

Layers:
  packets/    pure codecs: bytes/str <-> structured values, no I/O
  transport/  byte/line movers: serial ports, subprocesses, USB port discovery
  logic/      device services composed from packets + transport
  cli/        argument parsing, rendering and exit codes

Import direction is enforced by tests/test_layers.py.
"""
