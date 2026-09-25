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
  ESC combos). This 36-key Toucan2 omits the shield matrix's six outer-column switch positions, so those positions
  remain `&none`. Holding both far outer thumbs activates dedicated maintenance layer 3 without conflicting with
  the center Shift + L1 thumb chord: Bluetooth profiles stay in
  their previous left-hand positions, Bluetooth clear moves one key inward to `W`, and 2-second bootloader holds
  live on the real top pinkies (`Q` for the left half and `P` for the right). `layer_four` remains the mouse-click
  layer because `is_touching_processor` in `toucan.dtsi` hardcodes `&mo 4` while the trackpad is touched.
- **General configs**: [boards/shields/toucan/toucan_left.conf](boards/shields/toucan/toucan_left.conf) and [boards/shields/toucan/toucan_right.conf](boards/shields/toucan/toucan_right.conf)
- **Swipe shortcuts**: the `swipe_button_mapper` node in [boards/shields/toucan/toucan.dtsi](boards/shields/toucan/toucan.dtsi) maps standalone 3-finger swipes. Up sends `Option+\`` -- this Mac remaps Mission Control to `Opt+\`` under System Settings > Keyboard > Keyboard Shortcuts > Mission Control, so the `Ctrl+Up` default does nothing here. Left/right drive [AltTab](https://alt-tab-macos.netlify.app/), mirroring the "3-finger Horizontal Swipe" trigger set up on this Mac's built-in trackpad: right sends `Opt+Tab` (next window) and left sends `Opt+Shift+Tab` (previous window), matching AltTab's Shortcut 1. Because the firmware sends a discrete press/release, one swipe steps one window instead of holding the switcher open. Down stays unbound since every other Mission Control shortcut is unchecked on this Mac. The driver emits horizontal and vertical events independently, so swipe reasonably straight to avoid firing two shortcuts at once; `three-finger-swipe-throttle-ms` (1200ms) keeps one motion from repeating and therefore also caps how quickly swipes can be repeated.
- **Page navigation**: the 3-finger left/right swipe used to send `Cmd+[` / `Cmd+]`, but that slot now belongs to
  AltTab. Back/forward moved to a **two-finger** horizontal swipe via the `hscroll_shortcut` node
  ([src/input_processor_scroll_shortcut.c](src/input_processor_scroll_shortcut.c)). The driver only reports swipe
  buttons for three-finger movement, so a two-finger swipe arrives as plain scroll on `INPUT_REL_HWHEEL`; simply
  forwarding that as a horizontal wheel does *not* trigger macOS page navigation, since that is a native trackpad
  gesture rather than a wheel event. The processor therefore accumulates the axis and emits real `Cmd+[` / `Cmd+]`
  keystrokes, which also work in VS Code and anywhere else those are bound. It sits before `zip_scroll_scaler` so
  it sees raw counts instead of the 1/100-damped value, and fires at most once per gesture -- the rest of the
  stroke is swallowed so a long swipe navigates one page instead of several. Swap the two `bindings` if the
  directions come out reversed; raise/lower `threshold` to tune how deliberate the swipe must be.
- **Three-finger swipe arbitration**: a second local processor, `zmk,input-processor-swipe-arbiter`
  ([src/input_processor_swipe_arbiter.c](src/input_processor_swipe_arbiter.c), configured on the `swipe_arbiter`
  node). The Azoteq driver commits to a swipe direction from the *first* nonzero delta of a gesture and reports
  each axis independently, so fingers settling at touchdown can fire the wrong axis; its throttle then blocks the
  real direction for the rest of the window. That is why a clean up-swipe lands instantly while left/right is hard
  to trigger and hard to retry. With `three-finger-swipe-throttle-ms` turned down to 40 the arbiter sees a stream
  of samples, stays silent until one axis leads the other by `lead` samples, emits exactly one press/release pair
  in the winning direction, then blocks until the fingers lift. An unambiguous swipe still produces exactly one
  actuation, roughly 40ms later than before, and retries no longer wait out a 1200ms lockout.

  To revert to the raw driver behavior: drop `&swipe_arbiter` from the listener's `input-processors` and set
  `three-finger-swipe-throttle-ms` back to `1200`.
- **Invert scroll / trackpad settings**: the `tps43_trackpad` node in [boards/shields/toucan/toucan_right.overlay](boards/shields/toucan/toucan_right.overlay).
  `sensitivity` sits at the driver's 100 baseline; the feel of the pointer is shaped by the `pointer_accel` input
  processor instead (see below).
- **Pointer acceleration**: a local `zmk,input-processor-accel` processor, implemented in
  [src/input_processor_accel.c](src/input_processor_accel.c) and configured on the `pointer_accel` node in
  [boards/shields/toucan/toucan.dtsi](boards/shields/toucan/toucan.dtsi). It exists because a single flat
  `sensitivity` multiplier forces a choice between precise-but-slow and fast-but-twitchy. Instead:
  - `activation-distance = <5>` swallows the first 5 counts of travel after each new touch, so resting or brushing
    a finger while typing cannot nudge the cursor. The gate re-arms on every finger lift, which the processor
    detects by watching `INPUT_BTN_TOUCH` -- this is why `pointer_accel` must be the *first* entry in the
    listener's `input-processors`, since `is_touching_processor` consumes that event with `ZMK_INPUT_PROC_STOP`.
  - `min-factor = <800>` keeps slow movement at 0.8x for deliberate, fine positioning.
  - Past `speed-threshold` the factor climbs a quadratic curve to `max-factor` (3.0x) at `speed-max`, so a longer
    stroke crosses the screen quickly.
  - `track-remainders` is required, since a sub-1.0 `min-factor` would otherwise truncate slow drags to nothing.
  - Speed is measured **per axis**. X and Y are delivered as two separate events from the same report, so a single
    shared timestamp measures ~0ms elapsed for whichever axis is processed second and computes an enormous speed,
    pinning that axis at `max-factor` while the other stays near `min-factor`. That made diagonal movement veer
    wildly and the pointer feel uncontrollably fast.

  Tuning: raise `min-factor` if slow movement feels too heavy, raise `max-factor` or lower `speed-max` for a more
  aggressive ramp, and raise `activation-distance` if stray cursor movement still sneaks through while typing. Tap-to-click (`single-tap`) and two-finger-tap-to-right-click (`two-finger-tap`) already match System
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
