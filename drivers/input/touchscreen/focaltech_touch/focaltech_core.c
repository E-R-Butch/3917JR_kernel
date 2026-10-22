// SPDX-License-Identifier: GPL-2.0-only
/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2012-2019, FocalTech Systems, Ltd., all rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
/*****************************************************************************
*
* File Name: focaltech_core.c
*
* Author: Focaltech Driver Team
*
* Created: 2016-08-08
*
* Abstract: entrance for focaltech ts driver
*
* Version: V1.0
*
*****************************************************************************/

/*****************************************************************************
* Included header files
*****************************************************************************/
#include <linux/module.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#if defined(CONFIG_DRM)
#include <drm/drm_panel.h>
#elif defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#elif defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#define FTS_SUSPEND_LEVEL 1     /* Early-suspend level */
#endif
#include "focaltech_core.h"

/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/
#define FTS_DRIVER_NAME                     "fts_ts"
#define INTERVAL_READ_REG                   200  /* unit:ms */
#define TIMEOUT_READ_REG                    1000 /* unit:ms */
#define FTS_REG_OEM_RUNTIME_MODE             0xCF
#define FTS_OEM_RUNTIME_MODE_VALUE           0x02
#define FTS_DIAG_REG_INT_MODE                0xA4
#define FTS_DIAG_POLL_INTERVAL_MS            20U
#define FTS_DIAG_POLL_DURATION_MS            15000U
#define FTS_DIAG_POLL_TARGET                 \
	(FTS_DIAG_POLL_DURATION_MS / FTS_DIAG_POLL_INTERVAL_MS)
#if FTS_POWER_SOURCE_CUST_EN
#define FTS_VTG_MIN_UV                      3000000
#define FTS_VTG_MAX_UV                      3300000
#define FTS_LOAD_MAX_UA                     30000
#define FTS_I2C_VTG_MIN_UV                  1800000
#define FTS_I2C_VTG_MAX_UV                  1800000
#endif

/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/
struct fts_ts_data *fts_data;

#if defined(CONFIG_DRM)
static struct drm_panel *active_panel;
#endif

/* Manually triggered RAM-boot diagnostic; never injects synthetic input. */
struct fts_diag_reg_snapshot {
	u8 int_mode;
	u8 power_mode;
	u8 charger_mode;
	u8 flow_work_cnt;
	int int_mode_ret;
	int power_mode_ret;
	int charger_mode_ret;
	int flow_work_cnt_ret;
};

struct fts_diag_poll_stats {
	unsigned int generation;
	unsigned int polls;
	unsigned int ret_negative;
	unsigned int ret_zero;
	unsigned int ret_one;
	unsigned int ret_two;
	unsigned int ret_gt_two;
	unsigned int ret_unset;
	unsigned int touch_frames;
	unsigned int touch_points;
	unsigned int idle_frames;
	unsigned int all_ff_frames;
	unsigned int errors;
	unsigned int io_errors;
	unsigned int parse_errors;
	unsigned int gpio_high;
	unsigned int gpio_low;
	unsigned int gpio_invalid;
	unsigned int point_num[16];
	struct fts_diag_reg_snapshot start_regs;
	struct fts_diag_reg_snapshot end_regs;
};

static struct delayed_work fts_diag_poll_work;
static DEFINE_MUTEX(fts_diag_poll_control_lock);
static bool fts_diag_poll_initialized;
static bool fts_diag_poll_requested;
static bool fts_diag_poll_active;
static bool fts_diag_poll_stop;
static bool fts_diag_poll_blocked = true;
static bool fts_diag_poll_canceling;
static const char *fts_diag_poll_stop_reason = "not-started";
static const char *fts_diag_poll_last_reason = "never-run";
static int fts_diag_last_bus_ret;
static unsigned int fts_diag_poll_generation;
static struct fts_diag_poll_stats fts_diag_poll_last_stats;

/*****************************************************************************
* Static function prototypes
*****************************************************************************/
static int fts_ts_suspend(struct device *dev);
static int fts_ts_resume(struct device *dev);

static void fts_oem_runtime_mode_apply(struct fts_ts_data *ts_data,
		const char *phase)
{
	int ret;
	int irq_level = -1;

	ret = fts_write_reg(FTS_REG_OEM_RUNTIME_MODE,
			FTS_OEM_RUNTIME_MODE_VALUE);
	if (gpio_is_valid(ts_data->pdata->irq_gpio))
		irq_level = gpio_get_value(ts_data->pdata->irq_gpio);

	if (ret < 0)
		FTS_ERROR("3917JR OEM runtime mode phase=%s reg=0x%02x val=0x%02x ret=%d irq_gpio=%d irq_level=%d",
			phase, FTS_REG_OEM_RUNTIME_MODE,
			FTS_OEM_RUNTIME_MODE_VALUE, ret,
			ts_data->pdata->irq_gpio, irq_level);
	else
		FTS_INFO("3917JR OEM runtime mode phase=%s reg=0x%02x val=0x%02x ret=%d irq_gpio=%d irq_level=%d",
			phase, FTS_REG_OEM_RUNTIME_MODE,
			FTS_OEM_RUNTIME_MODE_VALUE, ret,
			ts_data->pdata->irq_gpio, irq_level);
}

/*****************************************************************************
*  Name: fts_wait_tp_to_valid
*  Brief: Read chip id until TP FW become valid(Timeout: TIMEOUT_READ_REG),
*         need call when reset/power on/resume...
*  Input:
*  Output:
*  Return: return 0 if tp valid, otherwise return error code
*****************************************************************************/
int fts_wait_tp_to_valid(void)
{
	int ret = 0;
	int cnt = 0;
	u8 idh = 0;
	u8 idl = 0;
	u8 chip_idh = fts_data->ic_info.ids.chip_idh;
	u8 chip_idl = fts_data->ic_info.ids.chip_idl;

	do {
		ret = fts_read_reg(FTS_REG_CHIP_ID, &idh);
		ret = fts_read_reg(FTS_REG_CHIP_ID2, &idl);
		if ((ret < 0) || (idh != chip_idh) || (idl != chip_idl)) {
			FTS_DEBUG("TP Not Ready,ReadData:0x%02x%02x", idh, idl);
		} else if ((idh == chip_idh) && (idl == chip_idl)) {
			FTS_INFO("TP Ready,Device ID:0x%02x%02x", idh, idl);
			return 0;
		}
		cnt++;
		msleep(INTERVAL_READ_REG);
	} while ((cnt * INTERVAL_READ_REG) < TIMEOUT_READ_REG);

	return -EIO;
}

/*****************************************************************************
*  Name: fts_tp_state_recovery
*  Brief: Need execute this function when reset
*  Input:
*  Output:
*  Return:
*****************************************************************************/
void fts_tp_state_recovery(struct fts_ts_data *ts_data)
{
	FTS_FUNC_ENTER();
	/* wait tp stable */
	fts_wait_tp_to_valid();
	/* recover TP charger state 0x8B */
	/* recover TP glove state 0xC0 */
	/* recover TP cover state 0xC1 */
	fts_ex_mode_recovery(ts_data);
	/* recover TP gesture state 0xD0 */
	fts_gesture_recovery(ts_data);
	FTS_FUNC_EXIT();
}

int fts_reset_proc(int hdelayms)
{
	FTS_DEBUG("tp reset");
	gpio_direction_output(fts_data->pdata->reset_gpio, 0);
	msleep(1);
	gpio_direction_output(fts_data->pdata->reset_gpio, 1);
	if (hdelayms) {
		msleep(hdelayms);
	}

	return 0;
}

void fts_irq_disable(void)
{
	unsigned long irqflags;

	FTS_FUNC_ENTER();
	spin_lock_irqsave(&fts_data->irq_lock, irqflags);

	if (!fts_data->irq_disabled) {
		disable_irq_nosync(fts_data->irq);
		fts_data->irq_disabled = true;
	}

	spin_unlock_irqrestore(&fts_data->irq_lock, irqflags);
	FTS_FUNC_EXIT();
}

void fts_irq_enable(void)
{
	unsigned long irqflags = 0;

	FTS_FUNC_ENTER();
	spin_lock_irqsave(&fts_data->irq_lock, irqflags);

	if (fts_data->irq_disabled) {
		enable_irq(fts_data->irq);
		fts_data->irq_disabled = false;
	}

	spin_unlock_irqrestore(&fts_data->irq_lock, irqflags);
	FTS_FUNC_EXIT();
}

void fts_hid2std(void)
{
	int ret = 0;
	u8 buf[3] = {0xEB, 0xAA, 0x09};

	ret = fts_write(buf, 3);
	if (ret < 0) {
		FTS_ERROR("hid2std cmd write fail");
		return;
	}

	msleep(10);
	buf[0] = buf[1] = buf[2] = 0;
	ret = fts_read(NULL, 0, buf, 3);
	if (ret < 0) {
		FTS_ERROR("hid2std cmd read fail");
	} else if ((0xEB == buf[0]) && (0xAA == buf[1]) && (0x08 == buf[2])) {
		FTS_DEBUG("hidi2c change to stdi2c successful");
	} else {
		FTS_DEBUG("hidi2c change to stdi2c not support or fail");
	}

}

static int fts_get_chip_types(
	struct fts_ts_data *ts_data,
	u8 id_h, u8 id_l, bool fw_valid)
{
	int i = 0;
	struct ft_chip_t ctype[] = {{0x81, 0x54, 0x52, 0x54, 0x52, 0x00,
					0x00, 0x54, 0x5C}};
	u32 ctype_entries = ARRAY_SIZE(ctype);

	if ((0x0 == id_h) || (0x0 == id_l)) {
		FTS_ERROR("id_h/id_l is 0");
		return -EINVAL;
	}

	FTS_DEBUG("verify id:0x%02x%02x", id_h, id_l);
	for (i = 0; i < ctype_entries; i++) {
		if (VALID == fw_valid) {
			if ((id_h == ctype[i].chip_idh)
				&& (id_l == ctype[i].chip_idl))
				break;
		} else {
			if (((id_h == ctype[i].rom_idh)
				&& (id_l == ctype[i].rom_idl))
				|| ((id_h == ctype[i].pb_idh)
				&& (id_l == ctype[i].pb_idl))
				|| ((id_h == ctype[i].bl_idh)
				&& (id_l == ctype[i].bl_idl)))
			break;
		}
	}

	if (i >= ctype_entries)
		return -ENODATA;

	ts_data->ic_info.ids = ctype[i];
	return 0;
}

