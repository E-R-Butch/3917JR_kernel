# 《为了修好 DroidSpaces，我们救回了酷派开源内核》旁白主稿

建议时长：10–12 分钟
叙述视角：第一人称
主题：修复 DroidSpaces 与 3917JR 原厂内核的能力断层，并为此挽回
不能复现量产机的酷派/Yulong 开源内核、补齐量产私有依赖

## 标题备选

1. 为了修好 DroidSpaces，我们救回了酷派的开源内核
2. DroidSpaces 只差一个 TUN？背后却是 16 轮内核适配
3. 从 900e 到 R16：给 3917JR 补齐量产私有依赖

## 封面短句

- 为了修好 DroidSpaces
- 救回酷派开源内核
- CRASHDUMP → R16

## 00:00 开场：先交代结果

这是一台乐天 Rakuten BIG s，型号 3917JR，代号 gaea。它背后的设备源码和
Yulong、酷派体系密切相关，出厂是 Android 10、Linux 4.19.81。

故事的起点不是为了换内核，也不是为了跑 Docker，而是 DroidSpaces。在这台
手机上，它的检查器缺 PID、UTS、IPC namespace、VETH、TUN 和多项容器能力；
root 已经有了，但编译时没进内核的功能，Magisk 也变不出来。

为了修好 DroidSpaces，我们才回头找 3917JR 的内核。酷派/Yulong 确实留下了
一份公开源码，但“仓库存在”不等于“量产源码完整”。它能编译，却缺少量产版
真正依赖的 OEM 实现、外置模块配套和闭源 HAL 接口细节。于是一个看似只差
几个 namespace 的需求，变成了把整份量产兼容链救回来的 16 轮工程。

最后的 R16 能正常解锁 `/data`，显示、触摸、Wi-Fi、音频和前后摄都能工作；
DroidSpaces 原先缺失的 namespace、TUN/TAP、VETH、bridge、OverlayFS 等
能力完成实机验证。Docker 只是随后用来证明这些能力不是检查器上的假绿灯。

最后才用完整 Ubuntu 24.04 和 Docker 29 做真实负载验收。大约 15 小时后，
新增的 panic、Oops、显示严重错误和网络 watchdog 仍然是零。

但这个结局不是“打开几个内核选项”换来的。中间经历了启动即 900e、相机
供电错位、Wi-Fi 模块被 SELinux 拦截、29 个音频模块加载时序、相机 HAL
结构体 ABI、Iris 显示空指针、触摸零 IRQ，以及一版直接进 Recovery。

而且从头到尾，我们都没有把实验内核刷进启动分区。每一版只用
`fastboot boot` 放进 RAM，失败后强制重启，就回到分区里的原厂内核。

## 00:55 为什么修 DroidSpaces 必须改内核

原厂系统有 root，但 DroidSpaces 的实机检查仍然失败。它要把 Android 上的
完整 Linux 用户空间隔离起来，依赖的不是一个 APK 权限，而是宿主内核提供的
进程、挂载、UTS、IPC、网络 namespace，以及 TUN/TAP、VETH、bridge、
SYSVIPC、POSIX_MQUEUE、DEVTMPFS 和部分 cgroup 能力。

这些不是 Magisk 模块能在运行时补出来的。Root 只是权限；DroidSpaces 需要
的 syscall、namespace 和虚拟网络设备，必须由正在运行的内核真正实现。

所以我们修的不是 DroidSpaces 界面，而是它脚下那层原厂内核能力断层。目标
也不只是让 requirements checker 变绿：创建 namespace、实际发 TUN ioctl、
拉起 veth/bridge，再让完整 Linux 用户空间和容器负载跑起来，才算修好。

真正困难的是，公开的 3917JR 内核并不等于量产源码。它缺的也不是一个文件，
而是三层东西：内核里的 OEM 驱动实现，和内核严格绑定的 vendor 模块，
以及闭源 Android HAL 对私有 UAPI、结构体和调用时序的假设。

## 01:35 第一次失败：能编译，不等于能启动

最早的公开源码重建版可以完整编译，boot header、DTB、ramdisk 也都通过
检查。Fastboot 显示 Sending OKAY、Booting OKAY，随后手机却没有进入
Android，而是枚举成 Qualcomm QUSB_BULK 05c6:900e。

