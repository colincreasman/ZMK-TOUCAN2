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
- **Swipe shortcuts**: the `swipe_button_mapper` node in [boards/shields/toucan/toucan.dtsi](boards/shields/toucan/toucan.dtsi)
- **Invert scroll / trackpad settings**: the `tps43_trackpad` node in [boards/shields/toucan/toucan_right.overlay](boards/shields/toucan/toucan_right.overlay)

# License

The code in this repo is available under the MIT license.

The included shield nice_view_gem is modified from https://github.com/M165437/nice-view-gem licensed under the MIT License.

The linked trackpad module is based on https://github.com/geeksville/zmk_driver_azoteq

ZMK code snippets are taken from the ZMK documentation under the MIT license.

The embedded font QuinqueFive is designed by GGBotNet, licensed under under the SIL Open Font License, Version 1.1.
