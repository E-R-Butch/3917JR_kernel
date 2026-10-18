# params 分区实测内容（结论：是空的，不需要解析）

日期：2026-10-18 ｜ 设备：3917JR / `2849a5ad`

## 原始 dump

```bash
su -c 'su 0 -c "dd if=/dev/block/by-name/params of=/data/local/tmp/params.bin bs=4096 count=1"'
# 4096 字节，非零仅 22 字节
```

```
0x0000  "DEVICE"                  （6 B）
0x0010  "V1160910LITO"            （12 B，批号/版本）
0x0028  "LITO"                    （4 B）
其余 0x002C–0x0FFF 全为 0x00
```

## 由此排除了两条“待办”

先前把下面两件事记成“需要补 `yl_params` 解析”，实测证明**设备上根本没有这份数据**：

### 1. WiFi MAC

`params` 里没有任何 MAC 形态的数据（既无 `AA:BB:CC` 文本，也无 6 字节二进制）。
`/vendor/lib/modules/qca_cld3_wlan.ko` 的 `get_mac_address()` 会调
`yl_params_kernel_read(buf, 512)` 再从 `buf+384` 取 6 字节；取到全 0 即判定无效，
驱动退化为**按序列号派生**的 MAC（本机 `96:22:8c:9d:d1:66`）。

⇒ 原厂同样如此。我们的 `yl_params_kernel_read` 返回 0（不写缓冲区）与原厂行为等价。

### 2. RT1011 单体校准

`rt1011_r0_cali_get: smartpa cali read data fail!` 这条日志在**原厂内核启动时同样出现**（已在原厂
RAM 启动的 dmesg 中抓到）。说明工厂未写入单体校准，驱动按默认 EQ/DRC 曲线工作。

⇒ 补“解析 params 里的校准段”是无意义的：那里没有数据。若要改音质，应走
`rt1011_bq_drc_coeff_put` 那套 ALSA 控件（`AdvanceMode` EQ/DRC 系数）直接调，
而不是伪造校准数据。

## 处理

- `drivers/misc/yl_params.c` 保持现状：`filp_open("/dev/block/by-name/params")` 读取
  `len` 字节，读不满返回 `-EIO`，不做任何猜测性解码。
- 不再为这两项排期。
