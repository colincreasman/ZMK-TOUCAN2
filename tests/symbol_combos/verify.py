#!/usr/bin/env python3
"""Verify symbol combo definitions and run their native key-event test."""

import os
from pathlib import Path
import re
import subprocess
import sys

test_dir = Path(__file__).resolve().parent
repo = test_dir.parents[1]
fixture = (test_dir / "native_posix_64.keymap").read_text()


def node(source, name):
    match = re.search(rf"(?m)^\s*{re.escape(name)}\s*\{{", source)
    if not match:
        return None
    start = match.start()
    end = source.index("{", start) + 1
    depth = 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return re.sub(r"\s+", "", re.sub(r"//.*", "", source[start:end]))


behaviors = (
    "lpar_fixed",
    "rpar_fixed",
    "lbkt_fixed",
    "rbkt_fixed",
    "lbrc_fixed",
    "rbrc_fixed",
    "minus_fixed",
    "equal_fixed",
    "under_fixed",
    "plus_fixed",
)
combos = {
    "combo_left_paren": ("27 28", "lpar_fixed", None),
    "combo_right_paren": ("31 32", "rpar_fixed", None),
    "combo_left_bracket": ("15 16", "lbkt_fixed", None),
    "combo_right_bracket": ("19 20", "rbkt_fixed", None),
    "combo_left_brace": ("3 4", "lbrc_fixed", None),
    "combo_right_brace": ("7 8", "rbrc_fixed", None),
    "combo_minus": ("25 26", "minus_fixed", None),
    "combo_equal": ("33 34", "equal_fixed", None),
    "combo_underscore": ("13 14", "under_fixed", None),
    "combo_plus": ("21 22", "plus_fixed", None),
}

for keymap in (
    repo / "config/toucan.keymap",
    repo / "boards/shields/toucan/toucan.keymap",
):
    source = keymap.read_text()
    fixture_source = fixture
    for name in behaviors:
        actual = node(source, name)
        expected = node(fixture_source, name)
        if actual != expected:
            sys.exit(f"Shift-invariant behavior {name} differs in {keymap}")
    combo_source = source.split("    combos {", 1)[1].split("    keymap {", 1)[0]
    fixture_combos = fixture_source.split("    combos {", 1)[1].split("    keymap {", 1)[0]
    for name, (positions, behavior, layers) in combos.items():
        actual = node(combo_source, name)
        expected = node(fixture_combos, name)
        if actual is None or expected is None:
            sys.exit(f"Missing combo {name} in {keymap} or native fixture")
        for value in (positions.replace(" ", ""), behavior):
            if value not in actual:
                sys.exit(f"{name} in {keymap} missing {value}")
        if layers and f"layers=<{layers.replace(' ', '')}>" not in actual:
            sys.exit(f"{name} in {keymap} must be on layers {layers}")
        if layers is None and "layers=" in actual:
            sys.exit(f"{name} in {keymap} must be available on every layer")
        normalized_actual = actual.replace("L0", "0").replace("L1", "1").replace("L4", "4")
        if normalized_actual != expected:
            sys.exit(f"Combo {name} in {keymap} differs from native test")

zmk = os.environ.get("ZMK_SRC_DIR")
if not zmk:
    sys.exit("Set ZMK_SRC_DIR to the ZMK app directory")
subprocess.run([str(Path(zmk) / "run-test.sh"), str(test_dir)], check=True)
