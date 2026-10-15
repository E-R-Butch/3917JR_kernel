# Contributing

This device has tightly coupled downstream display, touch, camera, audio,
storage, encryption, and modem code. Small-looking changes can prevent Android
from decrypting data or reaching userspace, so changes are intentionally staged.

## Change discipline

1. Keep upstream stable/CIP merges, device fixes, configuration changes, and
   packaging changes in separate commits.
2. Preserve authorship and commit messages for upstream backports. Add the
   upstream commit ID and source tag when a patch is manually adapted.
3. Rebuild every external module at each ABI checkpoint. Never reuse a `.ko`
   from a different kernel build.
4. Do not commit generated binaries, proprietary payloads, or signing keys.
5. Run `scripts/check-release-tree.sh` before publishing.
6. Record build identity and every completed test gate in the pull request or
   release notes.

## Testing

Compilation alone is not acceptance. Follow `docs/TEST_MATRIX.md`, beginning
with offline artifact checks and ending with a temporary `fastboot boot` on the
target device. A failure in UFS, Android data decryption, ADB, required module
loading, or a golden hardware function blocks promotion.

Never ask a tester to flash an experimental build persistently.

## Commit subjects

Use a short subsystem prefix where practical, for example:

```text
netfilter: enable gaea container nftables set
gaea: iris: preserve R16 ABYP recovery behavior
stable: merge Linux 4.19.100
```
