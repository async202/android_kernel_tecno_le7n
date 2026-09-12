/*
Novatek NT36672C LCM driver decompiled from Tecno Pova 2 stock kernel
*/
#define LOG_TAG "LCM"

#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#undef mdelay
#undef udelay
#define MDELAY(n) lcm_util.mdelay(n)
#define UDELAY(n) lcm_util.udelay(n)

#include "lcm_drv.h"

enum DTS_GPIO_STATE {
	DTS_GPIO_STATE_DEFAULT = 0,
	DTS_GPIO_STATE_LCM_RST_OUT0 = 1,
	DTS_GPIO_STATE_LCM_RST_OUT1 = 2,
	DTS_GPIO_STATE_LCD_BIAS_ENP0 = 3,
	DTS_GPIO_STATE_LCD_BIAS_ENP1 = 4,
	DTS_GPIO_STATE_LCD_BIAS_ENN0 = 5,
	DTS_GPIO_STATE_LCD_BIAS_ENN1 = 6,
	DTS_GPIO_STATE_MAX,
};
extern long disp_dts_gpio_select_state(enum DTS_GPIO_STATE s);

#define FRAME_WIDTH  (1080)
#define FRAME_HEIGHT (2460)

#define REGFLAG_DELAY_MS_V3_CMD (0xFFFC)
#define REGFLAG_END_OF_TABLE_CMD (0xFFFD)
#define REGFLAG_DELAY (0xFFFC)
#define REGFLAG_END_OF_TABLE (0xFFFD)

static struct LCM_UTIL_FUNCS lcm_util;

#define SET_RESET_PIN(v) (lcm_util.set_reset_pin((v)))
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
    lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)

struct LCM_setting_table {
    unsigned int cmd;
    unsigned char count;
    unsigned char para_list[64];
};

