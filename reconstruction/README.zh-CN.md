# 3917JR / gaea 内核重建说明

## 为什么需要重建

Yulong 发布的 3917JR GPL 仓库并不是一个完整 checkout：

- `msm-4.19/coolpad/` 整个目录缺失，但顶层 Kconfig 和 Makefile会引用它。
- camera、display、video 三个 techpack 是 gitlink，仓库没有 `.gitmodules`，对应对象无法直接取得。
- 3917JR 量产内核 `4.19.81-perf+` 关闭了 DroidSpaces 需要的 PID、UTS、IPC namespace 等选项。

本分支的目标是保存一个来源清楚、能够继续维护的社区重建基线，而不是声称恢复了不可获得的私有源码。

## 恢复方法

1. 以 Yulong 官方仓库提交 `d85e94e81b392193f9bf242e63eba8699fcc5195` 为外层基线。
2. 使用公开的 Xiaomi SM7250 Linux 4.19.81 树恢复 camera / display / video。与 Yulong 外层树共有 59,915 个路径，其中 59,209 个文件内容完全相同（98.82%），因此它比跨版本移植更接近缺失 gitlink 所处的代码世代。
3. 加入最小 `coolpad/` Kconfig / Makefile 骨架，使公开代码可以解析；没有公开来源的内建 OEM 驱动在重建配置中关闭。
4. 使用 Yulong 树已有的公开 FocalTech 驱动 `drivers/input/touchscreen/focaltech_touch` 代替缺失的私有 `CONFIG_YL_TOUCHSCREEN_FT3518` 包装层。
5. 补齐恢复显示子树需要的 DRM touch event、MI notifier、MIPI 大端亮度接口和 `drm_bridge` 字段。
6. 应用 DroidSpaces 非 GKI 的 qtaguid / cgroup 兼容修复，并启用 namespace、cgroup、devtmpfs、overlayfs 和虚拟网络配置。

## 已知缺口

下面这些量产内核功能没有可验证的 4.19.81 公开源码，安全基线中保持关闭：

- Pixelworks Iris3 的 3917JR 精确驱动与平台 glue。
- Coolpad 私有 FT3518 包装层及部分工厂测试代码。
- 指纹、NFC SN100F、OEMInfo、boot reason、reserved RAM、部分电源 / 重启与音频校准驱动。
- 私有固件、签名密钥、量产 ramdisk 和供应商二进制模块。

公开 TCL Iris3 代码与实机符号、DT 属性有较高重合，但来自 4.14 世代；在完成 4.19 API 审计和真机临时启动验证前，不直接混入本分支。关闭 Iris3 可能导致内屏无法点亮，因此当前 Image 只能视为“编译基线”，不是可安全刷写的发布版。

## 实机证据

- 设备：Rakuten BIG s / Yulong-Coolpad 3917JR，代号 `gaea`，SoC 为 SM7250 / lito。
- 量产版本：`Linux version 4.19.81-perf+`。
- `boot_a` 中的内核和 DTB 与公开 TWRP 设备树携带的预编译文件一致。
- 实机配置 SHA-256：`a12977dda55ea7e8a419325659d2abd81db10eb3952b171c9e1e4009050ded14`。
- 量产配置缺少 `CONFIG_PID_NS`、`CONFIG_UTS_NS`、`CONFIG_IPC_NS`、`CONFIG_SYSVIPC`、`CONFIG_CGROUP_DEVICE`、`CONFIG_DEVTMPFS`、`CONFIG_VETH`。

实机 boot / recovery / dtbo / vbmeta 镜像、ramdisk、固件和其他专有分区内容不进入公开仓库。

## 构建与检查

```bash
export TOOLCHAIN=/path/to/clang-r365631c
export OUT=/absolute/path/to/out
./reconstruction/scripts/build.sh
```

源码必须位于区分大小写的文件系统。macOS 默认 APFS checkout 会折叠一部分 Netfilter 大小写文件名，不能作为可信构建目录；本次验证使用 Linux 容器中的区分大小写卷。

构建脚本会：

1. 复制 `reconstruction/configs/gaea-stock.config`。
2. 应用 `reconstructed.config` 和 `droidspaces-4.19.config`。
3. 运行 `olddefconfig`。
4. 确认关键 namespace / cgroup / 网络选项实际为 `y`。
5. 构建 `Image`，输出最终配置、差异和 SHA-256。

2026-07-14 已在 Linux 区分大小写卷中使用 Android Clang `r365631c` 与 GNU Binutils 2.34 完成全树构建。旧版 `ld.lld` 在此高通 4.19 树的 `MODPOST vmlinux.o` 阶段会把输出尺寸计算溢出，因此脚本固定使用 `aarch64-linux-gnu-ld`。完整产物哈希与选项核验见 [BUILD_VERIFICATION.md](BUILD_VERIFICATION.md)。

在真机验证前，不应把编译成功等同于可以刷入。推荐的下一步是基于原始 `boot_a` ramdisk 和 DTB 生成临时 boot 镜像，核对大小与头部后，仅在用户明确确认时执行 `fastboot boot`；不要直接写入 boot 分区。
