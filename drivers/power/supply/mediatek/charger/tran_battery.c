// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Transsion Holdings
 * Ported for Tecno Pova 2 (LE7n)
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <mt-plat/charger_class.h>

#ifdef CONFIG_SWITCH
#include <linux/switch.h>
extern struct switch_dev otg_state_dev;
#endif

extern void mt_usb_host_connect(int delay);
extern void mt_usb_host_disconnect(int delay);

static int tran_otg_ctl_val;

static ssize_t OTG_CTL_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", tran_otg_ctl_val);
}

static ssize_t OTG_CTL_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;
	struct charger_device *primary_chg;

	if (kstrtoint(buf, 10, &val) != 0)
		return -EINVAL;

	primary_chg = get_charger_by_name("primary_chg");

	pr_info("[tran_battery] OTG_CTL set to %d\n", val);

	if (val != 0) {
		tran_otg_ctl_val = 1;
		if (primary_chg) {
			charger_dev_enable(primary_chg, false);
			charger_dev_enable_otg(primary_chg, true);
			charger_dev_set_boost_current_limit(primary_chg, 1500000);
		}
		mt_usb_host_connect(0);
#ifdef CONFIG_SWITCH
		switch_set_state(&otg_state_dev, 1);
#endif
	} else {
		tran_otg_ctl_val = 0;
		if (primary_chg) {
			charger_dev_enable_otg(primary_chg, false);
			charger_dev_enable(primary_chg, true);
		}
		mt_usb_host_disconnect(0);
#ifdef CONFIG_SWITCH
		switch_set_state(&otg_state_dev, 0);
#endif
	}

	return count;
}
static struct device_attribute dev_attr_OTG_CTL = __ATTR(OTG_CTL, 0664, OTG_CTL_show, OTG_CTL_store);

/* Additional Transsion battery sysfs stubs expected by Transsion framework & daemons */
static ssize_t tran_bat_temp_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "250\n");
}
static DEVICE_ATTR_RO(tran_bat_temp);

static ssize_t tran_pcb_thermal_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "250\n");
}
static DEVICE_ATTR_RO(tran_pcb_thermal);

static ssize_t fg_coulomb_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "0\n");
}
static DEVICE_ATTR_RO(fg_coulomb);

static ssize_t battery_qmax_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "7000000\n");
}
static DEVICE_ATTR_RO(battery_qmax);

static ssize_t tran_cam_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "0\n");
}
static DEVICE_ATTR_RO(tran_cam);

static ssize_t Charging_CallState_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "0\n");
}
static DEVICE_ATTR_RO(Charging_CallState);

static ssize_t CHG_CAPACITY_TEST_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "0\n");
}
static DEVICE_ATTR_RO(CHG_CAPACITY_TEST);

static ssize_t Pump_Express_VCharger_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "5000\n");
}
static DEVICE_ATTR_RO(Pump_Express_VCharger);

static ssize_t Pump_Express_ICharger_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "2000\n");
}
static DEVICE_ATTR_RO(Pump_Express_ICharger);

static struct attribute *tran_battery_attrs[] = {
	&dev_attr_OTG_CTL.attr,
	&dev_attr_tran_bat_temp.attr,
	&dev_attr_tran_pcb_thermal.attr,
	&dev_attr_fg_coulomb.attr,
	&dev_attr_battery_qmax.attr,
	&dev_attr_tran_cam.attr,
	&dev_attr_Charging_CallState.attr,
	&dev_attr_CHG_CAPACITY_TEST.attr,
	&dev_attr_Pump_Express_VCharger.attr,
	&dev_attr_Pump_Express_ICharger.attr,
	NULL,
};
ATTRIBUTE_GROUPS(tran_battery);

static int tran_battery_probe(struct platform_device *pdev)
{
	int ret;

	pr_info("[tran_battery] probing tran_battery driver\n");

	ret = sysfs_create_groups(&pdev->dev.kobj, tran_battery_groups);
	if (ret) {
		dev_err(&pdev->dev, "failed to create sysfs groups: %d\n", ret);
		return ret;
	}

	pr_info("[tran_battery] tran_battery probed successfully\n");
	return 0;
}

static int tran_battery_remove(struct platform_device *pdev)
{
	sysfs_remove_groups(&pdev->dev.kobj, tran_battery_groups);
	return 0;
}

static const struct of_device_id tran_battery_of_match[] = {
	{ .compatible = "tran,chg_fun", },
	{},
};
MODULE_DEVICE_TABLE(of, tran_battery_of_match);

static struct platform_driver tran_battery_driver = {
	.probe = tran_battery_probe,
	.remove = tran_battery_remove,
	.driver = {
		.name = "tran_battery",
		.of_match_table = tran_battery_of_match,
	},
};

static int __init tran_battery_init(void)
{
	return platform_driver_register(&tran_battery_driver);
}
late_initcall(tran_battery_init);

static void __exit tran_battery_exit(void)
{
	platform_driver_unregister(&tran_battery_driver);
}
module_exit(tran_battery_exit);

MODULE_DESCRIPTION("Transsion Battery and OTG Control Driver");
MODULE_LICENSE("GPL v2");