static struct LCM_setting_table init_setting[] = {
    {0xFF, 1, {0x10}},
    {0xFB, 1, {0x01}},
    {0xB0, 1, {0x00}},
    {0xC0, 1, {0x00}},
    {0xFF, 1, {0x20}},
    {0xFB, 1, {0x01}},
    {0x01, 1, {0x66}},
    {0x07, 1, {0x3C}},
    {0x1B, 1, {0x01}},
    {0x5C, 1, {0x90}},
    {0x5E, 1, {0xE6}},
    {0x69, 1, {0xD0}},
    {0x95, 1, {0xE5}},
    {0x96, 1, {0xE5}},
    {0xF2, 1, {0x64}},
    {0xF4, 1, {0x64}},
    {0xF6, 1, {0x64}},
    {0xF8, 1, {0x64}},
    {0xFF, 1, {0x24}},
    {0xFB, 1, {0x01}},
    {0x04, 1, {0x22}},
    {0x05, 1, {0x00}},
    {0x06, 1, {0xA3}},
    {0x07, 1, {0xA3}},
    {0x08, 1, {0xF}},
    {0x09, 1, {0xF}},
    {0x0A, 1, {0x17}},
    {0x0B, 1, {0x15}},
    {0x0C, 1, {0x13}},
    {0x0D, 1, {0x2D}},
    {0x0E, 1, {0x2C}},
    {0x0F, 1, {0x2F}},
    {0x10, 1, {0x2E}},
    {0x11, 1, {0x29}},
    {0x12, 1, {0x24}},
    {0x13, 1, {0x24}},
    {0x14, 1, {0x0B}},
    {0x15, 1, {0x0C}},
    {0x16, 1, {0x1C}},
    {0x17, 1, {0x01}},
    {0x1C, 1, {0x22}},
    {0x1D, 1, {0x00}},
    {0x1E, 1, {0xA3}},
    {0x1F, 1, {0xA3}},
    {0x20, 1, {0x0F}},
    {0x21, 1, {0x0F}},
    {0x22, 1, {0x17}},
    {0x23, 1, {0x15}},
    {0x24, 1, {0x13}},
    {0x25, 1, {0x2D}},
    {0x26, 1, {0x2C}},
    {0x27, 1, {0x2F}},
    {0x28, 1, {0x2E}},
    {0x29, 1, {0x29}},
    {0x2A, 1, {0x24}},
    {0x2B, 1, {0x24}},
    {0x2D, 1, {0x0B}},
    {0x2F, 1, {0x0C}},
    {0x30, 1, {0x1C}},
    {0x31, 1, {0x01}},
    {0x32, 1, {0x44}},
    {0x33, 1, {0x02}},
    {0x34, 1, {0x00}},
    {0x35, 1, {0x01}},
    {0x36, 1, {0x01}},
    {0x37, 1, {0x01}},
    {0x38, 1, {0x10}},
    {0x3B, 1, {0x04}},
    {0x4E, 1, {0x6C}},
    {0x4F, 1, {0x6C}},
    {0x53, 1, {0x6C}},
    {0x7A, 1, {0x83}},
    {0x7B, 1, {0x9F}},
    {0x7D, 1, {0x04}},
    {0x80, 1, {0x04}},
    {0x81, 1, {0x04}},
    {0x82, 1, {0x13}},
    {0x84, 1, {0x31}},
    {0x85, 1, {0x00}},
    {0x86, 1, {0x00}},
    {0x87, 1, {0x00}},
    {0x90, 1, {0x13}},
    {0x92, 1, {0x31}},
    {0x93, 1, {0x00}},
    {0x94, 1, {0x00}},
    {0x95, 1, {0x00}},
    {0x9C, 1, {0xF4}},
    {0x9D, 1, {0x01}},
    {0xA0, 1, {0x1F}},
    {0xA2, 1, {0x1F}},
    {0xA3, 1, {0x03}},
    {0xA4, 1, {0x04}},
    {0xA5, 1, {0x04}},
    {0xC4, 1, {0x80}},
    {0xC6, 1, {0xC0}},
    {0xC9, 1, {0x00}},
    {0xD9, 1, {0x80}},
    {0xE9, 1, {0x03}},
    {0xFF, 1, {0x25}},
    {0xFB, 1, {0x01}},
    {0x0F, 1, {0x1B}},
    {0x19, 1, {0xE4}},
    {0x21, 1, {0x40}},
    {0x58, 1, {0x0C}},
    {0x59, 1, {0x0A}},
    {0x5C, 1, {0x05}},
    {0x5F, 1, {0x10}},
    {0x66, 1, {0xD8}},
    {0x67, 1, {0x01}},
    {0x68, 1, {0x58}},
    {0x69, 1, {0x10}},
    {0x6B, 1, {0x00}},
    {0x6C, 1, {0x1D}},
    {0x71, 1, {0x1D}},
    {0x77, 1, {0x62}},
    {0x79, 1, {0x90}},
    {0x7E, 1, {0x15}},
    {0x7F, 1, {0x00}},
    {0x84, 1, {0x6D}},
    {0x8D, 1, {0x00}},
    {0xC0, 1, {0xD5}},
    {0xC1, 1, {0x11}},
    {0xC3, 1, {0x00}},
    {0xC4, 1, {0x11}},
    {0xC5, 1, {0x11}},
    {0xC6, 1, {0x11}},
    {0xEF, 1, {0x00}},
    {0xF0, 1, {0x00}},
    {0xF1, 1, {0x04}},
    {0xFF, 1, {0x26}},
    {0xFB, 1, {0x01}},
    {0x00, 1, {0x00}},
    {0x01, 1, {0xF1}},
    {0x02, 1, {0xF1}},
    {0x04, 1, {0xF7}},
    {0x05, 1, {0x08}},
    {0x06, 1, {0x27}},
    {0x07, 1, {0x27}},
    {0x08, 1, {0x27}},
    {0x14, 1, {0x06}},
    {0x74, 1, {0xAF}},
    {0x81, 1, {0x1F}},
    {0x83, 1, {0x03}},
    {0x84, 1, {0x03}},
    {0x85, 1, {0x01}},
    {0x86, 1, {0x03}},
    {0x87, 1, {0x01}},
    {0x88, 1, {0x09}},
    {0x8A, 1, {0x1A}},
    {0x8B, 1, {0x11}},
    {0x8C, 1, {0x24}},
    {0x8E, 1, {0x42}},
    {0x8F, 1, {0x11}},
    {0x90, 1, {0x11}},
    {0x91, 1, {0x11}},
    {0x9A, 1, {0x80}},
    {0x9B, 1, {0x08}},
    {0x9C, 1, {0x00}},
    {0x9D, 1, {0x00}},
    {0x9E, 1, {0x00}},
    {0xFF, 1, {0x27}},
    {0xFB, 1, {0x01}},
    {0x01, 1, {0x9C}},
    {0x20, 1, {0x82}},
    {0x21, 1, {0xCE}},
    {0x25, 1, {0x83}},
    {0x26, 1, {0x1C}},
    {0x6E, 1, {0x9A}},
    {0x6F, 1, {0x78}},
    {0x70, 1, {0x00}},
    {0x71, 1, {0x00}},
    {0x72, 1, {0x00}},
    {0x73, 1, {0x00}},
    {0x74, 1, {0x00}},
    {0x75, 1, {0x00}},
    {0x76, 1, {0x00}},
    {0x77, 1, {0x00}},
    {0x7D, 1, {0x09}},
    {0x7E, 1, {0xA5}},
    {0x7F, 1, {0x03}},
    {0x80, 1, {0x23}},
    {0x82, 1, {0x09}},
    {0x83, 1, {0xA5}},
    {0x88, 1, {0x02}},
    {0xFF, 1, {0x2A}},
    {0xFB, 1, {0x01}},
    {0x00, 1, {0x91}},
    {0x03, 1, {0x20}},
    {0x06, 1, {0x0C}},
    {0x07, 1, {0x50}},
    {0x0A, 1, {0x60}},
    {0x0C, 1, {0x0E}},
    {0x0D, 1, {0x40}},
    {0x0E, 1, {0x03}},
    {0x11, 1, {0xA3}},
    {0x15, 1, {0x0F}},
    {0x16, 1, {0x1B}},
    {0x19, 1, {0x0E}},
    {0x1A, 1, {0xEF}},
    {0x1B, 1, {0x14}},
    {0x1D, 1, {0x36}},
    {0x1E, 1, {0x74}},
    {0x1F, 1, {0x96}},
    {0x20, 1, {0x74}},
    {0x27, 1, {0x00}},
    {0x28, 1, {0xF7}},
    {0x29, 1, {0x06}},
    {0x2A, 1, {0x55}},
    {0x30, 1, {0x45}},
    {0x34, 1, {0xF9}},
    {0x35, 1, {0x33}},
    {0x36, 1, {0x15}},
    {0x37, 1, {0xF4}},
    {0x38, 1, {0x37}},
    {0x39, 1, {0x11}},
    {0x3A, 1, {0x45}},
    {0xEE, 1, {0x01}},
    {0xF0, 1, {0xAC}},
    {0xFF, 1, {0x2B}},
    {0xFB, 1, {0x01}},
    {0xB7, 1, {0x0C}},
    {0xB8, 1, {0x23}},
    {0xC0, 1, {0x02}},
    {0xFF, 1, {0xE0}},
    {0xFB, 1, {0x01}},
    {0x35, 1, {0x82}},
    {0xFF, 1, {0xF0}},
    {0xFB, 1, {0x01}},
    {0x1C, 1, {0x01}},
    {0x33, 1, {0x01}},
    {0x5A, 1, {0x00}},
    {0xD2, 1, {0x52}},
    {0xFF, 1, {0xD0}},
    {0xFB, 1, {0x01}},
    {0x53, 1, {0x22}},
    {0x54, 1, {0x02}},
    {0xFF, 1, {0xC0}},
    {0xFB, 1, {0x01}},
    {0x9C, 1, {0x11}},
    {0x9D, 1, {0x11}},
    {0xFF, 1, {0x10}},
    {0x35, 1, {0x00}},
    
