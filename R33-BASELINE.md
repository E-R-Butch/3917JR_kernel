# R33 基线（2026-10-18）

在 R31 基础上补齐 **NFC** 与 **TUN/TAP**，六项硬件/能力全部实机通过。

## 组成

| 项 | 内容 |
|---|---|
| 内核 | `4.19.81-perf+ (sinclair@zenith) #33`，源码 `mainline @ abb155fd` |
| 镜像 | 原厂 ramdisk/dtb/cmdline + 我们的 Image（header v2、4096 页） |
| 内核新增 | NXP PN553 I2C 驱动（`drivers/nfc/pn8xT/pn553-i2c/`，官方仓库源码 + 4.19 API 适配） |
| 配置 | `3917JR-gaea-R33.config`（`CONFIG_NFC_PN553_DEVICES=y`、`CONFIG_TUN=y`、`LOCALVERSION="-perf+"`） |
| Magisk | `audio_3917jr` 模块 + `post-fs-data.d/{audio-rt1011.sh,tun-node.sh}` + `service.d/gaea-logd.sh` |

## 实机验证

| 项 | 证据 |
|---|---|
| 显示 | 熄屏/唤醒 8 轮无 DSI 错误；亮度切换不爆闪 |
| WiFi | `wlan0` 有 IPv4，`ping -I wlan0` 0% 丢包 |
| 音频 | RT1011 注册成功，外放正常 |
| **NFC** | `NxpHal: Polling Loop Started` → `NfcService: Tag detected`（身份证贴靠立即触发）|
| **TUN/TAP** | `/dev/net/tun` 存在；`ip tuntap add dev X mode tun` 建接口成功 |
| 容器 | `SYSVIPC`/`PID_NS`/`UTS_NS`/`NET_NS`/`VETH`/`NF_TABLES`/`CFS_BANDWIDTH` 全开 |

## TUN/TAP 说明

驱动与配置本来就在（`/proc/misc` 有 `200 tun`、`CONFIG_TUN=y`），缺的只是设备节点。
`post-fs-data.d/tun-node.sh` 补 `mknod /dev/net/tun c 10 200`，开机自动生效。

## NFC 说明

芯片在 i2c-0 `0x28` 枚举为 `pn544`；NXP `pn553.c` 的 `of_match` 含 `nxp,pn544`，直接绑定，
创建 `/dev/pn553`。上层 `android.hardware.nfc@1.2-service` 与 `NxpHal` 随即工作。
身份证读取时 `Check NDEF Failed - status = 3` 属预期（身份证不走 NDEF）。

## 仍未做

| 项 | 状态 |
|---|---|
| RT1011 单体校准 | `params` 分区为空，走默认曲线与原厂一致 |
| HBM 功能 | 驱动为回退后的空壳，被 mi 门控挡掉 |
| 模块签名强制 | `MODULE_SIG_FORCE=n` |
| LTS 升版 | 仍 4.19.81 |
| 温控接口 / 超频 / root 隐藏 | 未做 |

## 相关文档

- `reference/NFC-PN553.md` — NFC 驱动来源与移植
- `reference/AUDIO-RT1011.md` — 音频根因与 Magisk 侧修法
- `reference/STOCK-DISPLAY-PATH.md` — 亮度大端结论
- `reference/PARAMS-PARTITION.md` — params 分区实测为空
