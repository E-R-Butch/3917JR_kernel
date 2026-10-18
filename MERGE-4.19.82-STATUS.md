# 4.19.82 合并 — 接手说明（WIP）

分支 `merge/4.19.82` @ `4c809da0e`。**未编译、未上机**，`mainline` 未受影响。

## 已完成

- `git apply --reject` 应用 `patch-4.19.81-82`（增量补丁，163 文件）→ **139 个文件干净应用**。
- 无关冲突已丢弃并记录（异构架构 / 未编译驱动）。
- 冲突按文件以 `.rej` 形式留在树里，未做任何猜测性合并。

## 剩余 17 处冲突

| 文件 | 优先级 | 说明 |
|---|---|---|
| `fs/f2fs/recovery.c` | **P0** | 本机 data 分区就是 f2fs |
| `fs/f2fs/super.c` | **P0** | 同上 |
| `drivers/block/zram/zram_drv.c` | **P0** | zram 在用（Scene/Femind 都依赖）|
| `drivers/usb/dwc3/gadget.c` | P1 | dequeue 真实修复；厂商树上下文不同，需按实际代码重打三处 hunk |
| `drivers/hid/*ff.c` ×13 | P2 | 力反馈统一 API 变更，可整组按上游替换；这些驱动不编译也安全 |

已丢弃的无关冲突：`arch/s390/mm/cmm.c`、`drivers/infiniband/hw/hfi1/sdma.c`、
`drivers/virt/vboxguest/vboxguest_utils.c`、`drivers/net/wireless/ath/ath6kl/usb.c`、
`drivers/net/wireless/ath/wil6210/txrx_edma.c`、`drivers/net/wireless/realtek/rtlwifi/ps.c`、
`drivers/nfc/pn533/usb.c`、`drivers/iio/imu/adis_buffer.c`、`arch/arm64/include/asm/cputype.h`
（该 hunk 只是加 Hisilicon 的 MIDR 宏，与本机无关）。

## 接手步骤

```bash
# 1. 进树（容器 gaea-kernel）
lxc exec gaea-kernel -- bash -lc 'cd /root/kernel && git checkout merge/4.19.82 && git status'

# 2. 解冲突（先 P0）
#    每个 .rej 里是上游 hunk；对照当前文件手工合并，然后删掉对应 .rej
find . -name '*.rej' | sort

# 3. 编译
export PATH=/root/tooling/toolchain/bin:/root/tooling/deps/binutils234/usr/bin:/root/tooling/deps/bin:/usr/local/bin:/usr/bin:/bin
export LD_LIBRARY_PATH=/root/tooling/deps/binutils234/usr/lib/x86_64-linux-gnu:/root/tooling/deps/root/usr/lib/x86_64-linux-gnu:/root/tooling/toolchain/lib64
export KBUILD_BUILD_USER=sinclair KBUILD_BUILD_HOST=zenith KBUILD_BUILD_VERSION=35
cd /root/kernel && scripts/config --file /root/kbuild/.config --set-str LOCALVERSION "-gaea-R35"
make O=/root/kbuild ARCH=arm64 CC=clang.real LD=aarch64-linux-gnu-ld \
     CLANG_TRIPLE=aarch64-linux-gnu- CROSS_COMPILE=aarch64-linux-gnu- \
     HOSTCFLAGS=-fcommon Image

# 4. 组装与上机
#    /root/kbuild/arch/arm64/boot/Image → aosp-build:/root/Image-R35
#    repack33.sh（复制成 repack35.sh 改镜像名）→ 用原厂 ramdisk/dtb/cmdline 重打包
#    fastboot boot <img>
```

## 版本号相关（已澄清，不必再纠结）

- `uname -r` **可以任意改**，与显示无关（R34 已实机对照）。识别版本靠 `/proc/version` 的 `#NN`。
- 原厂 `.ko` 能加载靠三件事：`same_magic` 告警放行、`check_version`/`check_modstruct_version` 放行、
  补 `yl_params_kernel_read`。升子版本不需要再动这三处。

## 现成基线（要回退时用）

| 产物 | 位置 |
|---|---|
| R33 内核 Image | 容器 `gaea-kernel:/root/archive/builds/R33/Image` |
| R33 配置 | 容器 `gaea-kernel:/root/archive/configs/3917JR-gaea-R33.config` |
| R33 完整镜像 | 容器 `aosp-build:/root/3917JR-R22-RAMBOOT_ONLY_NEVER_FLASH.img`（重打包产物，命名沿用）|
| R33 说明 | `R33-BASELINE.md` |

## 已知缺口（与本次合并无关）

HBM 驱动为回退空壳、RT1011 校准（params 分区为空，原厂同）、`MODULE_SIG_FORCE=n`、
Femind 前端未验收、DTBO 属性注入未并入。详见 `R33-BASELINE.md` 与 `docs/ROADMAP.md`。