static int fts_read_bootid(struct fts_ts_data *ts_data, u8 *id)
{
	int ret = 0;
	u8 chip_id[2] = { 0 };
	u8 id_cmd[4] = { 0 };
	u32 id_cmd_len = 0;

	id_cmd[0] = FTS_CMD_START1;
	id_cmd[1] = FTS_CMD_START2;
	ret = fts_write(id_cmd, 2);
	if (ret < 0) {
		FTS_ERROR("start cmd write fail");
		return ret;
	}

	msleep(FTS_CMD_START_DELAY);
	id_cmd[0] = FTS_CMD_READ_ID;
	id_cmd[1] = id_cmd[2] = id_cmd[3] = 0x00;
	if (ts_data->ic_info.is_incell)
		id_cmd_len = FTS_CMD_READ_ID_LEN_INCELL;
	else
		id_cmd_len = FTS_CMD_READ_ID_LEN;
	ret = fts_read(id_cmd, id_cmd_len, chip_id, 2);
	if ((ret < 0) || (0x0 == chip_id[0]) || (0x0 == chip_id[1])) {
		FTS_ERROR("read boot id fail,read:0x%02x%02x", chip_id[0],
				chip_id[1]);
		return -EIO;
	}

	id[0] = chip_id[0];
	id[1] = chip_id[1];
	return 0;
}

/*****************************************************************************
* Name: fts_get_ic_information
* Brief: read chip id to get ic information, after run the function, driver w-
*        ill know which IC is it.
*        If cant get the ic information, maybe not focaltech's touch IC, need
*        unregister the driver
* Input:
* Output:
* Return: return 0 if get correct ic information, otherwise return error code
*****************************************************************************/
static int fts_get_ic_information(struct fts_ts_data *ts_data)
{
	int ret = 0;
	int cnt = 0;
	u8 chip_id[2] = { 0 };

	ts_data->ic_info.is_incell = FTS_CHIP_IDC;
	ts_data->ic_info.hid_supported = FTS_HID_SUPPORTTED;

	do {
		ret = fts_read_reg(FTS_REG_CHIP_ID, &chip_id[0]);
		ret = fts_read_reg(FTS_REG_CHIP_ID2, &chip_id[1]);
		if ((ret < 0) || (0x0 == chip_id[0]) || (0x0 == chip_id[1])) {
			FTS_DEBUG("i2c read invalid, read:0x%02x%02x",
				chip_id[0], chip_id[1]);
		} else {
			ret = fts_get_chip_types(ts_data, chip_id[0],
					chip_id[1], VALID);
			if (!ret)
				break;
            else
                FTS_DEBUG("TP not ready, read:0x%02x%02x",
                          chip_id[0], chip_id[1]);
		}

		cnt++;
		msleep(INTERVAL_READ_REG);
	} while ((cnt * INTERVAL_READ_REG) < TIMEOUT_READ_REG);

	if ((cnt * INTERVAL_READ_REG) >= TIMEOUT_READ_REG) {
		FTS_INFO("fw is invalid, need read boot id");
		if (ts_data->ic_info.hid_supported) {
			fts_hid2std();
		}

		ret = fts_read_bootid(ts_data, &chip_id[0]);
		if (ret <  0) {
			FTS_ERROR("read boot id fail");
			return ret;
		}

		ret = fts_get_chip_types(ts_data, chip_id[0], chip_id[1],
				INVALID);
		if (ret < 0) {
			FTS_ERROR("can't get ic informaton");
			return ret;
		}
	}

	FTS_INFO("get ic information, chip id = 0x%02x%02x",
		ts_data->ic_info.ids.chip_idh, ts_data->ic_info.ids.chip_idl);

	return 0;
}

/*****************************************************************************
*  Reprot related
*****************************************************************************/
static void fts_show_touch_buffer(u8 *data, int datalen)
{
	int i = 0;
	int count = 0;
	char *tmpbuf = NULL;

	tmpbuf = kzalloc(1024, GFP_KERNEL);
	if (!tmpbuf) {
		FTS_ERROR("tmpbuf zalloc fail");
		return;
	}

	for (i = 0; i < datalen; i++) {
		count += snprintf(tmpbuf + count, 1024 - count, "%02X,",
				data[i]);
		if (count >= 1024)
			break;
	}
	FTS_DEBUG("point buffer:%s", tmpbuf);

	if (tmpbuf) {
		kfree(tmpbuf);
		tmpbuf = NULL;
	}
}

void fts_release_all_finger(void)
{
	struct input_dev *input_dev = fts_data->input_dev;
#if FTS_MT_PROTOCOL_B_EN
	u32 finger_count = 0;
	u32 max_touches = fts_data->pdata->max_touch_number;
#endif

	FTS_FUNC_ENTER();
	mutex_lock(&fts_data->report_mutex);
#if FTS_MT_PROTOCOL_B_EN
	for (finger_count = 0; finger_count < max_touches; finger_count++) {
		input_mt_slot(input_dev, finger_count);
		input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
	}
#else
	input_mt_sync(input_dev);
#endif
	input_report_key(input_dev, BTN_TOUCH, 0);
	input_sync(input_dev);

	fts_data->touchs = 0;
	fts_data->key_state = 0;
	mutex_unlock(&fts_data->report_mutex);
	FTS_FUNC_EXIT();
}

/*****************************************************************************
* Name: fts_input_report_key
* Brief: process key events,need report key-event if key enable.
*        if point's coordinate is in (x_dim-50,y_dim-50) ~ (x_dim+50,y_dim+50),
*        need report it to key event.
*        x_dim: parse from dts, means key x_coordinate, dimension:+-50
*        y_dim: parse from dts, means key y_coordinate, dimension:+-50
* Input:
* Output:
* Return: return 0 if it's key event, otherwise return error code
*****************************************************************************/
static int fts_input_report_key(struct fts_ts_data *data, int index)
{
	int i = 0;
	int x = data->events[index].x;
	int y = data->events[index].y;
	int *x_dim = &data->pdata->key_x_coords[0];
	int *y_dim = &data->pdata->key_y_coords[0];

	if (!data->pdata->have_key) {
		return -EINVAL;
	}

	for (i = 0; i < data->pdata->key_number; i++) {
		if ((x >= x_dim[i] - FTS_KEY_DIM)
			&& (x <= x_dim[i] + FTS_KEY_DIM)
			&& (y >= y_dim[i] - FTS_KEY_DIM)
			&& (y <= y_dim[i] + FTS_KEY_DIM)) {
			if (EVENT_DOWN(data->events[index].flag)
				&& !(data->key_state & (1 << i))) {
				input_report_key(data->input_dev,
						data->pdata->keys[i], 1);
				data->key_state |= (1 << i);
				FTS_DEBUG("Key%d(%d,%d) DOWN!", i, x, y);
			} else if (EVENT_UP(data->events[index].flag)
				&& (data->key_state & (1 << i))) {
				input_report_key(data->input_dev,
						data->pdata->keys[i], 0);
				data->key_state &= ~(1 << i);
				FTS_DEBUG("Key%d(%d,%d) Up!", i, x, y);
			}
			return 0;
		}
	}
	return -EINVAL;
}

#if FTS_MT_PROTOCOL_B_EN
static int fts_input_report_b(struct fts_ts_data *data)
{
	int i = 0;
	int uppoint = 0;
	int touchs = 0;
	bool va_reported = false;
	u32 max_touch_num = data->pdata->max_touch_number;
	struct ts_event *events = data->events;

	for (i = 0; i < data->touch_point; i++) {
		if (fts_input_report_key(data, i) == 0)
			continue;

		va_reported = true;
		input_mt_slot(data->input_dev, events[i].id);

		if (EVENT_DOWN(events[i].flag)) {
			input_mt_report_slot_state(data->input_dev,
					MT_TOOL_FINGER, true);

#if FTS_REPORT_PRESSURE_EN
			if (events[i].p <= 0) {
				events[i].p = 0x3f;
			}
			input_report_abs(data->input_dev, ABS_MT_PRESSURE,
					events[i].p);
#endif
			if (events[i].area <= 0) {
				events[i].area = 0x09;
			}
			input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR,
					events[i].area);
			input_report_abs(data->input_dev, ABS_MT_POSITION_X,
					events[i].x);
			input_report_abs(data->input_dev, ABS_MT_POSITION_Y,
					events[i].y);

			touchs |= BIT(events[i].id);
			data->touchs |= BIT(events[i].id);

			if ((data->log_level >= 2) || ((1 == data->log_level)
				&& (FTS_TOUCH_DOWN == events[i].flag))) {
				FTS_DEBUG("[B]P%d(%d, %d)[p:%d,tm:%d] DOWN!",
					events[i].id,
					events[i].x, events[i].y,
					events[i].p, events[i].area);
			}
		} else {
			uppoint++;
			input_mt_report_slot_state(data->input_dev,
					MT_TOOL_FINGER, false);
			data->touchs &= ~BIT(events[i].id);
			if (data->log_level >= 1) {
				FTS_DEBUG("[B]P%d UP!", events[i].id);
			}
		}
	}

	if (unlikely(data->touchs ^ touchs)) {
		for (i = 0; i < max_touch_num; i++)  {
			if (BIT(i) & (data->touchs ^ touchs)) {
				if (data->log_level >= 1) {
					FTS_DEBUG("[B]P%d UP!", i);
				}
				va_reported = true;
				input_mt_slot(data->input_dev, i);
				input_mt_report_slot_state(data->input_dev,
						MT_TOOL_FINGER, false);
			}
		}
	}
	data->touchs = touchs;

	if (va_reported) {
		/* touchs==0, there's no point but key */
		if (EVENT_NO_DOWN(data) || (!touchs)) {
			if (data->log_level >= 1) {
				FTS_DEBUG("[B]Points All Up!");
			}
			input_report_key(data->input_dev, BTN_TOUCH, 0);
		} else {
			input_report_key(data->input_dev, BTN_TOUCH, 1);
		}
	}

	input_sync(data->input_dev);
	return 0;
}

#else
static int fts_input_report_a(struct fts_ts_data *data)
{
	int i = 0;
	int touchs = 0;
	bool va_reported = false;
	struct ts_event *events = data->events;

	for (i = 0; i < data->touch_point; i++) {
		if (fts_input_report_key(data, i) == 0) {
			continue;
		}

		va_reported = true;
		if (EVENT_DOWN(events[i].flag)) {
			input_report_abs(data->input_dev, ABS_MT_TRACKING_ID,
					events[i].id);
#if FTS_REPORT_PRESSURE_EN
			if (events[i].p <= 0) {
				events[i].p = 0x3f;
			}
			input_report_abs(data->input_dev, ABS_MT_PRESSURE,
					events[i].p);
#endif
			if (events[i].area <= 0) {
				events[i].area = 0x09;
			}
			input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR,
					events[i].area);

			input_report_abs(data->input_dev, ABS_MT_POSITION_X,
					events[i].x);
			input_report_abs(data->input_dev, ABS_MT_POSITION_Y,
					events[i].y);

			input_mt_sync(data->input_dev);

			if ((data->log_level >= 2) || ((1 == data->log_level)
				&& (FTS_TOUCH_DOWN == events[i].flag))) {
				FTS_DEBUG("[A]P%d(%d, %d)[p:%d,tm:%d] DOWN!",
					events[i].id,
					events[i].x, events[i].y,
					events[i].p, events[i].area);
			}
			touchs++;
		}
	}

	/* last point down, current no point but key */
	if (data->touchs && !touchs) {
		va_reported = true;
	}
	data->touchs = touchs;

	if (va_reported) {
		if (EVENT_NO_DOWN(data)) {
			if (data->log_level >= 1) {
				FTS_DEBUG("[A]Points All Up!");
			}
			input_report_key(data->input_dev, BTN_TOUCH, 0);
			input_mt_sync(data->input_dev);
		} else {
			input_report_key(data->input_dev, BTN_TOUCH, 1);
		}
	}

	input_sync(data->input_dev);
	return 0;
}
#endif

