#!/usr/bin/env python3
"""Verify thumb bindings and run the native symbol/maintenance test."""

import os
from pathlib import Path
import re
import subprocess
import sys

test_dir = Path(__file__).resolve().parent
repo = test_dir.parents[1]
fixture = (test_dir / "native_posix_64.keymap").read_text()

for keymap in (
    repo / "config/toucan.keymap",
    repo / "boards/shields/toucan/toucan.keymap",
):
    source = keymap.read_text()
    for pattern in (
        r"lt\s*\{[^{}]*\}",
        r"ht:\s*hold_tap\s*\{[^{}]*\}",
    ):
        actual = re.search(pattern, source)
        expected = re.search(pattern, fixture)
        if not actual or not expected or actual.group() != expected.group():
            sys.exit(f"Timing in {keymap} differs from native test: {pattern}")
    base = source.split("layer_zero {", 1)[1].split("layer_one {", 1)[0]
    layer_one = source.split("layer_one {", 1)[1].split("layer_two {", 1)[0]
    if "&ht LGUI SQT" not in base or "&ht LGUI GRAVE" not in layer_one:
        sys.exit(f"Thumb symbol bindings missing from {keymap}")
    if "key-positions = <36 41>" not in source or "layers = <L0 L1 L2>" not in source:
        sys.exit(f"Maintenance combo missing from {keymap}")

zmk = os.environ.get("ZMK_SRC_DIR")
if not zmk:
    sys.exit("Set ZMK_SRC_DIR to the ZMK app directory")
subprocess.run([str(Path(zmk) / "run-test.sh"), str(test_dir)], check=True)
