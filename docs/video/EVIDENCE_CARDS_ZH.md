# 可直接制作的证据卡

这些卡片适合在缺少历史实拍时使用。画面应标注“根据留存配置/日志/manifest
重排”，不要伪装成当时的原始终端截图。

## 卡 1：原始目标不是换内核

```text
目标：修好 DroidSpaces

原厂已有 root
但宿主内核缺少 namespace / TUN / VETH / cgroup 能力
Magisk 无法在运行时补出未编译进内核的功能
```

## 卡 2：原厂配置与 R16

```diff
 CONFIG_NAMESPACES=y
-# CONFIG_SYSVIPC is not set
-# CONFIG_POSIX_MQUEUE is not set
-# CONFIG_UTS_NS is not set
-# CONFIG_PID_NS is not set
-# CONFIG_CGROUP_DEVICE is not set
-# CONFIG_DEVTMPFS is not set
-# CONFIG_VETH is not set
+CONFIG_SYSVIPC=y
+CONFIG_POSIX_MQUEUE=y
+CONFIG_UTS_NS=y
+CONFIG_IPC_NS=y
+CONFIG_PID_NS=y
+CONFIG_CGROUP_DEVICE=y
+CONFIG_DEVTMPFS=y
+CONFIG_TUN=y
+CONFIG_VETH=y
+CONFIG_BRIDGE=y
+CONFIG_OVERLAY_FS=y
```

来源：原厂 IKCONFIG 留存报告与 `configs/gaea-r16-golden.config`。

## 卡 3：能编译，不等于量产等价

```text
Compiler: PASS
Boot header / DTB: PASS
fastboot Sending / Booting: OKAY

Android / ADB: FAIL
USB: Qualcomm QUSB_BULK 05c6:900e

结论：bootloader 已交权，内核接管后崩溃
```

## 卡 4：R1 把猜测变成调用栈

```text
Sahara FULL dump
46 regions / about 6.45 GB

gen_pool_destroy
  -> cam_smmu_release_cb
  -> cam_smmu_probe
  -> Attempted to kill init

DT <base,length> 被误读为 <start,end>
失败清理又销毁空 pool
```

## 卡 5：所谓“补齐私有库”有三层

```text
1. 缺失 OEM 内建实现
   同 IC / 同平台 GPL 源码移植 + stock 行为对照

2. 内核耦合模块
   32 external + 5 core = 37 replacement modules
   2,799 imports / missing 0 / CRC mismatch 0

3. 闭源 Android HAL
   恢复它期待的 UAPI / 结构体 / 时序
   私有二进制只在本机验证，不进入 GitHub
```

## 卡 6：相机不是一个补丁

```text
SMMU
 -> CSIPHY
 -> Power enum
 -> GPIO rails
 -> Sensor probe
 -> CPAS / CSID / VFE
 -> LinkControl UAPI
 -> CHI userspace guard
 -> Audio / ADSP / PDR
 -> Preview + JPEG
```

角标：`相机最终结果包含本机闭源 CHI 兼容保护，不是纯内核修复`。

## 卡 7：R16 是 DroidSpaces 黄金基线

```text
R16 4.19.81-perf+ #21

Android /data: RUNNING_UNLOCKED
Android hardware regression: PASS
37 modules / 2,799 deps: 0 missing, 0 mismatch

DroidSpaces:
namespaces / TUN-TAP / VETH / bridge / OverlayFS: PASS
Ubuntu 24.04 / systemd PID 1: PASS

RAM-BOOT ONLY / NEVER FLASH
```

## 卡 8：Docker 只是验证不是假绿灯

```text
iptables-nft: NAT PREROUTING failed
iptables-legacy: Docker daemon started

Android F2FS as OverlayFS upperdir: failed
Docker VFS: Alpine + Nginx HTTP 200
256 MiB write + sync + SHA-256: PASS

15h+ test window
new panic / Oops / severe display / network watchdog: 0
```

角标：`VFS 较慢；CPU/内存/I/O cgroup 限额未形成可用闭环`。
