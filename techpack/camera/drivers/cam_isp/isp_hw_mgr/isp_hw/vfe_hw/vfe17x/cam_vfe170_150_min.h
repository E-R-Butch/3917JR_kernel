/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Minimal VFE170_150 data for the legacy camera-kernel implementation.
 *
 * The VFE170 and VFE170_150 active register maps are identical for the
 * first-round CAMIF/RDI and BUS paths used here.  Keep the shared register
 * tables, but provide the IRQ data that the legacy IRQ-controller based
 * CAMIF/RDI implementation dereferences at stream start.
 */

#ifndef _CAM_VFE170_150_MIN_H_
#define _CAM_VFE170_150_MIN_H_

static struct cam_vfe_camif_reg_data vfe_170_150_camif_reg_data = {
	.raw_crop_first_pixel_shift      = 16,
	.raw_crop_first_pixel_mask       = 0xFFFF,
	.raw_crop_last_pixel_shift       = 0x0,
	.raw_crop_last_pixel_mask        = 0x3FFF,
	.raw_crop_first_line_shift       = 16,
	.raw_crop_first_line_mask        = 0xFFFF,
	.raw_crop_last_line_shift        = 0,
	.raw_crop_last_line_mask         = 0x3FFF,
	.input_mux_sel_shift             = 5,
	.input_mux_sel_mask              = 0x3,
	.extern_reg_update_shift         = 4,
	.extern_reg_update_mask          = 1,
	.pixel_pattern_shift             = 0,
	.pixel_pattern_mask              = 0x7,
	.dsp_mode_shift                  = 23,
	.dsp_mode_mask                   = 0x1,
	.dsp_en_shift                    = 3,
	.dsp_en_mask                     = 0x1,
	.reg_update_cmd_data             = 0x1,
	.epoch_line_cfg                  = 0x00140014,
	.sof_irq_mask                    = 0x00000001,
	.epoch0_irq_mask                 = 0x00000004,
	.reg_update_irq_mask             = 0x00000010,
	.eof_irq_mask                    = 0x00000002,
	.error_irq_mask0                 = 0x0003FC00,
	.error_irq_mask1                 = 0x0FFF7E80,
	.subscribe_irq_mask0             = 0x00000017,
	.subscribe_irq_mask1             = 0x00000000,
	.enable_diagnostic_hw            = 0x1,
};

static struct cam_vfe_rdi_common_reg_data vfe170_150_rdi_reg_data = {
	.subscribe_irq_mask0 = 0x780001E0,
	.subscribe_irq_mask1 = 0x00000000,
	.error_irq_mask0     = 0x00000000,
	.error_irq_mask1     = 0x0000003C,
};

static struct cam_vfe_top_ver2_hw_info vfe170_150_top_hw_info = {
	.common_reg = &vfe170_top_common_reg,
	.camif_hw_info = {
		.common_reg = &vfe170_top_common_reg,
		.camif_reg  = &vfe170_camif_reg,
		.reg_data   = &vfe_170_150_camif_reg_data,
	},
	.camif_lite_hw_info = {
		.common_reg     = NULL,
		.camif_lite_reg = NULL,
		.reg_data       = NULL,
	},
	.rdi_hw_info = {
		.common_reg      = &vfe170_top_common_reg,
		.rdi_reg         = &vfe170_rdi_reg,
		.common_reg_data = &vfe170_150_rdi_reg_data,
		.reg_data = {
			&vfe_170_rdi_0_data,
			&vfe_170_rdi_1_data,
			&vfe_170_rdi_2_data,
			NULL,
		},
	},
	.num_mux = 4,
	.mux_type = {
		CAM_VFE_CAMIF_VER_2_0,
		CAM_VFE_RDI_VER_1_0,
		CAM_VFE_RDI_VER_1_0,
		CAM_VFE_RDI_VER_1_0,
	},
};

static struct cam_vfe_hw_info cam_vfe170_150_hw_info = {
	.irq_reg_info       = &vfe170_top_irq_reg_info,
	.bus_version        = CAM_VFE_BUS_VER_2_0,
	.bus_hw_info        = &vfe170_bus_hw_info,
	.top_version        = CAM_VFE_TOP_VER_2_0,
	.top_hw_info        = &vfe170_150_top_hw_info,
	.camif_version      = CAM_VFE_CAMIF_VER_2_0,
	.camif_reg          = &vfe170_camif_reg,
	.camif_lite_version = 0,
	.camif_lite_reg     = NULL,
};

#endif /* _CAM_VFE170_150_MIN_H_ */