UEFI 持久日志证明 AVB、DTBO 和 ExitBootServices 都已经完成。换句话说，
bootloader 确实把控制权交给了内核，内核接手后才死。

一开始还没有可靠 pstore，第一次硬重启后，易失的 panic 现场就没了。后来
我们补上 Sahara 只读采集流程，完整抓过 46 个区域、总计 6.45 GB 的内存，
也做了只读取 1 MB printk 缓冲的定点工具。

这次失败还暴露了源码本身的问题。活动设备树里有 Pixelworks Iris3、
FocalTech 触摸、Goodix、Yulong 保留内存等 OEM 路径；早期树里有些只有
Kconfig 名字，没有真正实现。

所以第一条经验是：make 成功、ABI 检查通过、bootloader 接受镜像，都不能
证明手机硬件兼容。

## 02:40 我们怎样救回这份开源内核

我们没有把某个闭源库“反编译后上传”，而是用三条证据线恢复量产行为。

第一条是 Qualcomm SM7250/Lito 的 Android 4.19 源码，提供主体框架。

第二条是同 IC、同平台的公开实现。Iris3 参考同代 Pixelworks 方案，触摸使用
公开的 3917JR FocalTech 驱动，相机硬件表则对照 Google 同平台 redbull
代码。

第三条是量产机的 stock Image 和闭源 vendor 栈。二进制不能自动变回源码，
但 kallsyms、字符串、反汇编、结构体偏移和调用顺序可以回答一个关键问题：
量产版到底怎样做。我们据此在可公开的内核源码里重建兼容行为，而不是发布
原厂二进制。

原厂 vendor 里的 38 个模块也不能直接塞进新内核。早期审计出现 1120 个
导入 CRC 不匹配、171 个符号缺失，模块签名也不受新内核信任。最后的方案是
按候选内核重新编译和签名 37 个模块，把 br_netfilter 直接编进内核，并检查
每一个实际依赖的 CRC、vermagic 和签名。

所以这里说的“补齐私有库”，准确地讲包含三件事：把能找到 GPL/公开来源的
内核驱动移植回来；把外置内核模块按新 ABI 重建；对无法公开的 CHI 等用户态
组件，只恢复它们所期待的接口兼容，并把二进制留在本机验证链里。

## 03:35 R1：第一次真正有用的 Crashdump

R1 仍然启动崩溃，但这次我们没有先重启，而是现场抓完完整 dump。

真正的 panic 不在 Iris，也不在触摸。PID 1 在 camera SMMU probe 失败清理时
调用了空 pool：设备树中的 `<base,length>` 被源码当成 `<start,end>`，随后
`gen_pool_destroy` 又没有判空，最后出现 `Attempted to kill init`。

修正区间计算和空指针清理以后，R2 第一次完整进入 Android。

这也是整个项目最重要的节奏：不是先做十套安全审计，也不是一次猜十个驱动，
而是每次 RAM boot，从日志或 dump 修掉一个确定 bug，再测试下一版。

## 04:25 进入系统，才发现真正的兼容地狱

R2 虽然开机了，但 Wi-Fi 消失、相机不可用、TUN 失败，实体屏幕文字还明显
过度锐化。手机信号倒是仍在，这说明基带没有一起挂掉。

R3 补了相机 CSIPHY 版本、TUN 设备路径和 Iris 硬件旁路。系统截图看起来
正常，肉眼却过锐，这反而证明问题在 screenshot 之后的 Iris/DSI 后处理链。

Wi-Fi 模块本体和签名都没问题，真正失败在 SELinux：init 域没有加载模块的
权限。用 Magisk 域加载同一文件后，wlan0、p2p0 立刻出现。R4 把这个已验证
时序固化后，Wi-Fi 真正完成四次握手和 DHCP，TUN 与去锐化也都通过。

但相机仍然全 NACK。

## 05:25 相机：枚举成功只是第一层

相机是这次最深的一条故障链。

先是电源序列枚举中混入了小米私有项，把 RESET 和 STANDBY 的编号全部错位。
修正编号后，相机还是没有反应。继续对照原厂内核才发现，VANA、VDIG、VAF
在这块板上没有 regulator，应该走 GPIO 供电；公开源码遇到 INVALID_VREG
却直接 break，把三路电源全跳过了。

