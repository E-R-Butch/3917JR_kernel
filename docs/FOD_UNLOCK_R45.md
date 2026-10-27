# FOD unlock bring-up — R45

This note records why lock-screen fingerprint unlock was dead on the
self-built kernel and what the working fix is. It is the fingerprint half of the
touch driver work; see `ADAPTATION_HISTORY_ZH.md` for the earlier display/audio
history.

## Symptom

With the vendor fingerprint stack untouched (template enrolled, TA/algorithm
healthy, HAL alive, `HAL deaths: 0`), pressing the sensor on the screen-on lock
screen or on AOD produced no `KEY_FOD_DOWN` for SystemUI, so no authentication
started and the phone never unlocked. Touch itself worked while the screen was
on: the normal FOD packets (`buf[1] = 0x01/0x02`) were delivered.

## Root cause

The stock FTS gesture dispatcher indexes its jump table with `gesture_id - 0x20`.
Table indices `0x54` / `0x27` therefore correspond to raw FOD gesture IDs
`0x74` / `0x47`. The port had been mapping the *raw* IDs `0x54` / `0x27`:

- raw `0x74` (FOD press while the panel is suspended, i.e. AOD) matched nothing;
- raw `0x54` is the letter-V gesture in the generic FTS table, so the FOD press
  stole it and produced no FOD key either;
- raw `0x02` release was fine only in the screen-on path, where `fts_fod_key_report()`
  handles the normal touch packets instead of the gesture table.

## Fix

- `focaltech_gesture.c`
  - `GESTURE_FOD_PRESS 0x74 -> KEY_FOD_DOWN (508)`,
    `GESTURE_FOD_RELEASE 0x47 -> KEY_FOD_UP (509)`.
  - restore raw `0x54 -> KEY_GESTURE_V`; raw `0x27` stays unmapped.
  - `fts_gesture_report()` now takes `struct fts_ts_data *` so the diagnostic can
    print the suspend state along with the gesture.
- `focaltech_core.c`
  - `fts_fod_key_report()` logs state transitions (raw event, point count,
    `suspended`) and the key actually emitted. Press/release de-duplication and
    normal-packet handling are unchanged.

Build configuration differs from the previous candidate only by
`CONFIG_LOCALVERSION="-gaea-R44" -> "-gaea-R45"`.

## Verified

Kernel `4.19.81-gaea-R45`, version `#45`, `sinclair@zenith`, RAM-booted with
`fastboot boot` on device `2849a5ad`; boot ID unchanged for the whole run.

- 2026-10-24 15:37 screen-on lock: driver `raw=0x01 -> key=508, suspended=0`,
  then `raw=0x02 -> key=509`; SystemUI/HAL `onAuthenticated(true)`.
- 2026-10-24 15:37 AOD: `gesture raw=0x74 key=508 suspended=1`;
  `onAuthenticated(true)`.
- `dumpsys fingerprint`: `accept 0 -> 2`, `reject 0`, `acquire 1 -> 7`,
  `HAL deaths 0`.
- 2026-10-27: further real unlocks on the same RAM-boot session, `accept 2 -> 8`;
  driver records `raw=0x01 -> key=508` / `raw=0x02 -> key=509` with
  `suspended=0` in that window.

## Limitations

- This candidate is **RAM-boot only**. `boot_a/boot_b/dtbo_a/dtbo_b` were not
  written; a reboot returns to the stock kernel. Persistent-flash verification
  (including reboot) is not part of this record yet.
- The raw `0x47` release gesture was not observed with a real finger; its mapping
  rests on the stock disassembly (jump-table index arithmetic) plus a static
  mapping check, not on a captured event.
- One unlock per path was captured on 2026-10-24; that does not establish
  long-term reliability or multi-finger behaviour.

## Artifacts

Binaries and logs stay outside Git per `RELEASE_POLICY.md`; hashes are recorded
in `manifests/R45_ARTIFACTS.sha256`.

Boot package: header v2, page 4096, OS 10.0.0 / patch 2023-02, kernel load
0x8000, ramdisk 0x1000000, tags 0x100, dtb 0x1f00000. The image carries the
device's own (Magisk-patched) ramdisk and DTB unchanged; only the kernel segment
differs from the flashed boot partition.

