# 给视频 Agent 的可直接复制任务

请根据本目录资料，制作一条 10–12 分钟、16:9、中文第一人称技术叙事视频。

## 核心命题

故事必须从修复 DroidSpaces 开始。原厂内核缺少它真正需要的 namespace、
TUN/TAP、VETH、bridge 和其他容器能力；root 与 Magisk 无法补出编译期能力。
为了让 DroidSpaces 真正可用，我们才去挽回酷派/Yulong 为 3917JR 留下、
但无法直接复现量产机的 Android 4.19 开源内核：从能编译却进入 Qualcomm
900e，到利用同 IC 公开
源码、stock 二进制行为、Sahara crashdump 和实机日志，逐层补齐缺失 OEM
驱动、37 个替换模块的 ABI/签名/加载时序，以及闭源 Android HAL 期待的接口，
最终得到通过 Android 硬件、DroidSpaces 与 Docker 验收的 R16。

主角是 DroidSpaces 修复，以及“为补足它脚下的内核能力，公开代码离量产
源码还有多远、我们怎样用工程证据把它救回来”。Docker 只作为结尾的真实
负载证明。

## 必读输入

依次读取：

1. `../ADAPTATION_HISTORY_ZH.md`
2. `VOICEOVER_ZH.md`
3. `SHOT_LIST_ZH.md`
4. `EVIDENCE_CARDS_ZH.md`
5. `EVIDENCE_INDEX_ZH.md`
6. `CLAIMS_AND_REDACTION_ZH.md`
7. `data/timeline.csv`
8. `data/facts.json`

如果运行环境能访问仓库根目录，还可读取 Git 忽略的
`video-agent-private/README.md`，但其中列出的原始材料只能用于核对和脱敏摘录，
不得整包上传、外发或直接放进成片。

## 成片必须回答的问题

1. DroidSpaces 原本在 3917JR 上具体缺什么，为什么原厂 root 仍不能补出
   namespace、VETH、TUN 等编译期能力？
2. 为什么酷派/Yulong 的公开仓库能编译，却不能直接启动量产机？
3. 我们怎样区分公开源码、同 IC donor、外置模块和闭源用户态组件？
4. R1 的 6.45 GB crashdump 怎样把猜测变成 camera SMMU 的确定调用栈？
5. 相机为什么从 sensor 枚举到真实预览还跨越供电、ISP、UAPI、CHI 与音频？
6. 37 个替换模块为什么必须和候选内核一起重编、签名并审计 2,799 个依赖？
7. Iris PT/ABYP 怎样造成启动崩溃、过度锐化和闪烁的取舍？
8. R16 怎样证明 DroidSpaces 不只检查全绿，而且能承载 Ubuntu、systemd 和
   真实容器网络/文件负载？
9. 为什么 R16 只是 RAM-boot 黄金基线，而不是可随便刷写的公开稳定版？

## 叙事约束

- 使用 `[实测]`、`[已定位]`、`[推断]`、`[未上机]` 区分证据强度；
- R13 是未上机的离线诊断构建，不能做成功或失败镜头；
- R15 是启动异常进入 Recovery，不得说成进入 Android 后 `/data` 密码解密失败；
- 相机最终通过还依赖本机闭源 CHI 用户态兼容保护，不得说成纯内核修复；
- 触摸最终实测正常，但不能把全部功劳归给 `0xCF=0x02`；
- 所有候选均为 `fastboot boot`，没有写入 `boot_a` 或 `boot_b`；
- 不得宣称 R16 已是 CIP、已可复现构建、可持久刷写或适合生产 Docker；
- 不得伪造已经丢失的历史截图。没有真实素材时，用明确标注的日志重排卡或图示。

## 输出物

请先输出以下四项，等待人工检查后再生成最终视频：

1. 逐分钟结构和每段核心结论；
2. 完整旁白修订稿；
3. 镜头表，注明“现有 / 需补录 / 图示 / 根据日志重排”；
4. 事实核对表，逐项标注来源文件和证据等级。

审阅通过后，再输出字幕、画面文案、封面标题和剪辑工程。任何需要连接手机、
启动 Colima、读取私有证据或重新 RAM-boot 的动作，都先请求用户授权。
