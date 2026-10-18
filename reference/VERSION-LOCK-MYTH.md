# 版本号“锁死”是误判 —— 更正记录

日期：2026-10-18 ｜ 结论：**`uname -r` 可以任意改，显示与模块都不受影响**

## 之前的错误结论

`docs/ROADMAP.md` 曾写：

> `uname -r` 必须锁死 `4.19.81-perf+`，改了会让原厂显示 HAL 走另一条路径（重复 VSYNC → 闪烁）。

## 为什么是错的

1. **扫描 vendor 侧没有任何显示组件读版本串**：

```
grep -l "uname|4.19.81" /vendor/{lib64,lib}/*.so /vendor/bin/hw/* /system/lib64/hw/*.so
→ 仅 libchrome.so / libdsutils.so / libllvm-glnext.so / libllvm-qcom.so / librs_adreno.so
  （其余命中都是 busybox 自带的 uname 命令名）
```

2. **当时的“对照”不成立**：R26（`-gaea-R26`）与 R27（`-perf+`）都是**两次不同窗口**的 logcat 计数，
   而且那两次构建**都还带着亮度字节序 bug**（`bl_is_big_endian` 默认小端）。那次比较测到的是噪声，
   不是版本串的影响。

3. **干净对照（R34）**：与 R33 同源同配置，唯一差别是 `CONFIG_LOCALVERSION="-gaea-R34"`。

| 内核 | `uname -r` | 30 秒重复 VSYNC | 用户目测亮度切换 |
|---|---|---|---|
| R33 | `4.19.81-perf+` | 0 / 10s | 不闪 ✓ |
| **R34** | **`4.19.81-gaea-R34`** | 6 / 30s（量级相当，SF 自身会丢弃） | **不闪** ✓ |

⇒ 版本串与显示无关；亮度爆闪的真正原因始终是**亮度命令字节序**（`mi_cfg` 小端 → `51 A2 02`
而非原厂 `51 02 A2`），R30 修的就是它。

## 仍然存在的真实约束（与显示无关）

| 约束 | 处理 |
|---|---|
| 原厂 `.ko` 的 `vermagic` | `same_magic` 打印一次告警后放行（`gaea: vermagic mismatch, allowed`） |
| `modversions` CRC 无法复现（198/413 不同，工具链决定） | `check_version` / `check_modstruct_version` 放行 |
| 缺失的厂商符号（如 `yl_params_kernel_read`） | 补 stub |

⇒ **升级子版本不再有任何版本号方面的阻塞**，只剩真正的合并工作。

## 已修正

- `docs/ROADMAP.md` 删除“版本串必须锁死”的规则，改为“版本串可任意设置，识别靠 `/proc/version` 的 `#NN`”。
- 本文件作为留档，防止再次把这条错误结论带进新会话。
