# 为了修好 DroidSpaces：挽回 3917JR 酷派/Yulong 开源内核的 R1–R16 记录

本文记录乐天 Rakuten BIG s 3917JR（代号 `gaea`）为了修复 DroidSpaces
所需宿主能力，从原厂 Android 10 内核审计，到挽回酷派/Yulong 公开源码、
补齐量产私有依赖并形成 R16 RAM-boot 测试基线的过程。它既是工程复盘，
也是视频叙事的事实底稿。

> **安全边界：**所有自定义候选均只通过 `fastboot boot` 临时载入内存。
> 没有把实验镜像写入 `boot_a`、`boot_b` 或其他手机分区。本文出现的
> R1–R16 是实验迭代编号，不等同于公开稳定版本。

## 1. 最初目标：让 DroidSpaces 真正可用

原厂 `4.19.81-perf+` 内核能正常运行 Android，但 DroidSpaces 的实机检查
显示缺少 PID、UTS、IPC namespace，以及 `SYSVIPC`、`POSIX_MQUEUE`、
`DEVTMPFS`、`VETH`、`CGROUP_DEVICE` 等编译期能力。Magisk 或普通 root
不能在运行时补出这些内核功能，因此修复 DroidSpaces 必须从宿主内核下手。

这里的“修好”不是只让检查页变绿，而是让 DroidSpaces 能实际创建隔离空间、
发出 TUN/TAP ioctl、建立 veth/bridge、启动完整 Linux 用户空间，并在不牺牲
Android 原有硬件的前提下承载真实容器负载。

因此真正目标不是“让内核编译成功”，而是在保留 Android 10 闭源 vendor 栈
和 DroidSpaces 主任务的前提下，同时满足：

- `/data` 文件级加密可以解锁；
- 显示、触摸、Wi-Fi、蜂窝、音频和相机仍可使用；
- 37 个外置 Qualcomm 模块与新内核 ABI、签名和加载顺序匹配；
- DroidSpaces 所需 namespace、TUN/TAP、VETH、bridge、OverlayFS 可用；
- 失败后能靠普通强制重启回到分区内的原厂内核。

## 2. 为什么“已经开源”仍然不能驱动量产机

最早的公开源码重建版能够完成编译和 boot 封装，也能被 bootloader 接受，
但在内核接管后进入 Qualcomm `QUSB_BULK 05c6:900e`。UEFI 日志证明 AVB、
DTBO 和 `ExitBootServices` 已走完，因此问题不在“fastboot 没发送成功”，
而在内核启动路径。

量产机的活动设备树包含 Pixelworks Iris3、FocalTech 触摸、Goodix、
OEMINFO、Yulong 保留内存和大量 Qualcomm camera/audio 路径。早期公共树中
部分选项只有 Kconfig 名称，没有量产机实际使用的实现。由此得到第一条核心
结论：

> 编译完成、boot header 正确、DTB 不变、模块 CRC 看起来正常，都不能单独
> 证明手机硬件兼容。

这里缺的不是单一“私有库”，而是三层量产配套：

1. 公开树中没有完整实现的 OEM 内建驱动与硬件表；
2. 与特定内核 ABI、签名和加载时序绑定的外置 Qualcomm/vendor 模块；
3. 闭源 Android HAL 对私有 UAPI、结构体布局和调用顺序的隐含约定。

我们没有把闭源二进制反编译后提交到 GitHub。最终工程由三类证据拼合：

1. Qualcomm SM7250/Lito Android 4.19 树作为主体；
2. 同 IC 或同平台公开源码作为实现参考；
3. 量产机 stock `Image` 的符号、字符串、反汇编和调用顺序作为行为 oracle。

与此同时，原厂 `/vendor/lib/modules` 中的模块不能直接复用。早期审计测得
1,120 个导入 CRC 不匹配、171 个导入符号缺失，签名链也不同。最终采用的是
按候选 `Module.symvers` 重编并签名 37 个模块，`br_netfilter` 则内建。对无法
公开的 CHI 用户态组件，只在本机验证其兼容保护，并在公开内核中恢复它所期待
的接口；stock 镜像、vendor blob、模块二进制和私钥均不进入公共仓库。

## 3. 调试循环

