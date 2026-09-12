// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2017 MediaTek Inc.
 * Copyright (C) 2021 Transsion Holdings
 */

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/types.h>

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "imgsensor_i2c.h"

extern int iReadRegI2CTiming(u8 *a_pSendData, u16 a_sizeSendData,
	u8 *a_pRecvData, u16 a_sizeRecvData, u16 i2cId, u16 timing);
extern int iWriteRegI2CTiming(u8 *a_pSendData,
	u16 a_sizeSendData, u16 i2cId, u16 timing);

#define PFX "GC6153_SERIAL_YUV"
#define GC6153_I2C_ADDR 0x80
#define GC6153_I2C_SPEED 400

struct gc6153_reg_val {
	kal_uint8 reg;
	kal_uint8 val;
};

static const struct gc6153_reg_val gc6153_init_regs[] = {
	{0xfe, 0xa0},
	{0xfe, 0xa0},
	{0xfe, 0xa0},
	{0xfa, 0x11},
	{0xfc, 0x00},
	{0xf6, 0x00},
	{0xfc, 0x12},
	{0xfe, 0x00},
	{0x01, 0x40},
	{0x02, 0x12},
	{0x0d, 0x40},
	{0x14, 0x7e},
	{0x16, 0x05},
	{0x17, 0x18},
	{0x1c, 0x31},
	{0x1d, 0xb9},
	{0x1f, 0x1a},
	{0x73, 0x20},
	{0x74, 0x71},
	{0x77, 0x22},
	{0x7a, 0x08},
	{0x11, 0x18},
	{0x13, 0x48},
	{0x12, 0xc8},
	{0x70, 0xc8},
	{0x7b, 0x18},
	{0x7d, 0x30},
	{0x7e, 0x02},
	{0xfe, 0x10},
	{0xfe, 0x00},
	{0xfe, 0x00},
	{0xfe, 0x00},
	{0xfe, 0x00},
	{0xfe, 0x00},
	{0xfe, 0x10},
	{0xfe, 0x00},
	{0x49, 0x61},
	{0x4a, 0x40},
	{0x4b, 0x58},
	{0xfe, 0x00},
	{0x39, 0x02},
	{0x3a, 0x80},
	{0x20, 0x7e},
	{0x26, 0x87},
	{0x33, 0x10},
	{0x37, 0x06},
	{0x2a, 0x21},
	{0x3f, 0x16},
	{0x52, 0xa6},
	{0x53, 0x81},
	{0x54, 0x43},
	{0x56, 0x78},
	{0x57, 0xaa},
	{0x58, 0xff},
	{0x5b, 0x60},
	{0x5c, 0x50},
	{0xab, 0x2a},
	{0xac, 0xb5},
	{0x5e, 0x06},
	{0x5f, 0x06},
	{0x60, 0x44},
	{0x61, 0xff},
	{0x62, 0x69},
	{0x63, 0x13},
	{0x65, 0x13},
	{0x66, 0x26},
	{0x67, 0x07},
	{0x68, 0xf5},
	{0x69, 0xea},
	{0x6a, 0x21},
	{0x6b, 0x21},
	{0x6c, 0xe4},
	{0x6d, 0xfb},
	{0x81, 0x3b},
	{0x82, 0x3b},
	{0x83, 0x4b},
	{0x84, 0x90},
	{0x86, 0xf0},
	{0x87, 0x1d},
	{0x88, 0x16},
	{0x8d, 0x74},
	{0x8e, 0x25},
	{0x90, 0x36},
	{0x92, 0x43},
	{0x9d, 0x32},
	{0x9e, 0x81},
	{0x9f, 0xf4},
	{0xa0, 0xa0},
	{0xa1, 0x04},
	{0xa3, 0x2d},
	{0xa4, 0x01},
	{0xb0, 0xc2},
	{0xb1, 0x1e},
	{0xb2, 0x10},
	{0xb3, 0x20},
	{0xb4, 0x2d},
	{0xb5, 0x1b},
	{0xb6, 0x2e},
	{0xb8, 0x13},
	{0xba, 0x60},
	{0xbb, 0x62},
	{0xbd, 0x78},
	{0xbe, 0x55},
	{0xbf, 0xa0},
	{0xc4, 0xe7},
	{0xc5, 0x15},
	{0xc6, 0x16},
	{0xc7, 0xeb},
	{0xc8, 0xe4},
	{0xc9, 0x16},
	{0xca, 0x16},
	{0xcb, 0xe9},
	{0x22, 0xf8},
	{0xfe, 0x02},
	{0x01, 0x01},
	{0x02, 0x02},
	{0x03, 0x20},
	{0x04, 0x20},
	{0x0a, 0x00},
	{0x13, 0x10},
	{0x24, 0x00},
	{0x28, 0x03},
	{0xfe, 0x00},
	{0xf2, 0x00},
	{0xfe, 0x00},
	{0xfe, 0x00},
};

