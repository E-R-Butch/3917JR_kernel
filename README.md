# 3917JR-CIP4.19

Community maintenance workspace for the Rakuten BIG s 3917JR (`gaea`), based
on the device's Qualcomm SM7250/Lito Android 4.19 kernel.

> [!CAUTION]
> This repository is **not a flashable release**. Test kernels for this device
> are RAM-boot only: use `fastboot boot`, never write an experimental image to
> `boot_a`, `boot_b`, or another persistent partition.

The work began as a DroidSpaces repair. The stock kernel omitted host features
such as PID/UTS/IPC namespaces, TUN/TAP, VETH, and related container support;
root or Magisk cannot add compile-time kernel capabilities. Restoring them while
keeping the production Android hardware stack working required reconstructing
missing OEM support and its private-dependency compatibility, not merely changing
a few Kconfig options.

## Current status

The current tree is an exact source snapshot of the previously validated R16
kernel (`4.19.81-perf+`, build `#21`) plus repository documentation. The source
snapshot was exported from the preserved build volume and verified by file,
path, and symlink aggregate hashes before Git initialization.

R16 was previously exercised for more than 15 hours on one 3917JR. Android data
decryption, display, touch, Wi-Fi, camera, cellular service, audio, DroidSpaces,
and a basic Docker workload passed. That result is the golden regression
baseline; it is not proof that later commits are safe.

The final camera validation combined public kernel repairs with a local,
hash-checked compatibility guard for the proprietary Qualcomm CHI userspace.
That vendor binary and its local override are not distributed by this repository.

This import is **not yet a reproducible release**. The exact R16 builder,
external-module build glue, final packager, and signing-key continuity still
need to be reconstructed and independently verified. See
[`docs/PROVENANCE.md`](docs/PROVENANCE.md).

## Goals

- Preserve the R16 host capabilities that made DroidSpaces genuinely usable,
  including real namespace and TUN/VETH/bridge operation rather than a UI-only pass.
- Preserve the device-specific R16 hardware support without replacing the
  Android 10 vendor stack.
- Move incrementally from 4.19.81 through upstream 4.19 stable checkpoints,
  then evaluate selected CIP 4.19 checkpoints.
- Add practical container features in isolated commits: complete nftables
  families, CPU and block-I/O limits, and an ext4-backed Docker data path.
- Keep every hardware and userspace regression test reproducible and RAM-boot
  only.

This project does not aim to turn the phone into a mainline Linux daily driver,
ship proprietary firmware or stock boot images, or promise Android/GKI KMI
compatibility that the pre-GKI device never had.

## Repository layout

- `configs/gaea-r16-golden.config` — exact R16 kernel configuration.
- `configs/fragments/` — proposed changes, kept separate from the golden
  configuration until tested.
- `docs/ROADMAP.md` — staged stable/CIP integration plan.
- `docs/TEST_MATRIX.md` — mandatory RAM-boot gates.
- `docs/PROVENANCE.md` — source and artifact identity.
- `docs/MACOS_WORKSPACE.md` — case-sensitive filesystem requirements on macOS.
- `docs/ADAPTATION_HISTORY_ZH.md` — evidence-led R1-R16 DroidSpaces adaptation history.
- `docs/video/README.md` — public-safe handoff package for a video-production agent.
- `external/README.md` — pinned external Qualcomm module sources.
- `manifests/` — hashes for reference artifacts that are deliberately not
  committed.
- `scripts/check-release-tree.sh` — public-tree preflight.

## Safety and release rules

No stock boot image, kernel binary, module binary, firmware blob, toolchain,
private signing key, or proprietary vendor payload belongs in the public Git
history. Every release candidate must pass the checks in
[`docs/RELEASE_POLICY.md`](docs/RELEASE_POLICY.md) and the on-device matrix in
[`docs/TEST_MATRIX.md`](docs/TEST_MATRIX.md).

## 中文说明

这是乐天 BIG s 3917JR（代号 `gaea`）的社区维护工程。当前提交首先保存已经
实机验证过的 R16 源码基线，后续才会分阶段合并 4.19 stable、CIP 安全修复和
容器功能。项目最初目标是修复 DroidSpaces 在原厂内核上缺失的 namespace、
TUN、VETH 等宿主能力；为了不破坏 Android 量产硬件栈，最终又重建了缺失的
OEM 支持和私有依赖兼容。任何实验版都只允许 `fastboot boot` 临时启动，禁止
写入手机启动分区。

当前源码基线完整，但精确工具链、37 个替换模块（32 个 Qualcomm external-tree
模块加 5 个同树 core 模块）、最终封装器和签名链仍需重建验证，因此暂时不能
宣称“可复现发布”。相机最终验收还包含本机闭源 CHI 用户态兼容保护，该二进制
与覆盖件不在公开仓库中。

## License and attribution

The Linux kernel is distributed under the licenses described in `COPYING` and
the per-file SPDX identifiers. Imported components retain their original
licenses. This is an independent community project and is not affiliated with
Rakuten, Qualcomm, the device manufacturer, kernel.org, or the CIP project.
