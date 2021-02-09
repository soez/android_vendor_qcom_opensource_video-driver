// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/of.h>

#include "msm_vidc_waipio.h"
#include "msm_vidc_platform.h"
#include "msm_vidc_debug.h"
#include "msm_vidc_internal.h"
#include "msm_vidc_core.h"
#include "msm_vidc_control.h"
#include "hfi_property.h"

#define DDR_TYPE_LPDDR4 0x6
#define DDR_TYPE_LPDDR4X 0x7
#define DDR_TYPE_LPDDR5 0x8
#define DDR_TYPE_LPDDR5X 0x9
#define DEFAULT_VIDEO_CONCEAL_COLOR_BLACK 0x8020010
#define MAX_LTR_FRAME_COUNT     2
#define MAX_BASE_LAYER_PRIORITY_ID 63
#define MIN_CHROMA_QP_OFFSET    -12
#define MAX_CHROMA_QP_OFFSET    0
#define MAX_BITRATE             220000000
#define MIN_QP_10BIT            -12
#define MIN_QP_8BIT             0
#define MAX_QP                  51
#define DEFAULT_QP              20
#define MAX_CONSTANT_QUALITY    100

#define UBWC_CONFIG(mc, ml, hbb, bs1, bs2, bs3, bsp) \
{	\
	.max_channels = mc,	\
	.mal_length = ml,	\
	.highest_bank_bit = hbb,	\
	.bank_swzl_level = bs1,	\
	.bank_swz2_level = bs2, \
	.bank_swz3_level = bs3, \
	.bank_spreading = bsp,	\
}

#define ENC     MSM_VIDC_ENCODER
#define DEC     MSM_VIDC_DECODER
#define H264    MSM_VIDC_H264
#define HEVC    MSM_VIDC_HEVC
#define VP9     MSM_VIDC_VP9
#define CODECS_ALL     (MSM_VIDC_H264 | MSM_VIDC_HEVC | \
			MSM_VIDC_VP9)

static struct msm_platform_core_capability core_data_waipio[] = {
	/* {type, value} */
	{ENC_CODECS, H264|HEVC},
	{DEC_CODECS, H264|HEVC|VP9},
	// TODO: MAX_SESSION_COUNT needs to be changed to 16
	{MAX_SESSION_COUNT, 3},
	{MAX_SECURE_SESSION_COUNT, 3},
	{MAX_MBPF, 173056},	/* (8192x4320)/256 + (4096x2176)/256*/
	{MAX_MBPS, 7833600},	/* max_load
					 * 7680x4320@60fps or 3840x2176@240fps
					 * which is greater than 4096x2176@120fps,
					 * 8192x4320@48fps
					 */
	{MAX_MBPF_HQ, 8160}, /* ((1920x1088)/256) */
	{MAX_MBPS_HQ, 489600}, /* ((1920x1088)/256)@60fps */
	{MAX_MBPF_B_FRAME, 32640}, /* 3840x2176/256 */
	{MAX_MBPS_B_FRAME, 1958400}, /* 3840x2176/256 MBs@60fps */
	{NUM_VPP_PIPE, 4},
	{SW_PC, 1},
	{SW_PC_DELAY, 20000}, /* 20000 ms (>HW_RESPONSE_TIMEOUT)*/
	{FW_UNLOAD, 0},
	{FW_UNLOAD_DELAY, 25000}, /* 25000 ms (>PC_DELAY)*/
	{HW_RESPONSE_TIMEOUT, 15000}, /* 1000 ms */
	{DEBUG_TIMEOUT, 0},
	// TODO: review below entries, and if required rename as PREFETCH
	{PREFIX_BUF_COUNT_PIX, 18},
	{PREFIX_BUF_SIZE_PIX, 13434880}, /* Calculated by VENUS_BUFFER_SIZE for 4096x2160 UBWC */
	{PREFIX_BUF_COUNT_NON_PIX, 1},
	{PREFIX_BUF_SIZE_NON_PIX, 209715200}, /*
		 * Internal buffer size is calculated for secure decode session
		 * of resolution 4k (4096x2160)
		 * Internal buf size = calculate_scratch_size() +
		 *	calculate_scratch1_size() + calculate_persist1_size()
		 * Take maximum between VP9 10bit, HEVC 10bit, AVC secure
		 * decoder sessions
		 */
	{PAGEFAULT_NON_FATAL, 1},
	{PAGETABLE_CACHING, 0},
	{DCVS, 1},
	{DECODE_BATCH, 1},
	{DECODE_BATCH_TIMEOUT, 200},
	{AV_SYNC_WINDOW_SIZE, 40},
};

static struct msm_platform_inst_capability instance_data_waipio[] = {
	/* {cap, domain, codec,
	 *      min, max, step_or_mask, value,
	 *      v4l2_id,
	 *      hfi_id,
	 *      flags,
	 *      parents,
	 *      children,
	 *      adjust, set}
	 */

	{FRAME_WIDTH, DEC, CODECS_ALL, 96, 8192, 1, 1920},
	{FRAME_WIDTH, ENC, CODECS_ALL, 128, 8192, 1, 1920},
	{LOSSLESS_FRAME_WIDTH, ENC, H264|HEVC, 128, 4096, 1, 1920},
	{SECURE_FRAME_WIDTH, ENC|DEC, CODECS_ALL, 128, 4096, 1, 1920},
	{HEVC_IMAGE_FRAME_WIDTH, ENC, HEVC, 128, 512, 1, 512},
	{HEIC_IMAGE_FRAME_WIDTH, ENC, HEVC, 512, 16384, 1, 16384},
	{FRAME_HEIGHT, DEC, CODECS_ALL, 96, 8192, 1, 1080},
	{FRAME_HEIGHT, ENC, CODECS_ALL, 128, 8192, 1, 1080},
	{LOSSLESS_FRAME_HEIGHT, ENC, H264|HEVC, 128, 4096, 1, 1080},
	{SECURE_FRAME_HEIGHT, ENC|DEC, CODECS_ALL, 128, 4096, 1, 1080},
	{HEVC_IMAGE_FRAME_HEIGHT, ENC, HEVC, 128, 512, 1, 512},
	{HEIC_IMAGE_FRAME_HEIGHT, ENC, HEVC, 512, 16384, 1, 16384},
	{PIX_FMTS, ENC, H264,
		MSM_VIDC_FMT_NV12,
		MSM_VIDC_FMT_NV12C,
		MSM_VIDC_FMT_NV12 | MSM_VIDC_FMT_NV21 | MSM_VIDC_FMT_NV12C,
		MSM_VIDC_FMT_NV12C},
	{PIX_FMTS, ENC, HEVC,
		MSM_VIDC_FMT_NV12,
		MSM_VIDC_FMT_TP10C,
		MSM_VIDC_FMT_NV12 | MSM_VIDC_FMT_NV21 | MSM_VIDC_FMT_NV12C |
		MSM_VIDC_FMT_P010 | MSM_VIDC_FMT_TP10C,
		MSM_VIDC_FMT_NV12C,
		0, 0,
		CAP_FLAG_ROOT,
		{0},
		{PROFILE, MIN_FRAME_QP, MAX_FRAME_QP, I_FRAME_QP}},

	{PIX_FMTS, DEC, HEVC,
		MSM_VIDC_FMT_NV12,
		MSM_VIDC_FMT_TP10C,
		MSM_VIDC_FMT_NV12 | MSM_VIDC_FMT_NV21 | MSM_VIDC_FMT_NV12C |
		MSM_VIDC_FMT_P010 | MSM_VIDC_FMT_TP10C,
		MSM_VIDC_FMT_NV12C,
		0, 0,
		CAP_FLAG_ROOT,
		{0},
		{PROFILE}},

	{PIX_FMTS, DEC, H264,
		MSM_VIDC_FMT_NV12,
		MSM_VIDC_FMT_NV12C,
		MSM_VIDC_FMT_NV12 | MSM_VIDC_FMT_NV21 | MSM_VIDC_FMT_NV12C,
		MSM_VIDC_FMT_NV12C},

