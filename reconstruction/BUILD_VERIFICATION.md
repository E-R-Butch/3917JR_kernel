# Build verification

本记录对应 `agent/reconstruct-3917jr-kernel` 分支的社区重建基线。2026-07-14 在 Linux 区分大小写容器卷中完成了全树编译和独立产物核对。

## 工具链

- Android Clang `r365631c`：Clang 9.0.8，Android build 5900059。
- GNU Binutils for Ubuntu 2.34：`aarch64-linux-gnu-ld`。
- 架构与目标：`ARCH=arm64`，目标 `Image`。
- 构建目录：Linux 区分大小写卷。macOS 默认不区分大小写的 checkout 不作为验证目录。

旧版 `ld.lld` 在本树的 `MODPOST vmlinux.o` 阶段出现输出尺寸计算溢出；改用同一容器中的 AArch64 GNU ld 后，全树、kallsyms、最终 `vmlinux` 和 `Image` 链接均完成。

## 结果

| 产物 | 大小 | SHA-256 |
| --- | ---: | --- |
| `3917JR-reconstructed-droidspaces.Image` | 29 MiB | `57918f5e5c43d436d59bee721f34c52c92fd680c132ceaffa4b1dd405b0a1081` |
| `3917JR-reconstructed-droidspaces.config` | 152 KiB | `15ed20f59c7dd11d1edfbce9ee61c7e88b2f518a251ff967c52f9b265c680044` |
| `3917JR-reconstructed-droidspaces.vmlinux` | 275 MiB | `9d22858292695b40dff1a6f24dbee9cd3a1a5ddddfa502ba79bb88a8eaf02eb1` |

`file` 将 `Image` 识别为 little-endian、4 KiB pages 的 Linux ARM64 boot executable；`vmlinux` 是静态链接的 AArch64 ELF，Build ID 为 `70ce2e703224a070554eccd150046a07c8e0b570`。

## DroidSpaces 配置核验

最终生成的 `.config` 中以下选项均为内建 `y`：

- `CONFIG_SYSVIPC`
- `CONFIG_POSIX_MQUEUE`
- `CONFIG_PID_NS`
- `CONFIG_UTS_NS`
- `CONFIG_IPC_NS`
- `CONFIG_CGROUP_DEVICE`
- `CONFIG_DEVTMPFS`
- `CONFIG_VETH`
- `CONFIG_TOUCHSCREEN_FTS`

构建日志同时确认 PID / UTS / IPC namespace、device cgroup、devtmpfs、veth、公开 FocalTech 驱动及 FT3518 升级对象、camera / display / video techpack 都参与了实际编译。

## 边界

- 本记录证明源码与配置能够完整生成 ARM64 内核，不证明该内核已在 3917JR 真机启动。
- 公开树没有 3917JR 的可验证 DTS 源码；后续临时启动镜像需继续配用从原厂 `boot_a` 提取并核对过的 DTB 和 ramdisk，这些专有镜像不进入仓库。
- Pixelworks Iris3 的精确 4.19 驱动仍缺失并保持关闭，真机可能无法点亮内屏。
- 未执行 `fastboot boot`，也未写入任何 boot 分区。
