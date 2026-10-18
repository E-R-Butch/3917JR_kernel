# Roadmap — Gaea (3917JR / SM7250) kernel

Last updated: 2026-10-18, at baseline **R33** (`4.19.81-perf+ #33`).

Status legend: **done** / **next** / **later** / **blocked**

---

## Phase A — Usability (done, baseline R33)

Everything below is verified on device; see `R33-BASELINE.md` and `reference/`.

| Item | State | Notes |
|---|---|---|
| Display resume (screen would not wake) | **done** | Root cause was the HBM patch sending raw DSI commands (`dsi_panel_tx_cmd_set`) from the FOD path, corrupting the command queue → `PANEL_DEAD`. Reverted to `dsi_panel_disp_param_set`. |
| Brightness flash on every brightness change | **done** | Xiaomi `mi_cfg` defaults to little-endian; stock sends big-endian (`51 02 A2`). Forced `bl_is_big_endian = true`. See `reference/STOCK-DISPLAY-PATH.md`. |
| Stock vendor modules loading | **done** | version string pinned to `4.19.81-perf+`; `same_magic` warns-and-allows; CRC checks skipped (`check_version`, `check_modstruct_version`); `yl_params_kernel_read` implemented. |
| WiFi | **done** | `wlan0` up, ICMP OK. Required the module-loading work above. |
| Audio (RT1011) | **done** | Driver was never in the insmod list. Fixed on the Magisk side: module `audio_3917jr` + `post-fs-data.d/audio-rt1011.sh`. See `reference/AUDIO-RT1011.md`. |
| NFC (NXP PN553) | **done** | Ported from `NXPNFCProject/NXPNFC_I2CDriver` (`pn8xT/pn553-i2c`) + 4.19 API fixes. `/dev/pn553`, tag detection verified. See `reference/NFC-PN553.md`. |
| TUN/TAP | **done** | Driver and `CONFIG_TUN=y` were present; only `/dev/net/tun` was missing. Created by `post-fs-data.d/tun-node.sh`. |
| Container features (DroidSpaces) | **done** | `SYSVIPC`, `*_NS`, `CGROUP_*`, `CFS_BANDWIDTH`, `VETH`, `NF_TABLES` enabled; `ANDROID_PARANOID_NETWORK` off. |
| Crash forensics | **done** | `service.d/gaea-logd.sh` snapshots dmesg/logcat/state into `/cache/gaea-log/` every 20 s. |

**Rule learned the hard way:** the reported `uname -r` must stay `4.19.81-perf+`.
Changing it makes the stock display HAL take a different path (duplicate VSYNC →
visible flicker), even though vendor modules would still load. Version upgrades
must therefore change the *code*, not the *reported string*; identify builds via
`/proc/version` (`#33`) and `KBUILD_BUILD_USER/HOST`.

---

## Phase B — Whitepaper milestones

Order is fixed: **R3 → R4 → R2 → R5 (branch only) → R1 (last)**.

### R3 — Container capability (done)
Covered by Phase A. DroidSpaces self-check green, TUN/TAP included.

### R4 — Thermal interface (next)
Goal: let a userspace tuner (Scene / Femind) own scheduling without the kernel or
vendor daemons fighting it.

Already established on device:
- 100 thermal zones exist, trips are already `125000` (hardware max).
- Kernel policy is `user_space` for the enabled zones.
- Cooling devices idle (`cur_state = 0`).
- Measured 10 minutes at full load with **zero** frequency write-back while
  `thermal-engine` and `perf-hal-2-1` were running and the SoC sat at ~42 °C.
- Enabled zones must stay at 2 (86/88). Enabling all 100 breaks `thermal-engine`
  with a NULL deref in `thermal_sensor_get_trip_temp` → crash loop.

Remaining work:
- Define the switchable surface (zones / cooling devices / `kgsl thermal_pwrlevel`)
  and document it for the Femind Magisk backend (`THERMAL-SWITCHES.md`).
- Decide whether to expose a single kernel-side toggle or keep it userspace.

### R2 — LTS upgrade (later, largest)
- Merge 4.19 sublevels in reviewable ranges; keep `SUBLEVEL`/`LOCALVERSION`
  pinned so the reported string stays `4.19.81-perf+` (see Phase A rule).
- Cover the gap noted earlier: patches up to 4.19.325 are not fully applied.
- Module signature: recovering the vendor key from a signed `.ko` PKCS#7 trailer
  would allow `MODULE_SIG_FORCE=y` again.

### R5 — Overclocking (later, branch only)
Separate branch, never merged into the daily baseline. Not started.

### R1 — Root hiding (last)
Deferred by policy: keep `IKCONFIG_PROC`, `KALLSYMS_ALL`, `DEBUG_INFO` on until
the final image, then collapse them in one pass.

---

## Phase C — Known gaps / unowned

| Item | State | Note |
|---|---|---|
| HBM (under-display bright region) | **blocked** | Current driver is the reverted no-op; the MI feature gate blocks `disp_param_set`. Redo with the DRM-committed path only. |
| RT1011 per-unit calibration | **blocked** | `params` partition is empty (22 non-zero bytes: `DEVICE` / `V1160910LITO` / `LITO`). Stock behaves identically. Tune via ALSA DRC/EQ controls instead. See `reference/PARAMS-PARTITION.md`. |
| Femind frontend (Liquid Glass UI) | **later** | Built and installed (`me.sinclair.femind.gaea`), never accepted on a working display. |
| DTBO property injection | **blocked** | Panel node lives in DTBO `dtbo_idx=13`; adding properties to the DTBO image parses but the overlay is not merged into the live tree. Driver-side forcing used instead. |
