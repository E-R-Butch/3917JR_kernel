# 论断边界与脱敏规则

视频 Agent 必须优先保证事实正确。下面按可信度和公开风险整理。

## 1. 证据等级

- **A：当前持久证据。** 仓库源码、配置、manifest、哈希或宿主机留存报告仍在。
- **B：当时实机记录。** 会话中有明确命令回读/日志，但原始文件可能曾位于
  `/private/tmp`，现在不一定仍存在。
- **C：用户实体观察。** 例如肉眼锐化、闪烁、触摸手感；截图不能代替。
- **D：工程推断。** 有多条证据支持但没有唯一调用栈，不可说成百分之百证明。

成片中的确定语气应主要用于 A/B；C 要明确“实体观察”；D 要使用“更像”、
“证据指向”或“尚不能唯一归因”。

## 2. 可以明确陈述

| 论断 | 等级 | 推荐说法 |
| --- | --- | --- |
| 原厂缺少 DroidSpaces 所需 namespace/VETH 等 | A/B | “实机 `droidspaces check` 和 `/proc/config.gz` 同时确认” |
| 所有候选都用 `fastboot boot` | A/B | “实验阶段没有写 boot_a/boot_b” |
| R1 camera SMMU panic | B | “完整 Sahara dump 给出确定调用栈” |
| R16 Image/boot 身份 | A | 展示完整 SHA-256 或前后各 8 位 |
| 37 模块 ABI 闭包 | A/B | “2,799 个实际依赖 0 mismatch、0 missing” |
| R16 `/data` 解锁 | B | “F2FS 读写挂载，用户状态 RUNNING_UNLOCKED” |
| 前后摄真实预览与 JPEG | B | “不是只看 CameraService 进程” |
| Docker/Nginx HTTP 200 | B | “iptables-legacy + VFS 条件下通过” |
| R16 不是可复现发布 | A | “源码和配置保存完整，流水线仍需重建” |

## 3. 必须带限定语

| 容易夸大的说法 | 正确边界 |
| --- | --- |
| “R16 修好了全部硬件” | 指纹未纳入最终验收；不要扩展到 NFC、VoLTE、待机功耗等未完整测试项 |
| “TUN 一开始完全没有” | stock 配置/节点/运行路径在不同阶段有差异；应说 DroidSpaces 实测最初失败，最终 ioctl 闭环通过 |
| “F2FS 不支持 OverlayFS” | 只说当前 Android F2FS + 4.19 OverlayFS 组合不能作为 Docker upperdir；不要推广到所有 F2FS/所有内核 |
| “cgroup v2 只有 pids 是内核缺功能” | 还受 Android 10 v1 层级占用和挂载/委派影响；升版本不会自动解决 |
| “触摸由 0xCF 一条命令修好” | 该命令是必要的 stock 差异，但单独写入后仍出现零 IRQ；最终成功不能唯一归因 |
| “人脸补光就是闪烁根因” | 它造成一次亮度尖峰，但关闭后用户仍观察到闪烁，只是叠加因素 |
| “R15 无法解密 data” | 用户后来确认实际是未进入系统而转入 Recovery；不要说 Android 已启动后密码失败 |
| “相机由开源源码完全修复” | 最终还依赖经哈希校验的闭源 CHI 兼容补丁/bind mount，不能随仓库分发 |
| “Crashdump 每次都是内核自然崩溃” | 至少两次由危险 debugfs 读取/诊断工具触发，必须单独标注 |

## 4. 禁止公开的内容

以下内容不能出现在画面、字幕、附件、Git 历史或视频简介下载链接中：

- 设备 PIN、锁屏密码、账号信息；
- ADB/fastboot 设备序列号；
- 家庭 Wi-Fi SSID、密码、MAC、内网拓扑和公网 IP；
- stock/修改版 boot、DTB/DTBO、vendor/system 镜像和闭源 CHI blob；
- `.ko`、`Image`、`vmlinux`、`System.map`、`Module.symvers` 二进制交付件；
- 6.45 GB 内存转储、原始 tombstone、可能包含个人数据的完整 logcat；
- 模块签名或 AVB 私钥、tokens、cookies；
- 手机上的照片内容、通知栏文字、联系人、SIM/eSIM 标识。

可以展示这些对象的文件名、大小、摘要哈希和经过裁切/脱敏的关键日志行。

## 5. 屏幕录制脱敏

重新录制手机时：

1. 开飞行模式或使用专用无个人账号环境；
2. 清空通知，隐藏状态栏运营商/热点名称；
3. 使用临时测试目录，不打开相册历史；
4. `adb devices` 画面把序列号打码；
5. `ip addr`、`dumpsys wifi`、`getprop` 只截需要的字段；
6. 相机示例只拍无个人信息的桌面物体；
7. 日志仅摘取关键 5–15 行，不整屏滚动原始 dump。

## 6. 开源/闭源边界

仓库能公开的是源码、配置、补丁思路、manifest 哈希、脱敏日志和测试方法。
原厂 boot/vendor 数据、闭源相机 blob、签名私钥和编译产物不属于公开源码。
视频可以讲“如何对照和兼容”，不能附带这些文件。