#if FTS_FOD_KEY_EN
/*
 * 原厂 FOD 按键上报（对照 stock-Image fts_irq_handler+0xb4..0x140）：
 * 每次读完触摸报文后看 buf[1]：
 *   0x01 = FOD 按下 -> 上报 EV_KEY 508 的「脉冲」（按下状态未置位时才发）
 *   0x02 = FOD 抬起 -> 上报 EV_KEY 509 的「脉冲」并复位状态
 * Android 侧把这个 508 映射成 keycode 243，SystemUI 的
 * StatusBar.dispatchKeyEvent -> handleFingerprintAuth -> startFingerprintAuth
 * 才是 setHbm(1)/downOrUp(1) 的唯一触发源；缺了它锁屏按 FOD 只会收到抬起事件。
 */
static void fts_fod_key_report(struct fts_ts_data *data, const u8 *buf)
{
	static bool fod_pressed;
	struct input_dev *input_dev = data->input_dev;
	u8 ev = buf[FTS_TOUCH_FOD_POS];

	if (ev == FTS_FOD_EVT_DOWN) {
		if (fod_pressed)
			return;
		fod_pressed = true;
		input_report_key(input_dev, KEY_FOD_DOWN, 1);
		input_sync(input_dev);
		input_report_key(input_dev, KEY_FOD_DOWN, 0);
		input_sync(input_dev);
		FTS_INFO("fod key: press (buf[1]=0x%02x)", ev);
	} else if (ev == FTS_FOD_EVT_UP) {
		fod_pressed = false;
		input_report_key(input_dev, KEY_FOD_UP, 1);
		input_sync(input_dev);
		input_report_key(input_dev, KEY_FOD_UP, 0);
		input_sync(input_dev);
		FTS_INFO("fod key: release (buf[1]=0x%02x)", ev);
	}
}
#endif

static int fts_read_touchdata(struct fts_ts_data *data)
{
	int ret = 0;
	u8 *buf = data->point_buf;

	memset(buf, 0xFF, data->pnt_buf_size);
	buf[0] = 0x01;

	if (data->gesture_mode) {
		if (0 == fts_gesture_readdata(data, NULL)) {
			FTS_INFO("succuss to get gesture data in irq handler");
			return 1;
		}
	}

	ret = fts_read(buf, 1, buf + 1, data->pnt_buf_size - 1);
	if (READ_ONCE(fts_diag_poll_active)) {
		WRITE_ONCE(fts_diag_last_bus_ret, ret);
		/*
		 * A combined register read is complete only when both I2C messages
		 * transfer.  During the manual diagnostic, never let a short
		 * transfer parse/report the prefilled (or stale) receive buffer.
		 * Keep the normal IRQ path's stock-compatible return semantics.
		 */
		if (ret != 2)
			return -EIO;
	}
	if (ret < 0) {
		FTS_ERROR("read touchdata failed, ret:%d", ret);
		return ret;
	}

#if FTS_FOD_KEY_EN
	fts_fod_key_report(data, buf);
#endif

	if (data->log_level >= 3) {
		fts_show_touch_buffer(buf, data->pnt_buf_size);
	}

	return 0;
}

static int fts_read_parse_touchdata(struct fts_ts_data *data)
{
	int ret = 0;
	int i = 0;
	u8 pointid = 0;
	int base = 0;
	struct ts_event *events = data->events;
	int max_touch_num = data->pdata->max_touch_number;
	u8 *buf = data->point_buf;

	ret = fts_read_touchdata(data);
	if (ret) {
		return ret;
	}

	data->point_num = buf[FTS_TOUCH_POINT_NUM] & 0x0F;
	data->touch_point = 0;

	if (data->ic_info.is_incell) {
		if ((data->point_num == 0x0F) && (buf[2] == 0xFF)
			&& (buf[3] == 0xFF) && (buf[4] == 0xFF)
			&& (buf[5] == 0xFF) && (buf[6] == 0xFF)) {
			FTS_DEBUG("touch buff is 0xff, need recovery state");
			fts_release_all_finger();
			fts_tp_state_recovery(data);
			return -EIO;
		}
	}

	if (data->point_num > max_touch_num) {
		if (!READ_ONCE(fts_diag_poll_active))
			FTS_INFO("invalid point_num(%d)", data->point_num);
		return -EIO;
	}

	for (i = 0; i < max_touch_num; i++) {
		base = FTS_ONE_TCH_LEN * i;
		pointid = (buf[FTS_TOUCH_ID_POS + base]) >> 4;
		if (pointid >= FTS_MAX_ID)
			break;
		else if (pointid >= max_touch_num) {
			FTS_ERROR("ID(%d) beyond max_touch_number", pointid);
			return -EINVAL;
		}

		data->touch_point++;
		events[i].x = ((buf[FTS_TOUCH_X_H_POS + base] & 0x0F) << 8) +
				(buf[FTS_TOUCH_X_L_POS + base] & 0xFF);
		events[i].y = ((buf[FTS_TOUCH_Y_H_POS + base] & 0x0F) << 8) +
				(buf[FTS_TOUCH_Y_L_POS + base] & 0xFF);
		events[i].flag = buf[FTS_TOUCH_EVENT_POS + base] >> 6;
		events[i].id = buf[FTS_TOUCH_ID_POS + base] >> 4;
		events[i].area = buf[FTS_TOUCH_AREA_POS + base] >> 4;
		events[i].p =  buf[FTS_TOUCH_PRE_POS + base];

		if (EVENT_DOWN(events[i].flag) && (data->point_num == 0)) {
			FTS_INFO("abnormal touch data from fw");
			return -EIO;
		}
	}

	if (data->touch_point == 0) {
		if (!READ_ONCE(fts_diag_poll_active))
			FTS_INFO("no touch point information");
		return -EIO;
	}

	return 0;
}

static int fts_irq_read_report(void)
{
	int ret = 0;
	struct fts_ts_data *ts_data = fts_data;

#if FTS_ESDCHECK_EN
	fts_esdcheck_set_intr(1);
#endif

#if FTS_POINT_REPORT_CHECK_EN
	fts_prc_queue_work(ts_data);
#endif

	ret = fts_read_parse_touchdata(ts_data);
	if (ret == 0) {
		mutex_lock(&ts_data->report_mutex);
#if FTS_MT_PROTOCOL_B_EN
		fts_input_report_b(ts_data);
#else
		fts_input_report_a(ts_data);
#endif
		mutex_unlock(&ts_data->report_mutex);
	}

#if FTS_ESDCHECK_EN
	fts_esdcheck_set_intr(0);
#endif

	return ret;
}

static irqreturn_t fts_irq_handler(int irq, void *data)
{
	fts_irq_read_report();
	return IRQ_HANDLED;
}

static void fts_diag_invalidate_reg_snapshot(
		struct fts_diag_reg_snapshot *snapshot)
{
	memset(snapshot, 0xFF, sizeof(*snapshot));
	snapshot->int_mode_ret = -ENODATA;
	snapshot->power_mode_ret = -ENODATA;
	snapshot->charger_mode_ret = -ENODATA;
	snapshot->flow_work_cnt_ret = -ENODATA;
}

static void fts_diag_read_reg_snapshot(struct fts_diag_reg_snapshot *snapshot)
{
	fts_diag_invalidate_reg_snapshot(snapshot);
	snapshot->int_mode_ret = fts_read_reg(FTS_DIAG_REG_INT_MODE,
		&snapshot->int_mode);
	snapshot->power_mode_ret = fts_read_reg(FTS_REG_POWER_MODE,
		&snapshot->power_mode);
	snapshot->charger_mode_ret = fts_read_reg(FTS_REG_CHARGER_MODE_EN,
		&snapshot->charger_mode);
	snapshot->flow_work_cnt_ret = fts_read_reg(FTS_REG_FLOW_WORK_CNT,
		&snapshot->flow_work_cnt);
}

static bool fts_diag_report_is_all_ff(struct fts_ts_data *ts_data)
{
	int i;

	for (i = 1; i < ts_data->pnt_buf_size; i++) {
		if (ts_data->point_buf[i] != 0xFF)
			return false;
	}

	return true;
}

static void fts_diag_log_reg_snapshot(const char *phase,
		const struct fts_diag_reg_snapshot *snapshot)
{
	FTS_INFO("R10_DIAG regs phase=%s A4=0x%02x(ret=%d) A5=0x%02x(ret=%d) 8B=0x%02x(ret=%d) 91=0x%02x(ret=%d)",
		phase, snapshot->int_mode, snapshot->int_mode_ret,
		snapshot->power_mode, snapshot->power_mode_ret,
		snapshot->charger_mode, snapshot->charger_mode_ret,
		snapshot->flow_work_cnt, snapshot->flow_work_cnt_ret);
}

static bool fts_diag_irq_disable(struct fts_ts_data *ts_data)
{
	unsigned long irqflags;
	bool acquired = false;

	spin_lock_irqsave(&ts_data->irq_lock, irqflags);
	if (!ts_data->irq_disabled) {
		disable_irq_nosync(ts_data->irq);
		ts_data->irq_disabled = true;
		acquired = true;
	}
	spin_unlock_irqrestore(&ts_data->irq_lock, irqflags);

	return acquired;
}