每轮遵循同一闭环：

```text
保存原厂基线
  -> 修改一个已被日志证明的变量
  -> 编译 Image 和受影响模块
  -> 检查 ABI、签名、DTB、ramdisk、AVB/boot 结构
  -> fastboot boot 临时启动
  -> Android 上线则抓 dmesg/logcat/功能结果
  -> 进入 900e 则先保存 crashdump，再强制重启
  -> 回到原厂内核，修下一处明确故障
```

曾经因为第一次 crash 后先重启、没有及时完成 Sahara dump，丢失了易失现场。
之后补上了只读 Qualcomm Sahara/ramdump 采集流程。完整 dump 约 6.45 GB，
必要时也使用按物理地址读取 printk 缓冲区的定点采集，避免每次都复制全部 DDR。

## 4. R1–R16 迭代时间线

表中的“通过”只表示当轮明确验证过的项目，不代表该版本整体合格。

| 轮次 | 主要现象或假设 | 修复/诊断动作 | 当轮结果 |
| --- | --- | --- | --- |
| 公共源码 v3 | bootloader 接受镜像后进入 900e，Android/ADB 均不上线 | 对照活动 DT、stock Image 和 OEM 路径 | 证明公共树不是量产等价源码；该镜像隔离，不再测试 |
| R1 | OEM 驱动补入后仍在启动早期 panic | 首次完整 Sahara dump 定位到 camera SMMU：DT `<base,length>` 被误当 `<start,end>`，失败清理又销毁空 pool | 得到真实栈 `gen_pool_destroy -> cam_smmu_release_cb -> cam_smmu_probe` |
| R2 | 修复后首次完整进入 Android；Wi-Fi、相机、TUN 不正常，屏幕肉眼过锐 | 修正 camera SMMU 区间和空指针清理 | `sys.boot_completed=1`，证明内核主体可启动；暴露后续真实硬件问题 |
| R3 | CSIPHY 版本缺失、TUN 路径缺失、Iris 仍处于错误图像路径；WLAN 加载被 SELinux 拒绝 | 增加 `csiphy-v1.2.2.2` 支持、`/dev/net/tun` 兼容路径、Iris 硬件 ABYP；验证 WLAN 模块本体 | TUN 与 ABYP 生效；WLAN 需从 Magisk 域加载；一次 debugfs GPIO 遍历触发了诊断副作用 crash |
| R4 | Wi-Fi/TUN/去锐化通过，但所有相机仍 NACK | 恢复相机电源序列 ABI：`RESET=8`、`STANDBY=9`、两路 GPIO 为 10/11；固化 WLAN 加载域 | Wi-Fi 真联网、TUN 全绿、过度锐化消失；传感器仍未真正上电 |
| R5 | VIO 开启，但 VANA/VDIG/VAF 因 `INVALID_VREG` 分支被跳过 | 按 stock 反汇编行为让三路实际由 GPIO 供电 | 五个 sensor 槽位 probe 成功，CameraService 枚举恢复；实际预览仍黑，触摸异常暴露 |
| R6 | 29 个音频模块没有随 ramdisk overlay 真正覆盖；原厂路径模块又被候选 keyring 拒绝 | 在运行中从 `/sbin` 按依赖顺序加载重签模块验证 | 29/29 模块可加载，证明是 overlay/路径问题而不是模块内容问题 |
| R7 | 音频与 WLAN 预加载稳定；相机有 sensor、没有主像素链路；触摸 IRQ 仍异常 | 固化 `/sbin` 音频→ADSP→WLAN 顺序；识别缺失的 CPAS/CSID/VFE compatible | 音频/WLAN 早期加载通过；进一步锁定相机硬件匹配缺口 |
| R8 | 主 CSID/VFE 终于绑定，相机设备可枚举但预览黑；触摸写入原厂 `0xCF=0x02` 后仍无事件 | 移植 CPAS170_V200、CSID170_200、VFE170_150；补 FTS 运行态寄存器 | Camera ISP probe 完成；相机故障推进到 HAL/stream-on；触摸问题仍未证明根因 |
| R9 | ADSP 因模块加载太晚错过原厂启动触发；触摸定时诊断窗口容易错过 | 模块就绪后自动写 `boot_adsp=1`；加入限时只读触摸轮询 | ADSP 自动 ONLINE、FastRPC 错误消失；触摸的全 `FF` 静止帧后来证明不足以定性 |
| R10 | 相机逻辑多摄路径使 provider 空指针退出；声卡没有注册；触摸需要可手动触发的证据 | 增加 root-only 手动触摸诊断；逆向 CHI 崩溃和音频 DAI 拓扑 | 相机故障锁定为逻辑相机 mutex 生命周期与后续 LinkControl ABI；发现重编 machine 模块引入了错误 WSA links |
| R11 | `StreamOn` 前 link-control ioctl 返回 `EINVAL`；错误 audio machine 拓扑拖垮 audioserver/framework | 将 `LinkControl` 恢复为 stock 的 32 字节布局、句柄偏移 `+24`；按原厂 DAI 表重编并签名 machine 模块；对闭源 CHI 做 RAM 内 null-guard | provider 不再在原崩溃点退出，stream-on 越过第一关；37 模块 CRC 闭包 2,799/2,799；声卡仍受 PDR 首次状态竞态阻塞 |
| R12 | APR/PDR 的 UP 事件发生在 listener 注册前，q6core 子设备与声卡没有创建 | 在 `audio_q6_pdr.ko` 中缓存并向晚注册 listener replay `SERVICE_UP`；保留经哈希校验的 CHI bind mount | 冷启动自动出现 card0、AudioFlinger/Policy 稳定；前后摄预览和拍照、Wi-Fi、LTE、TUN、DroidSpaces 全绿 |
| R13 | 为区分亮屏闪烁与强制 ABYP，制作仅撤销 ABYP handoff 的离线诊断构建 | 验证 ABI 不变，并继续按 stock 二进制恢复 PT 路径 | 主要作为离线过渡证据，没有作为最终实机结论；变更继续进入 R14 |
| R14 | 恢复 stock PT/continuous-splash 后启动即进入 900e | 定点读取本次 printk buffer | 精确 panic：`iris_send_none_lut_table_pkt -> __memcpy(NULL)`；证明 PT 命令缓冲未准备 |
| R15 | 补回原厂 `dsi_display_prepare -> iris3_display_prepare` 后能进 Android | 让固件解析和 Iris command buffer 先完成；在线测试 `IRIS_PEAKING=0` | R14 空指针消失，但首亮出现 TE/commit timeout 和 DSI FIFO underflow，过锐化随 PT/PQ 回归；随后一次启动失败实际进入 Recovery，不应描述成“Android 解密密码失败” |
| R16 | 需要恢复稳定显示，同时不能破坏 37 个模块 ABI | 回到保守 ABYP，撤销 R15 PT prepare；删除会改变 `task_struct`/CRC 的侵入式 hung-task 配置；保留可观测性 | Android 正常进入、`/data` 解锁、显示/触摸/Wi-Fi/音频/前后摄/TUN/DroidSpaces 通过；形成黄金 RAM-boot 基线 |

