# 视频证据索引

本索引把核心叙事与当前仍可读取的持久证据对应起来。`本机私有` 路径只用于
视频 Agent 核对，不能直接上传。

## 1. 当前仓库中的公开安全证据

| 主题 | 路径 | 用途 |
| --- | --- | --- |
| 项目状态与警告 | `README.md` | 开场/结尾状态卡 |
| 完整适配历史 | `docs/ADAPTATION_HISTORY_ZH.md` | 主叙事事实底稿 |
| 源码与黄金身份 | `docs/PROVENANCE.md` | R16 hash、donor、工具链、可复现边界 |
| 测试门槛 | `docs/TEST_MATRIX.md` | 展示“能开机不等于通过” |
| 发布边界 | `docs/RELEASE_POLICY.md` | 解释为什么 repo 不含 boot/模块/vendor blob |
| 升级路线 | `docs/ROADMAP.md` | 片尾“接下来做 4.19 LTS/CIP” |
| 精确 R16 配置 | `configs/gaea-r16-golden.config` | Kconfig 对比画面 |
| 外部模块 commit | `external/README.md` | 37 模块来源图 |
| 黄金哈希 | `manifests/R16_GOLDEN_ARTIFACTS.sha256` | 结果卡/校验画面 |
| 源码快照 manifest | `manifests/R16_SOURCE_SNAPSHOT.sha256` | 说明源码已固化 |
| 公共树预检 | `scripts/check-release-tree.sh` | 安全审计/终检画面 |

## 2. 本机持久工程证据（不要上传原文件）

下表路径都相对于原始工作站的 `w-x-z` 工程根目录；它的绝对位置只记录在
Git 忽略的 `video-agent-private/README.md` 中，不写入公共资料。

| 主题 | 本机路径 | 视频用法 |
| --- | --- | --- |
| 早期 B 站文案 | `outputs/3917JR-DroidSpaces-R16-B站视频文案-20260805.md` | 旧稿参考，不作为最新事实源 |
| 原厂能力缺失清单 | `outputs/gaea-dump-manifest.md` | 原厂 IKCONFIG 与 DroidSpaces 缺项；其中 stock component hash 以当前仓库 provenance 为准 |
| 原厂配置原件 | `work/gaea-stock-config.gz` | 与 R16 config 制作 namespace/TUN/VETH 前后对比；不要上传整份原厂工程 |
| DroidSpaces OSS 文档图 | `work/Droidspaces-OSS/Documentation/resources/` | 仅作官方功能示意，必须注明“非本机测试截图”并遵循其许可 |
| v3/R1 Crashdump 复盘 | `outputs/3917JR_DroidSpaces_OEMPort_ABYP_R1_20260803/evidence/CRASH_TRIAGE_20260803.md` | 裁切 UEFI→900e→无持久日志的关键段 |
| R1 源码重建来源 | `outputs/3917JR_DroidSpaces_OEMPort_ABYP_R1_20260803/source/SOURCE_RECONSTRUCTION_PROVENANCE.md` | donor/stock oracle 图；注意其中旧 stock hash 已由新 provenance 更正 |
| R1 37 模块审计 | `outputs/3917JR_DroidSpaces_OEMPort_ABYP_R1_20260803/modules/reports/3917JR_FINAL37_INDEPENDENT_READONLY_AUDIT_20260803.txt` | 模块 CRC/签名镜头 |
| 外部模块 handoff | `outputs/3917JR_DroidSpaces_OEMPort_ABYP_R1_20260803/modules/reports/FINAL-EXTERNAL32-FINAL37-BUILD-HANDOFF-20260803.md` | 音频/WLAN/data 来源说明 |
| R1 packager 审计 | `outputs/3917JR_DroidSpaces_OEMPort_ABYP_R1_20260803/packager/3917JR_RAMBOOT_PACKAGER_V4_FINAL_INDEPENDENT_REVIEW_20260803.md` | boot 结构分解 |
| R16 golden build | `artifacts/colima-preserved-20260810/r16-build/` | 仅展示目录名/哈希；不要上传二进制 |
| R16 boot manifest | `artifacts/colima-preserved-20260810/packager-r16/outputs/3917JR-DroidSpaces-R16-ABYP-DEBUG-ABI21-RAMBOOT_ONLY_NEVER_FLASH.img.manifest.txt` | 最终结果证据卡，优先使用 |
| 早期 debug build evidence | `artifacts/colima-preserved-20260810/export-bootdebug-v3-final/evidence/` | 展示 IKCONFIG、签名、module closure 的脱敏摘要 |

## 3. 当前已经不在本机的临时画面

旧稿曾引用 `/private/tmp` 下的相机预览、DroidSpaces 全绿截图和 R12/R16
报告。当前工作站重启后这些路径已不存在，不能把路径列表冒充成“素材已备份”。

需要重新录制的关键画面：

- 手机实体外观和型号；
- `uname -a` 显示 R16 `#21`（若当前仍能安全复现）；
- DroidSpaces 检查结果；
- 前后摄预览与拍照；
- Wi-Fi/TUN/namespace 简短终端证据；
- Docker Alpine/Nginx HTTP 200；
- 通知栏文字与触摸的实体屏幕画面。

若不重新 RAM boot，改用文字卡、hash、manifest 和复原示意图，不能伪造实机截图。

## 4. 推荐摘录的日志文本

以下是可以做成重排文字卡的短证据，不应伪装为完整原始终端截图：

```text
QUSB_BULK 05c6:900e
Attempted to kill init
gen_pool_destroy -> cam_smmu_release_cb -> cam_smmu_probe
```

```text
R14:
iris3_send_cont_splash_pkt
 -> iris_send_assembled_pkt
 -> iris_send_none_lut_table_pkt
 -> __memcpy(NULL)
```

```text
R16:
4.19.81-perf+ #21
replacement modules: 37
actual modversion dependencies: 2799
mismatch: 0
missing: 0
```

```text
Docker:
iptables-nft: CHAIN_ADD ... PREROUTING failed
iptables-legacy: daemon starts
F2FS upperdir: not supported
storage-driver: vfs
Nginx: HTTP/1.1 200 OK
```

## 5. 哈希身份

| 对象 | SHA-256 |
| --- | --- |
| R16 `Image` | `1162c3bf197b5395d4488923336cd28b0ed49a6874d2ffd8cf6d70d263d9f58a` |
| R16 RAM-boot | `7a1e1b93e842824f7df940adaf785fa0ded229f4aa8cee2a22916acd86cae07a` |
| R16 `.config` | `c68f0e88ee3003368e75847b3a47036abe827ce473a49a8b8a33293b2423998a` |
| R16 `Module.symvers` | `c1a157cf0859f9cb62f74794fd856f704029703c4b75bacc70fa900893411fb8` |
| R16 `vmlinux` | `8badf064b18b8ef7c92a798d1f1b0abac02fa2dc02271b0497136fa19d10bf68` |
| R16 `System.map` | `be48de0285118e6fd74dcba8c6fb01d6f8cd9227bfd4b75d52c808d0098db511` |

## 6. 证据使用原则

- 先引用当前仓库的 `PROVENANCE.md`，不要复用旧报告中已更正的 stock kernel/ramdisk hash；
- 原始日志若不可用，就把内容标为“根据当时实机记录重排”，不要做假终端；
- 每个实机结果都注明候选轮次；
- Docker 结果必须同时显示 legacy/VFS 条件；
- 视觉问题必须使用实体摄像画面或用户观察，不能只用系统 screenshot。