static void fts_diag_poll_workfn(struct work_struct *work)
{
	struct fts_ts_data *ts_data = fts_data;
	struct fts_diag_poll_stats stats;
	const char *reason = "complete";
	unsigned int i;
	unsigned long deadline;
	int bus_ret;
	int irq_level;
	int ret;
	bool complete_frame;
	bool irq_disabled_by_diag = false;

	(void)work;
	memset(&stats, 0, sizeof(stats));
	fts_diag_invalidate_reg_snapshot(&stats.start_regs);
	fts_diag_invalidate_reg_snapshot(&stats.end_regs);

	mutex_lock(&fts_diag_poll_control_lock);
	stats.generation = fts_diag_poll_generation;
	if (!fts_diag_poll_initialized || !fts_diag_poll_requested) {
		reason = "not-requested";
		mutex_unlock(&fts_diag_poll_control_lock);
		goto out_done;
	}
	if (fts_diag_poll_blocked || fts_diag_poll_stop) {
		reason = fts_diag_poll_stop ? fts_diag_poll_stop_reason :
			"blocked";
		mutex_unlock(&fts_diag_poll_control_lock);
		goto out_done;
	}
	WRITE_ONCE(fts_diag_poll_active, true);
	mutex_unlock(&fts_diag_poll_control_lock);

	if (!ts_data) {
		reason = "no-device";
		goto out_done;
	}

	if (READ_ONCE(fts_diag_poll_stop)) {
		reason = READ_ONCE(fts_diag_poll_stop_reason);
		goto out_done;
	}

	if (READ_ONCE(ts_data->suspended) || READ_ONCE(ts_data->fw_loading) ||
			READ_ONCE(ts_data->irq_disabled)) {
		reason = "device-not-ready";
		goto out_done;
	}

	irq_disabled_by_diag = fts_diag_irq_disable(ts_data);
	if (!irq_disabled_by_diag) {
		reason = "irq-already-disabled";
		goto out_done;
	}
	synchronize_irq(ts_data->irq);

	if (READ_ONCE(fts_diag_poll_stop) ||
			READ_ONCE(ts_data->suspended) ||
			READ_ONCE(ts_data->fw_loading)) {
		reason = READ_ONCE(fts_diag_poll_stop) ?
			READ_ONCE(fts_diag_poll_stop_reason) : "device-not-ready";
		goto out_restore_irq;
	}

	fts_diag_read_reg_snapshot(&stats.start_regs);
	fts_diag_log_reg_snapshot("start", &stats.start_regs);
	deadline = jiffies + msecs_to_jiffies(FTS_DIAG_POLL_DURATION_MS);
	FTS_INFO("R10_DIAG FTS poll start trigger=sysfs interval_ms=%u duration_ms=%u target=%u irq=%d irq_gpio=%d",
		FTS_DIAG_POLL_INTERVAL_MS, FTS_DIAG_POLL_DURATION_MS,
		FTS_DIAG_POLL_TARGET, ts_data->irq,
		ts_data->pdata->irq_gpio);

	for (i = 0; i < FTS_DIAG_POLL_TARGET; i++) {
		if (READ_ONCE(fts_diag_poll_stop)) {
			reason = READ_ONCE(fts_diag_poll_stop_reason);
			break;
		}
		if (READ_ONCE(ts_data->suspended) ||
				READ_ONCE(ts_data->fw_loading)) {
			reason = READ_ONCE(ts_data->fw_loading) ?
				"fw-loading" : "device-not-ready";
			break;
		}
		if (time_after_eq(jiffies, deadline)) {
			reason = "complete-duration";
			break;
		}

		if (gpio_is_valid(ts_data->pdata->irq_gpio)) {
			irq_level = gpio_get_value(ts_data->pdata->irq_gpio);
			if (irq_level == 0)
				stats.gpio_low++;
			else if (irq_level == 1)
				stats.gpio_high++;
			else
				stats.gpio_invalid++;
		} else {
			stats.gpio_invalid++;
		}

		WRITE_ONCE(fts_diag_last_bus_ret, -EINPROGRESS);
		/* Existing controller read/parse/report path only; no synthetic input. */
		ret = fts_irq_read_report();
		bus_ret = READ_ONCE(fts_diag_last_bus_ret);
		complete_frame = (bus_ret == 2);
		stats.polls++;

		if (bus_ret == -EINPROGRESS) {
			stats.ret_unset++;
			stats.errors++;
		} else if (bus_ret < 0) {
			stats.ret_negative++;
			stats.io_errors++;
			stats.errors++;
		} else if (bus_ret == 0) {
			stats.ret_zero++;
			stats.io_errors++;
			stats.errors++;
		} else if (bus_ret == 1) {
			stats.ret_one++;
			stats.io_errors++;
			stats.errors++;
		} else if (bus_ret == 2) {
			stats.ret_two++;
		} else {
			stats.ret_gt_two++;
			stats.io_errors++;
			stats.errors++;
		}

		if (complete_frame) {
			stats.point_num[ts_data->point_num & 0x0F]++;
			if (fts_diag_report_is_all_ff(ts_data))
				stats.all_ff_frames++;
		}

		if (complete_frame && (ret == 0) &&
				(ts_data->touch_point > 0)) {
			stats.touch_frames++;
			stats.touch_points += ts_data->touch_point;
		} else if (complete_frame) {
			if (ts_data->point_num == 0) {
				stats.idle_frames++;
			} else {
				stats.parse_errors++;
				stats.errors++;
			}
		}

		if ((i + 1) < FTS_DIAG_POLL_TARGET)
			msleep(FTS_DIAG_POLL_INTERVAL_MS);
	}

	if (!READ_ONCE(ts_data->fw_loading) &&
			!READ_ONCE(ts_data->suspended)) {
		fts_diag_read_reg_snapshot(&stats.end_regs);
		fts_diag_log_reg_snapshot("end", &stats.end_regs);
	}

out_restore_irq:
	WRITE_ONCE(fts_diag_poll_active, false);
	fts_release_all_finger();
	if (irq_disabled_by_diag)
		fts_irq_enable();

out_done:
	FTS_INFO("R10_DIAG FTS poll done generation=%u reason=%s polls=%u ret_lt0=%u ret_0=%u ret_1=%u ret_2=%u ret_gt2=%u ret_unset=%u touch_frames=%u touch_points=%u idle_frames=%u all_ff_frames=%u errors=%u io_errors=%u parse_errors=%u gpio_high=%u gpio_low=%u gpio_invalid=%u irq_restored=%u",
		stats.generation, reason, stats.polls, stats.ret_negative,
		stats.ret_zero, stats.ret_one, stats.ret_two,
		stats.ret_gt_two, stats.ret_unset, stats.touch_frames,
		stats.touch_points, stats.idle_frames, stats.all_ff_frames,
		stats.errors, stats.io_errors, stats.parse_errors,
		stats.gpio_high, stats.gpio_low, stats.gpio_invalid,
		irq_disabled_by_diag ? 1 : 0);
	FTS_INFO("R10_DIAG point_num 0=%u 1=%u 2=%u 3=%u 4=%u 5=%u 6=%u 7=%u 8=%u 9=%u 10=%u 11=%u 12=%u 13=%u 14=%u 15=%u",
		stats.point_num[0], stats.point_num[1], stats.point_num[2],
		stats.point_num[3], stats.point_num[4], stats.point_num[5],
		stats.point_num[6], stats.point_num[7], stats.point_num[8],
		stats.point_num[9], stats.point_num[10], stats.point_num[11],
		stats.point_num[12], stats.point_num[13], stats.point_num[14],
		stats.point_num[15]);

	mutex_lock(&fts_diag_poll_control_lock);
	fts_diag_poll_last_stats = stats;
	fts_diag_poll_last_reason = reason;
	WRITE_ONCE(fts_diag_poll_active, false);
	fts_diag_poll_requested = false;
	mutex_unlock(&fts_diag_poll_control_lock);
}

static int fts_diag_poll_start(struct fts_ts_data *ts_data)
{
	bool queued;
	int ret = 0;

	mutex_lock(&fts_diag_poll_control_lock);
	if (!fts_diag_poll_initialized || !ts_data || !fts_data) {
		ret = -ENODEV;
		goto out_unlock;
	}
	if (fts_diag_poll_blocked || fts_diag_poll_canceling ||
			READ_ONCE(ts_data->suspended) ||
			READ_ONCE(ts_data->fw_loading) ||
			READ_ONCE(ts_data->irq_disabled)) {
		ret = -EBUSY;
		goto out_unlock;
	}
	if (fts_diag_poll_requested || READ_ONCE(fts_diag_poll_active)) {
		ret = -EBUSY;
		goto out_unlock;
	}

	fts_diag_poll_generation++;
	fts_diag_poll_stop_reason = "not-stopped";
	WRITE_ONCE(fts_diag_poll_stop, false);
	WRITE_ONCE(fts_diag_poll_active, false);
	fts_diag_poll_requested = true;
	queued = schedule_delayed_work(&fts_diag_poll_work, 0);
	if (!queued) {
		fts_diag_poll_requested = false;
		ret = -EBUSY;
		FTS_ERROR("R10_DIAG FTS poll schedule failed");
		goto out_unlock;
	}

	FTS_INFO("R10_DIAG FTS poll requested generation=%u duration_ms=%u interval_ms=%u target=%u",
		fts_diag_poll_generation, FTS_DIAG_POLL_DURATION_MS,
		FTS_DIAG_POLL_INTERVAL_MS, FTS_DIAG_POLL_TARGET);

out_unlock:
	mutex_unlock(&fts_diag_poll_control_lock);
	return ret;
}

static void fts_diag_poll_cancel(const char *reason)
{
	bool canceled;
	bool was_requested;
	unsigned int generation;

	mutex_lock(&fts_diag_poll_control_lock);
	if (!fts_diag_poll_initialized) {
		mutex_unlock(&fts_diag_poll_control_lock);
		return;
	}
	fts_diag_poll_canceling = true;
	was_requested = fts_diag_poll_requested;
	generation = fts_diag_poll_generation;
	fts_diag_poll_stop_reason = reason;
	WRITE_ONCE(fts_diag_poll_stop, true);
	mutex_unlock(&fts_diag_poll_control_lock);

	canceled = cancel_delayed_work_sync(&fts_diag_poll_work);

	mutex_lock(&fts_diag_poll_control_lock);
	WRITE_ONCE(fts_diag_poll_active, false);
	fts_diag_poll_requested = false;
	fts_diag_poll_canceling = false;
	if (canceled && was_requested) {
		memset(&fts_diag_poll_last_stats, 0,
			sizeof(fts_diag_poll_last_stats));
		fts_diag_poll_last_stats.generation = generation;
		fts_diag_invalidate_reg_snapshot(
			&fts_diag_poll_last_stats.start_regs);
		fts_diag_invalidate_reg_snapshot(
			&fts_diag_poll_last_stats.end_regs);
		fts_diag_poll_last_reason = reason;
	}
	mutex_unlock(&fts_diag_poll_control_lock);
	if (canceled && was_requested)
		FTS_INFO("R10_DIAG FTS poll canceled before start reason=%s",
			reason);
}

static void fts_diag_poll_set_blocked(bool blocked)
{
	mutex_lock(&fts_diag_poll_control_lock);
	fts_diag_poll_blocked = blocked;
	mutex_unlock(&fts_diag_poll_control_lock);
}

