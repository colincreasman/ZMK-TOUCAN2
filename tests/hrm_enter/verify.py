#!/usr/bin/env python3
"""Run with ZMK_SRC_DIR set to a prepared ZMK app workspace."""

import os
from pathlib import Path
import re
import subprocess
import sys

test_dir = Path(__file__).resolve().parent
repo = test_dir.parents[1]
fixture = (test_dir / "native_posix_64.keymap").read_text()


def node(text, name):
    match = re.search(rf"(?m)^\s*(?:\w+:\s*)?{re.escape(name)}\s*\{{", text)
    if not match:
        sys.exit(f"Missing {name}")
    end = text.index("{", match.start()) + 1
    depth = 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return re.sub(r"\s+", "", text[match.start() : end + 1])


for keymap in (
    repo / "config/toucan.keymap",
    repo / "boards/shields/toucan/toucan.keymap",
):
    source = keymap.read_text()
    left = node(source, "home_row_mod_left")
    if left != node(fixture, "home_row_mod_left"):
        sys.exit(f"Left home-row mod in {keymap} differs from the native test")
    right = left.replace("hml:home_row_mod_left", "hmr:home_row_mod_right").replace(
        '"hrm_left"', '"hrm_right"'
    )
    if right != node(source, "home_row_mod_right"):
        sys.exit(f"Right home-row mod in {keymap} differs from the native test")
    if node(source, "home_row_mod_d") != node(fixture, "home_row_mod_d"):
        sys.exit(f"D home-row mod in {keymap} differs from the native test")
    if "&hmd LALT D" not in source:
        sys.exit(f"D home-row mod binding is missing from {keymap}")
    if "&lt L1 RET &ht LSHFT TAB &lt L1 SPACE  &kp BSPC" not in source:
        sys.exit(f"Enter/L1 or Backspace thumb binding is missing from {keymap}")
zmk = os.environ.get("ZMK_SRC_DIR")
if not zmk:
    sys.exit("Set ZMK_SRC_DIR to the ZMK app directory")
subprocess.run([str(Path(zmk) / "run-test.sh"), str(test_dir)], check=True)
