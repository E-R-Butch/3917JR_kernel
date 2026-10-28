# audio_src_3917jr

Audio DLKMs for the 3917JR (SM6125 / lito) built from source, replacing the
prebuilt binaries that shipped in the earlier `audio_3917jr` module.

- Source: CodeLinaro `LA.UM.8.12.3.1` (`platform/vendor/opensource/audio-kernel`)
- Added on top of upstream: the RT1011 speaker-amp codec (`asoc/codecs/rt1011/`)
  and the GAEA SEC-MI2S machine-driver hooks in `asoc/kona.c`
- Built as modules (`=m`) against a tree whose `CONFIG_LOCALVERSION` matches the
  running kernel, so `vermagic` and the symbol CRCs line up:
  `4.19.81-gaea-R45 SMP preempt mod_unload modversions aarch64`
- 30 `.ko` files, renamed to the vendor file names (`*_dlkm.ko` -> `audio_*.ko`)

## How it loads

Magisk overlays `system/vendor/lib/modules/` over the vendor directory, so the
stock `modprobe -a` run from `init.target.rc` during early-init picks up these
files in the vendor's own order (ADSP is already up at that point, which is why
the audio stack cannot be built into the kernel image on this platform).

Four modules are not in that modprobe list and are loaded by `post-fs-data.sh`
instead: `audio_snd_event`, `audio_wcd9xxx`, `audio_mbhc`, `audio_swr_ctrl`,
and `audio_rt1011` (the amp must be registered before the machine driver).

Do not leave a second copy of `audio_rt1011.ko` reachable by a script in
`/data/adb/post-fs-data.d/`: Magisk executes every file in that directory
regardless of extension, and the older binary will win the race and make this
one fail with `exports duplicate symbol rl6231_calc_dmic_clk`.

## Verification

```
$ lsmod | grep -c dlkm                                # 29/30 loaded
$ cat /proc/asound/cards                              # lito-mtp-snd-card
$ dmesg | grep 'rt1011 2-0039'                        # calibration lines

# playback smoke test
$ tinymix "SEC_MI2S_RX Audio Mixer MultiMedia1" 1
$ tinyplay /data/local/tmp/test440.wav -D 0 -d 0
$ dmesg | grep -iE 'mi2s|afe' | tail
  msm_mi2s_snd_hw_params, rt1011 clock setting...
  __afe_port_start: port id: 0x1002
  afe_close: port_id = 0x1002
$ tinymix "SEC_MI2S_RX Audio Mixer MultiMedia1" 0
```

Known noise: `swrm_runtime_suspend+0x3c8 [swr_ctrl_dlkm]` warns once per boot
(`enable_irq` on an IRQ that was never requested). The speaker path here is
rt1011 + SEC-MI2S and does not go through SoundWire.

## Rollback

```sh
su -c 'touch /data/adb/modules/audio_src_3917jr/disable'
su -c 'rm -f /data/adb/modules/audio_3917jr/disable'   # re-enable the old module
```