## 5. 几条最重要的修复链

### 5.1 启动 Crashdump：camera SMMU

R1 的完整内存转储证明，最初启动崩溃不是 Iris 或触摸猜测，而是 PID 1
在 camera SMMU probe 失败清理时 panic。修复 `<base,length>` 解析和空 pool
销毁后，R2 首次进入 Android。这个案例最能说明“每次从真实 crash 栈修一个
bug”的方法。

### 5.2 相机：从“识别到传感器”到“真正出画面”

相机不是一个补丁，而是多层连续故障：

1. SMMU probe panic；
2. CSIPHY 版本不识别；
3. 电源序列枚举被私有项错位；
4. VANA/VDIG/VAF 的 GPIO 供电被跳过；
5. CPAS、CSID、VFE 硬件表和 compatible 缺失；
6. 用户态 CHI 逻辑多摄退出路径空指针；
7. 内核 LinkControl UAPI 与 stock HAL 的结构尺寸/偏移不一致；
8. 音频/ADSP 不稳定又会连带拖垮 camera framework。

因此“CameraService 枚举到 7 个设备”从来没有被当作完成。最终验收要求
后摄和前摄都出现真实预览，并分别保存 JPEG。

需要诚实披露：R16 相机验收还依赖一项不在本 Git 仓库中的、经哈希校验的
闭源 CHI 用户态兼容补丁/bind mount。公开仓库不能分发该 vendor blob。