	{PIX_FMTS, DEC, VP9,
		MSM_VIDC_FMT_NV12,
		MSM_VIDC_FMT_TP10C,
		MSM_VIDC_FMT_NV12 | MSM_VIDC_FMT_NV21 | MSM_VIDC_FMT_NV12C |
		MSM_VIDC_FMT_P010 | MSM_VIDC_FMT_TP10C,
		MSM_VIDC_FMT_NV12C},

	{MIN_BUFFERS_INPUT, ENC|DEC, CODECS_ALL, 0, 64, 1, 4,
		V4L2_CID_MIN_BUFFERS_FOR_OUTPUT},
	{MIN_BUFFERS_OUTPUT, ENC|DEC, CODECS_ALL,
		0, 64, 1, 4,
		V4L2_CID_MIN_BUFFERS_FOR_CAPTURE,
		HFI_PROP_BUFFER_FW_MIN_OUTPUT_COUNT,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT},

	/* (8192 * 4320) / 256 */
	{MBPF, ENC, CODECS_ALL, 64, 138240, 1, 138240},
	{MBPF, DEC, CODECS_ALL, 36, 138240, 1, 138240},
	/* (4096 * 2304) / 256 */
	{LOSSLESS_MBPF, ENC, H264|HEVC, 64, 36864, 1, 36864},
	/* Batch Mode Decode */
	{BATCH_MBPF, DEC, CODECS_ALL, 64, 34816, 1, 34816},
	/* (4096 * 2304) / 256 */
	{SECURE_MBPF, ENC|DEC, CODECS_ALL, 64, 36864, 1, 36864},
	/* ((1920 * 1088) / 256) * 480 fps */
	{MBPS, ENC, CODECS_ALL, 64, 3916800, 1, 3916800},
	/* ((1920 * 1088) / 256) * 960 fps */
	{MBPS, DEC, CODECS_ALL, 64, 7833600, 1, 7833600},
	/* ((4096 * 2304) / 256) * 60 fps */
	{POWER_SAVE_MBPS, ENC, CODECS_ALL, 0, 2211840, 1, 2211840},

	{FRAME_RATE, ENC, CODECS_ALL,
		(MINIMUM_FPS << 16), (MAXIMUM_FPS << 16),
		1, (DEFAULT_FPS << 16),
		0,
		HFI_PROP_FRAME_RATE,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT,
		{0}, {0},
		NULL, msm_vidc_set_q16},

	{FRAME_RATE, DEC, CODECS_ALL,
		(MINIMUM_FPS << 16), (MAXIMUM_FPS << 16),
		1, (DEFAULT_FPS << 16),
		0,
		HFI_PROP_FRAME_RATE,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT},

	{OPERATING_RATE, ENC|DEC, CODECS_ALL,
		1, INT_MAX, 1, (DEFAULT_FPS << 16)},

	{SCALE_X, ENC, CODECS_ALL, 8192, 65536, 1, 8192},
	{SCALE_X, DEC, CODECS_ALL, 65536, 65536, 1, 65536},
	{SCALE_Y, ENC, CODECS_ALL, 8192, 65536, 1, 8192},
	{SCALE_Y, DEC, CODECS_ALL, 65536, 65536, 1, 65536},
	{B_FRAME, ENC, H264|HEVC,
		V4L2_MPEG_MSM_VIDC_DISABLE, V4L2_MPEG_MSM_VIDC_ENABLE,
		1, V4L2_MPEG_MSM_VIDC_DISABLE,
		V4L2_CID_MPEG_VIDEO_B_FRAMES,
		HFI_PROP_MAX_B_FRAMES,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT},

	{MB_CYCLES_VSP, ENC, CODECS_ALL, 25, 25, 1, 25},
	{MB_CYCLES_VSP, DEC, CODECS_ALL, 25, 25, 1, 25},
	{MB_CYCLES_VSP, DEC, VP9, 60, 60, 1, 60},
	{MB_CYCLES_VPP, ENC, CODECS_ALL, 675, 675, 1, 675},
	{MB_CYCLES_VPP, DEC, CODECS_ALL, 200, 200, 1, 200},
	{MB_CYCLES_LP, ENC, CODECS_ALL, 320, 320, 1, 320},
	{MB_CYCLES_LP, DEC, CODECS_ALL, 200, 200, 1, 200},
	{MB_CYCLES_FW, ENC|DEC, CODECS_ALL, 326389, 326389, 1, 326389},
	{MB_CYCLES_FW_VPP, ENC|DEC, CODECS_ALL, 44156, 44156, 1, 44156},

	{SECURE_MODE, ENC|DEC, CODECS_ALL,
		V4L2_MPEG_MSM_VIDC_DISABLE, V4L2_MPEG_MSM_VIDC_ENABLE,
		1, V4L2_MPEG_MSM_VIDC_DISABLE,
		V4L2_CID_MPEG_VIDC_SECURE,
		HFI_PROP_SECURE,
		CAP_FLAG_ROOT,
		{0},
		{0},
		NULL, msm_vidc_set_u32},

	{HFLIP, ENC, CODECS_ALL,
		V4L2_MPEG_MSM_VIDC_DISABLE,
		V4L2_MPEG_MSM_VIDC_ENABLE,
		1, V4L2_MPEG_MSM_VIDC_DISABLE,
		V4L2_CID_HFLIP,
		HFI_PROP_FLIP,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT | CAP_FLAG_DYNAMIC_ALLOWED},

	{VFLIP, ENC, CODECS_ALL,
		V4L2_MPEG_MSM_VIDC_DISABLE,
		V4L2_MPEG_MSM_VIDC_ENABLE,
		1, V4L2_MPEG_MSM_VIDC_DISABLE,
		V4L2_CID_VFLIP,
		HFI_PROP_FLIP,
		CAP_FLAG_OUTPUT_PORT | CAP_FLAG_DYNAMIC_ALLOWED},

	{ROTATION, ENC, CODECS_ALL,
		0, 270, 90, 0,
		V4L2_CID_ROTATE,
		HFI_PROP_ROTATION,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT},

	{SLICE_INTERFACE, DEC, CODECS_ALL,
		0, 0, 0, 0,
		V4L2_CID_MPEG_VIDEO_DECODER_SLICE_INTERFACE,
		0},

	{HEADER_MODE, ENC, CODECS_ALL,
		V4L2_MPEG_VIDEO_HEADER_MODE_SEPARATE,
		V4L2_MPEG_VIDEO_HEADER_MODE_JOINED_WITH_1ST_FRAME,
		BIT(V4L2_MPEG_VIDEO_HEADER_MODE_SEPARATE) |
		BIT(V4L2_MPEG_VIDEO_HEADER_MODE_JOINED_WITH_1ST_FRAME),
		V4L2_MPEG_VIDEO_HEADER_MODE_SEPARATE,
		V4L2_CID_MPEG_VIDEO_HEADER_MODE,
		HFI_PROP_SEQ_HEADER_MODE,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT | CAP_FLAG_MENU,
		{0}, {0},
		NULL, msm_vidc_set_header_mode},

	{PREPEND_SPSPPS_TO_IDR, ENC, CODECS_ALL,
		V4L2_MPEG_MSM_VIDC_DISABLE,
		V4L2_MPEG_MSM_VIDC_ENABLE,
		1, V4L2_MPEG_MSM_VIDC_DISABLE,
		V4L2_CID_MPEG_VIDEO_PREPEND_SPSPPS_TO_IDR},

	{META_SEQ_HDR_NAL, ENC, CODECS_ALL,
		V4L2_MPEG_MSM_VIDC_DISABLE,
		V4L2_MPEG_MSM_VIDC_ENABLE,
		1, V4L2_MPEG_MSM_VIDC_DISABLE,
		V4L2_CID_MPEG_VIDC_METADATA_SEQ_HEADER_NAL},

	/* TODO: Firmware introduced enumeration type for this
	 * with and without seq header.
	 */
	{REQUEST_I_FRAME, ENC, CODECS_ALL,
		0, 0, 0, 0,
		V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME,
		HFI_PROP_REQUEST_SYNC_FRAME,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT},