static ssize_t fts_diag_poll_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fts_diag_poll_stats stats;
	const char *reason;
	bool initialized;
	bool requested;
	bool active;
	bool blocked;
	unsigned int current_generation;
	int count = 0;

	(void)dev;
	(void)attr;
	mutex_lock(&fts_diag_poll_control_lock);
	stats = fts_diag_poll_last_stats;
	reason = fts_diag_poll_last_reason;
	initialized = fts_diag_poll_initialized;
	requested = fts_diag_poll_requested;
	active = READ_ONCE(fts_diag_poll_active);
	blocked = fts_diag_poll_blocked;
	current_generation = fts_diag_poll_generation;
	mutex_unlock(&fts_diag_poll_control_lock);

	count += scnprintf(buf + count, PAGE_SIZE - count,
		"initialized=%u requested=%u active=%u blocked=%u current_generation=%u last_generation=%u last_reason=%s\n",
		initialized, requested, active, blocked, current_generation,
		stats.generation, reason);
	count += scnprintf(buf + count, PAGE_SIZE - count,
		"polls=%u ret_lt0=%u ret_0=%u ret_1=%u ret_2=%u ret_gt2=%u ret_unset=%u touch_frames=%u touch_points=%u idle_frames=%u all_ff_frames=%u errors=%u io_errors=%u parse_errors=%u gpio_high=%u gpio_low=%u gpio_invalid=%u\n",
		stats.polls, stats.ret_negative, stats.ret_zero,
		stats.ret_one, stats.ret_two, stats.ret_gt_two,
		stats.ret_unset, stats.touch_frames, stats.touch_points,
		stats.idle_frames, stats.all_ff_frames, stats.errors,
		stats.io_errors, stats.parse_errors, stats.gpio_high,
		stats.gpio_low, stats.gpio_invalid);
	count += scnprintf(buf + count, PAGE_SIZE - count,
		"point_num=%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",
		stats.point_num[0], stats.point_num[1], stats.point_num[2],
		stats.point_num[3], stats.point_num[4], stats.point_num[5],
		stats.point_num[6], stats.point_num[7], stats.point_num[8],
		stats.point_num[9], stats.point_num[10], stats.point_num[11],
		stats.point_num[12], stats.point_num[13], stats.point_num[14],
		stats.point_num[15]);
	count += scnprintf(buf + count, PAGE_SIZE - count,
		"start=A4:%02x/%d,A5:%02x/%d,8B:%02x/%d,91:%02x/%d end=A4:%02x/%d,A5:%02x/%d,8B:%02x/%d,91:%02x/%d\n",
		stats.start_regs.int_mode, stats.start_regs.int_mode_ret,
		stats.start_regs.power_mode, stats.start_regs.power_mode_ret,
		stats.start_regs.charger_mode, stats.start_regs.charger_mode_ret,
		stats.start_regs.flow_work_cnt,
		stats.start_regs.flow_work_cnt_ret,
		stats.end_regs.int_mode, stats.end_regs.int_mode_ret,
		stats.end_regs.power_mode, stats.end_regs.power_mode_ret,
		stats.end_regs.charger_mode, stats.end_regs.charger_mode_ret,
		stats.end_regs.flow_work_cnt, stats.end_regs.flow_work_cnt_ret);

	return count;
}

static ssize_t fts_diag_poll_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct fts_ts_data *ts_data = fts_data;
	unsigned int command;
	int ret;

	(void)attr;
	ret = kstrtouint(buf, 0, &command);
	if (ret)
		return ret;
	if (!ts_data || (ts_data->dev != dev))
		return -ENODEV;

	if (command == 1) {
		ret = fts_diag_poll_start(ts_data);
		if (ret)
			return ret;
	} else if (command == 0) {
		fts_diag_poll_cancel("sysfs-stop");
	} else {
		return -EINVAL;
	}

	return count;
}

static DEVICE_ATTR(fts_diag_poll, 0600,
	fts_diag_poll_show, fts_diag_poll_store);

static int fts_diag_poll_sysfs_init(struct fts_ts_data *ts_data)
{
	int ret;

	INIT_DELAYED_WORK(&fts_diag_poll_work, fts_diag_poll_workfn);
	mutex_lock(&fts_diag_poll_control_lock);
	fts_diag_poll_initialized = true;
	fts_diag_poll_requested = false;
	WRITE_ONCE(fts_diag_poll_active, false);
	WRITE_ONCE(fts_diag_poll_stop, false);
	fts_diag_poll_blocked = false;
	fts_diag_poll_canceling = false;
	fts_diag_poll_stop_reason = "not-stopped";
	fts_diag_poll_last_reason = "never-run";
	memset(&fts_diag_poll_last_stats, 0,
		sizeof(fts_diag_poll_last_stats));
	fts_diag_invalidate_reg_snapshot(
		&fts_diag_poll_last_stats.start_regs);
	fts_diag_invalidate_reg_snapshot(
		&fts_diag_poll_last_stats.end_regs);
	mutex_unlock(&fts_diag_poll_control_lock);

	ret = device_create_file(ts_data->dev, &dev_attr_fts_diag_poll);
	if (ret) {
		mutex_lock(&fts_diag_poll_control_lock);
		fts_diag_poll_initialized = false;
		fts_diag_poll_blocked = true;
		mutex_unlock(&fts_diag_poll_control_lock);
		FTS_ERROR("R10_DIAG create fts_diag_poll failed ret=%d", ret);
		return ret;
	}

	FTS_INFO("R10_DIAG manual trigger ready: write 1 to fts_diag_poll (write 0 to cancel)");
	return 0;
}

static void fts_diag_poll_sysfs_exit(struct fts_ts_data *ts_data)
{
	fts_diag_poll_set_blocked(true);
	device_remove_file(ts_data->dev, &dev_attr_fts_diag_poll);
	fts_diag_poll_cancel("remove");
	mutex_lock(&fts_diag_poll_control_lock);
	fts_diag_poll_initialized = false;
	mutex_unlock(&fts_diag_poll_control_lock);
}

static int fts_irq_registration(struct fts_ts_data *ts_data)
{
	int ret = 0;
	struct fts_ts_platform_data *pdata = ts_data->pdata;

	ts_data->irq = gpio_to_irq(pdata->irq_gpio);
	pdata->irq_gpio_flags = IRQF_TRIGGER_FALLING;
	FTS_INFO("irq:%d, flag:%x", ts_data->irq, pdata->irq_gpio_flags);
	ret = request_threaded_irq(ts_data->irq, NULL, fts_irq_handler,
				pdata->irq_gpio_flags | IRQF_ONESHOT,
				FTS_DRIVER_NAME, ts_data);

	return ret;
}

static int fts_input_init(struct fts_ts_data *ts_data)
{
	int ret = 0;
	int key_num = 0;
	struct fts_ts_platform_data *pdata = ts_data->pdata;
	struct input_dev *input_dev;

	FTS_FUNC_ENTER();
	input_dev = input_allocate_device();
	if (!input_dev) {
		FTS_ERROR("Failed to allocate memory for input device");
		return -ENOMEM;
	}

	/* Init and register Input device */
	input_dev->name = FTS_DRIVER_NAME;

	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = ts_data->dev;

	input_set_drvdata(input_dev, ts_data);

	__set_bit(EV_SYN, input_dev->evbit);
	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(BTN_TOUCH, input_dev->keybit);
	__set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

	if (pdata->have_key) {
		FTS_INFO("set key capabilities");
		for (key_num = 0; key_num < pdata->key_number; key_num++)
			input_set_capability(input_dev, EV_KEY,
				pdata->keys[key_num]);
	}

#if FTS_MT_PROTOCOL_B_EN
	input_mt_init_slots(input_dev, pdata->max_touch_number,
			INPUT_MT_DIRECT);
#else
	input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0, 0x0F, 0, 0);
#endif
	input_set_abs_params(input_dev, ABS_MT_POSITION_X,
			pdata->x_min, pdata->x_max, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y,
			pdata->y_min, pdata->y_max, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 0xFF, 0, 0);
#if FTS_REPORT_PRESSURE_EN
	input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, 0xFF, 0, 0);
#endif

	ret = input_register_device(input_dev);
	if (ret) {
		FTS_ERROR("Input device registration failed");
		input_set_drvdata(input_dev, NULL);
		input_free_device(input_dev);
		input_dev = NULL;
		return ret;
	}

	ts_data->input_dev = input_dev;

	FTS_FUNC_EXIT();
	return 0;
}

static int fts_report_buffer_init(struct fts_ts_data *ts_data)
{
	int point_num = 0;
	int events_num = 0;

	point_num = ts_data->pdata->max_touch_number;
	ts_data->pnt_buf_size = point_num * FTS_ONE_TCH_LEN + 3;
	ts_data->point_buf = kzalloc(ts_data->pnt_buf_size + 1, GFP_KERNEL);
	if (!ts_data->point_buf) {
		FTS_ERROR("failed to alloc memory for point buf");
		return -ENOMEM;
	}

	events_num = point_num * sizeof(struct ts_event);
	ts_data->events = kzalloc(events_num, GFP_KERNEL);
	if (!ts_data->events) {
		FTS_ERROR("failed to alloc memory for point events");
		kfree_safe(ts_data->point_buf);
		return -ENOMEM;
	}

	return 0;
}

#if FTS_POWER_SOURCE_CUST_EN
/*****************************************************************************
* Power Control
*****************************************************************************/
#if FTS_PINCTRL_EN
static int fts_pinctrl_init(struct fts_ts_data *ts)
{
	int ret = 0;

	ts->pinctrl = devm_pinctrl_get(ts->dev);
	if (IS_ERR_OR_NULL(ts->pinctrl)) {
		FTS_ERROR("Failed to get pinctrl, please check dts");
		ret = PTR_ERR(ts->pinctrl);
		goto err_pinctrl_get;
	}

	ts->pins_active = pinctrl_lookup_state(ts->pinctrl, "pmx_ts_active");
	if (IS_ERR_OR_NULL(ts->pins_active)) {
		FTS_ERROR("Pin state[active] not found");
		ret = PTR_ERR(ts->pins_active);
		goto err_pinctrl_lookup;
	}

	ts->pins_suspend = pinctrl_lookup_state(ts->pinctrl, "pmx_ts_suspend");
	if (IS_ERR_OR_NULL(ts->pins_suspend)) {
		FTS_ERROR("Pin state[suspend] not found");
		ret = PTR_ERR(ts->pins_suspend);
		goto err_pinctrl_lookup;
	}

	ts->pins_release = pinctrl_lookup_state(ts->pinctrl, "pmx_ts_release");
	if (IS_ERR_OR_NULL(ts->pins_release)) {
		FTS_ERROR("Pin state[release] not found");
		ret = PTR_ERR(ts->pins_release);
	}

	return 0;
err_pinctrl_lookup:
	if (ts->pinctrl) {
		devm_pinctrl_put(ts->pinctrl);
	}
err_pinctrl_get:
	ts->pinctrl = NULL;
	ts->pins_release = NULL;
	ts->pins_suspend = NULL;
	ts->pins_active = NULL;
	return ret;
}

static int fts_pinctrl_select_normal(struct fts_ts_data *ts)
{
	int ret = 0;

	if (ts->pinctrl && ts->pins_active) {
		ret = pinctrl_select_state(ts->pinctrl, ts->pins_active);
		if (ret < 0) {
			FTS_ERROR("Set normal pin state error:%d", ret);
		}
	}

	return ret;
}

