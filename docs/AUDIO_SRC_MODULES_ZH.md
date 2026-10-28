# 音频栈改为源码编译模块（R45 内核，从 /data 覆盖）

日期：2026-10-28
设备：3917JR（`2849a5ad`），内核 `4.19.81-gaea-R45` #45

## 1. 结论：内建音频在这台机器上不可行

三种内建形态全部在 adbd 起来之前死掉，且**不产生任何日志**：

| 形态 | 内容 | 结果 |
| --- | --- | --- |
| R46 | 音频基础树 + RT1011 + gaea 钩子，全内建 | 挂死 |
| R46-diag | 同上 + PSTORE | 挂死 |
| R46a | 仅音频基础树（无 codec、无钩子），全内建 | 挂死 |
| R45-reader | R45 + PSTORE + ramoops DT | 挂死（另一路：进 EDL） |

机理：厂商把这套驱动作为 DLKM，在 `init.target.rc` 里由 `modprobe -a` 于 `audio_adsp_loader` **之后**加载——此时 ADSP 已把 LPASS 拉出复位、时钟已开。编进内核后 probe 提前到 `device_initcall`，在硬件未上电时访问 LPASS 寄存器，属于总线级死锁：无 oops、无 panic、不进 800/9008。

## 2. 为什么拿不到日志（已查清）

- 打开 `PANIC_ON_OOPS`、`BOOTPARAM_SOFTLOCKUP_PANIC`、`BOOTPARAM_HUNG_TASK_PANIC` 后仍不 panic：总线死锁连定时器中断都进不来，softlockup/hung_task 都无法触发。
- XBL 日志分区 `logfs` 里每次重启都是 `PonReason 0x81 / hard_reset`（电源键），说明 XBL 从未收到崩溃原因；`dip`/`logdump`/`rawdump` 三个分区全 0 字节，minidump 从未落盘。
- 主动 panic（`echo c > /proc/sysrq-trigger`）**会**进 9008 crash-dump 模式，`edl.py memorydump` 能枚举整个 dump 区表——这条通道成立，只是对"挂死"无效。

### 已就绪但本次未用上的取证能力

- `drivers/soc/qcom/klog_minidump.c`：2 MB 静态缓冲区 + `kmsg_dumper`，`fs_initcall` 时用 `msm_minidump_add_region()` 注册为 minidump 区域 `KLOG_BUF`。真 panic 时内核日志会随 minidump 被 XBL 收走，可用 `edl.py memorydump` + `klog_extract.py` 读出。**挂死场景无效**（没有 panic 就没有回调）。
- Mac 侧工具链：`~/Projects/edl`（bkerler EDL）+ venv `~/.hermes/venvs/edl`；`memorydump` 必须一次跑完，中途 Ctrl-C/超时会卡住 Sahara 会话，只能断电恢复。

## 3. 交付：`audio_src_3917jr` 模块

思路：保持厂商的加载架构（overlay 覆盖 `/vendor/lib/modules`，由 `init.target.rc` 的 `modprobe -a` 按原顺序加载），但把二进制换成本仓库源码编出来的。

- 源码：CLO `LA.UM.8.12.3.1`（`/root/audio-kernel-8.12`）+ RT1011 codec（`asoc/codecs/rt1011/`）+ gaea SEC-MI2S 机器驱动钩子（`asoc/kona.c`）
- 构建内核：`/root/kernel-r45mods`（`v4.19.81-gaea-r45` tag）+ `/root/kbuild-r45mods`，配置取 `.config.r46-start`，`CONFIG_LOCALVERSION=-gaea-R45`
- vermagic 与设备内核完全一致：`4.19.81-gaea-R45 SMP preempt mod_unload modversions aarch64`
- 产物 30 个 `.ko`，按厂商命名落到 `audio_*.ko`（`*_dlkm` → `audio_*`）
- 模块目录：`/data/adb/modules/audio_src_3917jr/`（`system/vendor/lib/modules/*.ko` + `post-fs-data.sh`）

### 关键构建坑

- 音频 `Makefile` 的 LITO 分支必须 `include litoauto.conf` 后紧跟 `export`，否则 `CONFIG_*=m` 传不到子目录，全部按未定义处理、一个 `.ko` 都不出。
- 三个符号链接指向不存在的 `kernel/msm-4.19/...`，需改为相对 `drivers/pinctrl`、`drivers/base/regmap`。
- 必须用顶层 `make modules`（`O=` 输出目录里出 `.ko`），子目录目标或 `M=techpack/audio` 只编译不链接模块。
- **Magisk 会执行 `post-fs-data.d` 下所有文件，不只看 `.sh`**：旧脚本改名成 `.sh.bak` 仍会被执行，导致旧 `audio_rt1011`（65 KB）先加载、我们的 `rt1011_dlkm` 报 `exports duplicate symbol rl6231_calc_dmic_clk`。必须把它移出该目录。

### 验证结果

- 29 个 `*_dlkm` 全部加载，`rt1011_dlkm`、`machine_dlkm` 的 build-id 与源码产物一致
- RT1011 校准复现：`ADC offset=0x0`、`Gain0 offset=0x1`、`Gain1 offset=0x4a`、`r0 resistance about 6.76 ohm`、`3917JR RT1011 calibration ready for factory R0`
- 声卡 `lito-mtp-snd-card` 在位，`/sys/kernel/debug/asoc/` 有 `components`/`dais`
- **外放实测通过**（用户确认听到）：`tinymix "SEC_MI2S_RX Audio Mixer MultiMedia1" 1` + `tinyplay` 放 2 秒 440 Hz，内核日志 `msm_mi2s_snd_hw_params, rt1011 clock setting...` → `__afe_port_start: port id: 0x1002` → 2.3 秒后 `afe_close`，无 `SEC MI2S bit clock stop failed`、无 rt1011 报错
- 回归：`wlan0` UP，`android.hardware.biometrics.fingerprint@2.1-service` 常驻
- 新增一条 WARNING（历史启动仅 1 条）：`swrm_runtime_suspend+0x3c8 [swr_ctrl_dlkm]` → `enable_irq` 对未申请 IRQ，属 CLO SWR 主控在无 SWR 从设备时的 suspend 路径问题；本机喇叭走 rt1011 + SEC-MI2S，不经 SWR，仅告警不阻断

## 4. 回滚

```sh
su -c 'touch /data/adb/modules/audio_src_3917jr/disable'
su -c 'rm /data/adb/modules/audio_3917jr/disable'
su -c 'mv /data/local/tmp/audio-rt1011.sh.disabled /data/adb/post-fs-data.d/audio-rt1011.sh'
# 重启
```

## 5. 待办

- 声音实测（外放）与 WiFi/指纹回归
- `swr_ctrl` suspend 的 `enable_irq` 告警：可在 `swrm_runtime_suspend` 里加 IRQ 有效性判断，需重编模块后验证
