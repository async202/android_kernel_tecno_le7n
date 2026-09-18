/*
 * @file   silead_fp_mtk.c
 * @brief  Contains silead_fp device implements for Mediatek platform.
 *
 * Copyright 2016-2018 Silead Inc.
 *
 * The code contained herein is licensed under the GNU General Public
 * License.
 */

#ifdef BSP_SIL_PLAT_MTK

#ifndef __SILEAD_FP_MTK__
#define __SILEAD_FP_MTK__

#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <mt-plat/upmu_common.h>

#if !defined(CONFIG_MTK_CLKMGR)
#include <linux/clk.h>
#endif

extern void mt_spi_enable_master_clk(struct spi_device *spidev);
extern void mt_spi_disable_master_clk(struct spi_device *spidev);

const static uint8_t TANAME[] = { 0x51, 0x1E, 0xAD, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

static irqreturn_t silfp_irq_handler(int irq, void *dev_id);
static void silfp_work_func(struct work_struct *work);
static int silfp_input_init(struct silfp_data *fp_dev);
static int silfp_pull_down_cs(struct silfp_data *fp_dev, int active);

/* -------------------------------------------------------------------- */
/*                            power supply                              */
/* -------------------------------------------------------------------- */
static void silfp_hw_poweron(struct silfp_data *fp_dev)
{
	int err = 0;

#ifdef BSP_SIL_POWER_SUPPLY_REGULATOR
	if (fp_dev->avdd_ldo) {
		err = regulator_set_voltage(fp_dev->avdd_ldo, AVDD_MIN, AVDD_MAX);
		err = regulator_enable(fp_dev->avdd_ldo);
	}
#endif

	fp_dev->power_is_off = 0;
	LOG_MSG_DEBUG(INFO_LOG, "%s: power supply ret:%d \n", __func__, err);
}

static void silfp_hw_poweroff(struct silfp_data *fp_dev)
{
	LOG_MSG_DEBUG(INFO_LOG, "[%s] enter.\n", __func__);
#ifdef BSP_SIL_POWER_SUPPLY_REGULATOR
	if (fp_dev->avdd_ldo && (regulator_is_enabled(fp_dev->avdd_ldo) > 0)) {
		regulator_disable(fp_dev->avdd_ldo);
	}
#endif
	fp_dev->power_is_off = 1;
}

static void silfp_power_deinit(struct silfp_data *fp_dev)
{
	LOG_MSG_DEBUG(INFO_LOG, "[%s] enter.\n", __func__);
#ifdef BSP_SIL_POWER_SUPPLY_REGULATOR
	if (fp_dev->avdd_ldo) {
		if (regulator_is_enabled(fp_dev->avdd_ldo) > 0)
			regulator_disable(fp_dev->avdd_ldo);
		regulator_put(fp_dev->avdd_ldo);
		fp_dev->avdd_ldo = NULL;
	}
#endif
}

/* -------------------------------------------------------------------- */
/*                            hardware reset                            */
/* -------------------------------------------------------------------- */
static void silfp_hw_reset(struct silfp_data *fp_dev, u8 delay)
{
	LOG_MSG_DEBUG(INFO_LOG, "[%s] enter, port=%d\n", __func__, fp_dev->rst_port);

	if (fp_dev->pin.pinctrl && !IS_ERR_OR_NULL(fp_dev->pin.pins_rst_l)) {
		pinctrl_select_state(fp_dev->pin.pinctrl, fp_dev->pin.pins_rst_l);
	}
	if (gpio_is_valid(fp_dev->rst_port)) {
		gpio_direction_output(fp_dev->rst_port, 0);
	}
	if (fp_dev->irq_no_use && fp_dev->pin.pinctrl && !IS_ERR_OR_NULL(fp_dev->pin.pins_irq_rst_h)) {
		pinctrl_select_state(fp_dev->pin.pinctrl, fp_dev->pin.pins_irq_rst_h);
	}
	mdelay((delay ? delay : 2) * RESET_TIME_MULTIPLE);

	if (fp_dev->pin.pinctrl && !IS_ERR_OR_NULL(fp_dev->pin.pins_rst_h)) {
		pinctrl_select_state(fp_dev->pin.pinctrl, fp_dev->pin.pins_rst_h);
	}
	if (gpio_is_valid(fp_dev->rst_port)) {
		gpio_direction_output(fp_dev->rst_port, 1);
	}
	if (fp_dev->irq_no_use && fp_dev->pin.pinctrl && !IS_ERR_OR_NULL(fp_dev->pin.pins_irq_rst_l)) {
		pinctrl_select_state(fp_dev->pin.pinctrl, fp_dev->pin.pins_irq_rst_l);
	}
	mdelay((delay ? delay : 3) * RESET_TIME_MULTIPLE);
}

/* -------------------------------------------------------------------- */
/*                            power  down                               */
/* -------------------------------------------------------------------- */
static void silfp_pwdn(struct silfp_data *fp_dev, u8 flag_avdd)
{
	LOG_MSG_DEBUG(INFO_LOG, "[%s] enter, port=%d\n", __func__, fp_dev->rst_port);

	if (SIFP_PWDN_FLASH == flag_avdd) {
		silfp_hw_poweroff(fp_dev);
		msleep(200 * RESET_TIME_MULTIPLE);
		silfp_hw_poweron(fp_dev);
	}
	if (fp_dev->pin.pinctrl && !IS_ERR_OR_NULL(fp_dev->pin.pins_rst_l)) {
		pinctrl_select_state(fp_dev->pin.pinctrl, fp_dev->pin.pins_rst_l);
	}
	if (gpio_is_valid(fp_dev->rst_port)) {
		gpio_direction_output(fp_dev->rst_port, 0);
	}
	if (fp_dev->irq_no_use && fp_dev->pin.pinctrl && !IS_ERR_OR_NULL(fp_dev->pin.pins_irq_rst_h)) {
		pinctrl_select_state(fp_dev->pin.pinctrl, fp_dev->pin.pins_irq_rst_h);
	}

	if (SIFP_PWDN_POWEROFF == flag_avdd) {
		silfp_pull_down_cs(fp_dev, 1);
		silfp_hw_poweroff(fp_dev);
	}
}

/* -------------------------------------------------------------------- */
/*                         init/deinit functions                        */
/* -------------------------------------------------------------------- */
static int silfp_parse_dts(struct silfp_data *fp_dev)
{
#ifdef CONFIG_OF
	struct device_node *node = NULL;

	if (fp_dev->spi && fp_dev->spi->dev.of_node)
		node = fp_dev->spi->dev.of_node;
	if (!node)
		node = of_find_compatible_node(NULL, NULL, "tran_fp");
	if (!node)
		node = of_find_compatible_node(NULL, NULL, "silead,silead_fp");
	if (!node)
		node = of_find_compatible_node(NULL, NULL, "mediatek,fp_node");

	if (node) {
		fp_dev->int_port = irq_of_parse_and_map(node, 0);
		if (!fp_dev->int_port) {
			int irq_gpio = of_get_named_gpio(node, "irq-gpio-std", 0);
			if (!gpio_is_valid(irq_gpio))
				irq_gpio = of_get_named_gpio(node, "irq-gpio", 0);
			if (gpio_is_valid(irq_gpio))
				fp_dev->int_port = gpio_to_irq(irq_gpio);
		}

		fp_dev->rst_port = of_get_named_gpio(node, "reset-gpio-std", 0);
		if (!gpio_is_valid(fp_dev->rst_port))
			fp_dev->rst_port = of_get_named_gpio(node, "reset-gpio", 0);

		fp_dev->avdd_ldo = regulator_get(&fp_dev->spi->dev, "vmch");
		if (IS_ERR_OR_NULL(fp_dev->avdd_ldo)) {
			fp_dev->avdd_ldo = regulator_get(&fp_dev->spi->dev, "vfp");
			if (IS_ERR_OR_NULL(fp_dev->avdd_ldo))
				fp_dev->avdd_ldo = NULL;
		}

		fp_dev->pin.pinctrl = devm_pinctrl_get(&fp_dev->spi->dev);
		if (!IS_ERR_OR_NULL(fp_dev->pin.pinctrl)) {
			fp_dev->pin.pins_irq = pinctrl_lookup_state(fp_dev->pin.pinctrl, "tran_fp_pin_irq");
			if (IS_ERR_OR_NULL(fp_dev->pin.pins_irq))
				fp_dev->pin.pins_irq = pinctrl_lookup_state(fp_dev->pin.pinctrl, "fingerprint_irq");

			fp_dev->pin.pins_rst_h = pinctrl_lookup_state(fp_dev->pin.pinctrl, "tran_fp_reset_high");
			if (IS_ERR_OR_NULL(fp_dev->pin.pins_rst_h))
				fp_dev->pin.pins_rst_h = pinctrl_lookup_state(fp_dev->pin.pinctrl, "reset_high");

			fp_dev->pin.pins_rst_l = pinctrl_lookup_state(fp_dev->pin.pinctrl, "tran_fp_reset_low");
			if (IS_ERR_OR_NULL(fp_dev->pin.pins_rst_l))
				fp_dev->pin.pins_rst_l = pinctrl_lookup_state(fp_dev->pin.pinctrl, "reset_low");

			fp_dev->pin.pins_default = pinctrl_lookup_state(fp_dev->pin.pinctrl, "tran_fp_default");
			if (IS_ERR_OR_NULL(fp_dev->pin.pins_default))
				fp_dev->pin.pins_default = pinctrl_lookup_state(fp_dev->pin.pinctrl, "fingerprint_default");
		}

		of_property_read_u32(node, "spi-id", &fp_dev->pin.spi_id);
		of_property_read_u32(node, "spi-irq", &fp_dev->pin.spi_irq);
		of_property_read_u32(node, "spi-reg", &fp_dev->pin.spi_reg);

		LOG_MSG_DEBUG(INFO_LOG, "%s: irq = %d, rst = %d\n", __func__, fp_dev->int_port, fp_dev->rst_port);
	} else {
		LOG_MSG_DEBUG(ERR_LOG, "%s: no compatible device node found\n", __func__);
	}

	if (!fp_dev->int_port && fp_dev->spi)
		fp_dev->int_port = fp_dev->spi->irq;

#endif /* CONFIG_OF */
	return 0;
}

/* -------------------------------------------------------------------- */
/*                     set spi to default status                        */
/* -------------------------------------------------------------------- */
static int silfp_set_default_spi_status(struct silfp_data *fp_dev, int enable)
{
	if (!IS_ERR_OR_NULL(fp_dev->pin.pins_miso_spi) && !IS_ERR_OR_NULL(fp_dev->pin.pins_miso_pulllow)) {
		if (!enable) {
			pinctrl_select_state(fp_dev->pin.pinctrl, fp_dev->pin.pins_miso_pulllow);
			pinctrl_select_state(fp_dev->pin.pinctrl, fp_dev->pin.pins_mosi_pulllow);
			pinctrl_select_state(fp_dev->pin.pinctrl, fp_dev->pin.pins_cs_pullhigh);
			pinctrl_select_state(fp_dev->pin.pinctrl, fp_dev->pin.pins_clk_pulllow);
		} else {
			pinctrl_select_state(fp_dev->pin.pinctrl, fp_dev->pin.pins_miso_spi);
			pinctrl_select_state(fp_dev->pin.pinctrl, fp_dev->pin.pins_mosi_spi);
			pinctrl_select_state(fp_dev->pin.pinctrl, fp_dev->pin.pins_cs_spi);
			pinctrl_select_state(fp_dev->pin.pinctrl, fp_dev->pin.pins_clk_spi);
		}
	}
	return 0;
}

/* -------------------------------------------------------------------- */
/*                      pull down cs-gpio                               */
/* -------------------------------------------------------------------- */
static int silfp_pull_down_cs(struct silfp_data *fp_dev, int pull_down)
{
	if (pull_down) {
		if (!IS_ERR_OR_NULL(fp_dev->pin.pins_cs_pulllow))
			pinctrl_select_state(fp_dev->pin.pinctrl, fp_dev->pin.pins_cs_pulllow);
	} else {
		if (!IS_ERR_OR_NULL(fp_dev->pin.pins_cs_pullhigh))
			pinctrl_select_state(fp_dev->pin.pinctrl, fp_dev->pin.pins_cs_pullhigh);
	}
	LOG_MSG_DEBUG(INFO_LOG, "%s, pull_down=%d\n", __func__, pull_down);
	mdelay(1);
	return 0;
}

static int silfp_set_spi(struct silfp_data *fp_dev, bool enable)
{
	if (enable && !atomic_read(&fp_dev->spionoff_count)) {
		atomic_inc(&fp_dev->spionoff_count);
		mt_spi_enable_master_clk(fp_dev->spi);
	} else if (!enable && atomic_read(&fp_dev->spionoff_count)) {
		atomic_dec(&fp_dev->spionoff_count);
		mt_spi_disable_master_clk(fp_dev->spi);
	}
	LOG_MSG_DEBUG(DBG_LOG, "[%s] done(%d)\n", __func__, enable);
	return 0;
}

static int silfp_irq_to_reset_init(struct silfp_data *fp_dev)
{
	int ret = 0;

	if (fp_dev->pin.pinctrl) {
		fp_dev->pin.pins_irq_rst_h = pinctrl_lookup_state(fp_dev->pin.pinctrl, "irq_rst-high");
		fp_dev->pin.pins_irq_rst_l = pinctrl_lookup_state(fp_dev->pin.pinctrl, "irq_rst-low");
	}

	silfp_irq_disable(fp_dev);
	if (fp_dev->irq > 0)
		free_irq(fp_dev->irq, fp_dev);
	if (fp_dev->pin.pinctrl && !IS_ERR_OR_NULL(fp_dev->pin.pins_irq_rst_l))
		pinctrl_select_state(fp_dev->pin.pinctrl, fp_dev->pin.pins_irq_rst_l);

	fp_dev->irq_no_use = 1;
	return ret;
}

static int silfp_set_feature(struct silfp_data *fp_dev, u8 feature)
{
	int ret = 0;

	switch (feature) {
	case FEATURE_FLASH_CS:
		LOG_MSG_DEBUG(INFO_LOG, "%s set feature flash cs\n", __func__);
		ret = silfp_irq_to_reset_init(fp_dev);
		break;
	default:
		break;
	}
	return ret;
}

static int silfp_resource_init(struct silfp_data *fp_dev, struct fp_dev_init_t *dev_info)
{
	int status = 0;
	int ret = 0;

	if (atomic_read(&fp_dev->init)) {
		atomic_inc(&fp_dev->init);
		LOG_MSG_DEBUG(DBG_LOG, "[%s] dev already init(%d).\n", __func__, atomic_read(&fp_dev->init));
		return status;
	}

	fp_dev->irq_no_use = 0;
	silfp_parse_dts(fp_dev);

	LOG_MSG_DEBUG(INFO_LOG, "[%s] int_port %d, rst_port %d.\n", __func__, fp_dev->int_port, fp_dev->rst_port);

	if (fp_dev->pin.pinctrl && !IS_ERR_OR_NULL(fp_dev->pin.pins_irq))
		pinctrl_select_state(fp_dev->pin.pinctrl, fp_dev->pin.pins_irq);

	fp_dev->irq = fp_dev->int_port;
	fp_dev->irq_is_disable = 0;

	if (fp_dev->irq > 0) {
		ret = request_irq(fp_dev->irq,
				  silfp_irq_handler,
				  IRQ_TYPE_EDGE_RISING,
				  "silfp",
				  fp_dev);
		if (ret < 0) {
			LOG_MSG_DEBUG(ERR_LOG, "[%s] Failed to request_irq(%d), ret=%d\n", __func__, fp_dev->irq, ret);
			status = -ENODEV;
			goto err_irq;
		} else {
			LOG_MSG_DEBUG(INFO_LOG, "[%s] Enable_irq_wake.\n", __func__);
			enable_irq_wake(fp_dev->irq);
			silfp_irq_disable(fp_dev);
		}
	}

	if (gpio_is_valid(fp_dev->rst_port)) {
		ret = gpio_request(fp_dev->rst_port, "SILFP_RST_PIN");
		if (ret < 0) {
			LOG_MSG_DEBUG(ERR_LOG, "[%s] Failed to request GPIO=%d, ret=%d\n", __func__, (s32)fp_dev->rst_port, ret);
		} else {
			gpio_direction_output(fp_dev->rst_port, 0);
		}
	}
	if (fp_dev->pin.pinctrl && !IS_ERR_OR_NULL(fp_dev->pin.pins_rst_l))
		pinctrl_select_state(fp_dev->pin.pinctrl, fp_dev->pin.pins_rst_l);

	silfp_hw_poweron(fp_dev);
	mdelay(3);

	if (gpio_is_valid(fp_dev->rst_port))
		gpio_direction_output(fp_dev->rst_port, 1);
	if (fp_dev->pin.pinctrl && !IS_ERR_OR_NULL(fp_dev->pin.pins_rst_h))
		pinctrl_select_state(fp_dev->pin.pinctrl, fp_dev->pin.pins_rst_h);

	silfp_set_default_spi_status(fp_dev, 0);

	if (silfp_input_init(fp_dev)) {
		goto err_input;
	}
	atomic_set(&fp_dev->init, 1);

	if (dev_info) {
		dev_info->reserve = PKG_SIZE;
		dev_info->reserve <<= 12;

		if (fp_dev->pin.spi_id) {
			dev_info->dev_id = (uint8_t)fp_dev->pin.spi_id;
			dev_info->reserve |= fp_dev->pin.spi_irq & 0x0FFF;
			dev_info->reg = fp_dev->pin.spi_reg;
		}
		memcpy(dev_info->ta, TANAME, sizeof(dev_info->ta));
	}

	return status;

err_input:
	if (gpio_is_valid(fp_dev->rst_port)) {
		gpio_free(fp_dev->rst_port);
	}

	if (fp_dev->irq > 0)
		free_irq(fp_dev->irq, fp_dev);

err_irq:
	fp_dev->int_port = 0;
	fp_dev->rst_port = 0;
	return status;
}

#endif /* __SILEAD_FP_MTK__ */

#endif /* BSP_SIL_PLAT_MTK */