	/* Enc: Keeping CABAC and CAVLC as same bitrate.
	 * Dec: there's no use of Bitrate cap
	 */
	{BIT_RATE, ENC, CODECS_ALL,
		1, 220000000, 1, 20000000,
		V4L2_CID_MPEG_VIDEO_BITRATE,
		HFI_PROP_TOTAL_BITRATE,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT | CAP_FLAG_DYNAMIC_ALLOWED,
		{0}, {0},
		NULL, msm_vidc_set_u32},

	{BITRATE_MODE, ENC, H264,
		V4L2_MPEG_VIDEO_BITRATE_MODE_VBR,
		V4L2_MPEG_VIDEO_BITRATE_MODE_CBR,
		BIT(V4L2_MPEG_VIDEO_BITRATE_MODE_VBR) |
		BIT(V4L2_MPEG_VIDEO_BITRATE_MODE_CBR),
		V4L2_MPEG_VIDEO_BITRATE_MODE_VBR,
		V4L2_CID_MPEG_VIDEO_BITRATE_MODE,
		HFI_PROP_RATE_CONTROL,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT | CAP_FLAG_MENU,
		{0},
		{LTR_COUNT, IR_RANDOM, TIME_DELTA_BASED_RC, I_FRAME_QP},
		msm_vidc_adjust_bitrate_mode, msm_vidc_set_u32_enum},

	{BITRATE_MODE, ENC, HEVC,
		V4L2_MPEG_VIDEO_BITRATE_MODE_VBR,
		V4L2_MPEG_VIDEO_BITRATE_MODE_CQ,
		BIT(V4L2_MPEG_VIDEO_BITRATE_MODE_VBR) |
		BIT(V4L2_MPEG_VIDEO_BITRATE_MODE_CBR) |
		BIT(V4L2_MPEG_VIDEO_BITRATE_MODE_CQ),
		V4L2_MPEG_VIDEO_BITRATE_MODE_VBR,
		V4L2_CID_MPEG_VIDEO_BITRATE_MODE,
		HFI_PROP_RATE_CONTROL,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT | CAP_FLAG_MENU,
		{0},
		{LTR_COUNT, IR_RANDOM, TIME_DELTA_BASED_RC,
			I_FRAME_QP, CONSTANT_QUALITY},
		msm_vidc_adjust_bitrate_mode, msm_vidc_set_u32_enum},

	{LOSSLESS, ENC, HEVC,
		V4L2_MPEG_MSM_VIDC_DISABLE, V4L2_MPEG_MSM_VIDC_ENABLE,
		1, V4L2_MPEG_MSM_VIDC_DISABLE,
		V4L2_CID_MPEG_VIDEO_HEVC_LOSSLESS_CU},

	{FRAME_SKIP_MODE, ENC, CODECS_ALL,
		V4L2_MPEG_MSM_VIDC_DISABLE, V4L2_MPEG_MSM_VIDC_ENABLE,
		1, V4L2_MPEG_MSM_VIDC_DISABLE,
		V4L2_CID_MPEG_VIDEO_FRAME_SKIP_MODE},

	{FRAME_RC_ENABLE, ENC, CODECS_ALL,
		V4L2_MPEG_MSM_VIDC_DISABLE, V4L2_MPEG_MSM_VIDC_ENABLE,
		1, V4L2_MPEG_MSM_VIDC_ENABLE,
		V4L2_CID_MPEG_VIDEO_FRAME_RC_ENABLE},

	{CONSTANT_QUALITY, ENC, HEVC,
		1, MAX_CONSTANT_QUALITY, 1, 90,
		V4L2_CID_MPEG_VIDEO_CONSTANT_QUALITY,
		HFI_PROP_CONSTANT_QUALITY,
		CAP_FLAG_OUTPUT_PORT,
		{BITRATE_MODE}, {0},
		NULL, msm_vidc_set_constant_quality},

	// TODO: GOP dependencies
	{GOP_SIZE, ENC, CODECS_ALL,
		0, INT_MAX, 1, 2 * DEFAULT_FPS - 1,
		V4L2_CID_MPEG_VIDEO_GOP_SIZE,
		HFI_PROP_MAX_GOP_FRAMES,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT,
		{0}, {0},
		NULL, msm_vidc_set_u32},

	{GOP_CLOSURE, ENC, CODECS_ALL,
		V4L2_MPEG_MSM_VIDC_DISABLE, V4L2_MPEG_MSM_VIDC_ENABLE,
		1, V4L2_MPEG_MSM_VIDC_ENABLE,
		V4L2_CID_MPEG_VIDEO_GOP_CLOSURE,
		0},

	{BLUR_TYPES, ENC, CODECS_ALL,
		VIDC_BLUR_NONE, VIDC_BLUR_ADAPTIVE, 1, VIDC_BLUR_NONE,
		V4L2_CID_MPEG_VIDC_VIDEO_BLUR_TYPES,
		HFI_PROP_BLUR_TYPES,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT,
		{0}, {0},
		NULL, msm_vidc_set_u32_enum},

	{BLUR_RESOLUTION, ENC, CODECS_ALL,
		0, S32_MAX, 1, 0,
		V4L2_CID_MPEG_VIDC_VIDEO_BLUR_RESOLUTION,
		HFI_PROP_BLUR_RESOLUTION,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT},

	/* Needed for control initialization. TODO */
	/* {CSC_CUSTOM_MATRIX, ENC, CODECS_ALL,
		V4L2_MPEG_MSM_VIDC_DISABLE,
		V4L2_MPEG_MSM_VIDC_ENABLE,
		1, V4L2_MPEG_MSM_VIDC_DISABLE,
		V4L2_CID_MPEG_VIDC_VIDEO_VPE_CSC_CUSTOM_MATRIX,
		HFI_PROP_CSC_MATRIX,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT}, */

	{HEIC, ENC, HEVC,
		V4L2_MPEG_MSM_VIDC_DISABLE, V4L2_MPEG_MSM_VIDC_ENABLE,
		1, V4L2_MPEG_MSM_VIDC_DISABLE,
		V4L2_CID_MPEG_VIDC_HEIC,
		HFI_PROP_HEIC_GRID_ENABLE,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT},

	{LOWLATENCY_MODE, ENC|DEC, CODECS_ALL,
		V4L2_MPEG_MSM_VIDC_DISABLE, V4L2_MPEG_MSM_VIDC_ENABLE,
		1, V4L2_MPEG_MSM_VIDC_DISABLE,
		V4L2_CID_MPEG_VIDC_LOWLATENCY_REQUEST,
		HFI_PROP_STAGE,
		CAP_FLAG_ROOT},

	{LTR_COUNT, ENC, H264|HEVC,
		0, 2, 1, 0,
		V4L2_CID_MPEG_VIDC_LTRCOUNT,
		HFI_PROP_LTR_COUNT,
		CAP_FLAG_OUTPUT_PORT,
		{BITRATE_MODE}, {USE_LTR, MARK_LTR},
		msm_vidc_adjust_ltr_count, msm_vidc_set_u32},

	{USE_LTR, ENC, H264|HEVC,
		0, ((1 << MAX_LTR_FRAME_COUNT) - 1), 1, 0,
		V4L2_CID_MPEG_VIDC_USELTRFRAME,
		HFI_PROP_LTR_USE,
		CAP_FLAG_INPUT_PORT | CAP_FLAG_DYNAMIC_ALLOWED,
		{LTR_COUNT}, {0},
		msm_vidc_adjust_use_ltr, msm_vidc_set_use_and_mark_ltr},

	{MARK_LTR, ENC, H264|HEVC,
		0, (MAX_LTR_FRAME_COUNT - 1), 1, 0,
		V4L2_CID_MPEG_VIDC_MARKLTRFRAME,
		HFI_PROP_LTR_MARK,
		CAP_FLAG_INPUT_PORT | CAP_FLAG_DYNAMIC_ALLOWED,
		{LTR_COUNT}, {0},
		msm_vidc_adjust_mark_ltr, msm_vidc_set_use_and_mark_ltr},

	{BASELAYER_PRIORITY, ENC, H264,
		0, MAX_BASE_LAYER_PRIORITY_ID, 1, 0,
		V4L2_CID_MPEG_VIDC_BASELAYER_PRIORITY,
		HFI_PROP_BASELAYER_PRIORITYID,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT},

