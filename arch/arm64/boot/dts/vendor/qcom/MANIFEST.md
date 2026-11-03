# 3917JR / gaea — recovered vendor device trees

Source: device partitions (`boot_a` base DTB + `dtbo_a` DTBO table, 16 entries).
Method: dtc decompile -> fixup-table-driven phandle/label rewrite -> recompile ->
compare against the original binaries (reference cells resolved through each
file's own phandle table).

Acceptance (all on homelab, dtc 1.7.0):
- 17/17 artifacts reference-identical to the device binaries
- original merge (base+overlay13) == recompiled merge: 0 differences
- recompiled merge vs live device FDT: 3408/3408 nodes, 0 missing
- rebuilt DTBO table: same header/entry structure, 16/16 entries clean
- the device boots with overlay entry **13** (`androidboot.dtbo_idx=13`)

| file | kind | board-id | fragments | oem | verify | device |
|---|---|---|---|---|---|---|
| base.dts | base |  |  |  | n/a(base) |  |
| dtbo-00.dts | overlay | 0x21 0x00 | 537 |  | clean |  |
| dtbo-01.dts | overlay | 0x01 0x00 | 660 | fts×1 | clean |  |
| dtbo-02.dts | overlay | 0x21 0x00 | 437 |  | clean |  |
| dtbo-03.dts | overlay | 0x08 0x00 | 823 | iris3×1, gw9558×1, fts×2, rt1011×1 | clean |  |
| dtbo-04.dts | overlay | 0x21 0x00 | 535 |  | clean |  |
| dtbo-05.dts | overlay | 0x0b 0x00 | 791 | fts×2 | clean |  |
| dtbo-06.dts | overlay | 0x0f 0x00 | 57 |  | clean |  |
| dtbo-07.dts | overlay | 0x1000b 0x00 | 639 | fts×2 | clean |  |
| dtbo-08.dts | overlay | 0x0b 0x00 | 789 | fts×2 | clean |  |
| dtbo-09.dts | overlay | 0x08 0x01 | 581 |  | clean |  |
| dtbo-10.dts | overlay | 0x0f 0x00 | 53 |  | clean |  |
| dtbo-11.dts | overlay | 0x08 0x00 | 577 |  | clean |  |
| dtbo-12.dts | overlay | 0x01 0x00 | 658 | fts×1 | clean |  |
| dtbo-13.dts | overlay | 0x08 0x00 | 821 | iris3×1, gw9558×1, fts×2, rt1011×1 | clean | IN USE |
| dtbo-14.dts | overlay | 0x01 0x00 | 585 | fts×1 | clean |  |
| dtbo-15.dts | overlay | 0x21 0x01 | 695 |  | clean |  |

board-id 0x08 covers the gaea family (entries 03, 09, 11, 13); only entry 13 is
confirmed by hardware (live FDT match). Iris3 / Goodix GW9558 / FocalTech FT3518 /
RT1011 live in the overlays, not in the base.

## 变体地图（2026-10-12 实测，base+overlay 合并后逐属性对比）

16 个 entry 的选择键是 `qcom,msm-id`（SoC 编号 + 硅版本）与 `qcom,board-id`：

| entry | msm-id | 硅版本 | board-id | 面板档 | OEM 栈 | 归属 |
|---|---|---|---|---|---|---|
| 13 | 0x190 / 0x1b8 | **v2 (0x20000)** | 0x08 0x00 | 26 个 panel 候选 | iris3 + GW9558 + FT3518 + RT1011 | **gaea = 本机**（cmdline `hardware_version=P2`，与 live FDT 完全一致）|
| 03 | 0x190 / 0x1b8 | v1 (0x10000) | 0x08 0x00 | 同上 26 个 | 同上 | gaea 的**早期硅批次**（同一产品，v1 芯片）|
| 11 / 09 | 0x1b2 / 0x1cb | v1 (0x10000) | 0x08 0x00 / 0x01 | 13 个 FHD 1080p | **无** iris3 / GW9558 / FT3518 / RT1011 | 兄弟产品 `bj`（与 `configs/products/bj/board_config` 对得上：LCD 面板、`PXLW_IRIS3=n`、触摸换成 FT8719、`RT1011=n`）|
| 其余 12 个 | 0x21 / 0x01 / 0x0b / 0x0f / 0x1000b | — | — | Sharp WQHD / Sharp 2k 等 | 无 | Qualcomm Lito MTP 的候选集，与这两款量产机无关 |

**#03 与 #13 的功能差异只有两条属性**（合并后逐属性比对，其余全同）：

- `/soc/qcom,msm-audio-apr:qcom,lpi-slew-base-tbl` —— 仅 v2 有（音频 LPI slew 表）
- `/soc/rsc@18200000:vdda-phy-always-on` —— 仅 v1 有（对早期硅的 RSC/PHY 供电 workaround）

典型的**硅版本 workaround**：同一台机换芯片批次，DT 只动这两处。

**本机在用的显示配置**：cmdline `msm_drm.dsi_display0=qcom,mdss_dsi_rm692a9_visionox_amoled_cmd`
→ 实际点亮的是 **Visionox rm692a9 AMOLED（cmd mode）**，overlay 里 26 个 panel 候选只是厂商随包带的备选表。

**对移植的结论**：gaea 的 DT 必须保留 AMOLED + iris3 + FT3518 + GW9558 + RT1011 这一套；
**不要把 `bj`（#09/#11）的配置混进来**（LCD、无 Iris3、FT8719 触摸、无指纹/功放）。
实际需要编译进 `dtb.img` 的是 base + #13（#03 作为同款不同硅批次保留）。

## 入树（2026-10-12）

本目录已放到源码树里 `arch/arm64/boot/dts/vendor` 符号链接的目标位置
（`<工作区>/vendor/qcom/proprietary/devicetree-4.19/`），`make dtbs` 可重建全部 17 个产物，
且与设备原厂件逐字节相同（base 401,351 B `ec4cf87d80…`，16 个 overlay 全部一致）。
Makefile 用 `dtb-y` 通道 + `DTC_FLAGS += -@`；细节见 `DEVICE-TREE-IN-TREE.md`。
