# 视频 Agent 入口

这套资料用于制作“为了修好 DroidSpaces，挽回酷派/Yulong 公开内核并补齐
量产私有依赖”的视频。
建议先读本文件，再按顺序读取：

1. [`../ADAPTATION_HISTORY_ZH.md`](../ADAPTATION_HISTORY_ZH.md)：完整工程事实与 R1–R16 时间线；
2. [`AGENT_PROMPT_ZH.md`](AGENT_PROMPT_ZH.md)：可直接复制给视频 Agent 的任务；
3. [`VOICEOVER_ZH.md`](VOICEOVER_ZH.md)：可直接改写/配音的 10–12 分钟主稿；
4. [`SHOT_LIST_ZH.md`](SHOT_LIST_ZH.md)：逐段镜头、屏录和图示需求；
5. [`EVIDENCE_CARDS_ZH.md`](EVIDENCE_CARDS_ZH.md)：可直接排版的脱敏结果卡；
6. [`EVIDENCE_INDEX_ZH.md`](EVIDENCE_INDEX_ZH.md)：每条核心论断对应的证据；
7. [`CLAIMS_AND_REDACTION_ZH.md`](CLAIMS_AND_REDACTION_ZH.md)：不能夸大、不能泄露、不能公开的内容；
8. [`data/timeline.csv`](data/timeline.csv) 和 [`data/facts.json`](data/facts.json)：机器可读事实；
9. [`diagrams/`](diagrams/)：可直接渲染或重新设计的 Mermaid 图。

如果在原始工作站运行，另读仓库根目录下被 Git 忽略的
`video-agent-private/README.md`。其中只有本机证据路径和补录清单，不属于
公开仓库内容。

## 推荐叙事

本片重点不是“旧手机跑 Docker”这一句话，而是以下工程主线：

> DroidSpaces 在原厂内核上先天缺 namespace、TUN、VETH 等能力 →
> 酷派/Yulong 留下了公开源码，但它不能直接复现量产机 → 用同 IC 源码、
> stock 二进制行为和 crashdump 重建缺失的 OEM 实现 → 重编 37 个外置模块
> 并对齐闭源 HAL 的接口假设 → 每轮只修一个真实调用栈或日志证明的问题 →
> R16 同时守住 Android 硬件、模块 ABI 与 `/data` 解锁，让 DroidSpaces
> 真正创建隔离环境；Docker 作为最终工作负载证明。

推荐把 Docker 作为结尾验收，不要让它挤占相机、音频、显示和 crashdump
这些真正体现适配难度的篇幅。

## 推荐成片规格

- 中文第一人称；
- 10–12 分钟技术叙事版，或压缩为 6–8 分钟快节奏版；
- 画面比例 16:9，至少 1080p；
- 日志截图统一使用等宽字体，关键行高亮，不展示完整个人环境；
- 每次出现候选 boot 名称时显示 `RAM-BOOT ONLY / NEVER FLASH`；
- 结尾明确“阶段性测试基线，不是可持久刷写的公开稳定版”。

## 三个必须保留的反转

1. **编译成功不等于能启动。** 最早候选走完 bootloader 后仍进 900e。
2. **相机枚举成功不等于有画面。** 从 sensor probe 到真正 stream-on 还跨了
   供电、ISP、UAPI、CHI 和音频/ADSP 多层问题。
3. **看到闪烁不等于显示内核一定有错。** 人脸补光是叠加因素，但关闭后仍
   闪，最终又回到 Iris PT/ABYP 和 DSI 时序证据。

## 可以自由改写的部分

- 标题、开场语气、转场、梗和节奏；
- 是否合并 R6–R10 为一段“音频/触摸/相机并发故障”；
- 是否把完整 16 轮做成快速蒙太奇；
- 图表颜色、字体、动画方式。

## 不得自由改写的部分

- 不得说实验镜像已经刷入启动分区；
- 不得说 R16 是 bit-identical、可复现或公开稳定版；
- 不得把任何一次工具触发的 crash 说成内核自然崩溃；
- 不得说截图直接证明/否定实体屏幕锐化；
- 不得把 R15 描述成“进入 Android 后密码无法解密”，实际确认是启动失败进入 Recovery；
- 不得说相机完全由开源内核修好，最终验收还依赖闭源 CHI 用户态兼容补丁；
- 不得公开 stock boot、vendor blob、模块二进制、内存转储、设备凭据或私钥。

## 一句话结论

这不是“给旧手机勾几个 Kconfig”，也不是单独修一个 App。为了补上
DroidSpaces 脚下的宿主能力，我们把一份不能直接驱动量产硬件的公开内核，
借助同 IC 源码、二进制行为 oracle、37 个替换模块和实机 crashdump 重新救活。
所谓“补齐私有库”必须准确表达为兼容性重建，不能暗示仓库分发了原厂闭源
二进制。
