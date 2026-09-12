// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

/*
 * DW9800WAF voice coil motor driver
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/fs.h>

#include "lens_info.h"

#define AF_DRVNAME "DW9800WAF_DRV"
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

int DW9800WAF_EnbleLowPower(int enable)
{
	char puSendCmd[2];

	puSendCmd[0] = 0x02;
	if (enable & 1)
		puSendCmd[1] = 0x01;
	else
		puSendCmd[1] = 0x00;

	if (!g_pstAF_I2Cclient)
		return -1;

	g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR >> 1;
	return i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2);
}

static inline int getAFInfo(__user struct stAF_MotorInfo *pstMotorInfo)
{
	struct stAF_MotorInfo stMotorInfo;

	stMotorInfo.u4MacroPosition = g_u4AF_MACRO;
	stMotorInfo.u4InfPosition = g_u4AF_INF;
	stMotorInfo.u4CurrentPosition = g_u4CurrPosition;
	stMotorInfo.bIsSupportSR = 1;
	stMotorInfo.bIsMotorMoving = 1;
	stMotorInfo.bIsMotorOpen = (*g_pAF_Opened > 0) ? 1 : 0;

	if (copy_to_user(pstMotorInfo, &stMotorInfo,
				sizeof(struct stAF_MotorInfo)))
		return -1;

	return 0;
}

static void dw9800_init_ringing(void)
{
	char puSendCmd[2];
	int ret;

	if (!g_pstAF_I2Cclient) {
		LOG_INF("no i2c client!\n");
		return;
	}

	g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR >> 1;

	/* Power down */
	puSendCmd[0] = 0x02; puSendCmd[1] = 0x01;
	ret = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2);
	LOG_INF("power down ret=%d\n", ret);

	/* Power up */
	puSendCmd[0] = 0x02; puSendCmd[1] = 0x00;
	ret = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2);
	LOG_INF("power up ret=%d\n", ret);

	udelay(1000);

	/* AESC setting / slew rate = 0x80 */
	puSendCmd[0] = 0x06; puSendCmd[1] = 0x80;
	ret = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2);
	LOG_INF("aesc ret=%d\n", ret);

	/* Set Tres = 0x65 */
	puSendCmd[0] = 0x07; puSendCmd[1] = 0x65;
	ret = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2);
	LOG_INF("tres ret=%d\n", ret);
}

static inline int moveAF(unsigned long a_u4Position)
{
	int ret = 0;
	char puSendCmd[3];

	if (!g_pstAF_I2Cclient)
		return -1;

	if (a_u4Position == 0)
		a_u4Position = 512;

	puSendCmd[0] = 0x03;
	puSendCmd[1] = (char)((a_u4Position >> 8) & 0xFF);
	puSendCmd[2] = (char)(a_u4Position & 0xFF);

	g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR >> 1;
	ret = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 3);
	if (ret < 0) {
		LOG_INF("moveAF pos=%lu failed: %d\n", a_u4Position, ret);
		return -1;
	}

	g_u4CurrPosition = a_u4Position;
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

long DW9800WAF_Ioctl(struct file *a_pstFile,
		unsigned int a_u4Command, unsigned long a_u4Param)
{
	long i4RetValue = 0;

	switch (a_u4Command) {
	case AFIOC_G_MOTORINFO:
		i4RetValue = getAFInfo((__user struct stAF_MotorInfo *) (a_u4Param));
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

	case AFIOC_S_SETPOWERDOWN:
		i4RetValue = DW9800WAF_EnbleLowPower(a_u4Param);
		break;

	default:
		i4RetValue = -1;
		break;
	}

	return i4RetValue;
}

int DW9800WAF_Release(struct inode *a_pstInode, struct file *a_pstFile)
{
	if (!g_pstAF_I2Cclient || !g_pAF_Opened)
		return 0;

	if (*g_pAF_Opened >= 1) {
		char puSendCmd[3];
		int i;

		/* Smooth damping to resting position 512 */
		puSendCmd[0] = 0x03;
		puSendCmd[1] = 0x00; /* 512 = 0x0200 */
		puSendCmd[2] = 0x00;
		g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR >> 1;
		i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 3);

		for (i = 0; i < 14; i++)
			udelay(1000);
	}

	if (*g_pAF_Opened) {
		char puSendCmd[2];

		spin_lock(g_pAF_SpinLock);
		*g_pAF_Opened = 0;
		spin_unlock(g_pAF_SpinLock);

		udelay(1000);

		puSendCmd[0] = 0x02;
		puSendCmd[1] = 0x01; /* Power down */
		g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR >> 1;
		i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2);
	}

	return 0;
}

int DW9800WAF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
		spinlock_t *pAF_SpinLock, int *pAF_Opened)
{
	g_pstAF_I2Cclient = pstAF_I2Cclient;
	g_pAF_SpinLock = pAF_SpinLock;
	g_pAF_Opened = pAF_Opened;

	if (*g_pAF_Opened == 1) {
		dw9800_init_ringing();

		g_u4CurrPosition = 0;

		spin_lock(g_pAF_SpinLock);
		*g_pAF_Opened = 2;
		spin_unlock(g_pAF_SpinLock);
	}

	return 1;
}

int DW9800WAF_GetFileName(unsigned char *pFileName)
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

void DW9800WAF_SetI2Cclient_first(struct i2c_client *pstAF_I2Cclient,
			  spinlock_t *pAF_SpinLock)
{
}