static int fts_pinctrl_select_suspend(struct fts_ts_data *ts)
{
	int ret = 0;

	if (ts->pinctrl && ts->pins_suspend) {
		ret = pinctrl_select_state(ts->pinctrl, ts->pins_suspend);
		if (ret < 0) {
			FTS_ERROR("Set suspend pin state error:%d", ret);
		}
	}

	return ret;
}

static int fts_pinctrl_select_release(struct fts_ts_data *ts)
{
	int ret = 0;

	if (ts->pinctrl) {
		if (IS_ERR_OR_NULL(ts->pins_release)) {
			devm_pinctrl_put(ts->pinctrl);
			ts->pinctrl = NULL;
		} else {
			ret = pinctrl_select_state(ts->pinctrl,
				ts->pins_release);
			if (ret < 0)
				FTS_ERROR("Set pin state error:%d", ret);
		}
	}

	return ret;
}
#endif /* FTS_PINCTRL_EN */

static int fts_power_source_ctrl(struct fts_ts_data *ts_data, int enable)
{
	int ret = 0;

	if (IS_ERR_OR_NULL(ts_data->vdd)) {
		FTS_ERROR("vdd is invalid");
		return -EINVAL;
	}

	FTS_FUNC_ENTER();
	if (enable) {
		if (ts_data->power_disabled) {
			FTS_DEBUG("regulator enable !");
			gpio_direction_output(ts_data->pdata->reset_gpio, 0);
			msleep(1);
			ret = regulator_enable(ts_data->vdd);
			if (ret) {
				FTS_ERROR("enable vdd regulator failed");
			}

			if (!IS_ERR_OR_NULL(ts_data->vcc_i2c)) {
				ret = regulator_enable(ts_data->vcc_i2c);
				if (ret) {
					FTS_ERROR("enable vcc_i2c failed");
				}
			}
			ts_data->power_disabled = false;
		}
	} else {
		if (!ts_data->power_disabled) {
			FTS_DEBUG("regulator disable !");
			gpio_direction_output(ts_data->pdata->reset_gpio, 0);
			msleep(1);
			ret = regulator_disable(ts_data->vdd);
			if (ret) {
				FTS_ERROR("disable vdd regulator failed");
			}
			if (!IS_ERR_OR_NULL(ts_data->vcc_i2c)) {
				ret = regulator_disable(ts_data->vcc_i2c);
				if (ret) {
					FTS_ERROR("disable vcc_i2c failed");
				}
			}
			ts_data->power_disabled = true;
		}
	}

	FTS_FUNC_EXIT();
	return ret;
}

/*****************************************************************************
* Name: fts_power_source_init
* Brief: Init regulator power:vdd/vcc_io(if have), generally, no vcc_io
*        vdd---->vdd-supply in dts, kernel will auto add "-supply" to parse
*        Must be call after fts_gpio_configure() execute,because this function
*        will operate reset-gpio which request gpio in fts_gpio_configure()
* Input:
* Output:
* Return: return 0 if init power successfully, otherwise return error code
*****************************************************************************/
static int fts_power_source_init(struct fts_ts_data *ts_data)
{
	int ret = 0;

	FTS_FUNC_ENTER();
	ts_data->vdd = regulator_get(ts_data->dev, "vdd");
	if (IS_ERR_OR_NULL(ts_data->vdd)) {
		ret = PTR_ERR(ts_data->vdd);
		FTS_ERROR("get vdd regulator failed,ret=%d", ret);
		return ret;
	}

	if (regulator_count_voltages(ts_data->vdd) > 0) {
		ret = regulator_set_voltage(ts_data->vdd, FTS_VTG_MIN_UV,
						FTS_VTG_MAX_UV);
		if (ret) {
			FTS_ERROR("vdd regulator set_vtg failed ret=%d", ret);
			regulator_put(ts_data->vdd);
			return ret;
		}

		ret = regulator_set_load(ts_data->vdd, FTS_LOAD_MAX_UA);
		if (ret) {
			FTS_ERROR("vdd regulator set_load failed ret=%d", ret);
			regulator_put(ts_data->vdd);
			return ret;
		}
	}

	ts_data->vcc_i2c = regulator_get(ts_data->dev, "vcc_i2c");
	if (!IS_ERR_OR_NULL(ts_data->vcc_i2c)) {
		if (regulator_count_voltages(ts_data->vcc_i2c) > 0) {
			ret = regulator_set_voltage(ts_data->vcc_i2c,
						FTS_I2C_VTG_MIN_UV,
						FTS_I2C_VTG_MAX_UV);
			if (ret) {
				FTS_ERROR("vcc_i2c set_vtg failed,ret=%d", ret);
				regulator_put(ts_data->vcc_i2c);
			}
		}
	}

#if FTS_PINCTRL_EN
	fts_pinctrl_init(ts_data);
	fts_pinctrl_select_normal(ts_data);
#endif

	ts_data->power_disabled = true;
	ret = fts_power_source_ctrl(ts_data, ENABLE);
	if (ret) {
		FTS_ERROR("fail to enable power(regulator)");
	}

	FTS_FUNC_EXIT();
	return ret;
}

static int fts_power_source_exit(struct fts_ts_data *ts_data)
{
#if FTS_PINCTRL_EN
	fts_pinctrl_select_release(ts_data);
#endif

	fts_power_source_ctrl(ts_data, DISABLE);

	if (!IS_ERR_OR_NULL(ts_data->vdd)) {
		if (regulator_count_voltages(ts_data->vdd) > 0)
			regulator_set_voltage(ts_data->vdd, 0, FTS_VTG_MAX_UV);
		regulator_put(ts_data->vdd);
	}

	if (!IS_ERR_OR_NULL(ts_data->vcc_i2c)) {
		if (regulator_count_voltages(ts_data->vcc_i2c) > 0)
			regulator_set_voltage(ts_data->vcc_i2c, 0,
					FTS_I2C_VTG_MAX_UV);
		regulator_put(ts_data->vcc_i2c);
	}

	return 0;
}

static int fts_power_source_suspend(struct fts_ts_data *ts_data)
{
	int ret = 0;

#if FTS_PINCTRL_EN
	fts_pinctrl_select_suspend(ts_data);
#endif

	ret = fts_power_source_ctrl(ts_data, DISABLE);
	if (ret < 0) {
		FTS_ERROR("power off fail, ret=%d", ret);
	}

	return ret;
}

static int fts_power_source_resume(struct fts_ts_data *ts_data)
{
	int ret = 0;

#if FTS_PINCTRL_EN
	fts_pinctrl_select_normal(ts_data);
#endif

	ret = fts_power_source_ctrl(ts_data, ENABLE);
	if (ret < 0) {
		FTS_ERROR("power on fail, ret=%d", ret);
	}

	return ret;
}
#endif /* FTS_POWER_SOURCE_CUST_EN */

static int fts_gpio_configure(struct fts_ts_data *data)
{
	int ret = 0;

	FTS_FUNC_ENTER();
	/* request irq gpio */
	if (gpio_is_valid(data->pdata->irq_gpio)) {
		ret = gpio_request(data->pdata->irq_gpio, "fts_irq_gpio");
		if (ret) {
			FTS_ERROR("[GPIO]irq gpio request failed");
			goto err_irq_gpio_req;
		}

		ret = gpio_direction_input(data->pdata->irq_gpio);
		if (ret) {
			FTS_ERROR("[GPIO]set_direction for irq gpio failed");
			goto err_irq_gpio_dir;
		}
	}

	/* request reset gpio */
	if (gpio_is_valid(data->pdata->reset_gpio)) {
		ret = gpio_request(data->pdata->reset_gpio, "fts_reset_gpio");
		if (ret) {
			FTS_ERROR("[GPIO]reset gpio request failed");
			goto err_irq_gpio_dir;
		}

		ret = gpio_direction_output(data->pdata->reset_gpio, 1);
		if (ret) {
			FTS_ERROR("[GPIO]set_direction for reset gpio failed");
			goto err_reset_gpio_dir;
		}
	}

	FTS_FUNC_EXIT();
	return 0;

err_reset_gpio_dir:
	if (gpio_is_valid(data->pdata->reset_gpio))
		gpio_free(data->pdata->reset_gpio);
err_irq_gpio_dir:
	if (gpio_is_valid(data->pdata->irq_gpio))
		gpio_free(data->pdata->irq_gpio);
err_irq_gpio_req:
	FTS_FUNC_EXIT();
	return ret;
}

static int fts_get_dt_coords(struct device *dev, char *name,
				struct fts_ts_platform_data *pdata)
{
	int ret = 0;
	u32 coords[FTS_COORDS_ARR_SIZE] = { 0 };
	struct property *prop;
	struct device_node *np = dev->of_node;
	int coords_size;

	prop = of_find_property(np, name, NULL);
	if (!prop)
		return -EINVAL;
	if (!prop->value)
		return -ENODATA;

	coords_size = prop->length / sizeof(u32);
	if (coords_size != FTS_COORDS_ARR_SIZE) {
		FTS_ERROR("invalid:%s, size:%d", name, coords_size);
		return -EINVAL;
	}

	ret = of_property_read_u32_array(np, name, coords, coords_size);
	if (ret < 0) {
		FTS_ERROR("Unable to read %s, please check dts", name);
		pdata->x_min = FTS_X_MIN_DISPLAY_DEFAULT;
		pdata->y_min = FTS_Y_MIN_DISPLAY_DEFAULT;
		pdata->x_max = FTS_X_MAX_DISPLAY_DEFAULT;
		pdata->y_max = FTS_Y_MAX_DISPLAY_DEFAULT;
		return -ENODATA;
	} else {
		pdata->x_min = coords[0];
		pdata->y_min = coords[1];
		pdata->x_max = coords[2];
		pdata->y_max = coords[3];
	}

	FTS_INFO("display x(%d %d) y(%d %d)", pdata->x_min, pdata->x_max,
		pdata->y_min, pdata->y_max);
	return 0;
}