	{IR_RANDOM, ENC, CODECS_ALL,
		0, INT_MAX, 1, 0,
		V4L2_CID_MPEG_VIDC_INTRA_REFRESH_PERIOD,
		HFI_PROP_IR_RANDOM_PERIOD,
		CAP_FLAG_OUTPUT_PORT,
		{BITRATE_MODE}, {0},
		msm_vidc_adjust_ir_random, msm_vidc_set_u32},

	{AU_DELIMITER, ENC, H264|HEVC,
		V4L2_MPEG_MSM_VIDC_DISABLE,
		V4L2_MPEG_MSM_VIDC_ENABLE,
		1, V4L2_MPEG_MSM_VIDC_DISABLE,
		V4L2_CID_MPEG_VIDC_AU_DELIMITER,
		HFI_PROP_AUD,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT,
		{0}, {0},
		NULL, msm_vidc_set_u32},

	{TIME_DELTA_BASED_RC, ENC, CODECS_ALL,
		V4L2_MPEG_MSM_VIDC_DISABLE, V4L2_MPEG_MSM_VIDC_ENABLE,
		1, V4L2_MPEG_MSM_VIDC_ENABLE,
		V4L2_CID_MPEG_VIDC_TIME_DELTA_BASED_RC,
		HFI_PROP_TIME_DELTA_BASED_RATE_CONTROL,
		CAP_FLAG_OUTPUT_PORT,
		{BITRATE_MODE}, {0},
		msm_vidc_adjust_delta_based_rc, msm_vidc_set_u32},

	{CONTENT_ADAPTIVE_CODING, ENC, CODECS_ALL,
		V4L2_MPEG_MSM_VIDC_DISABLE, V4L2_MPEG_MSM_VIDC_ENABLE,
		1, V4L2_MPEG_MSM_VIDC_ENABLE,
		V4L2_CID_MPEG_VIDC_CONTENT_ADAPTIVE_CODING,
		HFI_PROP_CONTENT_ADAPTIVE_CODING,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT},

	{BITRATE_BOOST, ENC, CODECS_ALL,
		0, 100, 25, 25,
		V4L2_CID_MPEG_VIDC_QUALITY_BITRATE_BOOST,
		HFI_PROP_BITRATE_BOOST,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT},

	{VBV_DELAY, ENC, CODECS_ALL,
		0, 1000, 500, 0,
		V4L2_CID_MPEG_VIDEO_VBV_DELAY},

	{MIN_FRAME_QP, ENC, H264,
		MIN_QP_8BIT, MAX_QP, 1, MIN_QP_8BIT,
		V4L2_CID_MPEG_VIDEO_H264_MIN_QP,
		HFI_PROP_MIN_QP_PACKED,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT,
		{0}, {0},
		NULL, msm_vidc_set_min_qp},

	{MIN_FRAME_QP, ENC, HEVC,
		MIN_QP_10BIT, MAX_QP, 1, MIN_QP_10BIT,
		V4L2_CID_MPEG_VIDEO_HEVC_MIN_QP,
		HFI_PROP_MIN_QP_PACKED,
		CAP_FLAG_OUTPUT_PORT,
		{PIX_FMTS}, {0},
		msm_vidc_adjust_hevc_min_qp, msm_vidc_set_min_qp},

	{I_FRAME_MIN_QP, ENC, H264,
		MIN_QP_8BIT, MAX_QP, 1, MIN_QP_8BIT,
		V4L2_CID_MPEG_VIDEO_H264_I_FRAME_MIN_QP},

	{I_FRAME_MIN_QP, ENC, HEVC,
		MIN_QP_10BIT, MAX_QP, 1, MIN_QP_10BIT,
		V4L2_CID_MPEG_VIDC_HEVC_I_FRAME_MIN_QP},

	{P_FRAME_MIN_QP, ENC, H264,
		MIN_QP_8BIT, MAX_QP, 1, MIN_QP_8BIT,
		V4L2_CID_MPEG_VIDEO_H264_P_FRAME_MIN_QP},

	{P_FRAME_MIN_QP, ENC, HEVC,
		MIN_QP_10BIT, MAX_QP, 1, MIN_QP_10BIT,
		V4L2_CID_MPEG_VIDC_HEVC_P_FRAME_MIN_QP},

	{B_FRAME_MIN_QP, ENC, H264,
		MIN_QP_8BIT, MAX_QP, 1, MIN_QP_8BIT,
		V4L2_CID_MPEG_VIDC_B_FRAME_MIN_QP},

	{B_FRAME_MIN_QP, ENC, HEVC,
		MIN_QP_10BIT, MAX_QP, 1, MIN_QP_10BIT,
		V4L2_CID_MPEG_VIDC_B_FRAME_MIN_QP},

	{MAX_FRAME_QP, ENC, H264,
		MIN_QP_8BIT, MAX_QP, 1, MAX_QP,
		V4L2_CID_MPEG_VIDEO_H264_MAX_QP,
		HFI_PROP_MAX_QP_PACKED,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT,
		{0}, {0},
		NULL, msm_vidc_set_min_qp},

	{MAX_FRAME_QP, ENC, HEVC,
		MIN_QP_10BIT, MAX_QP, 1, MAX_QP,
		V4L2_CID_MPEG_VIDEO_HEVC_MAX_QP,
		HFI_PROP_MAX_QP_PACKED,
		CAP_FLAG_OUTPUT_PORT,
		{PIX_FMTS}, {0},
		msm_vidc_adjust_hevc_max_qp, msm_vidc_set_min_qp},

	{I_FRAME_MAX_QP, ENC, H264,
		MIN_QP_8BIT, MAX_QP, 1, MAX_QP,
		V4L2_CID_MPEG_VIDEO_H264_I_FRAME_MAX_QP},

	{I_FRAME_MAX_QP, ENC, HEVC,
		MIN_QP_10BIT, MAX_QP, 1, MAX_QP,
		V4L2_CID_MPEG_VIDC_HEVC_I_FRAME_MAX_QP},

	{P_FRAME_MAX_QP, ENC, H264,
		MIN_QP_8BIT, MAX_QP, 1, MAX_QP,
		V4L2_CID_MPEG_VIDEO_H264_P_FRAME_MAX_QP},

	{P_FRAME_MAX_QP, ENC, HEVC,
		MIN_QP_10BIT, MAX_QP, 1, MAX_QP,
		V4L2_CID_MPEG_VIDC_HEVC_P_FRAME_MAX_QP},

	{B_FRAME_MAX_QP, ENC, H264,
		MIN_QP_8BIT, MAX_QP, 1, MAX_QP,
		V4L2_CID_MPEG_VIDC_B_FRAME_MAX_QP},

	{B_FRAME_MAX_QP, ENC, HEVC,
		MIN_QP_10BIT, MAX_QP, 1, MAX_QP,
		V4L2_CID_MPEG_VIDC_B_FRAME_MAX_QP},

	{HEVC_HIER_QP, ENC, HEVC,
		V4L2_MPEG_MSM_VIDC_DISABLE, V4L2_MPEG_MSM_VIDC_ENABLE,
		1, V4L2_MPEG_MSM_VIDC_DISABLE,
		V4L2_CID_MPEG_VIDEO_HEVC_HIER_QP,
		HFI_PROP_QP_PACKED,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT},

	{I_FRAME_QP, ENC, HEVC,
		MIN_QP_10BIT, MAX_QP, 1, DEFAULT_QP,
		V4L2_CID_MPEG_VIDEO_HEVC_I_FRAME_QP,
		HFI_PROP_QP_PACKED,
		CAP_FLAG_OUTPUT_PORT,
		{PIX_FMTS, BITRATE_MODE}, {0},
		msm_vidc_adjust_hevc_frame_qp, msm_vidc_set_frame_qp},

	{I_FRAME_QP, ENC, H264,
		MIN_QP_8BIT, MAX_QP, 1, DEFAULT_QP,
		V4L2_CID_MPEG_VIDEO_H264_I_FRAME_QP,
		HFI_PROP_QP_PACKED,
		CAP_FLAG_OUTPUT_PORT,
		{BITRATE_MODE}, {0},
		NULL, msm_vidc_set_frame_qp},

