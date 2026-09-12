// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

/*
 * FP5516WE4AF voice coil motor driver
 */

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>

#include "lens_info.h"

#define AF_DRVNAME "FP5516WE4AF_DRV"
#define AF_I2C_SLAVE_ADDR 0x18

#define AF_DEBUG
#ifdef AF_DEBUG
#define LOG_INF(format, args...) \
	pr_info(AF_DRVNAME " [%s] " format, __func__, ##args)
#else
#define LOG_INF(format, args...)
#endif

static struct i2c_client *g_pstAF_I2Cclient;
static int *g_pAF_Opened;
static spinlock_t *g_pAF_SpinLock;

static unsigned long g_u4AF_INF;
static unsigned long g_u4AF_MACRO = 1023;
static unsigned long g_u4CurrPosition;
static unsigned long g_u4TargetPosition;

static const u8 init_cmd_array[7][2] = {
	{0x02, 0x01},
	{0x02, 0x00},
	{0xFE, 0xFE},
	{0x02, 0x02},
	{0x06, 0x40},
	{0x07, 0x60},
	{0xFE, 0xFE},
};

static int initAF(void)
{
	int i;
	char buf[2];

	if (!g_pstAF_I2Cclient)
		return -1;

	g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR >> 1;

	for (i = 0; i < 7; i++) {
		if (init_cmd_array[i][0] == 0xFE) {
			udelay(100);
		} else {
			buf[0] = init_cmd_array[i][0];
			buf[1] = init_cmd_array[i][1];
			if (i2c_master_send(g_pstAF_I2Cclient, buf, 2) < 0) {
				LOG_INF("init cmd %d failed\n", i);
				return -1;
			}
		}
	}

	spin_lock(g_pAF_SpinLock);
	*g_pAF_Opened = 2;
	spin_unlock(g_pAF_SpinLock);

	return 0;
}

static inline int getAFInfo(__user struct stAF_MotorInfo *pstMotorInfo)
{
	struct stAF_MotorInfo stMotorInfo;

	stMotorInfo.u4MacroPosition = g_u4AF_MACRO;
	stMotorInfo.u4InfPosition = g_u4AF_INF;
	stMotorInfo.u4CurrentPosition = g_u4CurrPosition;
	stMotorInfo.bIsSupportSR = 1;
	stMotorInfo.bIsMotorMoving = 1;

	if (*g_pAF_Opened >= 1)
		stMotorInfo.bIsMotorOpen = 1;
	else
		stMotorInfo.bIsMotorOpen = 0;

	if (copy_to_user(pstMotorInfo, &stMotorInfo,
			 sizeof(struct stAF_MotorInfo)))
		LOG_INF("copy to user failed when getting motor information\n");

	return 0;
}

static inline int moveAF(unsigned long a_u4Position)
{
	char puSendCmd[3];
	int ret;

	if (*g_pAF_Opened == 1) {
		initAF();
	}

	if (a_u4Position == 0)
		a_u4Position = 512;

	if ((a_u4Position > g_u4AF_MACRO) || (a_u4Position < g_u4AF_INF))
		return -EINVAL;

	spin_lock(g_pAF_SpinLock);
	g_u4TargetPosition = a_u4Position;
	spin_unlock(g_pAF_SpinLock);

	puSendCmd[0] = 0x03;
	puSendCmd[1] = (char)((g_u4TargetPosition >> 8) & 0xFF);
	puSendCmd[2] = (char)(g_u4TargetPosition & 0xFF);

	if (!g_pstAF_I2Cclient)
		return -1;

	g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR >> 1;
	ret = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 3);
	if (ret < 0) {
		LOG_INF("I2C send failed!!\n");
		return -1;
	}

	spin_lock(g_pAF_SpinLock);
	g_u4CurrPosition = g_u4TargetPosition;
	spin_unlock(g_pAF_SpinLock);

	return 0;
}

static inline int setAFInf(unsigned long a_u4Position)
{
	spin_lock(g_pAF_SpinLock);
	g_u4AF_INF = a_u4Position;
	spin_unlock(g_pAF_SpinLock);
	return 0;
}

static inline int setAFMacro(unsigned long a_u4Position)
{
	spin_lock(g_pAF_SpinLock);
	g_u4AF_MACRO = a_u4Position;
	spin_unlock(g_pAF_SpinLock);
	return 0;
}

long FP5516WE4AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
		    unsigned long a_u4Param)
{
	long i4RetValue = 0;

	switch (a_u4Command) {
	case AFIOC_G_MOTORINFO:
		i4RetValue =
			getAFInfo((__user struct stAF_MotorInfo *)(a_u4Param));
		break;

	case AFIOC_T_MOVETO:
		i4RetValue = moveAF(a_u4Param);
		break;

	case AFIOC_T_SETINFPOS:
		i4RetValue = setAFInf(a_u4Param);
		break;

	case AFIOC_T_SETMACROPOS:
		i4RetValue = setAFMacro(a_u4Param);
		break;

	default:
		LOG_INF("No CMD\n");
		i4RetValue = -EPERM;
		break;
	}

	return i4RetValue;
}

int FP5516WE4AF_Release(struct inode *a_pstInode, struct file *a_pstFile)
{
	if (!g_pstAF_I2Cclient || !g_pAF_Opened)
		return 0;

	if (*g_pAF_Opened == 2) {
		char puSendCmd[3];
		int i;

		/* Smooth damping to resting position 512 */
		puSendCmd[0] = 0x03;
		puSendCmd[1] = 0x02; /* 512 = 0x0200 */
		puSendCmd[2] = 0x00;
		g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR >> 1;
		i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 3);

		for (i = 0; i < 12; i++)
			udelay(1000);
	}

	if (*g_pAF_Opened) {
		char puSendCmd[2];

		spin_lock(g_pAF_SpinLock);
		*g_pAF_Opened = 0;
		spin_unlock(g_pAF_SpinLock);

		udelay(1000);

		puSendCmd[0] = 0x02;
		puSendCmd[1] = 0x01; /* Power down mode */
		g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR >> 1;
		i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2);
	}

	return 0;
}

int FP5516WE4AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
			  spinlock_t *pAF_SpinLock, int *pAF_Opened)
{
	g_pstAF_I2Cclient = pstAF_I2Cclient;
	g_pAF_SpinLock = pAF_SpinLock;
	g_pAF_Opened = pAF_Opened;

	return 1;
}

int FP5516WE4AF_GetFileName(unsigned char *pFileName)
{
	#if SUPPORT_GETTING_LENS_FOLDER_NAME
	char FilePath[256];
	char *FileString;

	sprintf(FilePath, "%s", __FILE__);
	FileString = strrchr(FilePath, '/');
	*FileString = '\0';
	FileString = (strrchr(FilePath, '/') + 1);
	strncpy(pFileName, FileString, AF_MOTOR_NAME);
	LOG_INF("FileName : %s\n", pFileName);
	#else
	pFileName[0] = '\0';
	#endif
	return 1;
}
