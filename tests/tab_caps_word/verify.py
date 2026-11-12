#!/usr/bin/env python3
"""Verify the shipped behaviors and run the native Caps Word test."""

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
    for name in ("hold_tap", "a_caps_dance", "bslash_caps_dance", "a_shift", "bslash_shift"):
        pattern = rf"\w+:\s*{name}\s*\{{[^{{}}]*\}};"
        actual = re.search(pattern, source)
        expected = re.search(pattern, fixture)
        if not actual or not expected or actual.group() != expected.group():
            sys.exit(f"{name} in {keymap} differs from the native test")
    for binding in ("&a_shift LSHFT 0", "&bslash_shift RSHFT 0", "&ht LSHFT TAB"):
        if binding not in source:
            sys.exit(f"{binding} missing from {keymap}")
    if "tab_caps_dance" in source or "tab_shift" in source:
        sys.exit(f"Tab Caps Word behavior remains in {keymap}")

zmk = os.environ.get("ZMK_SRC_DIR")
if not zmk:
    sys.exit("Set ZMK_SRC_DIR to the ZMK app directory")
subprocess.run([str(Path(zmk) / "run-test.sh"), str(test_dir)], check=True)
