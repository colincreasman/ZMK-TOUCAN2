# Dongle Setup — ELI5

The dead-simple guide to flashing your Toucan2 and setting up the PandaKB dongle.

> All firmware files live in [`firmware/latest/`](../firmware/latest) (a symlink to
> the newest build, currently `firmware/dongle_setup/`).

## The 3 pieces

You have **3 little computers**: the **left half**, the **right half**, and the new
**dongle**. Right now the left half is the boss and the right half talks to it.
We're making the **dongle the new boss** — both halves talk to it, and the dongle
plugs into your computer by USB.

## How to flash *any* piece (the move you'll repeat)

1. Plug that piece into your computer with a USB cable.
2. **Double-tap its reset button** → a little USB drive pops up on your Mac (like a
   thumb drive).
3. **Drag the correct `.uf2` file onto that drive.** It copies, the drive
   disappears, done. That piece just got its new brain.

- Halves: the reset button is the tiny button on the XIAO chip.
- Dongle: **pop off the magnetic case first** (the case buttons are
  fake/decorative), then double-tap the reset button on the board.

## Do it in this order

### Step 1 — wipe old memories (folder `1_reset_first/`)

The halves still "remember" being paired to each other, so we clear that first.

| Flash this file | onto this |
|---|---|
| `BOTH_HALVES_settings_reset.uf2` | left half |
| `BOTH_HALVES_settings_reset.uf2` | right half (yes, same file) |
| `DONGLE_settings_reset.uf2` | dongle |

### Step 2 — install the real firmware (folder `2_then_flash/`)

| Flash this file | onto this |
|---|---|
| `DONGLE_toucan_dongle.uf2` | dongle |
| `LEFT_toucan_left_dongle_mode.uf2` | left half |
| `RIGHT_toucan_right.uf2` | right half |

### Step 3 — let them find each other

1. ⚠️ Turn **off any other ZMK keyboards** nearby (the dongle grabs the first
   halves it sees).
2. Plug the dongle into your computer's USB.
3. Turn both halves on.
4. Wait a few seconds. The **left half's screen changes from `SEARCHING` →
   `LINKED`**. That means it worked. 🎉

### Step 4 — clean up your Mac

Go to Bluetooth settings and **remove the old "Toucan"** entry. You don't pair over
Bluetooth anymore — the dongle *is* the connection now. Just plug it in and type.

## Two things to remember later

- **Moving to a different computer?** Just move the dongle. No pairing, ever. It
  works like any USB keyboard/mouse receiver.
- **Want to go back to no-dongle?** Flash the two files in the
  `standalone_no_dongle/` folder onto the halves (after a settings reset), and
  you're back to normal.

## If something's weird

- **Screen stuck on `SEARCHING`:** make sure both halves are powered on and no
  other ZMK keyboard is stealing them; re-seat the dongle.
- **A half won't show its flashing drive:** double-tap the reset a little faster,
  or use the keyboard combo — hold **both far outer thumbs** then hold **Q** (left)
  or **P** (right) for 2 seconds.
- **To update *just the dongle* later** without opening the case: hold both far
  outer thumbs, then hold **T** for 2 seconds → it pops into flashing mode.