static int fts_parse_dt(struct device *dev, struct fts_ts_platform_data *pdata)
{
	int ret = 0;
	struct device_node *np = dev->of_node;
	u32 temp_val = 0;

	FTS_FUNC_ENTER();

	ret = fts_get_dt_coords(dev, "focaltech,display-coords", pdata);
	if (ret < 0)
		FTS_ERROR("Unable to get display-coords");

	/* key */
	pdata->have_key = of_property_read_bool(np, "focaltech,have-key");
	if (pdata->have_key) {
		ret = of_property_read_u32(np, "focaltech,key-number",
					&pdata->key_number);
		if (ret < 0)
			FTS_ERROR("Key number undefined!");

		ret = of_property_read_u32_array(np, "focaltech,keys",
						pdata->keys, pdata->key_number);
		if (ret < 0)
			FTS_ERROR("Keys undefined!");
		else if (pdata->key_number > FTS_MAX_KEYS)
			pdata->key_number = FTS_MAX_KEYS;

		ret = of_property_read_u32_array(np, "focaltech,key-x-coords",
						pdata->key_x_coords,
						pdata->key_number);
		if (ret < 0)
			FTS_ERROR("Key Y Coords undefined!");

		ret = of_property_read_u32_array(np, "focaltech,key-y-coords",
						pdata->key_y_coords,
						pdata->key_number);
		if (ret < 0)
			FTS_ERROR("Key X Coords undefined!");

		FTS_INFO("VK Number:%d, key:(%d,%d,%d), "
			"coords:(%d,%d),(%d,%d),(%d,%d)",
			pdata->key_number,
			pdata->keys[0], pdata->keys[1], pdata->keys[2],
			pdata->key_x_coords[0], pdata->key_y_coords[0],
			pdata->key_x_coords[1], pdata->key_y_coords[1],
			pdata->key_x_coords[2], pdata->key_y_coords[2]);
	}

	/* reset, irq gpio info */
	pdata->reset_gpio = of_get_named_gpio_flags(np, "focaltech,reset-gpio",
			0, &pdata->reset_gpio_flags);
	if (!gpio_is_valid(pdata->reset_gpio))
		FTS_ERROR("Unable to get reset_gpio");

	pdata->irq_gpio = of_get_named_gpio_flags(np, "focaltech,irq-gpio",
			0, &pdata->irq_gpio_flags);
	if (!gpio_is_valid(pdata->irq_gpio))
		FTS_ERROR("Unable to get irq_gpio");

	ret = of_property_read_u32(np, "focaltech,max-touch-number", &temp_val);
	if (ret < 0) {
		FTS_ERROR("Unable to get max-touch-number, please check dts");
		pdata->max_touch_number = FTS_MAX_POINTS_SUPPORT;
	} else {
		/* max_touch_number must >= 2 */
		if (temp_val < 2)
			pdata->max_touch_number = 2;
		else if (temp_val > FTS_MAX_POINTS_SUPPORT)
			pdata->max_touch_number = FTS_MAX_POINTS_SUPPORT;
		else
			pdata->max_touch_number = temp_val;
	}

	FTS_INFO("max touch number:%d, irq gpio:%d, reset gpio:%d",
		pdata->max_touch_number, pdata->irq_gpio, pdata->reset_gpio);

	FTS_FUNC_EXIT();
	return 0;
}

#if defined(CONFIG_DRM)
static void fts_resume_work(struct work_struct *work)
{
	struct fts_ts_data *ts_data = container_of(work, struct fts_ts_data,
					resume_work);

	fts_ts_resume(ts_data->dev);
}

static int fb_notifier_callback(struct notifier_block *self,
				unsigned long event, void *data)
{
	struct drm_panel_notifier *evdata = data;
	int *blank = NULL;
	struct fts_ts_data *ts_data = container_of(self, struct fts_ts_data,
			fb_notif);

	if (!evdata)
		return 0;

	if (!(event == DRM_PANEL_EARLY_EVENT_BLANK ||
		event == DRM_PANEL_EVENT_BLANK)) {
		FTS_INFO("event(%lu) do not need process\n", event);
		return 0;
	}

	blank = evdata->data;
	FTS_DEBUG("FB event:%lu,blank:%d", event, *blank);
	switch (*blank) {
	case DRM_PANEL_BLANK_UNBLANK:
		if (event == DRM_PANEL_EARLY_EVENT_BLANK) {
			FTS_DEBUG("resume: event = %lu, not care\n", event);
		} else if (event == DRM_PANEL_EVENT_BLANK) {
			queue_work(fts_data->ts_workqueue,
				&fts_data->resume_work);
		}
		break;

	case DRM_PANEL_BLANK_POWERDOWN:
		if (event == DRM_PANEL_EARLY_EVENT_BLANK) {
			cancel_work_sync(&fts_data->resume_work);
			fts_ts_suspend(ts_data->dev);
		} else if (event == DRM_PANEL_EVENT_BLANK) {
			FTS_DEBUG("suspend: event = %lu, not care\n", event);
		}
		break;

	default:
		FTS_DEBUG("FB BLANK(%d) do not need process\n", *blank);
		break;
	}

	return 0;
}

#elif defined(CONFIG_FB)
static void fts_resume_work(struct work_struct *work)
{
	struct fts_ts_data *ts_data = container_of(work, struct fts_ts_data,
					resume_work);

	fts_ts_resume(ts_data->dev);
}

static int fb_notifier_callback(struct notifier_block *self,
				unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank = NULL;
	struct fts_ts_data *ts_data = container_of(self, struct fts_ts_data,
					fb_notif);

	if (!(event == FB_EARLY_EVENT_BLANK || event == FB_EVENT_BLANK)) {
		FTS_INFO("event(%lu) do not need process\n", event);
		return 0;
	}

	blank = evdata->data;
	FTS_INFO("FB event:%lu,blank:%d", event, *blank);
	switch (*blank) {
	case FB_BLANK_UNBLANK:
		if (FB_EARLY_EVENT_BLANK == event) {
			FTS_INFO("resume: event = %lu, not care\n", event);
		} else if (FB_EVENT_BLANK == event) {
			queue_work(fts_data->ts_workqueue,
				&fts_data->resume_work);
		}
		break;

	case FB_BLANK_POWERDOWN:
		if (FB_EARLY_EVENT_BLANK == event) {
			cancel_work_sync(&fts_data->resume_work);
			fts_ts_suspend(ts_data->dev);
		} else if (FB_EVENT_BLANK == event) {
			FTS_INFO("suspend: event = %lu, not care\n", event);
		}
		break;

	default:
		FTS_INFO("FB BLANK(%d) do not need process\n", *blank);
		break;
	}

	return 0;
}
#elif defined(CONFIG_HAS_EARLYSUSPEND)
static void fts_ts_early_suspend(struct early_suspend *handler)
{
	struct fts_ts_data *ts_data = container_of(handler, struct fts_ts_data,
					early_suspend);

	fts_ts_suspend(ts_data->dev);
}

static void fts_ts_late_resume(struct early_suspend *handler)
{
	struct fts_ts_data *ts_data = container_of(handler, struct fts_ts_data,
					early_suspend);

	fts_ts_resume(ts_data->dev);
}
#endif

static int fts_ts_probe_entry(struct fts_ts_data *ts_data)
{
	int ret = 0;
	int pdata_size = sizeof(struct fts_ts_platform_data);

	FTS_FUNC_ENTER();
	FTS_INFO("%s", FTS_DRIVER_VERSION);
	ts_data->pdata = kzalloc(pdata_size, GFP_KERNEL);
	if (!ts_data->pdata) {
		FTS_ERROR("allocate memory for platform_data fail");
		return -ENOMEM;
	}

	if (ts_data->dev->of_node) {
		ret = fts_parse_dt(ts_data->dev, ts_data->pdata);
		if (ret)
			FTS_ERROR("device-tree parse fail");
	} else {
		if (!ts_data->dev->platform_data) {
			FTS_ERROR("platform_data is null");
			return -ENODEV;
		}
		memcpy(ts_data->pdata, ts_data->dev->platform_data, pdata_size);
	}

	ts_data->ts_workqueue = create_singlethread_workqueue("fts_wq");
	if (!ts_data->ts_workqueue) {
		FTS_ERROR("create fts workqueue fail");
	}

	spin_lock_init(&ts_data->irq_lock);
	mutex_init(&ts_data->report_mutex);
	mutex_init(&ts_data->bus_lock);

	/* Init communication interface */
	ret = fts_bus_init(ts_data);
	if (ret) {
		FTS_ERROR("bus initialize fail");
		goto err_bus_init;
	}

	ret = fts_input_init(ts_data);
	if (ret) {
		FTS_ERROR("input initialize fail");
		goto err_input_init;
	}

	ret = fts_report_buffer_init(ts_data);
	if (ret) {
		FTS_ERROR("report buffer init fail");
		goto err_report_buffer;
	}

	ret = fts_gpio_configure(ts_data);
	if (ret) {
		FTS_ERROR("configure the gpios fail");
		goto err_gpio_config;
	}

#if FTS_POWER_SOURCE_CUST_EN
	ret = fts_power_source_init(ts_data);
	if (ret) {
		FTS_ERROR("fail to get power(regulator)");
		goto err_power_init;
	}
#endif

#if (!FTS_CHIP_IDC)
	fts_reset_proc(200);
#endif

	ret = fts_get_ic_information(ts_data);
	if (ret) {
		FTS_ERROR("not focal IC, unregister driver");
		goto err_irq_req;
	}

	/* 原厂在 chip id 确定后才等 TP 就绪（日志 "TP Ready, Device ID:0x%02x%02x"）。
	 * 放在 get_ic_information 之前时期望值还是 0，永远等不到。
	 */
	fts_wait_tp_to_valid();

#if IS_ENABLED(CONFIG_TOUCHSCREEN_FTS_FW_UPDATE)
	ret = fts_create_apk_debug_channel(ts_data);
	if (ret) {
		FTS_ERROR("create apk debug node fail");
	}

	ret = fts_create_sysfs(ts_data);
	if (ret) {
		FTS_ERROR("create sysfs node fail");
	}
#else
	/* The normal probe still performs reset and operational register writes.
	 * This guard only guarantees that no flash/LIC/debug write path is made
	 * reachable during first-round bring-up.
	 */
	FTS_INFO("flash-read-only probe: FW/LIC/debug writes disabled");
#endif

#if FTS_POINT_REPORT_CHECK_EN
	ret = fts_point_report_check_init(ts_data);
	if (ret) {
		FTS_ERROR("init point report check fail");
	}
#endif

	ret = fts_ex_mode_init(ts_data);
	if (ret) {
		FTS_ERROR("init glove/cover/charger fail");
	}

	ret = fts_gesture_init(ts_data);
	if (ret) {
		FTS_ERROR("init gesture fail");
	}


#if FTS_ESDCHECK_EN
	ret = fts_esdcheck_init(ts_data);
	if (ret) {
		FTS_ERROR("init esd check fail");
	}
#endif

	ret = fts_irq_registration(ts_data);
	if (ret) {
		FTS_ERROR("request irq failed");
		goto err_irq_req;
	}
	fts_oem_runtime_mode_apply(ts_data, "probe");

#if IS_ENABLED(CONFIG_TOUCHSCREEN_FTS_FW_UPDATE)
	ret = fts_fwupg_init(ts_data);
	if (ret) {
		FTS_ERROR("init fw upgrade fail");
	}
#endif

	ret = fts_diag_poll_sysfs_init(ts_data);
	if (ret)
		FTS_ERROR("R10_DIAG manual sysfs unavailable ret=%d", ret);

#if defined(CONFIG_DRM)
	if (ts_data->ts_workqueue) {
		INIT_WORK(&ts_data->resume_work, fts_resume_work);
	}
	ts_data->fb_notif.notifier_call = fb_notifier_callback;

	if (active_panel &&
		drm_panel_notifier_register(active_panel,
			&ts_data->fb_notif) < 0)
		FTS_ERROR("register notifier failed!\n");

#elif defined(CONFIG_FB)
	if (ts_data->ts_workqueue) {
		INIT_WORK(&ts_data->resume_work, fts_resume_work);
	}
	ts_data->fb_notif.notifier_call = fb_notifier_callback;
	ret = fb_register_client(&ts_data->fb_notif);
	if (ret) {
		FTS_ERROR("[FB]Unable to register fb_notifier: %d", ret);
	}
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	ts_data->early_suspend.level = 
		EARLY_SUSPEND_LEVEL_BLANK_SCREEN + FTS_SUSPEND_LEVEL;
	ts_data->early_suspend.suspend = fts_ts_early_suspend;
	ts_data->early_suspend.resume = fts_ts_late_resume;
	register_early_suspend(&ts_data->early_suspend);
#endif

	FTS_FUNC_EXIT();
	return 0;

err_irq_req:
#if FTS_POWER_SOURCE_CUST_EN
err_power_init:
	fts_power_source_exit(ts_data);
#endif
	if (gpio_is_valid(ts_data->pdata->reset_gpio))
		gpio_free(ts_data->pdata->reset_gpio);
	if (gpio_is_valid(ts_data->pdata->irq_gpio))
		gpio_free(ts_data->pdata->irq_gpio);
err_gpio_config:
	kfree_safe(ts_data->point_buf);
	kfree_safe(ts_data->events);
err_report_buffer:
	input_unregister_device(ts_data->input_dev);
err_input_init:
	if (ts_data->ts_workqueue)
		destroy_workqueue(ts_data->ts_workqueue);
err_bus_init:
	kfree_safe(ts_data->bus_tx_buf);
	kfree_safe(ts_data->bus_rx_buf);
	kfree_safe(ts_data->pdata);

	FTS_FUNC_EXIT();
	return ret;
}

