# R31 基线（2026-10-18）

自编内核 + Magisk 侧修复组合，**显示 / WiFi / 音频 / 容器**四项均实机通过。

## 组成

| 项 | 内容 |
|---|---|
| 内核 | `4.19.81-perf+ (sinclair@zenith) #30`，源码 `mainline @ 5dfdd639d` |
| 镜像 | boot.img 由**原厂** ramdisk/dtb/cmdline + 我们的 Image 重打包（header v2、4096 页） |
| 配置 | `3917JR-gaea-R31.config`（fix 基线 + `LOCALVERSION="-perf+"` + `MODULE_SIG_FORCE=n`） |
| Magisk 模块 | `audio_3917jr`（31 个音频 .ko，覆盖 `/vendor/lib/modules/`） |
| Magisk 脚本 | `post-fs-data.d/audio-rt1011.sh`（HAL 之前 insmod RT1011） |
| 取证脚本 | `service.d/gaea-logd.sh`（20 秒落盘 dmesg/logcat/状态到 `/cache/gaea-log/`） |

> 注：内核串保持 `4.19.81-perf+` 与原厂一致（改版本串会让原厂 HWC 进入异常路径，
> 实测出现重复 VSYNC 与高频闪烁）。识别版本用 `/proc/version` 里的 `#30`。

## 已验证

| 项 | 结果 |
|---|---|
| 熄屏/唤醒 8 轮 | 正常，`dsi` / `PANEL_DEAD` 计数 0 |
| 亮度切换（dim ↔ 恢复） | 不爆闪（`bl_is_big_endian = true`）|
| WiFi | `wlan0` 起来，`ping -I wlan0` 0% 丢包 |
| 音频外放 | 正常（RT1011 已注册）|
| 容器能力 | `SYSVIPC`/`PID_NS`/`UTS_NS`/`NET_NS`/`VETH`/`NF_TABLES`/`CFS_BANDWIDTH` 全开 |
| 原厂模块 | 32 个全部加载（vermagic 不一致时告警放行）|

## 已知缺陷（下一步）

| 项 | 状态 |
|---|---|
| NFC | 配置未开（`CONFIG_YL_NFC_NXP_SN100F`） |
| RT1011 单体校准 | `params` 分区读取未实现，音量/保护曲线走默认值 |
| HBM 功能 | 驱动为回退后的空壳，被 mi 门控挡掉，不生效 |
| 模块签名强制 | `MODULE_SIG_FORCE=n`（无原厂私钥）|
| LTS 升版 | 仍是 4.19.81 |
| 温控接口 / 超频 / root 隐藏 | 未做 |

## 相关文档

- `reference/STOCK-DISPLAY-PATH.md` — 亮度字节序的大端结论与证据链
- `reference/AUDIO-RT1011.md` — 音频根因（insmod 列表缺失）与 Magisk 侧修法
- `R24-BASELINE.md` — 显示/WiFi 三个根因的详细记录
