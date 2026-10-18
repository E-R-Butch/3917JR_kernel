# 原厂内核的显示亮度路径（逆向结论）

日期：2026-10-18 ｜ 依据：原厂 `stock-Image` 字符串扫描 + 本项目 `iris3-reverse/IRIS3-DISPLAY-ANALYSIS.md` §5 + 实机对照

## 一句话

**原厂内核里完全没有小米的显示配置层**，所以面板亮度字节序由原生 QTI 路径固定，不能通过 DT 属性调整；我们的树（小米基线）多出一层 `mi_cfg`，其默认小端会把亮度命令发错字节序，导致每次亮度变化爆闪。

## 证据 1：原厂 Image 里没有 mi 显示配置字符串

```bash
S=/root/archive/evidence/host-work-snapshot/3917jr-audio-20260907/stock-Image   # 35,272,716 B
for k in "bl-dcs-big-endian" "mi,feature-enabled" "mi,mdss-dsi-bl" \
         "display_brightness_big_endian" "mi_cfg"; do
  echo "$k : $(strings $S | grep -c "$k")"
done
```

输出：

```
bl-dcs-big-endian : 0
mi,feature-enabled : 0
mi,mdss-dsi-bl : 0
display_brightness_big_endian : 0
mi_cfg : 0
```

`strings $S | grep -oE "mi,mdss-dsi-bl[a-z-]*" | sort -u` 无任何输出。

⇒ 原厂内核**不解析** `mi,mdss-dsi-bl-dcs-big-endian-type`，也不存在 `mi_cfg` 结构 —— 该属性在原厂树上没有意义。

## 证据 2：原厂实际下发的字节序

`iris3-reverse/IRIS3-DISPLAY-ANALYSIS.md` §5（来自本项目历史 dmesg 与 PT 路径反编译）：

> ABYP 关掉 DBC/PQ 后，亮度只能走通用 MIPI DCS 路径；该路径的字节序取决于 DT 布尔属性
> `mi,mdss-dsi-bl-dcs-big-endian-type`（原厂子树缺失时默认 false，发送 `51 A2 02`
> 而非原厂 PT 路径的 `51 02 A2`）。R17D3 补上该属性后用户确认不再爆闪。

即原厂/PT 路径发的是 **`51 02 A2`**（高字节在前 = 大端）。

## 证据 3：本次实机对照

| 构建 | 字节序 | 现象 |
|---|---|---|
| R27/R28 之前（默认小端） | `51 A2 02` | 每次亮度变化（变暗/变亮）整屏爆闪 |
| 单次 sysfs 写入 | — | 不闪（说明不是写入频率或 PWM） |
| 8 张截图逐字节相同 | — | 画面内容未变 ⇒ 闪烁在面板/背光层，不是合成层 |

## 我们的处理（代码侧，不是 DT）

本树面板节点位于 **DTBO 的 fragment** 里（`androidboot.dtbo_idx=13`），实测把属性补进 DTBO 后
生效 DT（`/proc/device-tree`）里仍取不到该节点，overlay 未并入 —— 因此 DT 路线不可用。

改为在驱动里直接按原厂行为取大端：

```c
/* techpack/display/msm/dsi/dsi_panel_mi.c */
mi_cfg->bl_is_big_endian = true;
```

对应 `dsi_panel.c:806` 走 `mipi_dsi_dcs_set_display_brightness_big_endian()`。

## 待验证

R30（含上述改动）尚需实机确认亮度切换是否还闪；若不闪则本条闭环，
若仍闪则下一步逆向原厂 `dsi_panel_set_backlight` 的具体指令序列（需要先符号化 stock Image）。