    /* 0x11 (Sleep Out) - STRICTLY 0 PARAMETERS */
    {0x11, 0, {}},
    {REGFLAG_DELAY, 120, {}},
    
    /* 0x29 (Display On) - STRICTLY 0 PARAMETERS */
    {0x29, 0, {}},
    {REGFLAG_DELAY, 20, {}},
    
    {REGFLAG_END_OF_TABLE, 0x00, {}}
};

static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
    unsigned int i;
    for (i = 0; i < count; i++) {
        unsigned int cmd = table[i].cmd;
        switch (cmd) {
        case REGFLAG_DELAY_MS_V3_CMD:
            MDELAY(table[i].count);
            break;
        case REGFLAG_END_OF_TABLE_CMD:
            return;
        default:
            dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);
            break;
        }
    }
}

static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
    memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}

static void lcm_get_params(struct LCM_PARAMS *params)
{
    memset(params, 0, sizeof(struct LCM_PARAMS));

    params->type = LCM_TYPE_DSI;
    params->ctrl = LCM_CTRL_NONE;
    params->width = FRAME_WIDTH;
    params->height = FRAME_HEIGHT;

    params->dsi.mode   = BURST_VDO_MODE;
    params->dsi.LANE_NUM = LCM_FOUR_LANE;

    params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
    params->dsi.data_format.trans_seq   = LCM_DSI_TRANS_SEQ_MSB_FIRST;
    params->dsi.data_format.padding     = LCM_DSI_PADDING_ON_LSB;
    params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;

    params->dsi.intermediat_buffer_num = 0;
    params->dsi.PS         = LCM_PACKED_PS_24BIT_RGB888;
    params->dsi.word_count = FRAME_WIDTH * 3;

    /* Vertical timings */
    params->dsi.vertical_sync_active = 10;
    params->dsi.vertical_backporch   = 10;
    params->dsi.vertical_frontporch  = 54;
    params->dsi.vertical_active_line = FRAME_HEIGHT;

    /* Horizontal timings */
    params->dsi.horizontal_sync_active    = 4;
    params->dsi.horizontal_backporch      = 22;
    params->dsi.horizontal_frontporch     = 20;
    params->dsi.horizontal_blanking_pixel = 0;

    params->dsi.PLL_CLOCK = 552;
    params->dsi.ssc_disable = 1;
    params->dsi.ssc_range = 4;
    params->dsi.vertical_frontporch_for_low_power = 54;

    /* Continuous HS clock */
    params->dsi.cont_clock             = 1;
    params->dsi.noncont_clock          = 0;
    params->dsi.lcm_ext_te_enable      = 1;
    params->dsi.lcm_ext_te_monitor     = 1;
    params->dsi.clk_lp_per_line_enable = 0;

    params->dsi.esd_check_enable = 0;
    params->dsi.customization_esd_check_enable = 0;
}

