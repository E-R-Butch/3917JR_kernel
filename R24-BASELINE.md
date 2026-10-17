# R24 基线（2026-10-17）

第一个**显示正常 + WiFi 正常 + 容器能力完整**的自编内核。

```
版本串  4.19.81-perf+ (sinclair@zenith) #24
镜像    3917JR-gaea-R24-BASELINE-RAMBOOT_ONLY_NEVER_FLASH.img  32 MB
         sha256 1b3f47ea86221412…
提交    mainline @ 22ad3774e
配置    /root/archive/configs/3917JR-gaea-R24.config
```

## 三个根因（都是之前反复踩的）

### 1. 屏幕熄屏后再也点不亮（PANEL_DEAD）

现象：锁屏后按电源键不亮；`mWakefulness=Awake` 但物理屏黑；dmesg 刷
`dsi_ctrl_dma_cmd_wait_for_done: Command transfer failed`、`TE check failed`、
`esd check failed report PANEL_DEAD`。

根因：HBM 补丁把 `yl_display_hbm.c` 从
`dsi_panel_disp_param_set(DISPPARAM_HBM_FOD_ON)` 改成了
**裸发 DSI 命令** `dsi_panel_tx_cmd_set(DSI_CMD_SET_HBM)`。指纹 HAL 持续发
FOD 请求，裸命令打乱 DSI 命令队列，之后面板 `DSI_CMD_SET_ON` 超时，
TE 丢失 → 面板被判死。

修复：回退该补丁（提交 `09671010d`），驱动回到 `disp_param_set` 路径。
HBM 功能因此不生效（被小米 feature 门控挡掉），但不再破坏显示。

### 2. 频闪 + 过度锐化

同源：DSI 命令队列被打乱后 iris3（Pixelworks）反复重新初始化，
表现为高频亮闪和字体过度锐化。修复同上。

### 3. WiFi 完全起不来（wlan0 不存在）

两条独立原因，必须都解决：

**a) 版本串**：原厂 `qca_cld3_wlan.ko` 的 vermagic 是
`4.19.81-perf+ SMP preempt mod_unload modversions aarch64`，
任何自定义版本串都会让 `insmod` 报 `Exec format error`。
→ `CONFIG_LOCALVERSION="-perf+"` 必须与原厂逐字一致。

**b) 缺符号**：即使版本串对上了，模块仍报
`wlan: Unknown symbol yl_params_kernel_read (err -2)` ——
这是 **Coolpad 自己的参数区驱动**，本树（小米基线）完全没有。
→ 新增 `drivers/misc/yl_params.c`，导出 `yl_params_kernel_read()`
（返回 0、不写调用者缓冲区，让 WiFi 走默认值）。

moodversions CRC 也不同，故 `kernel/module.c` 跳过三处校验：
`check_version` / `check_modstruct_version` / `same_magic`。

## 配置要点

以**上一版能正常显示的 fix 配置**为底（不是原厂 `/proc/config.gz`）：

| 配置 | 值 | 原因 |
|---|---|---|
| `CONFIG_LOCALVERSION` | `"-perf+"` | 原厂模块 vermagic 必须匹配 |
| `CONFIG_MODVERSIONS` | `y` | 与原厂一致 |
| `CONFIG_MODULE_SIG_FORCE` | `n` | 我们没有原厂签名私钥 |
| `CONFIG_SYSTEM_TRUSTED_KEYS` | `""` | 同上 |
| `CONFIG_ANDROID_PARANOID_NETWORK` | `n` | 开了会按 inet 组限制 socket，网络整体失常 |
| `CONFIG_SYSVIPC` / `PID_NS` / `UTS_NS` / `NET_NS` / `IPC_NS` | `y` | DroidSpaces 容器依赖 |
| `CONFIG_CFS_BANDWIDTH` / `CGROUP_PIDS` / `CGROUP_DEVICE` | `y` | 同上 |
| `CONFIG_VETH` / `NF_TABLES` / `DEVTMPFS` | `y` | 容器网络与 /dev |
| `CONFIG_PXLW_IRIS3_RECOVERY_ABYP` | `y` | 与原厂一致（关掉会反复重初始化 iris3） |

## 重建

```bash
bash /root/build-r18b.sh      # 或按上面配置手工 make
```

产物组装：用原厂 `boot_a.img` 解包出的 ramdisk/dtb/cmdline，
只替换内核段后 `mkbootimg --header_version 2` 重打包（见 repack.sh）。

## 实测验收

| 项 | 结果 |
|---|---|
| 锁屏/唤醒 8 轮 | 正常 ✓ |
| DSI 传输错误 | 0 ✓ |
| PANEL_DEAD | 0 ✓ |
| 频闪 / 过度锐化 | 无 ✓ |
| WiFi | `ping -I wlan0 192.168.31.1` 3/3、0% 丢包、2.9 ms ✓ |
| 已加载模块 | 32（原厂 31 + wlan）✓ |
| 指纹服务 | running ✓ |
| 频率 | P7 满频 2.4 GHz ✓ |

## 未完成

- **HBM 功能**：当前是空实现（被 MI feature 门控挡住），
  要恢复需在 `dsi_panel` 里正确注册 `qcom,mdss-dsi-hbm-on/off`
  并用 `disp_param_set` 而非裸 DSI 命令。
- **模块签名**：`MODULE_SIG_FORCE=n` 是临时方案；
  原厂 `certs/3917jr-existing-modules.pem` 可从已签名 `.ko` 的
  PKCS#7 尾段提取后恢复强制校验。
- LTS 升版（4.19.157+）、GPU 超频、root 隐藏（白皮书 M2–M5）。