	{P_FRAME_QP, ENC, HEVC,
		MIN_QP_10BIT, MAX_QP, 1, DEFAULT_QP,
		V4L2_CID_MPEG_VIDEO_HEVC_P_FRAME_QP},

	{P_FRAME_QP, ENC, H264,
		MIN_QP_8BIT, MAX_QP, 1, DEFAULT_QP,
		V4L2_CID_MPEG_VIDEO_H264_P_FRAME_QP},

	{P_FRAME_QP, ENC, VP9, 0, 127, 1, 40},

	{B_FRAME_QP, ENC, HEVC,
		MIN_QP_10BIT, MAX_QP, 1, DEFAULT_QP,
		V4L2_CID_MPEG_VIDEO_HEVC_B_FRAME_QP},

	{B_FRAME_QP, ENC, H264,
		MIN_QP_8BIT, MAX_QP, 1, DEFAULT_QP,
		V4L2_CID_MPEG_VIDEO_H264_B_FRAME_QP},

	{L0_QP, ENC, HEVC,
		0, 51, 1, 20,
		V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L0_QP,
		HFI_PROP_QP_PACKED,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT},

	{L1_QP, ENC, HEVC,
		0, 51, 1, 20,
		V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L1_QP,
		HFI_PROP_QP_PACKED,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT},

	{L2_QP, ENC, HEVC,
		0, 51, 1, 20,
		V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L2_QP,
		HFI_PROP_QP_PACKED,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT},

	{L3_QP, ENC, HEVC,
		0, 51, 1, 20,
		V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L3_QP,
		HFI_PROP_QP_PACKED,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT},

	{L4_QP, ENC, HEVC,
		0, 51, 1, 20,
		V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L4_QP,
		HFI_PROP_QP_PACKED,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT},

	{L5_QP, ENC, HEVC,
		0, 51, 1, 20,
		V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L5_QP,
		HFI_PROP_QP_PACKED,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT},

	{HIER_LAYER_QP, ENC, H264,
		0, 0x0060033, 1, 20,
		V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER_QP,
		HFI_PROP_QP_PACKED,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT},

	{HIER_CODING_TYPE, ENC, HEVC,
		V4L2_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_B,
		V4L2_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_P,
		BIT(V4L2_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_B) |
		BIT(V4L2_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_P),
		V4L2_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_P,
		V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_TYPE,
		HFI_PROP_LAYER_ENCODING_TYPE,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT | CAP_FLAG_MENU,
		{0}, {0},
		NULL, msm_vidc_set_u32_enum},

	/* TODO(AS) - ctrl init failing. Need to fix
	{HIER_CODING_TYPE, ENC, H264,
		V4L2_MPEG_VIDEO_H264_HIERARCHICAL_CODING_B,
		V4L2_MPEG_VIDEO_H264_HIERARCHICAL_CODING_P,
		BIT(V4L2_MPEG_VIDEO_H264_HIERARCHICAL_CODING_B) |
		BIT(V4L2_MPEG_VIDEO_H264_HIERARCHICAL_CODING_P),
		V4L2_MPEG_VIDEO_H264_HIERARCHICAL_CODING_P,
		V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_TYPE,
		HFI_PROP_LAYER_ENCODING_TYPE,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT | CAP_FLAG_MENU},
	*/

	//TODO (AS)
	{HIER_CODING, ENC, H264,
		V4L2_MPEG_MSM_VIDC_DISABLE, V4L2_MPEG_MSM_VIDC_ENABLE,
		1, V4L2_MPEG_MSM_VIDC_DISABLE,
		V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING,
		HFI_PROP_LAYER_ENCODING_TYPE,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT},

	// TODO: add relationship with GOP_SIZE in caps
	{HIER_CODING_LAYER, ENC, HEVC,
		0, 5, 1, 0,
		V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_LAYER,
		HFI_PROP_LAYER_COUNT,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT},

	{HIER_CODING_LAYER, ENC, H264,
		0, 6, 1, 0,
		V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER,
		HFI_PROP_LAYER_COUNT,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT},

	{L0_BR, ENC, HEVC,
		1, 220000000, 1, 20000000,
		V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L0_BR,
		HFI_PROP_BITRATE_LAYER1,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT},

	{L1_BR, ENC, HEVC,
		1, 220000000, 1, 20000000,
		V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L1_BR,
		HFI_PROP_BITRATE_LAYER2,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT},

	{L2_BR, ENC, HEVC,
		1, 220000000, 1, 20000000,
		V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L2_BR,
		HFI_PROP_BITRATE_LAYER3,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT},

	{L3_BR, ENC, HEVC,
		1, 220000000, 1, 20000000,
		V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L3_BR,
		HFI_PROP_BITRATE_LAYER4,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT},

	{L4_BR, ENC, HEVC,
		1, 220000000, 1, 20000000,
		V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L4_BR,
		HFI_PROP_BITRATE_LAYER5,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT},

	{L5_BR, ENC, HEVC,
		1, 220000000, 1, 20000000,
		V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L5_BR,
		HFI_PROP_BITRATE_LAYER6,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT},

	{ENTROPY_MODE, ENC, H264,
		V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CAVLC,
		V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CABAC,
		BIT(V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CAVLC) |
		BIT(V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CABAC),
		V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CABAC,
		V4L2_CID_MPEG_VIDEO_H264_ENTROPY_MODE,
		HFI_PROP_CABAC_SESSION,
		CAP_FLAG_OUTPUT_PORT | CAP_FLAG_MENU,
		{PROFILE},
		{0},
		msm_vidc_adjust_entropy_mode, msm_vidc_set_u32},

	{ENTROPY_MODE, DEC, CODECS_ALL,
		V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CAVLC,
		V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CABAC,
		BIT(V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CAVLC) |
		BIT(V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CABAC),
		V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CABAC,
		0,
		HFI_PROP_CABAC_SESSION},

	/* H264 does not support 10 bit, PIX_FMTS would not be a Parent for this */
	{PROFILE, ENC, H264,
		V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE,
		V4L2_MPEG_VIDEO_H264_PROFILE_CONSTRAINED_HIGH,
		BIT(V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE) |
		BIT(V4L2_MPEG_VIDEO_H264_PROFILE_CONSTRAINED_HIGH) |
		BIT(V4L2_MPEG_VIDEO_H264_PROFILE_CONSTRAINED_BASELINE) |
		BIT(V4L2_MPEG_VIDEO_H264_PROFILE_MAIN) |
		BIT(V4L2_MPEG_VIDEO_H264_PROFILE_HIGH),
		V4L2_MPEG_VIDEO_H264_PROFILE_HIGH,
		V4L2_CID_MPEG_VIDEO_H264_PROFILE,
		HFI_PROP_PROFILE,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT | CAP_FLAG_MENU,
		{0},
		{ENTROPY_MODE, TRANSFORM_8X8},
		NULL, msm_vidc_set_u32_enum},

	{PROFILE, DEC, H264,
		V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE,
		V4L2_MPEG_VIDEO_H264_PROFILE_CONSTRAINED_HIGH,
		BIT(V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE) |
		BIT(V4L2_MPEG_VIDEO_H264_PROFILE_CONSTRAINED_HIGH) |
		BIT(V4L2_MPEG_VIDEO_H264_PROFILE_CONSTRAINED_BASELINE) |
		BIT(V4L2_MPEG_VIDEO_H264_PROFILE_MAIN) |
		BIT(V4L2_MPEG_VIDEO_H264_PROFILE_HIGH),
		V4L2_MPEG_VIDEO_H264_PROFILE_HIGH,
		V4L2_CID_MPEG_VIDEO_H264_PROFILE,
		HFI_PROP_PROFILE,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT | CAP_FLAG_MENU,
		{0},
		{ENTROPY_MODE},
		NULL, msm_vidc_set_u32_enum},

	{PROFILE, ENC|DEC, HEVC,
		V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN,
		V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN_10,
		BIT(V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN) |
		BIT(V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN_STILL_PICTURE) |
		BIT(V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN_10),
		V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN,
		V4L2_CID_MPEG_VIDEO_HEVC_PROFILE,
		HFI_PROP_PROFILE,
		CAP_FLAG_OUTPUT_PORT | CAP_FLAG_MENU,
		{PIX_FMTS},
		{0},
		msm_vidc_adjust_profile, msm_vidc_set_u32_enum},