R5 补上 GPIO 供电后，五个实际 sensor 槽位全部 probe success，CameraService
也恢复到原厂相同的 7 个设备。

但“枚举到 7 个相机”不等于有预览。实际打开仍然黑屏，因为主 CPAS、CSID、
VFE 的 hardware table 和 compatible 还没匹配。R8 补完这些以后，ISP probe
终于完成，失败点又向上推进到了 Qualcomm camera HAL 的 stream-on。

接着又遇到两层 ABI：闭源 CHI 的逻辑多摄退出路径会解锁空 mutex；而原厂 HAL
发送的 LinkControl 是 32 字节、handle 在偏移 +24，公开内核却按 24 字节、
偏移 +16 读取，必然返回 EINVAL。

我们给 CHI 做了经哈希校验、重启可恢复的兼容保护，同时把内核 UAPI 恢复成
stock 布局。到这一步，相机请求终于真正跑过 configure 和 StreamOn。

这里必须说清：最终相机验收不是百分之百纯开源方案，它仍依赖不在公开仓库
里的闭源 CHI 用户态兼容补丁。

## 06:55 音频为什么会拖垮相机和触摸

中间有一版，触摸失效、相机 provider 崩，audioserver 每 21 秒超时，最后连
system_server 都被 watchdog 拉起。

最初 29 个音频模块根本没有从 ramdisk overlay 正确覆盖。把同一批重签模块
从 `/sbin` 按依赖顺序加载，29 个全部成功，证明问题在路径和时序。

随后又发现重编的 machine 模块带进了板上不存在的 WSA macro 链路，声卡无法
注册。我们按原厂 DAI 表重建 topology，37 个模块的 2799 个实际 CRC 依赖全部
匹配。

最后一层是 PDR 竞态：ADSP 的 SERVICE_UP 发生得太早，listener 注册得太晚，
所以 q6core 子设备和 card0 从未创建。R12 在晚注册时 replay 已经发生的 UP
状态，让原 APR 路径自己完成 populate。

冷启动后，日志出现 `Q6 is Up`，card0、116 个 `/dev/snd` 节点、audioserver
都自动出现。音频稳定以后，前后相机预览和拍照也真正通过。

## 08:05 显示与触摸：别把肉眼现象简单归因

R12 已经很好用，但亮屏仍会闪。我们曾发现人脸解锁补光会把亮度从 129 拉到
255，再降回 129。关闭补光后，这个亮度尖峰消失了，但用户仍能看到闪烁，
所以它只是叠加现象，不是最终根因。

R14 尝试恢复原厂 Iris PT 连续启动路径，手机却直接进入 900e。定点读取
printk 后，调用栈落在 `iris_send_none_lut_table_pkt` 对空指针 memcpy。

原厂会在 `dsi_display_prepare` 里先调用 `iris3_display_prepare`，而移植树漏了。
R15 补回这一步后，固件和命令缓冲都成功创建，空指针消失；但首亮又出现
TE/commit timeout 和 DSI FIFO underflow，PT 的 PQ 锐化也一起回来了。后来
这版还出现启动失败进入 Recovery。

最终 R16 不再追求 Iris 高级图像处理，而是回到保守 analog bypass。它还撤掉
会改变 task_struct、连锁影响 7463 个导出 CRC 的 hung-task 调试选项，只保留
不会破坏模块 ABI 的日志、符号和 watchdog 能力。

触摸也有一个重要教训。原厂确实会写 FTS 的 0xCF=0x02，但单独补这条以后
仍出现零 IRQ；而全 FF 触摸报告在原厂静止态同样能看到。所以最终只能诚实地
说 R16 实体触摸验证正常，不能把功劳硬归到一条寄存器写入。

## 09:35 R16：为什么它才是黄金基线

R16 的内核标识是 `4.19.81-perf+ #21`。它正常进入 Android，不是 Recovery；
`/data` 以 F2FS 读写挂载，用户状态是 RUNNING_UNLOCKED。

