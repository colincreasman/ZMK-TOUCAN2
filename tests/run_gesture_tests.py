#!/usr/bin/env python3
"""Compile and run the actual gesture behavior with deterministic host-event stubs."""

import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
headers = [
    "zephyr/device.h",
    "zephyr/kernel.h",
    "zephyr/init.h",
    "zephyr/logging/log.h",
    "zephyr/dt-bindings/input/input-event-codes.h",
    "zephyr/input/input.h",
    "drivers/behavior.h",
    "drivers/input_processor.h",
    "dt-bindings/zmk/keys.h",
    "zmk/behavior.h",
    "zmk/event_manager.h",
    "zmk/events/keycode_state_changed.h",
    "zmk/events/endpoint_changed.h",
    "zmk/events/ble_active_profile_changed.h",
    "zmk/events/battery_state_changed.h",
    "zmk/events/position_state_changed.h",
    "zmk/events/split_peripheral_status_changed.h",
]

with tempfile.TemporaryDirectory(prefix="toucan-gesture-test-") as directory:
    directory = Path(directory)
    for header in headers:
        path = directory / header
        path.parent.mkdir(parents=True, exist_ok=True)
        path.touch()
    targets = [
        ("gesture_session", "gesture_session.c", []),
        ("os_status_central", "os_status.c", ["-DTEST_CENTRAL=1"]),
        ("os_status_peripheral", "os_status.c", ["-DTEST_CENTRAL=0"]),
    ]
    for name, source, flags in targets:
        executable = directory / name
        subprocess.run(
            [
                os.environ.get("CC", "cc"),
                "-std=c11",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-Wno-unused-function",
                "-Wno-unused-const-variable",
                "-Wno-unused-variable",
                "-I" + str(directory),
                "-I" + str(root / "dts"),
                *flags,
                str(root / "tests" / source),
                "-o",
                str(executable),
            ],
            check=True,
        )
        subprocess.run([str(executable)], check=True)