	{PROFILE, DEC, VP9,
		V4L2_MPEG_VIDEO_VP9_PROFILE_0,
		V4L2_MPEG_VIDEO_VP9_PROFILE_2,
		BIT(V4L2_MPEG_VIDEO_VP9_PROFILE_0) |
		BIT(V4L2_MPEG_VIDEO_VP9_PROFILE_2),
		V4L2_MPEG_VIDEO_VP9_PROFILE_0,
		V4L2_CID_MPEG_VIDEO_VP9_PROFILE,
		HFI_PROP_PROFILE,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT | CAP_FLAG_MENU,
		{0},
		{0},
		NULL, msm_vidc_set_u32_enum},

	{LEVEL, ENC|DEC, H264,
		V4L2_MPEG_VIDEO_H264_LEVEL_1_0,
		V4L2_MPEG_VIDEO_H264_LEVEL_5_1,
		BIT(V4L2_MPEG_VIDEO_H264_LEVEL_1_0) |
		BIT(V4L2_MPEG_VIDEO_H264_LEVEL_1B) |
		BIT(V4L2_MPEG_VIDEO_H264_LEVEL_1_1) |
		BIT(V4L2_MPEG_VIDEO_H264_LEVEL_1_2) |
		BIT(V4L2_MPEG_VIDEO_H264_LEVEL_1_3) |
		BIT(V4L2_MPEG_VIDEO_H264_LEVEL_2_0) |
		BIT(V4L2_MPEG_VIDEO_H264_LEVEL_2_1) |
		BIT(V4L2_MPEG_VIDEO_H264_LEVEL_2_2) |
		BIT(V4L2_MPEG_VIDEO_H264_LEVEL_3_0) |
		BIT(V4L2_MPEG_VIDEO_H264_LEVEL_3_1) |
		BIT(V4L2_MPEG_VIDEO_H264_LEVEL_3_2) |
		BIT(V4L2_MPEG_VIDEO_H264_LEVEL_4_0) |
		BIT(V4L2_MPEG_VIDEO_H264_LEVEL_4_1) |
		BIT(V4L2_MPEG_VIDEO_H264_LEVEL_4_2) |
		BIT(V4L2_MPEG_VIDEO_H264_LEVEL_5_0) |
		BIT(V4L2_MPEG_VIDEO_H264_LEVEL_5_1),
		V4L2_MPEG_VIDEO_H264_LEVEL_5_1,
		V4L2_CID_MPEG_VIDEO_H264_LEVEL,
		HFI_PROP_LEVEL,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT | CAP_FLAG_MENU,
		{0},
		{0},
		NULL, msm_vidc_set_u32_enum},

	{LEVEL, ENC|DEC, HEVC,
		V4L2_MPEG_VIDEO_HEVC_LEVEL_1,
		V4L2_MPEG_VIDEO_HEVC_LEVEL_6_2,
		BIT(V4L2_MPEG_VIDEO_HEVC_LEVEL_1) |
		BIT(V4L2_MPEG_VIDEO_HEVC_LEVEL_2) |
		BIT(V4L2_MPEG_VIDEO_HEVC_LEVEL_2_1) |
		BIT(V4L2_MPEG_VIDEO_HEVC_LEVEL_3) |
		BIT(V4L2_MPEG_VIDEO_HEVC_LEVEL_3_1) |
		BIT(V4L2_MPEG_VIDEO_HEVC_LEVEL_4) |
		BIT(V4L2_MPEG_VIDEO_HEVC_LEVEL_4_1) |
		BIT(V4L2_MPEG_VIDEO_HEVC_LEVEL_5) |
		BIT(V4L2_MPEG_VIDEO_HEVC_LEVEL_5_1) |
		BIT(V4L2_MPEG_VIDEO_HEVC_LEVEL_5_2) |
		BIT(V4L2_MPEG_VIDEO_HEVC_LEVEL_6) |
		BIT(V4L2_MPEG_VIDEO_HEVC_LEVEL_6_1) |
		BIT(V4L2_MPEG_VIDEO_HEVC_LEVEL_6_2),
		V4L2_MPEG_VIDEO_HEVC_LEVEL_6_2,
		V4L2_CID_MPEG_VIDEO_HEVC_LEVEL,
		HFI_PROP_LEVEL,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT | CAP_FLAG_MENU,
		{0},
		{0},
		NULL, msm_vidc_set_u32_enum},

	/* TODO: Bring the VP9 Level upstream GKI change, and level cap here:
	 *	go/videogki
	 */

	{HEVC_TIER, ENC|DEC, HEVC,
		V4L2_MPEG_VIDEO_HEVC_TIER_MAIN,
		V4L2_MPEG_VIDEO_HEVC_TIER_HIGH,
		BIT(V4L2_MPEG_VIDEO_HEVC_TIER_MAIN) |
		BIT(V4L2_MPEG_VIDEO_HEVC_TIER_HIGH),
		V4L2_MPEG_VIDEO_HEVC_TIER_HIGH,
		V4L2_CID_MPEG_VIDEO_HEVC_TIER,
		HFI_PROP_TIER,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT | CAP_FLAG_MENU,
		{0},
		{0},
		NULL, msm_vidc_set_u32_enum},

	{LF_MODE, ENC, H264,
		V4L2_MPEG_VIDEO_H264_LOOP_FILTER_MODE_ENABLED,
		DB_H264_DISABLE_SLICE_BOUNDARY,
		BIT(V4L2_MPEG_VIDEO_H264_LOOP_FILTER_MODE_ENABLED) |
		BIT(V4L2_MPEG_VIDEO_H264_LOOP_FILTER_MODE_DISABLED) |
		BIT(DB_H264_DISABLE_SLICE_BOUNDARY),
		V4L2_MPEG_VIDEO_H264_LOOP_FILTER_MODE_ENABLED,
		V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_MODE,
		HFI_PROP_DEBLOCKING_MODE,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT | CAP_FLAG_MENU,
		{0}, {0},
		NULL, msm_vidc_set_deblock_mode},

	{LF_MODE, ENC, HEVC,
		V4L2_MPEG_VIDEO_HEVC_LOOP_FILTER_MODE_DISABLED,
		DB_HEVC_DISABLE_SLICE_BOUNDARY,
		BIT(V4L2_MPEG_VIDEO_HEVC_LOOP_FILTER_MODE_DISABLED) |
		BIT(V4L2_MPEG_VIDEO_HEVC_LOOP_FILTER_MODE_ENABLED) |
		BIT(DB_HEVC_DISABLE_SLICE_BOUNDARY),
		V4L2_MPEG_VIDEO_HEVC_LOOP_FILTER_MODE_ENABLED,
		V4L2_CID_MPEG_VIDEO_HEVC_LOOP_FILTER_MODE,
		HFI_PROP_DEBLOCKING_MODE,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT | CAP_FLAG_MENU,
		{0}, {0},
		NULL, msm_vidc_set_deblock_mode},

	{LF_ALPHA, ENC, H264,
		-6, 6, 1, 0,
		V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_ALPHA},

	{LF_ALPHA, ENC, HEVC,
		-6, 6, 1, 0,
		V4L2_CID_MPEG_VIDEO_HEVC_LF_TC_OFFSET_DIV2},

	{LF_BETA, ENC, H264,
		-6, 6, 1, 0,
		V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_BETA},

	{LF_BETA, ENC, HEVC,
		-6, 6, 1, 0,
		V4L2_CID_MPEG_VIDEO_HEVC_LF_BETA_OFFSET_DIV2},

	{SLICE_MAX_BYTES, ENC, H264|HEVC,
		1, MAX_BITRATE / DEFAULT_FPS / 8 / 10,
		1, MAX_BITRATE / DEFAULT_FPS / 8 / 10,
		V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_BYTES,
		HFI_PROP_MULTI_SLICE_BYTES_COUNT,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT},