static kal_uint16 read_cmos_sensor(kal_uint8 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[1] = { (char)addr };

	iReadRegI2CTiming(pu_send_cmd, 1, (u8 *)&get_byte, 1, GC6153_I2C_ADDR, GC6153_I2C_SPEED);
	return get_byte;
}

static void write_cmos_sensor(kal_uint8 addr, kal_uint8 val)
{
	char pu_send_cmd[2] = { (char)addr, (char)val };

	iWriteRegI2CTiming(pu_send_cmd, 2, GC6153_I2C_ADDR, GC6153_I2C_SPEED);
}

static kal_uint32 get_imgsensor_id(kal_uint32 *sensor_id)
{
	kal_uint32 id = 0;

	id = ((read_cmos_sensor(0xf0) << 8) | read_cmos_sensor(0xf1));
	if (id != 0x6153) {
		pr_info(PFX " read id failed: 0x%x, try again\n", id);
		id = ((read_cmos_sensor(0xf0) << 8) | read_cmos_sensor(0xf1));
	}

	if (sensor_id)
		*sensor_id = id;

	if (id != 0x6153) {
		pr_info(PFX " read id failed: 0x%x\n", id);
		if (sensor_id)
			*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}

	pr_info(PFX " Sensor found ID = 0x%x\n", id);
	return ERROR_NONE;
}

static kal_uint32 open(void)
{
	kal_uint32 sensor_id = 0;
	int i;

	if (get_imgsensor_id(&sensor_id) != ERROR_NONE)
		return ERROR_SENSOR_CONNECT_FAIL;

	pr_info(PFX " open sensor\n");
	for (i = 0; i < ARRAY_SIZE(gc6153_init_regs); i++) {
		write_cmos_sensor(gc6153_init_regs[i].reg, gc6153_init_regs[i].val);
	}

	return ERROR_NONE;
}

static kal_uint32 close(void)
{
	return ERROR_NONE;
}

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			  MSDK_SENSOR_INFO_STRUCT *sensor_info,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	if (!sensor_info)
		return ERROR_INVALID_PARA;

	pr_info(PFX " get_info scenario_id: %d\n", scenario_id);

	((char *)sensor_info)[138] = 0;
	*(kal_uint32 *)(((char *)sensor_info) + 13) = 0x02000101;
	*(kal_uint16 *)(((char *)sensor_info) + 136) = 1;
	((char *)sensor_info)[8] = 24;
	((char *)sensor_info)[17] = 3;
	*(kal_uint16 *)(((char *)sensor_info) + 18) = 515;

	return ERROR_NONE;
}

static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	return ERROR_NONE;
}

static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			 MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			 MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	return ERROR_NONE;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
				 kal_uint8 *feature_para,
				 kal_uint32 *feature_para_len)
{
	if (!feature_para)
		return ERROR_INVALID_PARA;

	switch (feature_id) {
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		get_imgsensor_id((kal_uint32 *)feature_para);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
	case 3132:
		write_cmos_sensor(0xfe, 0x00);
		*(kal_int16 *)feature_para = (kal_int16)read_cmos_sensor(0x93);
		break;
	default:
		break;
	}

	return ERROR_NONE;
}

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

kal_uint32 GC6153_SERIAL_YUV_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	if (pfFunc)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}
