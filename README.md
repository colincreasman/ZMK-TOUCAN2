# ZMK config for beekeeb Toucan2 Keyboard

[The beekeeb Toucan2 Keyboard](https://beekeeb.com/introducing-toucan2/) is a wireless split 42-key column‑stagger keyboard that a display and a trackpad, with an aggressive stagger on the pinky columns.

# Customizations

Forked from [beekeeb/zmk-keyboard-toucan2](https://github.com/beekeeb/zmk-keyboard-toucan2) -- the Toucan2-specific
firmware repo, **not** [zmk-keyboard-toucan](https://github.com/beekeeb/zmk-keyboard-toucan) (that repo targets
the original Toucan1's Cirque Pinnacle SPI trackpad; Toucan2 replaced it with an Azoteq TPS43 I2C trackpad, so the
two repos are not interchangeable -- flashing Toucan1 firmware onto Toucan2 hardware builds and boots fine but
leaves the trackpad completely dead since the wrong driver is loaded).

- **Keymap**: [config/toucan.keymap](config/toucan.keymap), ported from my
  [ZMK-TOTEMIST](https://github.com/colincreasman/ZMK-TOTEMIST) config (homerow mods, thumb hold-taps, layer-tap,
  ESC combos). The Toucan2's 42-key matrix has 6 more keys (an outer pinky column per side) than the 36-key
  totemist, so those extra positions are bound to `&none`. `layer_four` (the mouse-click layer) is preserved from
  the stock keymap since `is_touching_processor` in `toucan.dtsi` hardcodes `&mo 4` to activate it while the
  trackpad is touched -- see the comments in the keymap for the full layer/position map.
- **General configs**: [boards/shields/toucan/toucan_left.conf](boards/shields/toucan/toucan_left.conf) and [boards/shields/toucan/toucan_right.conf](boards/shields/toucan/toucan_right.conf)
- **Swipe shortcuts**: the `swipe_button_mapper` node in [boards/shields/toucan/toucan.dtsi](boards/shields/toucan/toucan.dtsi), tuned to mirror this Mac's actual System Settings > Trackpad gestures
  (Mission Control on via 3-finger up, "swipe between full-screen applications"/App Exposé off, "swipe between
  pages" repurposed onto 3-finger left/right since this driver has no distinct 2-finger swipe).
- **Invert scroll / trackpad settings**: the `tps43_trackpad` node in [boards/shields/toucan/toucan_right.overlay](boards/shields/toucan/toucan_right.overlay).
  `sensitivity` is bumped from the driver default (100) to 150 to get closer to this Mac's "Fast" tracking-speed
  setting. Tap-to-click (`single-tap`) and two-finger-tap-to-right-click (`two-finger-tap`) already match System
  Settings 1:1. There's no firmware equivalent for Force Click/haptic feedback or click-pressure firmness --
  this is a flat capacitive trackpad with no physical click mechanism or haptic actuator, so those macOS settings
  have no analog here.

# Companion app: LinearMouse

Because this trackpad enumerates over I2C/Bluetooth as a generic HID pointing device rather than Apple's
proprietary multitouch trackpad protocol, macOS applies its "external mouse" scroll-event handling to it instead
of the "Trackpad" pane's smoothed/inertial scrolling -- scrolling feels comparatively abrupt and jittery even with
the firmware-side `scroll` gesture enabled. [LinearMouse](https://linearmouse.app) fixes this at the OS level by
re-applying smoothed/inertial scrolling to this specific device (matched by USB product name "Toucan", vendor ID
`0x1D50`, product ID `0x615E`) without affecting other mice/trackpads. Config lives at
`~/.config/linearmouse/linearmouse.json`; scroll direction is left un-reversed there since the firmware's
`invert-scroll-y` already produces natural-scrolling direction, so reversing it again in LinearMouse would cancel
that out.

# License

The code in this repo is available under the MIT license.

The included shield nice_view_gem is modified from https://github.com/M165437/nice-view-gem licensed under the MIT License.

The linked trackpad module is based on https://github.com/geeksville/zmk_driver_azoteq

ZMK code snippets are taken from the ZMK documentation under the MIT license.

The embedded font QuinqueFive is designed by GGBotNet, licensed under under the SIL Open Font License, Version 1.1.