	{SLICE_MAX_MB, ENC, H264|HEVC,
		1, (MAX_WIDTH * MAX_HEIGHT) / 256 / DEFAULT_FPS / 10,
		1, (MAX_WIDTH * MAX_HEIGHT) / 256 / DEFAULT_FPS / 10,
		V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_MB,
		HFI_PROP_MULTI_SLICE_MB_COUNT,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT},

	{SLICE_MODE, ENC, H264|HEVC,
		V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_SINGLE,
		V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_MAX_BYTES,
		BIT(V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_SINGLE) |
		BIT(V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_MAX_MB) |
		BIT(V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_MAX_BYTES),
		V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_SINGLE,
		V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MODE,
		0,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT | CAP_FLAG_MENU},

	// TODO: MB level RC - mapping
	{MB_RC, ENC, CODECS_ALL,
		V4L2_MPEG_MSM_VIDC_DISABLE, V4L2_MPEG_MSM_VIDC_ENABLE,
		1, V4L2_MPEG_MSM_VIDC_ENABLE,
		V4L2_CID_MPEG_VIDEO_MB_RC_ENABLE,
		0,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT},

	{TRANSFORM_8X8, ENC, H264,
		V4L2_MPEG_MSM_VIDC_DISABLE, V4L2_MPEG_MSM_VIDC_ENABLE,
		1, V4L2_MPEG_MSM_VIDC_ENABLE,
		V4L2_CID_MPEG_VIDEO_H264_8X8_TRANSFORM,
		HFI_PROP_8X8_TRANSFORM,
		CAP_FLAG_OUTPUT_PORT,
		{PROFILE}, {0},
		msm_vidc_adjust_transform_8x8, msm_vidc_set_u32},

	{CHROMA_QP_INDEX_OFFSET, ENC, HEVC,
		MIN_CHROMA_QP_OFFSET, MAX_CHROMA_QP_OFFSET,
		1, MAX_CHROMA_QP_OFFSET,
		V4L2_CID_MPEG_VIDEO_H264_CHROMA_QP_INDEX_OFFSET,
		HFI_PROP_CHROMA_QP_OFFSET,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT},

	{DISPLAY_DELAY_ENABLE, DEC, CODECS_ALL,
		V4L2_MPEG_MSM_VIDC_DISABLE, V4L2_MPEG_MSM_VIDC_ENABLE,
		1, V4L2_MPEG_MSM_VIDC_DISABLE,
		V4L2_CID_MPEG_MFC51_VIDEO_DECODER_H264_DISPLAY_DELAY_ENABLE,
		HFI_PROP_DECODE_ORDER_OUTPUT,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT},

	{DISPLAY_DELAY, DEC, CODECS_ALL,
		0, 1, 1, 0,
		V4L2_CID_MPEG_MFC51_VIDEO_DECODER_H264_DISPLAY_DELAY,
		HFI_PROP_DECODE_ORDER_OUTPUT,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT},

	/* conceal color */
	{CONCEAL_COLOR_8BIT, DEC, CODECS_ALL, 0x0, 0xff3fcff, 1,
		DEFAULT_VIDEO_CONCEAL_COLOR_BLACK,
		V4L2_CID_MPEG_VIDEO_MUTE_YUV,
		HFI_PROP_CONCEAL_COLOR_8BIT,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT},

	{CONCEAL_COLOR_10BIT, DEC, CODECS_ALL, 0x0, 0x3fffffff, 1,
		DEFAULT_VIDEO_CONCEAL_COLOR_BLACK,
		V4L2_CID_MPEG_VIDEO_MUTE_YUV,
		HFI_PROP_CONCEAL_COLOR_10BIT,
		CAP_FLAG_ROOT | CAP_FLAG_OUTPUT_PORT},

	// TODO
	{STAGE, DEC|ENC, CODECS_ALL,
		MSM_VIDC_STAGE_1,
		MSM_VIDC_STAGE_2, 1,
		MSM_VIDC_STAGE_2},
	{PIPE, DEC|ENC, CODECS_ALL,
		MSM_VIDC_PIPE_1,
		MSM_VIDC_PIPE_4, 1,
		MSM_VIDC_PIPE_4},
	{POC, DEC, H264, 0, 18, 1, 1},
	{QUALITY_MODE, ENC, CODECS_ALL,
		MSM_VIDC_MAX_QUALITY_MODE,
		MSM_VIDC_POWER_SAVE_MODE, 1,
		MSM_VIDC_POWER_SAVE_MODE},

	{CODED_FRAMES, DEC, CODECS_ALL,
		CODED_FRAMES_MBS_ONLY, CODED_FRAMES_ADAPTIVE_FIELDS,
		1, CODED_FRAMES_MBS_ONLY,
		0,
		HFI_PROP_CODED_FRAMES},

	{BIT_DEPTH, DEC, CODECS_ALL, BIT_DEPTH_8, BIT_DEPTH_10, 1, BIT_DEPTH_8,
		0,
		HFI_PROP_LUMA_CHROMA_BIT_DEPTH},

	{CODEC_CONFIG, DEC, H264|HEVC, 0, 1, 1, 0,
		V4L2_CID_MPEG_VIDC_CODEC_CONFIG},

	{BITSTREAM_SIZE_OVERWRITE, DEC, CODECS_ALL, 0, INT_MAX, 1, 0,
		V4L2_CID_MPEG_VIDC_MIN_BITSTREAM_SIZE_OVERWRITE},

	{THUMBNAIL_MODE, DEC, CODECS_ALL,
		V4L2_MPEG_MSM_VIDC_DISABLE, V4L2_MPEG_MSM_VIDC_ENABLE,
		1, V4L2_MPEG_MSM_VIDC_DISABLE,
		0,
		HFI_PROP_THUMBNAIL_MODE},

	{DEFAULT_HEADER, DEC, CODECS_ALL,
		V4L2_MPEG_MSM_VIDC_DISABLE, V4L2_MPEG_MSM_VIDC_ENABLE,
		1, V4L2_MPEG_MSM_VIDC_DISABLE,
		0,
		HFI_PROP_DEC_DEFAULT_HEADER},

	{RAP_FRAME, DEC, CODECS_ALL,
		V4L2_MPEG_MSM_VIDC_DISABLE, V4L2_MPEG_MSM_VIDC_ENABLE,
		1, V4L2_MPEG_MSM_VIDC_ENABLE,
		0,
		HFI_PROP_DEC_START_FROM_RAP_FRAME,
		CAP_FLAG_INPUT_PORT,
		{THUMBNAIL_MODE}},

	{META_LTR_MARK_USE, ENC, CODECS_ALL,
		V4L2_MPEG_MSM_VIDC_DISABLE, V4L2_MPEG_MSM_VIDC_ENABLE,
		1, V4L2_MPEG_MSM_VIDC_DISABLE,
		V4L2_CID_MPEG_VIDC_METADATA_LTR_MARK_USE_DETAILS,
		HFI_PROP_LTR_MARK_USE_DETAILS},

	{META_SEQ_HDR_NAL, ENC, CODECS_ALL,
		V4L2_MPEG_MSM_VIDC_DISABLE, V4L2_MPEG_MSM_VIDC_ENABLE,
		1, V4L2_MPEG_MSM_VIDC_DISABLE,
		V4L2_CID_MPEG_VIDC_METADATA_SEQ_HEADER_NAL,
		HFI_PROP_METADATA_SEQ_HEADER_NAL},

	{META_DPB_MISR, DEC, CODECS_ALL,
		V4L2_MPEG_MSM_VIDC_DISABLE, V4L2_MPEG_MSM_VIDC_ENABLE,
		1, V4L2_MPEG_MSM_VIDC_DISABLE,
		V4L2_CID_MPEG_VIDC_METADATA_DPB_LUMA_CHROMA_MISR,
		HFI_PROP_DPB_LUMA_CHROMA_MISR},

	{META_OPB_MISR, DEC, CODECS_ALL,
		V4L2_MPEG_MSM_VIDC_DISABLE, V4L2_MPEG_MSM_VIDC_ENABLE,
		1, V4L2_MPEG_MSM_VIDC_DISABLE,
		V4L2_CID_MPEG_VIDC_METADATA_OPB_LUMA_CHROMA_MISR,
		HFI_PROP_OPB_LUMA_CHROMA_MISR},