static int fts_ts_remove_entry(struct fts_ts_data *ts_data)
{
	FTS_FUNC_ENTER();
	fts_diag_poll_sysfs_exit(ts_data);

#if FTS_POINT_REPORT_CHECK_EN
	fts_point_report_check_exit(ts_data);
#endif

#if IS_ENABLED(CONFIG_TOUCHSCREEN_FTS_FW_UPDATE)
	fts_release_apk_debug_channel(ts_data);
	fts_remove_sysfs(ts_data);
#endif
	fts_ex_mode_exit(ts_data);

#if IS_ENABLED(CONFIG_TOUCHSCREEN_FTS_FW_UPDATE)
	fts_fwupg_exit(ts_data);
#endif


#if FTS_ESDCHECK_EN
	fts_esdcheck_exit(ts_data);
#endif

	fts_gesture_exit(ts_data);
	fts_bus_exit(ts_data);

	free_irq(ts_data->irq, ts_data);
	input_unregister_device(ts_data->input_dev);

	if (ts_data->ts_workqueue)
		destroy_workqueue(ts_data->ts_workqueue);

#if defined(CONFIG_DRM)
	if (active_panel)
		drm_panel_notifier_unregister(active_panel, &ts_data->fb_notif);

#elif defined(CONFIG_FB)
	if (fb_unregister_client(&ts_data->fb_notif))
		FTS_ERROR("Error occurred while unregistering fb_notifier.");
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&ts_data->early_suspend);
#endif

	if (gpio_is_valid(ts_data->pdata->reset_gpio))
		gpio_free(ts_data->pdata->reset_gpio);

	if (gpio_is_valid(ts_data->pdata->irq_gpio))
		gpio_free(ts_data->pdata->irq_gpio);

#if FTS_POWER_SOURCE_CUST_EN
	fts_power_source_exit(ts_data);
#endif

	kfree_safe(ts_data->point_buf);
	kfree_safe(ts_data->events);

	kfree_safe(ts_data->pdata);
	kfree_safe(ts_data);

	FTS_FUNC_EXIT();

	return 0;
}

static int fts_ts_suspend(struct device *dev)
{
	int ret = 0;
	struct fts_ts_data *ts_data = fts_data;

	FTS_FUNC_ENTER();
	fts_diag_poll_set_blocked(true);
	fts_diag_poll_cancel("suspend");
	if (ts_data->suspended) {
		FTS_INFO("Already in suspend state");
		return 0;
	}

	if (ts_data->fw_loading) {
		FTS_INFO("fw upgrade in process, can't suspend");
		fts_diag_poll_set_blocked(false);
		return 0;
	}

#if FTS_ESDCHECK_EN
	fts_esdcheck_suspend();
#endif

	if (ts_data->gesture_mode) {
		fts_gesture_suspend(ts_data);
	} else {
		fts_irq_disable();

		FTS_INFO("make TP enter into sleep mode");
		ret = fts_write_reg(FTS_REG_POWER_MODE,
				FTS_REG_POWER_MODE_SLEEP);
		if (ret < 0)
			FTS_ERROR("set TP to sleep mode fail, ret=%d", ret);

		if (!ts_data->ic_info.is_incell) {
#if FTS_POWER_SOURCE_CUST_EN
			ret = fts_power_source_suspend(ts_data);
			if (ret < 0) {
				FTS_ERROR("power enter suspend fail");
			}
#endif
		}
	}

	fts_release_all_finger();
	ts_data->suspended = true;
	FTS_FUNC_EXIT();
	return 0;
}

static int fts_ts_resume(struct device *dev)
{
	struct fts_ts_data *ts_data = fts_data;

	FTS_FUNC_ENTER();
	if (!ts_data->suspended) {
		FTS_DEBUG("Already in awake state");
		fts_diag_poll_set_blocked(false);
		return 0;
	}

	fts_release_all_finger();

	if (!ts_data->ic_info.is_incell) {
#if FTS_POWER_SOURCE_CUST_EN
		fts_power_source_resume(ts_data);
#endif
		fts_reset_proc(200);
	}

	fts_wait_tp_to_valid();
	fts_ex_mode_recovery(ts_data);
	fts_oem_runtime_mode_apply(ts_data, "resume");

#if FTS_ESDCHECK_EN
	fts_esdcheck_resume();
#endif

	if (ts_data->gesture_mode) {
		fts_gesture_resume(ts_data);
	} else {
		fts_irq_enable();
	}

	ts_data->suspended = false;
	fts_diag_poll_set_blocked(false);
	FTS_FUNC_EXIT();
	return 0;
}

/*****************************************************************************
* TP Driver
*****************************************************************************/
static int fts_ts_check_dt(struct device_node *np)
{
	int i;
	int count;
	struct device_node *node;
	struct drm_panel *panel;

	count = of_count_phandle_with_args(np, "panel", NULL);
	if (count <= 0)
		return 0;

	for (i = 0; i < count; i++) {
		node = of_parse_phandle(np, "panel", i);
		panel = of_drm_find_panel(node);
		of_node_put(node);
		if (!IS_ERR(panel)) {
			active_panel = panel;
			return 0;
		}
	}

	return -ENODEV;
}

static int fts_ts_check_default_tp(struct device_node *dt, const char *prop)
{
	const char **active_tp = NULL;
	int count, tmp, score = 0;
	const char *active;
	int ret, i;

	count = of_property_count_strings(dt->parent, prop);
	if (count <= 0 || count > 3)
		return -ENODEV;

	active_tp = kcalloc(count, sizeof(char *),  GFP_KERNEL);
	if (!active_tp) {
		FTS_ERROR("FTS alloc failed\n");
		return -ENOMEM;
	}

	ret = of_property_read_string_array(dt->parent, prop,
			active_tp, count);
	if (ret < 0) {
		FTS_ERROR("fail to read %s %d\n", prop, ret);
		ret = -ENODEV;
		goto out;
	}

	for (i = 0; i < count; i++) {
		active = active_tp[i];
		if (active != NULL) {
			tmp = of_device_is_compatible(dt, active);
			if (tmp > 0)
				score++;
		}
	}

	if (score <= 0) {
		FTS_INFO("not match this driver\n");
		ret = -ENODEV;
		goto out;
	}
	ret = 0;
out:
	kfree(active_tp);
	return ret;
}

static int fts_ts_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int ret = 0;
	struct fts_ts_data *ts_data = NULL;
	struct device_node *dp = client->dev.of_node;

	FTS_INFO("Touch Screen(I2C BUS) driver prboe...");
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		FTS_ERROR("I2C not supported");
		return -ENODEV;
	}

	if (fts_ts_check_dt(dp)) {
		if (!fts_ts_check_default_tp(dp, "qcom,i2c-touch-active"))
			ret = -EPROBE_DEFER;
		else
			ret = -ENODEV;

		return ret;
	}

	/* malloc memory for global struct variable */
	ts_data = kzalloc(sizeof(*ts_data), GFP_KERNEL);
	if (!ts_data) {
		FTS_ERROR("allocate memory for fts_data fail");
		return -ENOMEM;
	}

	fts_data = ts_data;
	ts_data->client = client;
	ts_data->dev = &client->dev;
	ts_data->log_level = 1;
	ts_data->fw_is_running = 0;
	i2c_set_clientdata(client, ts_data);

	ret = fts_ts_probe_entry(ts_data);
	if (ret) {
		FTS_ERROR("Touch Screen(I2C BUS) driver probe fail");
		kfree_safe(ts_data);
		return ret;
	}

	FTS_INFO("Touch Screen(I2C BUS) driver prboe successfully");
	return 0;
}

static int fts_ts_remove(struct i2c_client *client)
{
	return fts_ts_remove_entry(i2c_get_clientdata(client));
}

static const struct i2c_device_id fts_ts_id[] = {
	{FTS_DRIVER_NAME, 0},
	{},
};
static const struct of_device_id fts_dt_match[] = {
	{.compatible = "focaltech,fts", },
    {.compatible = "focaltech,fts_ts", },
    {},
};
MODULE_DEVICE_TABLE(of, fts_dt_match);

static struct i2c_driver fts_ts_driver = {
	.probe = fts_ts_probe,
	.remove = fts_ts_remove,
	.driver = {
		.name = FTS_DRIVER_NAME,
		.of_match_table = of_match_ptr(fts_dt_match),
	},
	.id_table = fts_ts_id,
};

static int __init fts_ts_init(void)
{
	int ret = 0;

	FTS_FUNC_ENTER();
	ret = i2c_add_driver(&fts_ts_driver);
	if ( ret != 0 ) {
		FTS_ERROR("Focaltech touch screen driver init failed!");
	}
	FTS_FUNC_EXIT();
	return ret;
}

static void __exit fts_ts_exit(void)
{
	i2c_del_driver(&fts_ts_driver);
}
late_initcall(fts_ts_init);
module_exit(fts_ts_exit);

MODULE_AUTHOR("FocalTech Driver Team");
MODULE_DESCRIPTION("FocalTech Touchscreen Driver");
MODULE_LICENSE("GPL v2");