static int nt50358_write_byte(unsigned char cmd, unsigned char data)
{
    unsigned char buf[2] = {cmd, data};
    struct i2c_msg msg = {
        .addr = 0x3E,
        .flags = 0,
        .len = 2,
        .buf = buf,
    };
    struct i2c_adapter *adap;
    int ret = -1;
    int bus;

    adap = i2c_get_adapter(6);
    if (adap) {
        ret = i2c_transfer(adap, &msg, 1);
        i2c_put_adapter(adap);
        if (ret == 1) {
            pr_info("[LCM] NT50358 write (0x%02X=0x%02X) on i2c6 SUCCESS\n", cmd, data);
            return 0;
        }
    }

    for (bus = 0; bus <= 8; bus++) {
        if (bus == 6) continue;
        adap = i2c_get_adapter(bus);
        if (adap) {
            ret = i2c_transfer(adap, &msg, 1);
            i2c_put_adapter(adap);
            if (ret == 1) {
                pr_info("[LCM] NT50358 write (0x%02X=0x%02X) on i2c%d SUCCESS\n", cmd, data, bus);
                return 0;
            }
        }
    }
    pr_warn("[LCM] NT50358 write (0x%02X=0x%02X) failed on all buses\n", cmd, data);
    return ret;
}

static unsigned char g_lcm_suspend_power_flag = 0;