	{META_INTERLACE, DEC, CODECS_ALL,
		V4L2_MPEG_MSM_VIDC_DISABLE, V4L2_MPEG_MSM_VIDC_ENABLE,
		1, V4L2_MPEG_MSM_VIDC_DISABLE,
		V4L2_CID_MPEG_VIDC_METADATA_INTERLACE,
		HFI_PROP_INTERLACE_INFO},

	{META_TIMESTAMP, DEC | ENC, CODECS_ALL,
		V4L2_MPEG_MSM_VIDC_DISABLE, V4L2_MPEG_MSM_VIDC_ENABLE,
		1, V4L2_MPEG_MSM_VIDC_DISABLE,
		V4L2_CID_MPEG_VIDC_METADATA_TIMESTAMP,
		HFI_PROP_TIMESTAMP},

	{META_CONCEALED_MB_CNT, DEC, CODECS_ALL,
		V4L2_MPEG_MSM_VIDC_DISABLE, V4L2_MPEG_MSM_VIDC_ENABLE,
		1, V4L2_MPEG_MSM_VIDC_DISABLE,
		V4L2_CID_MPEG_VIDC_METADATA_CONCEALED_MB_COUNT,
		HFI_PROP_CONEALED_MB_COUNT},

	{META_HIST_INFO, DEC, CODECS_ALL,
		V4L2_MPEG_MSM_VIDC_DISABLE, V4L2_MPEG_MSM_VIDC_ENABLE,
		1, V4L2_MPEG_MSM_VIDC_DISABLE,
		V4L2_CID_MPEG_VIDC_METADATA_HISTOGRAM_INFO,
		HFI_PROP_HISTOGRAM_INFO},

	{META_SEI_MASTERING_DISP, DEC, CODECS_ALL,
		V4L2_MPEG_MSM_VIDC_DISABLE, V4L2_MPEG_MSM_VIDC_ENABLE,
		1, V4L2_MPEG_MSM_VIDC_DISABLE,
		V4L2_CID_MPEG_VIDC_METADATA_SEI_MASTERING_DISPLAY_COLOUR,
		HFI_PROP_SEI_MASTERING_DISPLAY_COLOUR},

	{META_SEI_CLL, DEC, CODECS_ALL,
		V4L2_MPEG_MSM_VIDC_DISABLE, V4L2_MPEG_MSM_VIDC_ENABLE,
		1, V4L2_MPEG_MSM_VIDC_DISABLE,
		V4L2_CID_MPEG_VIDC_METADATA_SEI_CONTENT_LIGHT_LEVEL,
		HFI_PROP_SEI_CONTENT_LIGHT_LEVEL},

	{META_HDR10PLUS, DEC, CODECS_ALL,
		V4L2_MPEG_MSM_VIDC_DISABLE, V4L2_MPEG_MSM_VIDC_ENABLE,
		1, V4L2_MPEG_MSM_VIDC_DISABLE,
		V4L2_CID_MPEG_VIDC_METADATA_HDR10PLUS,
		HFI_PROP_SEI_HDR10PLUS_USERDATA},

	{META_EVA_STATS, ENC, CODECS_ALL,
		V4L2_MPEG_MSM_VIDC_DISABLE, V4L2_MPEG_MSM_VIDC_ENABLE,
		1, V4L2_MPEG_MSM_VIDC_DISABLE,
		V4L2_CID_MPEG_VIDC_METADATA_EVA_STATS,
		HFI_PROP_EVA_STAT_INFO},

	{META_BUF_TAG, DEC | ENC, CODECS_ALL,
		V4L2_MPEG_MSM_VIDC_DISABLE, V4L2_MPEG_MSM_VIDC_ENABLE,
		1, V4L2_MPEG_MSM_VIDC_DISABLE,
		V4L2_CID_MPEG_VIDC_METADATA_BUFFER_TAG,
		HFI_PROP_BUFFER_TAG},

	{META_SUBFRAME_OUTPUT, DEC, CODECS_ALL,
		V4L2_MPEG_MSM_VIDC_DISABLE, V4L2_MPEG_MSM_VIDC_ENABLE,
		1, V4L2_MPEG_MSM_VIDC_DISABLE,
		V4L2_CID_MPEG_VIDC_METADATA_SUBFRAME_OUTPUT,
		HFI_PROP_SUBFRAME_OUTPUT},

	{META_ENC_QP_METADATA, ENC, CODECS_ALL,
		V4L2_MPEG_MSM_VIDC_DISABLE, V4L2_MPEG_MSM_VIDC_ENABLE,
		1, V4L2_MPEG_MSM_VIDC_DISABLE,
		V4L2_CID_MPEG_VIDC_METADATA_ENC_QP_METADATA,
		HFI_PROP_ENC_QP_METADATA},

	{META_ROI_INFO, ENC, CODECS_ALL,
		V4L2_MPEG_MSM_VIDC_DISABLE, V4L2_MPEG_MSM_VIDC_ENABLE,
		1, V4L2_MPEG_MSM_VIDC_DISABLE,
		V4L2_CID_MPEG_VIDC_METADATA_ROI_INFO,
		HFI_PROP_ROI_INFO},
};

/*
 * Custom conversion coefficients for resolution: 176x144 negative
 * coeffs are converted to s4.9 format
 * (e.g. -22 converted to ((1 << 13) - 22)
 * 3x3 transformation matrix coefficients in s4.9 fixed point format
 */
static u32 vpe_csc_custom_matrix_coeff[MAX_MATRIX_COEFFS] = {
	440, 8140, 8098, 0, 460, 52, 0, 34, 463
};

/* offset coefficients in s9 fixed point format */
static u32 vpe_csc_custom_bias_coeff[MAX_BIAS_COEFFS] = {
	53, 0, 4
};

/* clamping value for Y/U/V([min,max] for Y/U/V) */
static u32 vpe_csc_custom_limit_coeff[MAX_LIMIT_COEFFS] = {
	16, 235, 16, 240, 16, 240
};

/* Default UBWC config for LPDDR5 */
static struct msm_vidc_ubwc_config_data ubwc_config_waipio[] = {
	UBWC_CONFIG(8, 32, 16, 0, 1, 1, 1),
};

static struct msm_vidc_platform_data waipio_data = {
	.core_data = core_data_waipio,
	.core_data_size = ARRAY_SIZE(core_data_waipio),
	.instance_data = instance_data_waipio,
	.instance_data_size = ARRAY_SIZE(instance_data_waipio),
	.csc_data.vpe_csc_custom_bias_coeff = vpe_csc_custom_bias_coeff,
	.csc_data.vpe_csc_custom_matrix_coeff = vpe_csc_custom_matrix_coeff,
	.csc_data.vpe_csc_custom_limit_coeff = vpe_csc_custom_limit_coeff,
	.ubwc_config = ubwc_config_waipio,
};

static int msm_vidc_init_data(struct msm_vidc_core *core)
{
	int rc = 0;
	struct msm_vidc_ubwc_config_data *ubwc_config;
	u32 ddr_type;

	if (!core || !core->platform) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	d_vpr_h("%s: initialize waipio data\n", __func__);

	ubwc_config = waipio_data.ubwc_config;
	ddr_type = of_fdt_get_ddrtype();
	if (ddr_type == -ENOENT)
		d_vpr_e("Failed to get ddr type, use LPDDR5\n");

	if (ddr_type == DDR_TYPE_LPDDR4 || ddr_type == DDR_TYPE_LPDDR4X)
		ubwc_config->highest_bank_bit = 0xf;
	d_vpr_h("%s: DDR Type 0x%x hbb 0x%x\n", __func__,
		ddr_type, ubwc_config->highest_bank_bit);

	core->platform->data = waipio_data;

	return rc;
}

int msm_vidc_init_platform_waipio(struct msm_vidc_core *core)
{
	int rc = 0;

	rc = msm_vidc_init_data(core);
	if (rc)
		return rc;

	return 0;
}

int msm_vidc_deinit_platform_waipio(struct msm_vidc_core *core)
{
	/* do nothing */
	return 0;
}