### 5.3 音频：不是 29 个模块加载了就结束

音频经历了模块路径、签名、加载顺序、machine DAI 拓扑和 PDR 状态 replay
五层问题。R12 的关键修复不是粗暴强制 ADSP 状态，而是让晚注册 listener
收到已经发生过的 `SERVICE_UP`，从而由原始 APR 路径完成 q6core populate、
声卡注册和 audioserver 启动。

### 5.4 显示：ABYP、PT、过锐化和闪烁是两条问题线

系统截图取在 Pixelworks 后处理之前，因此“截图正常、肉眼过锐”将问题定位到
Iris/DSI 后段。强制 ABYP 能消除 PQ 过锐，但恢复 stock PT 又暴露命令缓冲
初始化缺口和 TE/DSI 时序错误。R16 最终选择保守 ABYP，不追求 Iris 高级
图像处理，以换取稳定显示。

一次亮度 `129 -> 255 -> 129` 确实来自人脸解锁屏幕补光，但关闭补光后用户
仍看到闪烁，因此它只是叠加现象，不是最终根因。视频不能把它说成唯一修复。

### 5.5 触摸：避免把不充分的诊断当根因

FTS `0xCF=0x02` 是 stock 中存在、公开树遗漏的运行态写入，但 R8 写入成功后
仍有零 IRQ。R9 的全 `FF` 报告窗后来又在原厂静止态复现，因此不能单独证明
控制器故障。最终 R16 的实体触摸由用户确认正常，但目前没有证据把成功严格
归因到某一条单独触摸补丁；更可能与完整显示/电源/系统稳定链共同有关。

## 6. R16 最终验证

R16 标识为 `4.19.81-perf+ #21`。黄金 `Image` SHA-256：

```text
1162c3bf197b5395d4488923336cd28b0ed49a6874d2ffd8cf6d70d263d9f58a
```

黄金 RAM-boot SHA-256：

```text
7a1e1b93e842824f7df940adaf785fa0ded229f4aa8cee2a22916acd86cae07a
```

已记录的实机结果：

- `/data` 以 F2FS 读写挂载，用户状态 `RUNNING_UNLOCKED`；
- 37 个替换模块的 2,799 个实际 ABI 依赖为 0 mismatch、0 missing；
- Wi-Fi 联网，蜂窝注册和数据路径存在；
- 29 个音频模块、ADSP、card0、AudioFlinger/AudioPolicy 正常；
- 前后摄真实预览和拍照通过；
- 用户确认触摸、首亮闪烁和通知栏过锐化均不存在；
- DroidSpaces 的 namespace、TUN/TAP、VETH、bridge、OverlayFS 等检查及
  实际接口操作通过；完整 Ubuntu 用户空间能以 systemd 为 PID 1 运行；
- 约 15 小时后执行 Docker/Alpine/Nginx/256 MiB 写入测试，新增
  panic/Oops、显示严重错误和网络 watchdog 计数均为 0。

Docker 是 DroidSpaces 修复后的真实负载验收，仍有明确边界：

- `iptables-nft` 在 NAT `PREROUTING` 建链失败，需要 `iptables-legacy`；
- Android F2FS 不能作为当前 4.19 OverlayFS upperdir，测试使用 VFS；
- CPU、内存和 I/O cgroup 限额尚未形成可用闭环。

## 7. 当前工程状态

R16 源码快照和黄金配置已进入本仓库，但这仍不是“clone 后即可产出同一个
boot”的可复现发布：精确 builder、最终 v10 packager、外置模块 build glue
和签名链仍需重建。R16 继续作为只读黄金回归基线；后续 4.19 stable/CIP
升级必须逐阶段重新编译 37 个模块并完成同一套 RAM-boot 门禁。

更多事实身份见 [`PROVENANCE.md`](PROVENANCE.md)，测试门槛见
[`TEST_MATRIX.md`](TEST_MATRIX.md)，视频制作资料见 [`video/README.md`](video/README.md)。
