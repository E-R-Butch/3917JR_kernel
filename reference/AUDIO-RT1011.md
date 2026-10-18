# 音频（RT1011 功放）在自编内核上的启用方式

日期：2026-10-18 ｜ 结论来自 R31 实机验证（外放正常）

## 根因

R30 内核启动后 `kona-asoc-snd` 反复报：

```
ASoC: CODEC DAI rt1011-aif not registered
```

排查顺序与结论：

| 检查 | 结果 |
|---|---|
| 源码树里有没有 rt1011 驱动 | `sound/soc/codecs/rt1011*` **为空**，Kconfig 无 `SND_SOC_RT1011` ⇒ 本树（小米基线）根本没带这个驱动 |
| 原厂配置 | `CONFIG_SND_SOC_RT1011=y`（原厂内置） |
| 设备 `/vendor/lib/modules/` | 只有 stock 的 audio_*.ko，**没有** audio_rt1011.ko |
| 手动 insmod 已移植的 `audio_rt1011.ko` | **rc=0，加载成功** ⇒ 模块本身没问题，只是**开机时没人加载它** |

⇒ 不是内核缺能力，是 **insmod 列表里没有这一项**（原厂把它放在自己那套初始化里，我们的树/镜像里没有）。

## 修法（与内核解耦）

1. 复用 R17-r3 已验收的音频模块集
   （`/root/archive/evidence/host-work-snapshot/3917jr-audio-20260907/candidate-fix-p2/`，
   31 个 .ko，含 `audio_rt1011.ko` 与打过 MI2S 路由补丁的 `audio_machine_lito.ko`），
   做成 Magisk 模块 `audio_3917jr`：`system/vendor/lib/modules/*.ko`。

2. 用 `post-fs-data.d` 在 audio HAL / machine 驱动之前先加载 codec：

```sh
# /data/adb/post-fs-data.d/audio-rt1011.sh
M=/data/adb/modules/audio_3917jr/system/vendor/lib/modules
for k in audio_rt1011 audio_snd_event; do
  [ -f "$M/$k.ko" ] && insmod "$M/$k.ko" 2>/dev/null
done
```

3. 重启验证：`grep -c rt1011 /proc/modules` = 1，`rt1011-aif not registered` 计数 = 0，外放正常。

**这条修复不在内核里**：换内核不用重编，只要 Magisk 与模块在就生效。

## 遗留

`rt1011_r0_cali_get: smartpa cali read data fail!` —— RT1011 的单体校准数据存在 `params` 分区，
读取接口是 Coolpad 私有的（与 WiFi 需要的 `yl_params_kernel_read` 同一块存储）。
我们的 `yl_params_kernel_read` 目前返回 0，故校准走默认值：**能出声，但音量/保护曲线不是这台机器的标定值**。
要补需实现 `params` 分区里 RT1011 校准段的解析（格式未知，需逆向原厂读取路径）。
