# 镜头与素材清单

本表按 10–12 分钟主稿设计。`现有` 表示可以从仓库/持久报告制作；`需补录`
表示当前没有可公开的真实画面；`图示` 表示可以用本目录 Mermaid 重制。

| 时间 | 旁白主题 | 推荐画面 | 状态 |
| --- | --- | --- | --- |
| 00:00–00:20 | 最初目标：修 DroidSpaces | 原厂 requirements 缺项、root 也无能为力、`不是 App 权限` 大字卡 | 历史记录重排+图示 |
| 00:20–00:55 | 为此救回酷派公开内核 | Yulong/Coolpad 源码树、量产机、`能编译 != 能启动`，再快切 R1–R16 | 仓库录屏+图示 |
| 00:55–01:35 | DroidSpaces 依赖宿主能力 | 原厂与 R16 `.config` 对比；namespace→TUN→VETH→bridge 动画 | 历史记录可重排；建议补录 |
| 01:35–02:10 | 编译成功但 900e | fastboot OKAY 后 USB 变为 QUSB_BULK；UEFI 到 EBS | 没有原始视频时用流程图+脱敏日志 |
| 02:10–02:40 | Crashdump 工具 | 46 regions、6.45 GB、只读 MEM_READ 示意 | 图示；不要展示 dump 内容 |
| 02:40–03:35 | 挽回公开树、补私有依赖 | SM7250 base + Iris donor + 3917JR FTS + stock binary oracle；三层依赖卡 | 图示；可录 Git diff/源码 |
| 03:35–04:25 | R1 camera SMMU panic | 调用栈逐行出现，随后 R2 首次 boot_completed | 现有报告可制作文字卡 |
| 04:25–05:25 | R2–R4 Wi-Fi/TUN/Iris | 四格状态变化；截图正常/实体过锐的解释 | 图示；锐化前后无素材则不要伪造 |
| 05:25–06:55 | 相机多层修复 | 电源序列→GPIO rails→sensor→CPAS/CSID/VFE→LinkControl→CHI | 强烈建议动画；使用 `failure-layers.mmd` |
| 06:55–08:05 | 音频/PDR | 29 模块依赖图、`Q6 is Up`、card0/AudioFlinger | 可从记录重排；建议补录终端摘要 |
| 08:05–09:35 | Iris PT/ABYP 与触摸 | R14 空指针栈、R15 PT+PQ、R16 ABYP；触摸证据边界 | 图示+代码浏览 |
| 09:35–10:20 | R16 与 DroidSpaces 黄金验收 | `/data`、37/2799、Android 硬件、namespace、真实 TUN ioctl、VETH/bridge、Ubuntu PID 1 | 需补录或结果卡 |
| 10:20–11:15 | Docker 证明不是假绿灯 | nft 失败→legacy；F2FS upperdir 失败→VFS；Alpine/Nginx 200 | 需补录；可用重排终端卡 |
| 11:15–结尾 | 阶段完成与边界 | RAM-BOOT ONLY、golden hashes、4.19 LTS roadmap | 现有仓库可录屏 |

## 1. 最优先补录的 8 个画面

1. 手机背面/正面与型号页，避免 IMEI、账号；
2. bootloader 中只展示 `fastboot boot`，绝不执行 flash；
3. `uname -a` 与 build `#21`；
4. DroidSpaces capability check 全页；
5. 后摄、前摄各 3 秒真实预览与一次拍照；
6. 实体手指滑动通知栏，体现触摸和无过锐/闪烁；
7. `docker ps`、Alpine HTTPS、Nginx `18080->80` 与 HTTP 200；
8. 仓库 `PROVENANCE.md`、R16 hash 和 R1–R16 时间线滚动。

如果手机当前不是 R16，不要为了视频擅自重新测试。可以先用已有文字证据制作，
等用户另行授权后再按 `TEST_MATRIX.md` 的 RAM-boot 流程补录。

## 2. 建议录制的代码位置

只展示少量相关代码，不要无目的滚动整棵 Linux：

- `techpack/camera/`：camera SMMU、sensor power、CSID/VFE、LinkControl；
- `coolpad/drivers/pixelworks/`：Iris3 ABYP/PT；
- `drivers/input/touchscreen/focaltech_touch/`：FTS OEM runtime 与诊断；
- `configs/gaea-r16-golden.config`：DroidSpaces 与 debug 能力；
- `external/README.md`：外部模块 commit pins；
- `docs/PROVENANCE.md`：黄金身份与可复现边界。

## 3. 推荐动画

### A. 故障修复循环

使用 `diagrams/debug-loop.mmd`，突出“boot→证据→单点修复→再 boot”。

### B. 相机故障层级

使用 `diagrams/failure-layers.mmd`，每修一层，下一层才显现：

```text
SMMU -> CSIPHY -> Power ABI -> GPIO rails -> CPAS/CSID/VFE
     -> LinkControl UAPI -> CHI -> Audio/ADSP -> Preview/JPEG
```

### C. 两层容器栈

使用 `diagrams/runtime-stack.mmd`，明确 Android、DroidSpaces、Ubuntu、Docker
共用同一个 R16 宿主内核，不要画成虚拟机拥有另一个 Linux 内核。

## 4. 终端画面模板

终端裁切只留命令和 3–8 行输出。推荐统一结构：

```text
$ uname -a
Linux localhost 4.19.81-perf+ #21 ... aarch64

$ droidspaces check
All required features found!

$ docker ps
...

$ wget -S -O- http://172.17.0.1:18080/
HTTP/1.1 200 OK
```

上述是展示模板。只有重新实机录制或有对应原始日志时才做“真实终端”样式；
否则标注“根据当时实机记录重排”。

## 5. 画面风格

- 颜色：stock 灰、Crashdump 红、测试候选黄、R16 绿；
- 代码/日志使用 2–3 个高亮，不整页荧光；
- R1–R16 用一条横向时间轴，失败轮次不要全部做成长段；
- 相机和音频用层级动画，显示“修完一层才露出下一层”；
- `RAM-BOOT ONLY` 作为固定角标贯穿所有 boot 测试画面。

## 6. 片尾卡

建议同时展示：

```text
R16 4.19.81-perf+ #21
DroidSpaces: PASS
Docker/Nginx: HTTP 200
15h+ test window: panic/Oops 0

RAM-BOOT ONLY / NEVER FLASH
Not yet a reproducible public release
```
