# Yulong / Coolpad 3917JR kernel

这是 Rakuten BIG s（Yulong / Coolpad 3917JR，设备代号 `gaea`）Linux 4.19.81 内核源码的社区归档与可编译重建分支。

原始 Yulong GPL 仓库缺少 `coolpad/`，并把 `techpack/camera`、`techpack/display`、`techpack/video` 留成了无法解析的 gitlink。本分支用同代公开 SM7250 4.19 源码恢复这些构建依赖，保留来源和补丁记录，并加入 DroidSpaces 所需的 namespace、cgroup 与网络配置。

## 当前状态

- 已恢复 camera / display / video techpack 的公开源码。
- 已补齐恢复显示子树所需的公开 DRM 接口。
- 已加入最小 `coolpad/` 构建骨架；缺失的私有 OEM 驱动不会伪装成原厂源码。
- 已保存从实机 `boot_a` 提取的内核配置，以及重建和 DroidSpaces 配置片段。
- 已启用 PID、UTS、IPC namespace、System V IPC、device cgroup、devtmpfs、veth 等 DroidSpaces 依赖。
- 已用 Android Clang `r365631c` 和 AArch64 GNU ld 完整编译出 ARM64 `Image`；哈希和配置核验见 [构建验证记录](reconstruction/BUILD_VERIFICATION.md)。
- 当前产物仍未在真机启动验证，请先阅读 [重建说明](reconstruction/README.zh-CN.md)。

## 构建

建议使用 Yulong 构建配置对应的 Android Clang `r365631c`，并安装 AArch64 GNU binutils（构建脚本调用 `aarch64-linux-gnu-ld`；旧版 `ld.lld` 链接此树时会发生尺寸溢出）：

```bash
export TOOLCHAIN=/path/to/clang-r365631c
./reconstruction/scripts/build.sh
```

默认输出目录是 `out/gaea-reconstructed`。脚本会从实机配置开始，应用重建与 DroidSpaces 配置片段，验证关键选项，再构建 arm64 `Image`。

请在 Linux 或区分大小写的文件系统中 checkout 和构建。内核树包含 Linux 正常但 macOS / Windows 默认文件系统会冲突的大小写文件名；macOS 上建议使用区分大小写的 APFS 卷或 Linux 容器卷。

## 重要限制

这不是泄露的 Yulong 私有源码，也不是已经证明可刷入的成品内核。Pixelworks Iris3、部分指纹 / NFC / OEMInfo / 电源复位等厂商驱动没有可验证的同版本公开来源，安全基线中保持关闭。不要在没有原始分区备份和可用恢复路径的情况下刷写。

详细来源、恢复方法与已知缺口见：

- [重建与验证说明](reconstruction/README.zh-CN.md)
- [构建验证记录](reconstruction/BUILD_VERIFICATION.md)
- [源码来源与许可](reconstruction/SOURCES.md)
- [配置片段](reconstruction/configs)
- [恢复补丁记录](reconstruction/patches)