static void lcm_init_power(void)
{
    pr_info("[LCM] %s: LCD bias enable (+5.5V / -5.5V)\n", __func__);

    disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_BIAS_ENP1);
    MDELAY(5);
    disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_BIAS_ENN1);
    MDELAY(1);

    nt50358_write_byte(0x00, 0x0F);
    MDELAY(1);
    nt50358_write_byte(0x01, 0x0F);
    MDELAY(1);
    nt50358_write_byte(0x03, 0x33);
    MDELAY(1);
    nt50358_write_byte(0xFF, 0x80);
    MDELAY(15);
}

static void lcm_suspend_power(void)
{
    if (g_lcm_suspend_power_flag != 0)
        return;

    pr_info("[LCM] %s: LCD bias disable\n", __func__);
    disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_BIAS_ENN0);
    MDELAY(5);
    disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_BIAS_ENP0);
    g_lcm_suspend_power_flag = 1;
}

static void lcm_resume_power(void)
{
    if (g_lcm_suspend_power_flag != 1)
        return;

    pr_info("[LCM] %s: LCD bias resume (+5.5V / -5.5V)\n", __func__);
    disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_BIAS_ENP1);
    MDELAY(5);
    disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_BIAS_ENN1);
    MDELAY(1);

    nt50358_write_byte(0x00, 0x0F);
    MDELAY(1);
    nt50358_write_byte(0x01, 0x0F);
    MDELAY(1);
    nt50358_write_byte(0x03, 0x33);
    MDELAY(1);
    nt50358_write_byte(0xFF, 0x80);
    MDELAY(15);

    g_lcm_suspend_power_flag = 0;
}

static void lcm_init(void)
{
    pr_info("[LCM] %s: start panel reset and init\n", __func__);

    disp_dts_gpio_select_state(DTS_GPIO_STATE_LCM_RST_OUT1);
    MDELAY(5);
    disp_dts_gpio_select_state(DTS_GPIO_STATE_LCM_RST_OUT0);
    MDELAY(5);
    disp_dts_gpio_select_state(DTS_GPIO_STATE_LCM_RST_OUT1);
    MDELAY(15);

    push_table(init_setting, sizeof(init_setting) / sizeof(struct LCM_setting_table), 1);
    pr_info("[LCM] %s: init table pushed\n", __func__);
}

static void lcm_suspend(void)
{
    pr_info("[LCM] %s: panel suspend\n", __func__);
    dsi_set_cmdq_V2(0x28, 0, NULL, 1);
    MDELAY(20);
    dsi_set_cmdq_V2(0x10, 0, NULL, 1);
    MDELAY(120);
}

static void lcm_resume(void)
{
    pr_info("[LCM] %s: start panel reset and resume\n", __func__);

    disp_dts_gpio_select_state(DTS_GPIO_STATE_LCM_RST_OUT1);
    MDELAY(5);
    disp_dts_gpio_select_state(DTS_GPIO_STATE_LCM_RST_OUT0);
    MDELAY(5);
    disp_dts_gpio_select_state(DTS_GPIO_STATE_LCM_RST_OUT1);
    MDELAY(15);

    push_table(init_setting, sizeof(init_setting) / sizeof(struct LCM_setting_table), 1);
    pr_info("[LCM] %s: resume table pushed\n", __func__);
}

static unsigned int lcm_compare_id(void)
{
    return 1;
}

struct LCM_DRIVER nt36672c_fhdp_dsi_vdo_boe_txd_le7_lcm_drv = {
    .name = "nt36672c_fhdp_dsi_vdo_boe_txd_le7",
    .set_util_funcs = lcm_set_util_funcs,
    .get_params = lcm_get_params,
    .init = lcm_init,
    .suspend = lcm_suspend,
    .resume = lcm_resume,
    .compare_id = lcm_compare_id,
    .init_power = lcm_init_power,
    .suspend_power = lcm_suspend_power,
    .resume_power = lcm_resume_power,
    .set_backlight_cmdq = NULL,
};

