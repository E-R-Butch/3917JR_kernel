# NFC（NXP PN553）驱动来源与移植

日期：2026-10-18 ｜ R33 实机验证：`/dev/pn553` 出现、`android.hardware.nfc@1.2-service` 运行

## 怎么定位到源头（不是逐函数逆向）

原厂 `stock-Image` 里带调试路径字符串，直接给出了驱动位置：

```
/home/buildfarm/jenkins/workspace/buildfarml_nj_04/kernel/msm-4.19/coolpad/drivers/nfc/nxp/pn8xT/pn553-i2c/pn553.c
[NFC-P61] : ...
Error p61_dev_read ...
```

按这个路径去搜，**NXP 官方仓库**用的是同一个目录结构：

```
https://github.com/NXPNFCProject/NXPNFC_I2CDriver  →  pn8xT/pn553-i2c/pn553.c
```

⇒ 同一份源码，无需逆向。原厂只是把它放在 `drivers/nfc/nxp/` 下。

## 移植步骤

1. 取 `pn8xT/pn553-i2c/{pn553.c,pn553.h,Kconfig,Makefile}`（1575 + 164 行，自带 P61 逻辑，166 处引用，无外部依赖）。
2. 放到 `drivers/nfc/pn8xT/pn553-i2c/`，在 `drivers/nfc/Kconfig` 加 `source`、`drivers/nfc/Makefile` 加 `obj-$(CONFIG_NFC_PN553_DEVICES)`。
3. 配置 `CONFIG_NFC_PN553_DEVICES=y`（驱动自带 Kconfig，`default y`）。
4. 修两处内核 API 漂移（这份驱动是 4.9 时代的）：

| 报错 | 修法 |
|---|---|
| `implicit declaration of function 'force_sig_info'` | 加 `#include <linux/sched/signal.h>` |
| `implicit declaration of 'init_timer' / 'setup_timer'` | 4.15+ 已移除；改为 `timer_setup(&secure_timer, (void (*)(struct timer_list *))secure_timer_callback, 0);`，删掉 `init_timer` |

## 绑定方式

- 芯片已在 i2c-0 地址 0x28 枚举（`/sys/bus/i2c/devices/0-0028/name = pn544`）。
- 驱动的 `of_device_id` 含 `nxp,pn544` / `nxp,pn548`，因此能直接绑上现成的 i2c 设备。
- 绑上后创建字符设备 `/dev/pn553`（`nfc:nfc`，`crw-rw----`），上层的 `android.hardware.nfc@1.2-service` 从此可用。

## 验证

```
ls -l /dev/pn553                      → crw-rw---- 1 nfc nfc 10, 59 ✓
ps -A | grep nfc                      → android.hardware.nfc@1.2-service ✓
```

功能验收（开关 NFC、贴卡）仍需现场确认。

## 备注

- `nqnfcinfo` 服务启动失败（`/system/vendor/bin/nqnfcinfo` 不存在）与本驱动无关，是诊断工具缺失，可忽略。
- 未做：PN553 的固件下载路径（`/vendor/firmware/`）与 eSE 相关功能未逐项验证。