37 个模块的 2799 个实际 ABI 依赖为 0 mismatch、0 missing。Wi-Fi 联网，
29 个音频模块和 ADSP 正常，前后摄真实预览和拍照通过。用户在实体屏幕上确认
触摸、首亮闪烁和通知栏过锐化都不存在。DroidSpaces 原来最后失败的 TUN/TAP
也通过实际 ioctl 创建、拉起和删除接口。

这时我们才把它称为阶段性黄金测试基线，而不是在“桌面出现”那一刻宣布成功。

## 10:15 最后才用 Docker 做真实负载

Ubuntu 24.04 在 DroidSpaces 中以 systemd 为 PID 1。Docker 第一次启动失败，
不是容器能力全坏，而是默认 iptables-nft 在这套 Android 4.19 上无法创建 NAT
PREROUTING 链。切换 iptables-legacy 后，daemon 正常启动。

第一次 docker run 又失败：内核有 OverlayFS，但 Android 的 F2FS `/data`
不能作为这套 4.19 OverlayFS 的 upperdir。改用 VFS 后，Alpine 启动，HTTPS
返回 200；Nginx 映射 18080 到 80，另一容器通过网桥访问得到 HTTP 200；
256 MiB 写入、sync 和 SHA-256 校验完成。

这不是说限制不存在。VFS 更慢、更占空间；iptables 需要 legacy；CPU、内存和
I/O cgroup 限额还没有可用闭环。但它证明了这套内核不是只让检查页变绿，
而是真的能承载 namespace、bridge、runc、containerd 和应用层网络。

## 11:15 结尾

这次适配最大的收获，不是把版本号从原厂改成 R16，也不是屏幕上一个 Docker
logo，而是让 DroidSpaces 从“原厂内核先天缺能力”变成真实可用；为此我们又
把一份无法直接驱动量产硬件的酷派/Yulong 开源内核，重新变成了可启动、
可调试、可继续维护的工程基线。

真正的工作，是在不写手机分区的前提下，把 boot、设备树、缺失的 OEM 内建
驱动、37 个替换模块、模块签名、SELinux、init 时序、闭源 camera HAL 和
真实硬件行为一层层对齐。能公开重建的代码进入工程；不能公开的原厂镜像、
模块、HAL 和签名私钥只作为本地证据，不混进 GitHub。

R16 仍是 RAM-BOOT ONLY 的测试基线，不是随便刷入 boot 分区的稳定版。当前
仓库已经保存最终源码、配置和黄金哈希，但精确 builder、最终 packager、外置
模块构建脚本和签名链还要继续补全。

下一阶段不是从头移植主线，而是在这套能用的 4.19 基线上，分段合并 stable/CIP
安全修复，并补齐 nftables、cgroup 限额和 ext4-backed overlay2。

从 DroidSpaces 的缺项页，到“开源仓库能编译、量产机却跑不起来”，再到
R16 让 Android 全套硬件和 DroidSpaces 一起工作，这份酷派系 4.19 内核终于
被挽回了。Docker 只是最后证明修复后的 DroidSpaces 确实承载了真实 Linux
和容器工作负载。

## 简介区文案

为了修复 DroidSpaces 在 Rakuten BIG s 3917JR（gaea）上缺失的 namespace、
TUN、VETH 和容器能力，我们挽回了酷派/Yulong Android 4.19.81 开源内核。
公开树能编译却无法复现量产机；我们通过同 IC 源码移植、stock 二进制行为
对照、外置模块 ABI 重建和闭源 HAL 接口兼容，补齐量产私有依赖。过程中经历
Qualcomm 900e Crashdump、camera SMMU panic、相机供电/ISP/UAPI/CHI 多层
兼容、37 个 vendor 模块 ABI 与签名重建、音频 PDR 竞态、Pixelworks Iris3
PT/ABYP 显示问题和触摸诊断。最终 R16 通过 Android 硬件回归、DroidSpaces
和 Docker/Alpine/Nginx 实测。所有候选均只用 `fastboot boot` 临时启动，
不是持久刷写发布版。

## 推荐标签

Android、Linux 内核、酷派、Yulong、DroidSpaces、Docker、Rakuten BIG s、3917JR、SM7250、
Qualcomm Crashdump、手机改造、相机驱动、Pixelworks Iris3、极客 DIY
