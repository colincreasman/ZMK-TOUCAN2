# ZMK config for beekeeb Toucan2 Keyboard

[The beekeeb Toucan2 Keyboard](https://beekeeb.com/introducing-toucan2/) is a wireless split 42-key column‑stagger keyboard that a display and a trackpad, with an aggressive stagger on the pinky columns.

> **Known-good baseline:** the tag `known-good-base` is the hardware-confirmed setup to build on (dongle mode,
> 41° trackpad rotation, tuned gestures, macOS/Windows modes, LinearMouse config). If an experiment goes wrong,
> `git checkout known-good-base` gets back to it.

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
  their previous left-hand positions, Bluetooth clear moves one key inward to `W`, 2-second bootloader holds
  live on the real top pinkies (`Q` for the left half and `P` for the right) with a third on `T` for the USB
  dongle, and the right-hand home row carries
  the macOS/Windows selector (`J`/`K`). `layer_four` remains the mouse-click
  layer because `is_touching_processor` in `toucan.dtsi` hardcodes `&mo 4` while the trackpad is touched.
- **General configs**: [boards/shields/toucan/toucan_left.conf](boards/shields/toucan/toucan_left.conf) and [boards/shields/toucan/toucan_right.conf](boards/shields/toucan/toucan_right.conf)
- **Host-OS mode**: the trackpad gestures send different shortcuts on macOS and Windows. Which set is used is a
  **runtime** setting, not a build option, because this keyboard moves between machines over its Bluetooth
  profiles and reflashing to switch host would be impractical. On layer 3 the right-hand home row mirrors the
  Bluetooth profile keys opposite it: **`J` selects macOS, `K` selects Windows**. The choice is persisted, so the
  board comes back up in whichever mode it was last left in.

  Implemented by `zmk,behavior-os-select` ([src/behavior_os_select.c](src/behavior_os_select.c)), which wraps two
  bindings and picks between them when the gesture fires, and `zmk,behavior-os-mode`
  ([src/behavior_os_mode.c](src/behavior_os_mode.c)), which sets the mode (`0` = macOS, `1` = Windows, `2` =
  toggle). The per-OS keycodes live on the `os_*` behaviors at the top of `toucan.dtsi`. The mode is resolved at
  press and remembered for the matching release, so switching mid-press cannot strand a modifier. To change which
  mode a freshly-reset board powers up in, set `CONFIG_TOUCAN_OS_MODE_DEFAULT_WIN=y` in the shield `.conf` files.

  | Gesture | macOS | Windows |
  |---|---|---|
  | 3-finger up | `Opt+\`` (Mission Control) | `Win+Tab` (Task View) |
  | 3-finger left | `Cmd+Shift+\`` (previous window) | `Alt+Shift+Tab` |
  | 3-finger right | `Cmd+\`` (next window) | `Alt+Tab` |
  | 2-finger left | `Cmd+[` (back) | `Alt+Left` |
  | 2-finger right | `Cmd+]` (forward) | `Alt+Right` |

  One Windows caveat: `Alt+Tab` is a *stateful* switcher, like the AltTab app the macOS bindings originally used.
  A discrete swipe right steps one window forward, which is what is wanted, but a lone swipe left sends
  `Alt+Shift+Tab` with no switcher open, so Windows steps backwards through most-recent-window order rather than
  reversing an open switcher. Swiping right then left in quick succession behaves as expected.
- **Swipe shortcuts**: the `swipe_button_mapper` node in [boards/shields/toucan/toucan.dtsi](boards/shields/toucan/toucan.dtsi) maps standalone 3-finger swipes to the `os_*` behaviors above. On macOS, up sends `Option+\`` -- this Mac remaps Mission Control to `Opt+\`` under System Settings > Keyboard > Keyboard Shortcuts > Mission Control, so the `Ctrl+Up` default does nothing here. Left/right cycle windows with `Cmd+\`` / `Cmd+Shift+\``, matching the thumb-button mapping on the Logi mouse. These replaced [AltTab](https://alt-tab-macos.netlify.app/)'s `Opt+Tab` / `Opt+Shift+Tab`, which cannot work from a swipe: AltTab only interprets `Shift+Tab` while its switcher is already held open, so a standalone `Opt+Shift+Tab` just leaked the raw keystroke into the focused app. The `Cmd+\`` pair is stateless, so a single discrete press/release does the whole job. Down stays unbound since every other Mission Control shortcut is unchecked on this Mac. The driver emits horizontal and vertical events independently, so swipe reasonably straight to avoid firing two shortcuts at once; `three-finger-swipe-throttle-ms` (40ms) plus the swipe arbiter below keep one motion from repeating.

> **Gesture tuning is dialled in -- do not change these values casually.**
> Hardware testing confirmed the 2- and 3-finger gestures now trigger reliably and never cross-fire: 3-finger up
> for Mission Control lands with near-zero misfires even when alternating with 2-finger scrolling. That state is
> tagged `gestures-dialed-in`, so it can always be recovered with `git checkout gestures-dialed-in`.
>
> These values only work as a set -- change them together or not at all:
>
> | Setting | Value | Where |
> |---|---|---|
> | `three-finger-swipe-throttle-ms` | `40` | `toucan_right.overlay` |
> | `swipe_arbiter` `lead` / `max-samples` | `2` / `8` | `toucan.dtsi` |
> | `hscroll_shortcut` `threshold` | `120` | `toucan.dtsi` |
>
> In particular, raising the driver throttle back toward its old `1200` *without* also removing `&swipe_arbiter`
> will make horizontal swipes hard to trigger again -- that exact combination was the original bug.

- **Page navigation**: back/forward lives on a **two-finger** horizontal swipe via the `hscroll_shortcut` node
  ([src/input_processor_scroll_shortcut.c](src/input_processor_scroll_shortcut.c)); the three-finger left/right
  slot it used to share now belongs to window cycling. The driver only reports swipe
  buttons for three-finger movement, so a two-finger swipe arrives as plain scroll on `INPUT_REL_HWHEEL`; simply
  forwarding that as a horizontal wheel does *not* trigger macOS page navigation, since that is a native trackpad
  gesture rather than a wheel event. The processor therefore accumulates the axis and emits real keystrokes
  (`Cmd+[` / `Cmd+]` on macOS, `Alt+Left` / `Alt+Right` on Windows), which also work in VS Code and anywhere else
  those are bound. It sits before `zip_scroll_scaler` so
  it sees raw counts instead of the 1/100-damped value, and fires at most once per gesture -- the rest of the
  stroke is swallowed so a long swipe navigates one page instead of several. Raise/lower `threshold` to tune how
  deliberate the swipe must be.

  `angle` matches the pointer rotation, and exists for the same reason: the pad is mounted at an angle, so a
  swipe that is horizontal *to the hand* is a diagonal *to the sensor*. Without it you have to swipe along the
  pad's physical axis while the cursor uses the hand's axis. The driver reports only the dominant axis per
  sample, so both axes are accumulated across the gesture and the total is rotated before the direction is
  judged. `y-invert` undoes the driver's `invert-scroll-y` so the rotation sees the same orientation as the
  pointer axes. Vertical events pass through untouched, and firing requires the rotated horizontal component to
  beat the vertical one, so ordinary scrolling can never trip it.

  **Three-finger swipes cannot be corrected this way.** The driver resolves those into discrete
  `INPUT_BTN_NORTH/EAST/SOUTH/WEST` events internally, using the raw sensor axes, so the direction is already
  collapsed before any input processor sees it -- the magnitudes needed to rotate are gone. `swipe_arbiter`
  compensates as far as is possible by picking the dominant axis over several samples.
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
- **Pointer rotation**: this is beekeeb's **"Thumb Angle"** trackpad variant, where the pad sits at roughly 40
  degrees to the keyboard so it faces the thumb. The alternative **"Column Angle"** variant mounts the same pad
  square to the case. Fingers arrive square to the keyboard either way, so on this variant a stroke that feels
  "straight up" reaches the sensor as a diagonal. The `pointer_rotate` node
  ([src/input_processor_rotate.c](src/input_processor_rotate.c)) rotates the reported X/Y pair to cancel that
  mount angle out, making the pad behave like the Column Angle version.
  - Sign convention: **positive rotates movement counter-clockwise on screen, negative clockwise**. Change the
    magnitude if the correction is too strong or too weak.
  - Tuned on hardware: `+35` was the right direction but too little, `+40` was very close (tagged
    `trackpad-dialed-in`), and the current **`+41`** adds the last ~2% and is confirmed on hardware (tagged
    `known-good-base`). `+44` overshot: over-correcting makes
    the leftover error point the other way, which feels like the rotation has reversed even though it has not,
    and `-44` tested unusable, confirming positive is the right direction. The angle is in whole degrees; adjust
    a degree at a time.
  - `hscroll_shortcut` carries the same `angle` so two-finger swipes are judged in the same frame as the cursor;
    **always change the two together**.
  - In dongle mode this processing runs on the **dongle**, so rotation and acceleration changes mean reflashing
    the dongle (hold both far outer thumbs and hold `T` for 2 seconds), not the right half.
  - It runs first in the chain so the activation gate and acceleration curve both operate in the hand's frame.
  - Rotation needs both axes at once, but they arrive as two separate events. The driver always reports X
    immediately followed by Y for the same sample, so the processor buffers X, does the maths when Y arrives,
    emits the rotated Y on that event, and carries the rotated X to the next report. One axis is therefore one
    report (~10ms) behind the other, which is not perceptible.
  - Sine is a fixed-point table covering 0-90 degrees with the other quadrants derived from it, so the rotation is
    exact to a whole degree with no floating point. The table is consulted in the device's init hook rather than
    the config initializer, since a function call is not a constant expression.
  - A negative `angle` in devicetree arrives as a raw 32-bit cell (its two's-complement bit pattern), so the
    driver casts it back to `int32_t`. Comparing the raw macro against a negative bound does not work.
  - **Only pointer movement is rotated.** Scroll and the two/three-finger swipes are resolved inside the Azoteq
    driver, upstream of every input processor, so gesture behavior is unaffected.
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
  Settings 1:1. Press-and-hold-to-drag (`press-and-hold`, "click and hold") is also enabled: hold a finger down
  past `hold-time` (300ms) and the left button is held down, so movement drags and lifting releases -- adjust
  `hold-time` in [toucan_right.overlay](boards/shields/toucan/toucan_right.overlay) for a snappier or safer hold.
  This is the Azoteq's equivalent of dragging; macOS's "tap to drag" tap-and-a-half (tap, lift, re-touch) is a
  software behavior macOS layers on the trackpad rather than a hardware gesture, so it has no direct analog, but
  the drag outcome is the same. There's likewise no firmware equivalent for Force Click/haptic feedback or
  click-pressure firmness -- this is a flat capacitive trackpad with no physical click mechanism or haptic
  actuator, so those macOS settings have no analog here.

# Dongle mode (PandaKB USB dongle)

The keyboard can run through [PandaKB's ZMK dongle](https://pandakb.com/shop/keyboard-kit/pandakb-zmk-split-keyboard-dongle/)
(an nRF52840 nice!nano v2 with a 1.3" SH1106 OLED). The dongle becomes the split **central** and both halves
become its **peripherals**, so it behaves like any mouse/keyboard receiver: plug it into a computer's USB port
and the keyboard is simply there as a USB keyboard and mouse, with no Bluetooth pairing on that computer, ever.
The halves bond to the dongle once, and that bond persists across power cycles and computers.

It is a firmware mode, not a runtime switch. ZMK fixes a part's split role at build time, so **in dongle mode the
halves cannot connect to a computer without the dongle**. Switching modes means reflashing, as described below.

## What is built

| Artifact | Flash to | Mode |
|---|---|---|
| `toucan_dongle` | dongle | dongle |
| `toucan_left_dongle_mode` | left half | dongle |
| `toucan_right rgbled_adapter-seeeduino_xiao_ble-zmk` | right half | **both** -- the right half is a peripheral either way |
| `toucan_left rgbled_adapter nice_view_gem-seeeduino_xiao_ble-zmk` | left half | standalone |
| `settings_reset_dongle` | dongle | reset (nice!nano board) |
| `settings_reset-seeeduino_xiao_ble-zmk` | either half | reset (XIAO board) |

## One-time setup

Bonds from standalone mode must be cleared first: flashing new firmware does not erase stored settings, and the
right half is still bonded to the left half as its old central.

1. Turn **off** any other ZMK keyboards nearby. A central claims the first unpaired peripherals it finds.
2. Put each device into its UF2 bootloader and flash its settings reset:
   - Halves: double-tap the reset button, or hold both far outer thumbs (layer 3) and hold `Q` (left) / `P`
     (right) for 2 seconds. Flash `settings_reset-seeeduino_xiao_ble-zmk.uf2`.
   - Dongle: pop off the magnetic enclosure (its buttons are cosmetic) and double-tap the board's reset
     button. Flash `settings_reset_dongle`.
3. Flash the real firmware the same way: `toucan_dongle` to the dongle, `toucan_left_dongle_mode` to the left
   half, and the usual right-half firmware to the right half.
4. Plug the dongle into USB and power both halves on near it. They bond within a few seconds, and the left
   half's screen changes from `SEARCHING` to `LINKED`.
5. On any computer that previously paired the Toucan over Bluetooth, remove that old pairing; it is now stale.

macOS may show the Keyboard Setup Assistant the first time the dongle is plugged in (dismiss it, or pick ANSI),
and newer Macs may ask to allow the new USB accessory.

## Day to day

- **Everything lives on the dongle**: the keymap, combos, gesture processing and the macOS/Windows gesture mode
  (layer 3 `J`/`K`), which is stored on the dongle and therefore travels with it between computers.
- **Updating firmware**: to update the dongle later without opening it, hold both far outer thumbs and hold `T`
  for 2 seconds. That is a third bootloader hold, retargeted at the central via the long-press behavior's
  `central` option. In standalone mode the central is the left half, so the same hold just duplicates `Q`.
- **The dongle has a small battery** and can also connect to hosts over Bluetooth itself, using the layer 3
  Bluetooth profile keys. Over USB, none of that is needed.
- **Screens**: the dongle's OLED shows the active layer, modifiers, output and battery levels
  ([englmaxi/zmk-dongle-display](https://github.com/englmaxi/zmk-dongle-display)). Layers are named `ABC`, `NUM`,
  `MED`, `SYS` and `PTR` so it has something to show; three characters also keeps the left half's standalone
  layer arc the same width as its old `L#n` fallback. In dongle mode the left half's screen shows its own
  battery and whether its link to the dongle is up.
- **The dongle never deep-sleeps.** Deep sleep is only woken by a key press, and the dongle has no keys.

## Undongling

Flash `settings_reset` to both halves, then the standalone left firmware to the left half. The right half's
firmware is unchanged; it re-pairs with the left half as its central.

## How it is put together

- `boards/shields/toucan/toucan_dongle.*`: the keyless central. It reuses `toucan.dtsi` for the exact same matrix
  transform, physical layout and input pipeline, swaps in a mock kscan, and enables the trackpad listener.
  Board-agnostic: any BLE-capable ZMK board works as the dongle.
- `boards/shields/pandakb_dongle/`: PandaKB's OLED wiring, copied verbatim from their own dongle firmware
  ([PandaKBLab/zmk-corne-j-dongle](https://github.com/PandaKBLab/zmk-corne-j-dongle)). Its quirky panel values
  (`width = <129>`, `segment-offset = <1>`) are intentional. Kept separate so the dongle shield stays generic.
- `boards/shields/toucan/toucan_left_peripheral.overlay`: a modifier shield listed after `toucan_left`. Its
  presence flips the left half's role to peripheral through `Kconfig.defconfig`, so no cmake arguments are
  needed. It also disables the trackpad listener and the `trackpad_split` proxy. **ZMK's usual dongle recipe of
  just adding `-DCONFIG_ZMK_SPLIT_ROLE_CENTRAL=n` does not work for this keyboard**, because on a peripheral ZMK
  `BUILD_ASSERT`s that every enabled `zmk,input-split` names a `device`, and the left half's proxy has none.
- `nice_view_gem/widgets/screen_peripheral.c`: this keyboard's customized nice!view screen only ever had a central
  view, so a peripheral build of it could not link. Two more latent peripheral-only bugs came with it: the gem
  selected `ZMK_WPM` unconditionally, but ZMK builds `wpm.c` for every role while the keycode event it needs is
  central-only (now selected for the central only); and the output widget, which reads central-only state, was
  compiled for every role (now central only). Standalone builds are unaffected by all three changes.
- Central-only battery options are defaulted in `Kconfig.defconfig` keyed on the role rather than set in
  `toucan_left.conf`, so they follow whichever part is central.

# Companion app: LinearMouse

Because this trackpad enumerates over I2C/Bluetooth as a generic HID pointing device rather than Apple's
proprietary multitouch trackpad protocol, macOS applies its "external mouse" scroll-event handling to it instead
of the "Trackpad" pane's smoothed/inertial scrolling -- scrolling feels comparatively abrupt and jittery even with
the firmware-side `scroll` gesture enabled. [LinearMouse](https://linearmouse.app) fixes this at the OS level by
re-applying smoothed/inertial scrolling to this specific device without affecting other mice/trackpads. The
config is kept in this repo at [linearmouse/linearmouse.json](linearmouse/linearmouse.json), a copy of the live
`~/.config/linearmouse/linearmouse.json`. It has schemes for both ways the keyboard can connect: standalone, as
USB product name "Toucan" (vendor ID `0x1D50`, product ID `0x615E`), and dongle mode, as "Toucan Dongle". The
dongle schemes are also pinned to that dongle's USB serial number, so a replacement dongle needs them updated.
Scroll direction is left un-reversed there since the firmware's
`invert-scroll-y` already produces natural-scrolling direction, so reversing it again in LinearMouse would cancel
that out.

# License

The code in this repo is available under the MIT license.

The included shield nice_view_gem is modified from https://github.com/M165437/nice-view-gem licensed under the MIT License.

The linked trackpad module is based on https://github.com/geeksville/zmk_driver_azoteq

ZMK code snippets are taken from the ZMK documentation under the MIT license.

The embedded font QuinqueFive is designed by GGBotNet, licensed under under the SIL Open Font License, Version 1.1.
