/*
 * Code for AM335X EVM.
 *
 * Copyright (C) 2011 Texas Instruments, Inc. - http://www.ti.com/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/i2c/at24.h>
#include <linux/phy.h>
#include <linux/gpio.h>
#include <linux/leds.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/input/matrix_keypad.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/wl12xx.h>
#include <linux/ethtool.h>
#include <linux/mfd/tps65910.h>
#include <linux/mfd/tps65217.h>
#include <linux/pwm_backlight.h>
#include <linux/reboot.h>
#include <linux/pwm/pwm.h>
#include <linux/opp.h>
#include <linux/w1-gpio.h>
#include <linux/can/platform/mcp251x.h>
#include <linux/input/ti_tscadc.h>

#include <sound/tlv320aic3x.h>

/* LCD controller is similar to DA850 */
#include <video/da8xx-fb.h>

#include <mach/hardware.h>
#include <mach/board-am335xevm.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/hardware/asp.h>

#include <plat/omap_device.h>
#include <plat/irqs.h>
#include <plat/board.h>
#include <plat/common.h>
#include <plat/lcdc.h>
#include <plat/usb.h>
#include <plat/mmc.h>
#include <plat/emif.h>
#include <plat/nand.h>
#include <plat/dma-33xx.h>

#include <media/soc_camera.h>
#include <media/mt9t112.h>

#include "board-flash.h"
#include "cpuidle33xx.h"
#include "mux.h"
#include "devices.h"
#include "hsmmc.h"

/* Convert GPIO signal to GPIO pin number */
#define GPIO_TO_PIN(bank, gpio) (32 * (bank) + (gpio))

/* TLK PHY IDs */
#define TLK110_PHY_ID		0x2000A201
#define TLK110_PHY_MASK		0xfffffff0

/* BBB PHY IDs */
#define BBB_PHY_ID		0x7c0f1
#define BBB_PHY_MASK		0xfffffffe

/* TLK110 PHY register offsets */
#define TLK110_COARSEGAIN_REG	0x00A3
#define TLK110_LPFHPF_REG	0x00AC
#define TLK110_SPAREANALOG_REG	0x00B9
#define TLK110_VRCR_REG		0x00D0
#define TLK110_SETFFE_REG	0x0107
#define TLK110_FTSP_REG		0x0154
#define TLK110_ALFATPIDL_REG	0x002A
#define TLK110_PSCOEF21_REG	0x0096
#define TLK110_PSCOEF3_REG	0x0097
#define TLK110_ALFAFACTOR1_REG	0x002C
#define TLK110_ALFAFACTOR2_REG	0x0023
#define TLK110_CFGPS_REG	0x0095
#define TLK110_FTSPTXGAIN_REG	0x0150
#define TLK110_SWSCR3_REG	0x000B
#define TLK110_SCFALLBACK_REG	0x0040
#define TLK110_PHYRCR_REG	0x001F

/* TLK110 register writes values */
#define TLK110_COARSEGAIN_VAL	0x0000
#define TLK110_LPFHPF_VAL	0x8000
#define TLK110_SPANALOG_VAL	0x0000
#define TLK110_VRCR_VAL		0x0008
#define TLK110_SETFFE_VAL	0x0605
#define TLK110_FTSP_VAL		0x0255
#define TLK110_ALFATPIDL_VAL	0x7998
#define TLK110_PSCOEF21_VAL	0x3A20
#define TLK110_PSCOEF3_VAL	0x003F
#define TLK110_ALFACTOR1_VAL	0xFF80
#define TLK110_ALFACTOR2_VAL	0x021C
#define TLK110_CFGPS_VAL	0x0000
#define TLK110_FTSPTXGAIN_VAL	0x6A88
#define TLK110_SWSCR3_VAL	0x0000
#define TLK110_SCFALLBACK_VAL	0xC11D
#define TLK110_PHYRCR_VAL	0x4000

#if defined(CONFIG_TLK110_WORKAROUND) || \
		defined(CONFIG_TLK110_WORKAROUND_MODULE)
#define am335x_tlk110_phy_init()\
	do {	\
		phy_register_fixup_for_uid(TLK110_PHY_ID,\
					TLK110_PHY_MASK,\
					am335x_tlk110_phy_fixup);\
	} while (0);
#else
#define am335x_tlk110_phy_init() do { } while (0);
#endif

static const struct display_panel disp_panel = {
	WVGA,
	32,
	32,
	COLOR_ACTIVE,
};


#if defined(CONFIG_CHARGER_GPIO)

#include <linux/power_supply.h>
#include <linux/power/gpio-charger.h>

/* charger */
static char *beaglebone_batteries[] = {
	"battery",
};

static struct gpio_charger_platform_data beaglebone_charger_pdata = {
	.name = "battery-cape",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.gpio = GPIO_TO_PIN(1,16),
	.gpio_active_low = 0,
	.supplied_to = beaglebone_batteries,
	.num_supplicants = ARRAY_SIZE(beaglebone_batteries),
};

static struct platform_device beaglebone_charger_device = {
	.name = "gpio-charger",
	.dev = {
		.platform_data = &beaglebone_charger_pdata,
	},
};

#endif /* gpio-charger */


/* LCD backlight platform Data */
#define AM335X_BACKLIGHT_MAX_BRIGHTNESS        100
#define AM335X_BACKLIGHT_DEFAULT_BRIGHTNESS    50
#define AM335X_PWM_PERIOD_NANO_SECONDS        (1000000 * 5)

#define PWM_DEVICE_ID   "ecap.0"

static struct platform_pwm_backlight_data am335x_backlight_data = {
	.pwm_id         = PWM_DEVICE_ID,
	.ch             = -1,
	.lth_brightness	= 21,
	.max_brightness = AM335X_BACKLIGHT_MAX_BRIGHTNESS,
	.dft_brightness = AM335X_BACKLIGHT_DEFAULT_BRIGHTNESS,
	.pwm_period_ns  = AM335X_PWM_PERIOD_NANO_SECONDS,
};

static struct lcd_ctrl_config lcd_cfg = {
	&disp_panel,
	.ac_bias		= 255,
	.ac_bias_intrpt		= 0,
	.dma_burst_sz		= 16,
	.bpp			= 32,
	.fdd			= 0x80,
	.tft_alt_mode		= 0,
	.stn_565_mode		= 0,
	.mono_8bit_mode		= 0,
	.invert_line_clock	= 1,
	.invert_frm_clock	= 1,
	.sync_edge		= 0,
	.sync_ctrl		= 1,
	.raster_order		= 0,
};

struct da8xx_lcdc_platform_data TFC_S9700RTWV35TR_01B_pdata = {
	.manu_name		= "ThreeFive",
	.controller_data	= &lcd_cfg,
	.type			= "TFC_S9700RTWV35TR_01B",
};

#include "common.h"

static const struct display_panel bbtoys7_panel = {
	WVGA,
	16,
	16,
	COLOR_ACTIVE,
};

#define BBTOYS7LCD_PWM_DEVICE_ID   "ehrpwm.1:0"

static struct platform_pwm_backlight_data bbtoys7lcd_backlight_data = {
	.pwm_id         = BBTOYS7LCD_PWM_DEVICE_ID,
	.ch             = -1,
	.max_brightness = AM335X_BACKLIGHT_MAX_BRIGHTNESS,
	.dft_brightness = AM335X_BACKLIGHT_DEFAULT_BRIGHTNESS,
	.pwm_period_ns  = AM335X_PWM_PERIOD_NANO_SECONDS,
};

static struct lcd_ctrl_config bbtoys7_cfg = {
	&bbtoys7_panel,
	.ac_bias		= 255,
	.ac_bias_intrpt		= 0,
	.dma_burst_sz		= 16,
	.bpp			= 16,
	.fdd			= 0x80,
	.tft_alt_mode		= 0,
	.stn_565_mode		= 0,
	.mono_8bit_mode		= 0,
	.invert_line_clock	= 1,
	.invert_frm_clock	= 1,
	.sync_edge		= 0,
	.sync_ctrl		= 1,
	.raster_order		= 0,
};

struct da8xx_lcdc_platform_data bbtoys7_pdata = {
	.manu_name		= "ThreeFive",
	.controller_data	= &bbtoys7_cfg,
	.type			= "TFC_S9700RTWV35TR_01B",
};

static struct lcd_ctrl_config bbtoys35_cfg = {
	&bbtoys7_panel,
	.ac_bias		= 255,
	.ac_bias_intrpt		= 0,
	.dma_burst_sz		= 16,
	.bpp			= 16,
	.fdd			= 0x80,
	.tft_alt_mode		= 0,
	.stn_565_mode		= 0,
	.mono_8bit_mode		= 0,
	.invert_line_clock	= 1,
	.invert_frm_clock	= 1,
	.sync_edge		= 0,
	.sync_ctrl		= 1,
	.raster_order		= 0,
};

struct da8xx_lcdc_platform_data bbtoys35_pdata = {
	.manu_name		= "BBToys",
	.controller_data	= &bbtoys35_cfg,
	.type			= "CDTech_S035Q01",
};

struct da8xx_lcdc_platform_data bbtoys43_pdata = {
	.manu_name		= "BBToys",
	.controller_data	= &lcd_cfg,
	.type			= "NHD-4.3-ATXI#-T-1",
};

static const struct display_panel dvi_panel = {
	WVGA,
	16,
	16,
	COLOR_ACTIVE,
};

static struct lcd_ctrl_config dvi_cfg = {
	&dvi_panel,
	.ac_bias		= 255,
	.ac_bias_intrpt		= 0,
	.dma_burst_sz		= 16,
	.bpp			= 16,
	.fdd			= 0x80,
	.tft_alt_mode		= 0,
	.stn_565_mode		= 0,
	.mono_8bit_mode		= 0,
	.invert_line_clock	= 1,
	.invert_frm_clock	= 1,
	.sync_edge		= 0,
	.sync_ctrl		= 1,
	.raster_order		= 0,
};

struct da8xx_lcdc_platform_data dvi_pdata = {
	.manu_name		= "BBToys",
	.controller_data	= &dvi_cfg,
	.type			= "1024x768@60",
};

/* TSc controller */
#include <linux/lis3lv02d.h>

/* TSc controller */
static struct tsc_data am335x_touchscreen_data  = {
	.wires  = 4,
	.x_plate_resistance = 200,
	.mode = TI_TSCADC_TSCMODE,
};

static struct tsc_data bone_touchscreen_data  = {
	.mode = TI_TSCADC_GENMODE,
};

static u8 am335x_iis_serializer_direction1[] = {
	INACTIVE_MODE,	INACTIVE_MODE,	RX_MODE,	TX_MODE,
	INACTIVE_MODE,	INACTIVE_MODE,	INACTIVE_MODE,	INACTIVE_MODE,
	INACTIVE_MODE,	INACTIVE_MODE,	INACTIVE_MODE,	INACTIVE_MODE,
	INACTIVE_MODE,	INACTIVE_MODE,	INACTIVE_MODE,	INACTIVE_MODE,
};

static struct snd_platform_data am335x_evm_snd_data1 = {
	.tx_dma_offset	= 0x46400000,	/* McASP1 */
	.rx_dma_offset	= 0x46400000,
	.op_mode	= DAVINCI_MCASP_IIS_MODE,
	.num_serializer	= ARRAY_SIZE(am335x_iis_serializer_direction1),
	.tdm_slots	= 2,
	.serial_dir	= am335x_iis_serializer_direction1,
	.asp_chan_q	= EVENTQ_2,
	.version	= MCASP_VERSION_3,
	.txnumevt	= 1,
	.rxnumevt	= 1,
};

static struct snd_platform_data bone_snd_data1 = {
	.tx_dma_offset	= 0x46000000,	/* McASP0*/
	.rx_dma_offset	= 0x46000000,
	.op_mode	= DAVINCI_MCASP_IIS_MODE,
	.num_serializer	= ARRAY_SIZE(am335x_iis_serializer_direction1),
	.tdm_slots	= 2,
	.serial_dir	= am335x_iis_serializer_direction1,
	.asp_chan_q	= EVENTQ_2,
	.version	= MCASP_VERSION_3,
	.txnumevt	= 1,
	.rxnumevt	= 1,
};

static struct omap2_hsmmc_info am335x_mmc[] __initdata = {
	{
		.mmc            = 1,
		.caps           = MMC_CAP_4_BIT_DATA,
		.gpio_cd        = GPIO_TO_PIN(0, 6),
		.gpio_wp        = GPIO_TO_PIN(3, 18),
		.ocr_mask       = MMC_VDD_32_33 | MMC_VDD_33_34, /* 3V3 */
	},
	{
		.mmc            = 0,	/* will be set at runtime */
	},
	{
		.mmc            = 0,	/* will be set at runtime */
	},
	{}      /* Terminator */
};


#ifdef CONFIG_OMAP_MUX
static struct omap_board_mux board_mux[] __initdata = {
	AM33XX_MUX(I2C0_SDA, OMAP_MUX_MODE0 | AM33XX_SLEWCTRL_SLOW |
			AM33XX_INPUT_EN | AM33XX_PIN_OUTPUT),
	AM33XX_MUX(I2C0_SCL, OMAP_MUX_MODE0 | AM33XX_SLEWCTRL_SLOW |
			AM33XX_INPUT_EN | AM33XX_PIN_OUTPUT),
	{ .reg_offset = OMAP_MUX_TERMINATOR },
};
#else
#define	board_mux	NULL
#endif

/* module pin mux structure */
struct pinmux_config {
	const char *string_name; /* signal name format */
	int val; /* Options for the mux register value */
};

struct evm_dev_cfg {
	void (*device_init)(int evm_id, int profile);

/*
* If the device is required on both baseboard & daughter board (ex i2c),
* specify DEV_ON_BASEBOARD
*/
#define DEV_ON_BASEBOARD	0
#define DEV_ON_DGHTR_BRD	1
	u32 device_on;

	u32 profile;	/* Profiles (0-7) in which the module is present */
};

/* AM335X - CPLD Register Offsets */
#define	CPLD_DEVICE_HDR	0x00 /* CPLD Header */
#define	CPLD_DEVICE_ID	0x04 /* CPLD identification */
#define	CPLD_DEVICE_REV	0x0C /* Revision of the CPLD code */
#define	CPLD_CFG_REG	0x10 /* Configuration Register */

static struct i2c_client *cpld_client;
static u32 am335x_evm_id;
static struct omap_board_config_kernel am335x_evm_config[] __initdata = {
};

/*
* EVM Config held in On-Board eeprom device.
*
* Header Format
*
*  Name			Size	Contents
*			(Bytes)
*-------------------------------------------------------------
*  Header		4	0xAA, 0x55, 0x33, 0xEE
*
*  Board Name		8	Name for board in ASCII.
*				example "A33515BB" = "AM335X
				Low Cost EVM board"
*
*  Version		4	Hardware version code for board in
*				in ASCII. "1.0A" = rev.01.0A
*
*  Serial Number	12	Serial number of the board. This is a 12
*				character string which is WWYY4P16nnnn, where
*				WW = 2 digit week of the year of production
*				YY = 2 digit year of production
*				nnnn = incrementing board number
*
*  Configuration option	32	Codes(TBD) to show the configuration
*				setup on this board.
*
*  Available		32720	Available space for other non-volatile
*				data.
*/
struct am335x_evm_eeprom_config {
	u32	header;
	u8	name[8];
	char	version[4];
	u8	serial[12];
	u8	opt[32];
};

/*
* EVM Config held in daughter board eeprom device.
*
* Header Format
*
*  Name			Size		Contents
*			(Bytes)
*-------------------------------------------------------------
*  Header		4	0xAA, 0x55, 0x33, 0xEE
*
*  Board Name		8	Name for board in ASCII.
*				example "A335GPBD" = "AM335x
*				General Purpose Daughterboard"
*
*  Version		4	Hardware version code for board in
*				in ASCII. "1.0A" = rev.01.0A
*  Serial Number	12	Serial number of the board. This is a 12
*				character string which is: WWYY4P13nnnn, where
*				WW = 2 digit week of the year of production
*				YY = 2 digit year of production
*				nnnn = incrementing board number
*  Configuration Option	32	Codes to show the configuration
*				setup on this board.
*  CPLD Version	8		CPLD code version for board in ASCII
*				"CPLD1.0A" = rev. 01.0A of the CPLD
*  Available	32700		Available space for other non-volatile
*				codes/data
*/

struct am335x_eeprom_config1 {
	u32	header;
	u8	name[8];
	char	version[4];
	u8	serial[12];
	u8	opt[32];
	u8	cpld_ver[8];
};

static struct am335x_evm_eeprom_config config;
static struct am335x_eeprom_config1 config1;
static bool daughter_brd_detected;

struct beaglebone_cape_eeprom_config {
	u32	header;
	char  format_revision[2];
	char	name[32];
	char	version[4];
	char	manufacturer[16];
	char	partnumber[16];
	u16  numpins;
	char	serial[12];
	u16	muxdata[74];
	u16  current_3v3;
	u16  current_vdd5v;
	u16  current_sys5v;
	u16  dc;
};

static struct beaglebone_cape_eeprom_config cape_config;
static bool beaglebone_cape_detected;

/* keep track of ADC pin usage */
static int capecount = 0;
static bool beaglebone_tsadcpins_free = 1;
static bool beaglebone_leds_free = 1;
static bool beaglebone_spi1_free = 1;
static bool beaglebone_w1gpio_free = 1;
static bool beaglebone_skip_mmc0_init = 0;

#define GP_EVM_REV_IS_1_0		0x1
#define GP_EVM_REV_IS_1_0A		0x1
#define GP_EVM_REV_IS_1_1A		0x2
#define GP_EVM_REV_IS_UNKNOWN		0xFF
#define GP_EVM_ACTUALLY_BEAGLEBONE  0xBB
static unsigned int gp_evm_revision = GP_EVM_REV_IS_UNKNOWN;

unsigned int gigabit_enable = 1;

#define EEPROM_MAC_ADDRESS_OFFSET	60 /* 4+8+4+12+32 */
#define EEPROM_NO_OF_MAC_ADDR		3
static char am335x_mac_addr[EEPROM_NO_OF_MAC_ADDR][ETH_ALEN];

#define AM335X_EEPROM_HEADER		0xEE3355AA

/* current profile if exists else PROFILE_0 on error */
static u32 am335x_get_profile_selection(void)
{
	int val = 0;

	if (!cpld_client)
		/* error checking is not done in func's calling this routine.
		so return profile 0 on error */
		return 0;

	val = i2c_smbus_read_word_data(cpld_client, CPLD_CFG_REG);
	if (val < 0)
		return 0;	/* default to Profile 0 on Error */
	else
		return val & 0x7;
}

static struct pinmux_config haptics_pin_mux[] = {
	{"gpmc_ad9.ehrpwm2B",		OMAP_MUX_MODE4 |
		AM33XX_PIN_OUTPUT},
	{NULL, 0},
};

/* Module pin mux for battery cape */
static struct pinmux_config batterycape_pin_mux[] = {
	{"gpmc_a0.gpio1_16",  OMAP_MUX_MODE7 | AM33XX_PIN_INPUT},
	{NULL, 0},
};


/* Module pin mux for LCDC */
static struct pinmux_config lcdc_pin_mux[] = {
	{"gpmc_ad8.lcd_data23",		OMAP_MUX_MODE1 | AM33XX_PIN_OUTPUT},
	{"gpmc_ad9.lcd_data22",		OMAP_MUX_MODE1 | AM33XX_PIN_OUTPUT},
	{"gpmc_ad10.lcd_data21",	OMAP_MUX_MODE1 | AM33XX_PIN_OUTPUT},
	{"gpmc_ad11.lcd_data20",	OMAP_MUX_MODE1 | AM33XX_PIN_OUTPUT},
	{"gpmc_ad12.lcd_data19",	OMAP_MUX_MODE1 | AM33XX_PIN_OUTPUT},
	{"gpmc_ad13.lcd_data18",	OMAP_MUX_MODE1 | AM33XX_PIN_OUTPUT},
	{"gpmc_ad14.lcd_data17",	OMAP_MUX_MODE1 | AM33XX_PIN_OUTPUT},
	{"gpmc_ad15.lcd_data16",	OMAP_MUX_MODE1 | AM33XX_PIN_OUTPUT},
	{NULL, 0},
};


static struct pinmux_config lcdc16_pin_mux[] = {
	{"lcd_data0.lcd_data0",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
        | AM33XX_PULL_DISA},
	{"lcd_data1.lcd_data1",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
        | AM33XX_PULL_DISA},
	{"lcd_data2.lcd_data2",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
        | AM33XX_PULL_DISA},
	{"lcd_data3.lcd_data3",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
        | AM33XX_PULL_DISA},
	{"lcd_data4.lcd_data4",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
        | AM33XX_PULL_DISA},
	{"lcd_data5.lcd_data5",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
        | AM33XX_PULL_DISA},
	{"lcd_data6.lcd_data6",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
        | AM33XX_PULL_DISA},
	{"lcd_data7.lcd_data7",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
        | AM33XX_PULL_DISA},
	{"lcd_data8.lcd_data8",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
        | AM33XX_PULL_DISA},
	{"lcd_data9.lcd_data9",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
        | AM33XX_PULL_DISA},
	{"lcd_data10.lcd_data10",	OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
        | AM33XX_PULL_DISA},
	{"lcd_data11.lcd_data11",	OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
        | AM33XX_PULL_DISA},
	{"lcd_data12.lcd_data12",	OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
        | AM33XX_PULL_DISA},
	{"lcd_data13.lcd_data13",	OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
        | AM33XX_PULL_DISA},
	{"lcd_data14.lcd_data14",	OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
        | AM33XX_PULL_DISA},
	{"lcd_data15.lcd_data15",	OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
        | AM33XX_PULL_DISA},
	{"lcd_vsync.lcd_vsync",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT},
	{"lcd_hsync.lcd_hsync",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT},
	{"lcd_pclk.lcd_pclk",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT},
	{"lcd_ac_bias_en.lcd_ac_bias_en", OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT},
	{NULL, 0},
};


/* Module pin mux for Beagleboardtoys DVI cape */
static struct pinmux_config dvia1_pin_mux[] = {
	{"gpmc_a2.gpio1_18", OMAP_MUX_MODE7 | AM33XX_PIN_OUTPUT}, // USR0 LED
	{"gpmc_a3.gpio1_19", OMAP_MUX_MODE7 | AM33XX_PIN_OUTPUT}, // USR1 LED
	{"gpmc_ad7.gpmc_ad7", OMAP_MUX_MODE7 | AM33XX_PIN_OUTPUT}, // DVI PDn
	{NULL, 0},
};

static struct pinmux_config dvia2_pin_mux[] = {
	{"gpmc_a2.gpio1_18", OMAP_MUX_MODE7 | AM33XX_PIN_OUTPUT}, // USR0 LED
	{"gpmc_a3.gpio1_19", OMAP_MUX_MODE7 | AM33XX_PIN_OUTPUT}, // USR1 LED
	{"gpmc_csn2.gpio1_31", OMAP_MUX_MODE7 | AM33XX_PIN_OUTPUT}, // DVI PDn
	{NULL, 0},
};

/* Module pin mux for Beagleboardtoys 3.5" LCD cape */
static struct pinmux_config bbtoys35_pin_mux[] = {
	{"gpmc_a2.gpio1_18", OMAP_MUX_MODE7 | AM33XX_PIN_OUTPUT}, // USR0 LED
	{NULL, 0},
};

/* Module pin mux for Beagleboardtoys 7" LCD cape */
static struct pinmux_config bbtoys7_pin_mux[] = {
	{"ecap0_in_pwm0_out.gpio0_7", OMAP_MUX_MODE7 | AM33XX_PIN_OUTPUT}, // AVDD_EN
	{"gpmc_a2.ehrpwm1A", OMAP_MUX_MODE6 | AM33XX_PIN_OUTPUT}, // Backlight
	{NULL, 0},
};

/* Module pin mux for Beagleboardtoys 7" LCD cape */
static struct pinmux_config bbtoys7a2_pin_mux[] = {
	{"gpmc_csn2.gpio1_31", OMAP_MUX_MODE7 | AM33XX_PIN_OUTPUT}, // AVDD_EN
	{"gpmc_a2.ehrpwm1A", OMAP_MUX_MODE6 | AM33XX_PIN_OUTPUT}, // Backlight
	{NULL, 0},
};

/* Module pin mux for Beagleboardtoys 7" LCD cape */
static struct pinmux_config bbtoys7a3_pin_mux[] = {
	{"spi0_sclk.gpio0_2", OMAP_MUX_MODE7 | AM33XX_PIN_OUTPUT}, // AVDD_EN
	{"gpmc_a2.ehrpwm1A", OMAP_MUX_MODE6 | AM33XX_PIN_OUTPUT}, // Backlight
	{NULL, 0},
};

/* Module pin mux for Beagleboardtoys 3" LCD cape */
static struct pinmux_config bbtoys3_pin_mux[] = {
	{"gpmc_a2.ehrpwm1A", OMAP_MUX_MODE6 | AM33XX_PIN_OUTPUT}, // Backlight
	{NULL, 0},
};

static struct pinmux_config w1_gpio_pin_mux[] = {
	{"gpmc_ad3.gpio1_3",	OMAP_MUX_MODE7 | AM33XX_PIN_INPUT_PULLUP},
	{NULL, 0},
};

static struct pinmux_config tsc_pin_mux[] = {
	{"ain0.ain0",           OMAP_MUX_MODE0 | AM33XX_INPUT_EN},
	{"ain1.ain1",           OMAP_MUX_MODE0 | AM33XX_INPUT_EN},
	{"ain2.ain2",           OMAP_MUX_MODE0 | AM33XX_INPUT_EN},
	{"ain3.ain3",           OMAP_MUX_MODE0 | AM33XX_INPUT_EN},
	{"ain4.ain4",           OMAP_MUX_MODE0 | AM33XX_INPUT_EN},
	{"ain5.ain5",           OMAP_MUX_MODE0 | AM33XX_INPUT_EN},
	{"ain6.ain6",           OMAP_MUX_MODE0 | AM33XX_INPUT_EN},
	{"ain7.ain7",           OMAP_MUX_MODE0 | AM33XX_INPUT_EN},
	{"vrefp.vrefp",         OMAP_MUX_MODE0 | AM33XX_INPUT_EN},
	{"vrefn.vrefn",         OMAP_MUX_MODE0 | AM33XX_INPUT_EN},
	{NULL, 0},
};

/* Pin mux for nand flash module */
static struct pinmux_config nand_pin_mux[] = {
	{"gpmc_ad0.gpmc_ad0",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad1.gpmc_ad1",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad2.gpmc_ad2",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad3.gpmc_ad3",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad4.gpmc_ad4",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad5.gpmc_ad5",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad6.gpmc_ad6",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad7.gpmc_ad7",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_wait0.gpmc_wait0", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_wpn.gpmc_wpn",	  OMAP_MUX_MODE7 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_csn0.gpmc_csn0",	  OMAP_MUX_MODE0 | AM33XX_PULL_DISA},
	{"gpmc_advn_ale.gpmc_advn_ale",  OMAP_MUX_MODE0 | AM33XX_PULL_DISA},
	{"gpmc_oen_ren.gpmc_oen_ren",	 OMAP_MUX_MODE0 | AM33XX_PULL_DISA},
	{"gpmc_wen.gpmc_wen",     OMAP_MUX_MODE0 | AM33XX_PULL_DISA},
	{"gpmc_ben0_cle.gpmc_ben0_cle",	 OMAP_MUX_MODE0 | AM33XX_PULL_DISA},
	{NULL, 0},
};

/* Module pin mux for SPI fash */
static struct pinmux_config spi0_pin_mux[] = {
	{"spi0_sclk.spi0_sclk", OMAP_MUX_MODE0 | AM33XX_PULL_ENBL
							| AM33XX_INPUT_EN},
	{"spi0_d0.spi0_d0", OMAP_MUX_MODE0 | AM33XX_PULL_ENBL | AM33XX_PULL_UP
							| AM33XX_INPUT_EN},
	{"spi0_d1.spi0_d1", OMAP_MUX_MODE0 | AM33XX_PULL_ENBL
							| AM33XX_INPUT_EN},
	{"spi0_cs0.spi0_cs0", OMAP_MUX_MODE0 | AM33XX_PULL_ENBL | AM33XX_PULL_UP
							| AM33XX_INPUT_EN},
	{NULL, 0},
};

/* Module pin mux for SPI flash */
static struct pinmux_config spi1_pin_mux[] = {
	{"mcasp0_aclkx.spi1_sclk", OMAP_MUX_MODE3 | AM33XX_PULL_ENBL
		| AM33XX_INPUT_EN},
	{"mcasp0_fsx.spi1_d0", OMAP_MUX_MODE3 | AM33XX_PULL_ENBL
		| AM33XX_PULL_UP | AM33XX_INPUT_EN},
	{"mcasp0_axr0.spi1_d1", OMAP_MUX_MODE3 | AM33XX_PULL_ENBL
		| AM33XX_INPUT_EN},
	{"mcasp0_ahclkr.spi1_cs0", OMAP_MUX_MODE3 | AM33XX_PULL_ENBL
		| AM33XX_PULL_UP | AM33XX_INPUT_EN},
	{NULL, 0},
};

/* Module pin mux for rgmii1 */
static struct pinmux_config rgmii1_pin_mux[] = {
	{"mii1_txen.rgmii1_tctl", OMAP_MUX_MODE2 | AM33XX_PIN_OUTPUT},
	{"mii1_rxdv.rgmii1_rctl", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_txd3.rgmii1_td3", OMAP_MUX_MODE2 | AM33XX_PIN_OUTPUT},
	{"mii1_txd2.rgmii1_td2", OMAP_MUX_MODE2 | AM33XX_PIN_OUTPUT},
	{"mii1_txd1.rgmii1_td1", OMAP_MUX_MODE2 | AM33XX_PIN_OUTPUT},
	{"mii1_txd0.rgmii1_td0", OMAP_MUX_MODE2 | AM33XX_PIN_OUTPUT},
	{"mii1_txclk.rgmii1_tclk", OMAP_MUX_MODE2 | AM33XX_PIN_OUTPUT},
	{"mii1_rxclk.rgmii1_rclk", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_rxd3.rgmii1_rd3", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_rxd2.rgmii1_rd2", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_rxd1.rgmii1_rd1", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_rxd0.rgmii1_rd0", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mdio_data.mdio_data", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mdio_clk.mdio_clk", OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT_PULLUP},
	{NULL, 0},
};

/* Module pin mux for rgmii2 */
static struct pinmux_config rgmii2_pin_mux[] = {
	{"gpmc_a0.rgmii2_tctl", OMAP_MUX_MODE2 | AM33XX_PIN_OUTPUT},
	{"gpmc_a1.rgmii2_rctl", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLDOWN},
	{"gpmc_a2.rgmii2_td3", OMAP_MUX_MODE2 | AM33XX_PIN_OUTPUT},
	{"gpmc_a3.rgmii2_td2", OMAP_MUX_MODE2 | AM33XX_PIN_OUTPUT},
	{"gpmc_a4.rgmii2_td1", OMAP_MUX_MODE2 | AM33XX_PIN_OUTPUT},
	{"gpmc_a5.rgmii2_td0", OMAP_MUX_MODE2 | AM33XX_PIN_OUTPUT},
	{"gpmc_a6.rgmii2_tclk", OMAP_MUX_MODE2 | AM33XX_PIN_OUTPUT},
	{"gpmc_a7.rgmii2_rclk", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLDOWN},
	{"gpmc_a8.rgmii2_rd3", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLDOWN},
	{"gpmc_a9.rgmii2_rd2", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLDOWN},
	{"gpmc_a10.rgmii2_rd1", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLDOWN},
	{"gpmc_a11.rgmii2_rd0", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mdio_data.mdio_data", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mdio_clk.mdio_clk", OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT_PULLUP},
	{NULL, 0},
};

/* Module pin mux for mii1 */
static struct pinmux_config mii1_pin_mux[] = {
	{"mii1_rxerr.mii1_rxerr", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_txen.mii1_txen", OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT},
	{"mii1_rxdv.mii1_rxdv", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_txd3.mii1_txd3", OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT},
	{"mii1_txd2.mii1_txd2", OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT},
	{"mii1_txd1.mii1_txd1", OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT},
	{"mii1_txd0.mii1_txd0", OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT},
	{"mii1_txclk.mii1_txclk", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_rxclk.mii1_rxclk", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_rxd3.mii1_rxd3", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_rxd2.mii1_rxd2", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_rxd1.mii1_rxd1", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_rxd0.mii1_rxd0", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mdio_data.mdio_data", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mdio_clk.mdio_clk", OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT_PULLUP},
	{NULL, 0},
};

/* Module pin mux for rmii1 */
static struct pinmux_config rmii1_pin_mux[] = {
	{"mii1_crs.rmii1_crs_dv", OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_rxerr.mii1_rxerr", OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_txen.mii1_txen", OMAP_MUX_MODE1 | AM33XX_PIN_OUTPUT},
	{"mii1_txd1.mii1_txd1", OMAP_MUX_MODE1 | AM33XX_PIN_OUTPUT},
	{"mii1_txd0.mii1_txd0", OMAP_MUX_MODE1 | AM33XX_PIN_OUTPUT},
	{"mii1_rxd1.mii1_rxd1", OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_rxd0.mii1_rxd0", OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLDOWN},
	{"rmii1_refclk.rmii1_refclk", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mdio_data.mdio_data", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mdio_clk.mdio_clk", OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT_PULLUP},
	{NULL, 0},
};

static struct pinmux_config i2c1_pin_mux[] = {
	{"spi0_d1.i2c1_sda",    OMAP_MUX_MODE2 | AM33XX_SLEWCTRL_SLOW |
					AM33XX_PULL_ENBL | AM33XX_INPUT_EN},
	{"spi0_cs0.i2c1_scl",   OMAP_MUX_MODE2 | AM33XX_SLEWCTRL_SLOW |
					AM33XX_PULL_ENBL | AM33XX_INPUT_EN},
	{NULL, 0},
};

/* Pin mux for GPMC bus */
static struct pinmux_config gpmc_pin_mux[] = {
	{"gpmc_ad0.gpmc_ad0",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad1.gpmc_ad1",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad2.gpmc_ad2",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad3.gpmc_ad3",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad4.gpmc_ad4",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad5.gpmc_ad5",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad6.gpmc_ad6",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad7.gpmc_ad7",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad8.gpmc_ad8",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad9.gpmc_ad9",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad10.gpmc_ad10",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad11.gpmc_ad11",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad12.gpmc_ad12",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad13.gpmc_ad13",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad14.gpmc_ad14",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad15.gpmc_ad15",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_wait0.gpmc_wait0", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_wpn.gpmc_wpn",	  OMAP_MUX_MODE7 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_csn1.gpmc_csn1",	  OMAP_MUX_MODE0 | AM33XX_PULL_DISA},
	{"gpmc_advn_ale.gpmc_advn_ale",  OMAP_MUX_MODE0 | AM33XX_PULL_DISA},
	{"gpmc_oen_ren.gpmc_oen_ren",	 OMAP_MUX_MODE0 | AM33XX_PULL_DISA},
	{"gpmc_wen.gpmc_wen",     OMAP_MUX_MODE0 | AM33XX_PULL_DISA},
	{"gpmc_ben0_cle.gpmc_ben0_cle",	 OMAP_MUX_MODE0 | AM33XX_PULL_DISA},
	{"gpmc_clk.gpmc_clk",	 OMAP_MUX_MODE0 | AM33XX_PIN_INPUT},
	{"ecap0_in_pwm0_out.xdma_event_intr2", OMAP_MUX_MODE6 | AM33XX_PIN_INPUT}, // DMAREQ
	{NULL, 0},
};

static struct pinmux_config camera_cape_pin_mux[] = {
	{"spi0_d1.gpio0_4",    OMAP_MUX_MODE7 | AM33XX_PIN_OUTPUT },		// QL CSSP and Camera Sensor Reset
	{"spi0_cs0.gpio0_5",   OMAP_MUX_MODE7 | AM33XX_PIN_OUTPUT_PULLUP },	// 1V8 and 2V8 Power Enable
	{"gpmc_csn1.gpio0_30",   OMAP_MUX_MODE7 | AM33XX_PIN_INPUT},		// Sensor orientation detect: low -> frontfacing, high -> backfacing
	{NULL, 0},
};

static struct pinmux_config i2c2_pin_mux[] = {
	{"uart1_ctsn.i2c2_sda",    OMAP_MUX_MODE3 | AM33XX_SLEWCTRL_SLOW |
					AM33XX_PIN_INPUT_PULLUP},
	{"uart1_rtsn.i2c2_scl",   OMAP_MUX_MODE3 | AM33XX_SLEWCTRL_SLOW |
					AM33XX_PIN_INPUT_PULLUP},
	{NULL, 0},
};

/* Module pin mux for mcasp1 */
static struct pinmux_config mcasp1_pin_mux[] = {
	{"mii1_crs.mcasp1_aclkx", OMAP_MUX_MODE4 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_rxerr.mcasp1_fsx", OMAP_MUX_MODE4 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_col.mcasp1_axr2", OMAP_MUX_MODE4 | AM33XX_PIN_INPUT_PULLDOWN},
	{"rmii1_refclk.mcasp1_axr3", OMAP_MUX_MODE4 | AM33XX_PIN_INPUT_PULLDOWN},
	{NULL, 0},
};

/* Module pin mux for mcasp0 */
static struct pinmux_config mcasp0_pin_mux[] = {
	{"mcasp0_aclkx.mcasp0_aclkx", OMAP_MUX_MODE0 |AM33XX_PIN_INPUT_PULLDOWN},
	{"mcasp0_fsx.mcasp0_fsx", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mcasp0_ahclkr.mcasp0_axr2", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mcasp0_ahclkx.mcasp0_axr3", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLDOWN},
	{NULL, 0},
};

/* Module pin mux for mmc0 */
static struct pinmux_config mmc0_pin_mux[] = {
	{"mmc0_dat3.mmc0_dat3",	OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mmc0_dat2.mmc0_dat2",	OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mmc0_dat1.mmc0_dat1",	OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mmc0_dat0.mmc0_dat0",	OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mmc0_clk.mmc0_clk",	OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mmc0_cmd.mmc0_cmd",	OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mcasp0_aclkr.mmc0_sdwp", OMAP_MUX_MODE7 | AM33XX_PIN_INPUT_PULLUP},
	{"spi0_cs1.mmc0_sdcd",  OMAP_MUX_MODE7 | AM33XX_PIN_INPUT_PULLUP},
	{NULL, 0},
};

static struct pinmux_config mmc0_no_cd_pin_mux[] = {
	{"mmc0_dat3.mmc0_dat3",	OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mmc0_dat2.mmc0_dat2",	OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mmc0_dat1.mmc0_dat1",	OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mmc0_dat0.mmc0_dat0",	OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mmc0_clk.mmc0_clk",	OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mmc0_cmd.mmc0_cmd",	OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mcasp0_aclkr.mmc0_sdwp", OMAP_MUX_MODE4 | AM33XX_PIN_INPUT_PULLDOWN},
	{NULL, 0},
};

/* Module pin mux for mmc1 */
static struct pinmux_config mmc1_pin_mux[] = {
	{"gpmc_ad7.mmc1_dat7",	OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad6.mmc1_dat6",	OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad5.mmc1_dat5",	OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad4.mmc1_dat4",	OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad3.mmc1_dat3",	OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad2.mmc1_dat2",	OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad1.mmc1_dat1",	OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad0.mmc1_dat0",	OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_csn1.mmc1_clk",	OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_csn2.mmc1_cmd",	OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_csn0.gpio1_29",	OMAP_MUX_MODE7 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_advn_ale.mmc1_sdcd", OMAP_MUX_MODE7 | AM33XX_PIN_INPUT_PULLUP},
	{"uart1_rxd.mmc1_sdwp", OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLDOWN},
	{NULL, 0},
};

/* Module pin mux for uart3 */
static struct pinmux_config uart3_pin_mux[] = {
	{"spi0_cs1.uart3_rxd", AM33XX_PIN_INPUT_PULLUP},
	{"ecap0_in_pwm0_out.uart3_txd", AM33XX_PULL_ENBL},
	{NULL, 0},
};

static struct pinmux_config d_can_gp_pin_mux[] = {
	{"uart0_ctsn.d_can1_tx", OMAP_MUX_MODE2 | AM33XX_PULL_ENBL},
	{"uart0_rtsn.d_can1_rx", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLUP},
	{NULL, 0},
};

static struct pinmux_config d_can_ia_pin_mux[] = {
	{"uart0_rxd.d_can0_tx", OMAP_MUX_MODE2 | AM33XX_PULL_ENBL},
	{"uart0_txd.d_can0_rx", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLUP},
	{NULL, 0},
};

static struct pinmux_config tt3201_pin_mux[] = {
	{"uart1_rxd.d_can1_tx", OMAP_MUX_MODE2 | AM33XX_PIN_OUTPUT },
	{"uart1_txd.d_can1_rx", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLUP },
	{"mcasp0_fsr.gpio3_19", OMAP_MUX_MODE7 | AM33XX_PIN_INPUT_PULLUP },
	{"mcasp0_ahclkx.gpio3_21", OMAP_MUX_MODE7 | AM33XX_PIN_INPUT_PULLUP },
	{"ecap0_in_pwm0_out.spi1_cs1", OMAP_MUX_MODE2 | AM33XX_PIN_OUTPUT_PULLUP },
	{NULL, 0},
};

static struct pinmux_config uart1dcan1_pin_mux[] = {
	{"uart1_rxd.d_can1_tx", OMAP_MUX_MODE2 | AM33XX_PIN_OUTPUT },
	{"uart1_txd.d_can1_rx", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLUP },
	{NULL, 0},
};

static struct pinmux_config uart1_pin_mux[] = {
	{"uart1_txd.uart1_txd", OMAP_MUX_MODE0 | AM33XX_PULL_ENBL},
	{"uart1_rxd.uart1_rxd", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{NULL, 0},
};

/* Module pin mux for uart2 */
static struct pinmux_config uart2_pin_mux[] = {
	{"spi0_d0.uart2_txd", OMAP_MUX_MODE1 | AM33XX_PIN_OUTPUT},
	{"spi0_sclk.uart2_rxd", OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLUP},
	{NULL, 0},
};


/*
* @pin_mux - single module pin-mux structure which defines pin-mux
*			details for all its pins.
*/
static void setup_pin_mux(struct pinmux_config *pin_mux)
{
	int i;

	for (i = 0; pin_mux->string_name != NULL; pin_mux++)
		omap_mux_init_signal(pin_mux->string_name, pin_mux->val);

}

/* Matrix GPIO Keypad Support for profile-0 only: TODO */

/* pinmux for keypad device */
static struct pinmux_config matrix_keypad_pin_mux[] = {
	{"gpmc_a5.gpio1_21",  OMAP_MUX_MODE7 | AM33XX_PIN_OUTPUT},
	{"gpmc_a6.gpio1_22",  OMAP_MUX_MODE7 | AM33XX_PIN_OUTPUT},
	{"gpmc_a9.gpio1_25",  OMAP_MUX_MODE7 | AM33XX_PIN_INPUT},
	{"gpmc_a10.gpio1_26", OMAP_MUX_MODE7 | AM33XX_PIN_INPUT},
	{"gpmc_a11.gpio1_27", OMAP_MUX_MODE7 | AM33XX_PIN_INPUT},
	{NULL, 0},
};

/* Keys mapping */
static const uint32_t am335x_evm_matrix_keys[] = {
	KEY(0, 0, KEY_MENU),
	KEY(1, 0, KEY_BACK),
	KEY(2, 0, KEY_LEFT),

	KEY(0, 1, KEY_RIGHT),
	KEY(1, 1, KEY_ENTER),
	KEY(2, 1, KEY_DOWN),
};

const struct matrix_keymap_data am335x_evm_keymap_data = {
	.keymap      = am335x_evm_matrix_keys,
	.keymap_size = ARRAY_SIZE(am335x_evm_matrix_keys),
};

static const unsigned int am335x_evm_keypad_row_gpios[] = {
	GPIO_TO_PIN(1, 25), GPIO_TO_PIN(1, 26), GPIO_TO_PIN(1, 27)
};

static const unsigned int am335x_evm_keypad_col_gpios[] = {
	GPIO_TO_PIN(1, 21), GPIO_TO_PIN(1, 22)
};

static struct matrix_keypad_platform_data am335x_evm_keypad_platform_data = {
	.keymap_data       = &am335x_evm_keymap_data,
	.row_gpios         = am335x_evm_keypad_row_gpios,
	.num_row_gpios     = ARRAY_SIZE(am335x_evm_keypad_row_gpios),
	.col_gpios         = am335x_evm_keypad_col_gpios,
	.num_col_gpios     = ARRAY_SIZE(am335x_evm_keypad_col_gpios),
	.active_low        = false,
	.debounce_ms       = 5,
	.col_scan_delay_us = 2,
};

static struct platform_device am335x_evm_keyboard = {
	.name  = "matrix-keypad",
	.id    = -1,
	.dev   = {
		.platform_data = &am335x_evm_keypad_platform_data,
	},
};

static void matrix_keypad_init(int evm_id, int profile)
{
	int err;

	setup_pin_mux(matrix_keypad_pin_mux);
	err = platform_device_register(&am335x_evm_keyboard);
	if (err) {
		pr_err("failed to register matrix keypad (2x3) device\n");
	}
}


/* pinmux for keypad device */
static struct pinmux_config volume_keys_pin_mux[] = {
	{"spi0_sclk.gpio0_2",  OMAP_MUX_MODE7 | AM33XX_PIN_INPUT},
	{"spi0_d0.gpio0_3",    OMAP_MUX_MODE7 | AM33XX_PIN_INPUT},
	{NULL, 0},
};

/* Configure GPIOs for Volume Keys */
static struct gpio_keys_button am335x_evm_volume_gpio_buttons[] = {
	{
		.code                   = KEY_VOLUMEUP,
		.gpio                   = GPIO_TO_PIN(0, 2),
		.active_low             = true,
		.desc                   = "volume-up",
		.type                   = EV_KEY,
		.wakeup                 = 1,
	},
	{
		.code                   = KEY_VOLUMEDOWN,
		.gpio                   = GPIO_TO_PIN(0, 3),
		.active_low             = true,
		.desc                   = "volume-down",
		.type                   = EV_KEY,
		.wakeup                 = 1,
	},
};

static struct gpio_keys_platform_data am335x_evm_volume_gpio_key_info = {
	.buttons        = am335x_evm_volume_gpio_buttons,
	.nbuttons       = ARRAY_SIZE(am335x_evm_volume_gpio_buttons),
};

static struct platform_device am335x_evm_volume_keys = {
	.name   = "gpio-keys",
	.id     = -1,
	.dev    = {
		.platform_data  = &am335x_evm_volume_gpio_key_info,
	},
};

static void volume_keys_init(int evm_id, int profile)
{
	int err;

	setup_pin_mux(volume_keys_pin_mux);
	err = platform_device_register(&am335x_evm_volume_keys);
	if (err)
		pr_err("failed to register matrix keypad (2x3) device\n");
}

/* pinmux for lcd7 keys */
static struct pinmux_config lcd7_keys_pin_mux[] = {
	{"gpmc_a0.gpio1_16",  OMAP_MUX_MODE7 | AM33XX_PIN_INPUT}, //left
	{"gpmc_a1.gpio1_17",    OMAP_MUX_MODE7 | AM33XX_PIN_INPUT}, //right
	{"gpmc_a3.gpio1_19",  OMAP_MUX_MODE7 | AM33XX_PIN_INPUT}, //up
	{"mcasp0_axr0.gpio3_16",    OMAP_MUX_MODE7 | AM33XX_PIN_INPUT}, //down
	{"mcasp0_fsr.gpio3_19",    OMAP_MUX_MODE7 | AM33XX_PIN_INPUT}, //enter
	{NULL, 0},
};

/* Configure GPIOs for lcd7 keys */
static struct gpio_keys_button beaglebone_lcd7_gpio_keys[] = {
	{
		.code                   = KEY_LEFT,
		.gpio                   = GPIO_TO_PIN(1, 16),
		.active_low             = true,
		.desc                   = "left",
		.type                   = EV_KEY,
		.wakeup                 = 1,
	},
	{
		.code                   = KEY_RIGHT,
		.gpio                   = GPIO_TO_PIN(1, 17),
		.active_low             = true,
		.desc                   = "right",
		.type                   = EV_KEY,
		.wakeup                 = 1,
	},
	{
		.code                   = KEY_UP,
		.gpio                   = GPIO_TO_PIN(1, 19),
		.active_low             = true,
		.desc                   = "up",
		.type                   = EV_KEY,
		.wakeup                 = 1,
	},
	{
		.code                   = KEY_DOWN,
		.gpio                   = GPIO_TO_PIN(3, 16),
		.active_low             = true,
		.desc                   = "down",
		.type                   = EV_KEY,
		.wakeup                 = 1,
	},
	{
		.code                   = KEY_ENTER,
		.gpio                   = GPIO_TO_PIN(3, 19),
		.active_low             = true,
		.desc                   = "enter",
		.type                   = EV_KEY,
		.wakeup                 = 1,
	},
};

static struct gpio_keys_platform_data beaglebone_lcd7_gpio_key_info = {
	.buttons        = beaglebone_lcd7_gpio_keys,
	.nbuttons       = ARRAY_SIZE(beaglebone_lcd7_gpio_keys),
};

static struct platform_device beaglebone_lcd7_keys = {
	.name   = "gpio-keys",
	.id     = -1,
	.dev    = {
		.platform_data  = &beaglebone_lcd7_gpio_key_info,
	},
};

static void beaglebone_lcd7_keys_init(int evm_id, int profile)
{
	int err;
	setup_pin_mux(lcd7_keys_pin_mux);
	err = platform_device_register(&beaglebone_lcd7_keys);
	if (err)
		pr_err("failed to register gpio keys for LCD7 cape\n");
}

/* pinmux for lcd7 keys */
static struct pinmux_config lcd7a3_keys_pin_mux[] = {
	{"gpmc_a0.gpio1_16",  OMAP_MUX_MODE7 | AM33XX_PIN_INPUT}, //left
	{"gpmc_a1.gpio1_17",    OMAP_MUX_MODE7 | AM33XX_PIN_INPUT}, //right
	{"gpmc_a3.gpio1_19",  OMAP_MUX_MODE7 | AM33XX_PIN_INPUT}, //up
	{"mcasp0_axr0.gpio3_16",    OMAP_MUX_MODE7 | AM33XX_PIN_INPUT}, //down
	{"sdpi0_d0.gpio0_3",    OMAP_MUX_MODE7 | AM33XX_PIN_INPUT}, //enter
	{NULL, 0},
};

/* Configure GPIOs for lcd7 keys */
static struct gpio_keys_button beaglebone_lcd7a3_gpio_keys[] = {
	{
		.code                   = KEY_LEFT,
		.gpio                   = GPIO_TO_PIN(1, 16),
		.active_low             = true,
		.desc                   = "left",
		.type                   = EV_KEY,
		.wakeup                 = 1,
	},
	{
		.code                   = KEY_RIGHT,
		.gpio                   = GPIO_TO_PIN(1, 17),
		.active_low             = true,
		.desc                   = "right",
		.type                   = EV_KEY,
		.wakeup                 = 1,
	},
	{
		.code                   = KEY_UP,
		.gpio                   = GPIO_TO_PIN(1, 19),
		.active_low             = true,
		.desc                   = "up",
		.type                   = EV_KEY,
		.wakeup                 = 1,
	},
	{
		.code                   = KEY_DOWN,
		.gpio                   = GPIO_TO_PIN(3, 16),
		.active_low             = true,
		.desc                   = "down",
		.type                   = EV_KEY,
		.wakeup                 = 1,
	},
	{
		.code                   = KEY_ENTER,
		.gpio                   = GPIO_TO_PIN(0, 3),
		.active_low             = true,
		.desc                   = "enter",
		.type                   = EV_KEY,
		.wakeup                 = 1,
	},
};

static struct gpio_keys_platform_data beaglebone_lcd7a3_gpio_key_info = {
	.buttons        = beaglebone_lcd7a3_gpio_keys,
	.nbuttons       = ARRAY_SIZE(beaglebone_lcd7a3_gpio_keys),
};

static struct platform_device beaglebone_lcd7a3_keys = {
	.name   = "gpio-keys",
	.id     = -1,
	.dev    = {
		.platform_data  = &beaglebone_lcd7a3_gpio_key_info,
	},
};

static void beaglebone_lcd7a3_keys_init(int evm_id, int profile)
{
	int err;
	setup_pin_mux(lcd7a3_keys_pin_mux);
	err = platform_device_register(&beaglebone_lcd7a3_keys);
	if (err)
		pr_err("failed to register gpio keys for LCD7 cape\n");
}

/* pinmux for lcd3 keys */
static struct pinmux_config lcd3_keys_pin_mux[] = {
	{"gpmc_a0.gpio1_16",  OMAP_MUX_MODE7 | AM33XX_PIN_INPUT},
	{"gpmc_a1.gpio1_17",    OMAP_MUX_MODE7 | AM33XX_PIN_INPUT},
	{"mcasp0_fsr.gpio3_19",  OMAP_MUX_MODE7 | AM33XX_PIN_INPUT},
	{"gpmc_ben1.gpio1_28",    OMAP_MUX_MODE7 | AM33XX_PIN_INPUT},
	{"ecap0_in_pwm0_out.gpio0_7",    OMAP_MUX_MODE7 | AM33XX_PIN_INPUT},
	{NULL, 0},
};

/* Configure GPIOs for lcd3 rev A or earlier keys */
static struct gpio_keys_button beaglebone_lcd3_gpio_keys[] = {
	{
		.code                   = KEY_LEFT,
		.gpio                   = GPIO_TO_PIN(1, 16),
		.active_low             = true,
		.desc                   = "left",
		.type                   = EV_KEY,
		.wakeup                 = 1,
	},
	{
		.code                   = KEY_RIGHT,
		.gpio                   = GPIO_TO_PIN(1, 17),
		.active_low             = true,
		.desc                   = "right",
		.type                   = EV_KEY,
		.wakeup                 = 1,
	},
	{
		.code                   = KEY_UP,
		.gpio                   = GPIO_TO_PIN(3, 19),
		.active_low             = true,
		.desc                   = "up",
		.type                   = EV_KEY,
		.wakeup                 = 1,
	},
	{
		.code                   = KEY_DOWN,
		.gpio                   = GPIO_TO_PIN(1, 28),
		.active_low             = true,
		.desc                   = "down",
		.type                   = EV_KEY,
		.wakeup                 = 1,
	},
	{
		.code                   = KEY_ENTER,
		.gpio                   = GPIO_TO_PIN(0, 7),
		.active_low             = true,
		.desc                   = "down",
		.type                   = EV_KEY,
		.wakeup                 = 1,
	},
};

static struct gpio_keys_platform_data beaglebone_lcd3_gpio_key_info = {
	.buttons        = beaglebone_lcd3_gpio_keys,
	.nbuttons       = ARRAY_SIZE(beaglebone_lcd3_gpio_keys),
};

static struct platform_device beaglebone_lcd3_keys = {
	.name   = "gpio-keys",
	.id     = -1,
	.dev    = {
		.platform_data  = &beaglebone_lcd3_gpio_key_info,
	},
};

/* pinmux for lcd3 A1 or newer keys */
static struct pinmux_config lcd3a1_keys_pin_mux[] = {
	{"gpmc_a0.gpio1_16",  OMAP_MUX_MODE7 | AM33XX_PIN_INPUT}, // Left
	{"gpmc_a1.gpio1_17",    OMAP_MUX_MODE7 | AM33XX_PIN_INPUT}, //Right
	{"gpmc_a3.gpio1_19", OMAP_MUX_MODE7 | AM33XX_PIN_INPUT}, // Up
	{"mcasp0_axr0.gpio3_16",    OMAP_MUX_MODE7 | AM33XX_PIN_INPUT}, //Down
	{"mcasp0_fsr.gpio3_19",  OMAP_MUX_MODE7 | AM33XX_PIN_INPUT}, // Enter
	{NULL, 0},
};

/* Configure GPIOs for lcd3 rev A1 or newer keys */
static struct gpio_keys_button lcd3a1_gpio_keys[] = {
	{
		.code                   = KEY_LEFT,
		.gpio                   = GPIO_TO_PIN(1, 16),
		.active_low             = true,
		.desc                   = "left",
		.type                   = EV_KEY,
		.wakeup                 = 1,
	},
	{
		.code                   = KEY_RIGHT,
		.gpio                   = GPIO_TO_PIN(1, 17),
		.active_low             = true,
		.desc                   = "right",
		.type                   = EV_KEY,
		.wakeup                 = 1,
	},
	{
		.code                   = KEY_UP,
		.gpio                   = GPIO_TO_PIN(1, 19),
		.active_low             = true,
		.desc                   = "up",
		.type                   = EV_KEY,
		.wakeup                 = 1,
	},
	{
		.code                   = KEY_DOWN,
		.gpio                   = GPIO_TO_PIN(3, 16),
		.active_low             = true,
		.desc                   = "down",
		.type                   = EV_KEY,
		.wakeup                 = 1,
	},
	{
		.code                   = KEY_ENTER,
		.gpio                   = GPIO_TO_PIN(3, 19),
		.active_low             = true,
		.desc                   = "enter",
		.type                   = EV_KEY,
		.wakeup                 = 1,
	},
};

static struct gpio_keys_platform_data lcd3a1_gpio_key_info = {
	.buttons        = lcd3a1_gpio_keys,
	.nbuttons       = ARRAY_SIZE(lcd3a1_gpio_keys),
};

static struct platform_device lcd3a1_keys = {
	.name   = "gpio-keys",
	.id     = -1,
	.dev    = {
		.platform_data  = &lcd3a1_gpio_key_info,
	},
};

/* pinmux for lcd3 A2 or newer keys */
static struct pinmux_config lcd3a2_keys_pin_mux[] = {
	{"gpmc_a0.gpio1_16",  OMAP_MUX_MODE7 | AM33XX_PIN_INPUT}, // Left
	{"gpmc_a1.gpio1_17",    OMAP_MUX_MODE7 | AM33XX_PIN_INPUT}, //Right
	{"gpmc_a3.gpio1_19", OMAP_MUX_MODE7 | AM33XX_PIN_INPUT}, // Up
	{"mcasp0_axr0.gpio3_16",    OMAP_MUX_MODE7 | AM33XX_PIN_INPUT}, //Down
	{"spi0_d0.gpio0_3",    OMAP_MUX_MODE7 | AM33XX_PIN_INPUT}, // Enter
	{NULL, 0},
};

/* Configure GPIOs for lcd3 rev A2 or newer keys */
static struct gpio_keys_button lcd3a2_gpio_keys[] = {
	{
		.code                   = KEY_LEFT,
		.gpio                   = GPIO_TO_PIN(1, 16),
		.active_low             = true,
		.desc                   = "left",
		.type                   = EV_KEY,
		.wakeup                 = 1,
	},
	{
		.code                   = KEY_RIGHT,
		.gpio                   = GPIO_TO_PIN(1, 17),
		.active_low             = true,
		.desc                   = "right",
		.type                   = EV_KEY,
		.wakeup                 = 1,
	},
	{
		.code                   = KEY_UP,
		.gpio                   = GPIO_TO_PIN(1, 19),
		.active_low             = true,
		.desc                   = "up",
		.type                   = EV_KEY,
		.wakeup                 = 1,
	},
	{
		.code                   = KEY_DOWN,
		.gpio                   = GPIO_TO_PIN(3, 16),
		.active_low             = true,
		.desc                   = "down",
		.type                   = EV_KEY,
		.wakeup                 = 1,
	},
	{
		.code                   = KEY_ENTER,
		.gpio                   = GPIO_TO_PIN(0, 3),
		.active_low             = true,
		.desc                   = "enter",
		.type                   = EV_KEY,
		.wakeup                 = 1,
	},
};

static struct gpio_keys_platform_data lcd3a2_gpio_key_info = {
	.buttons        = lcd3a2_gpio_keys,
	.nbuttons       = ARRAY_SIZE(lcd3a2_gpio_keys),
};

static struct platform_device lcd3a2_keys = {
	.name   = "gpio-keys",
	.id     = -1,
	.dev    = {
		.platform_data  = &lcd3a2_gpio_key_info,
	},
};

/*
* @evm_id - evm id which needs to be configured
* @dev_cfg - single evm structure which includes
*				all module inits, pin-mux defines
* @profile - if present, else PROFILE_NONE
* @dghtr_brd_flg - Whether Daughter board is present or not
*/
static void _configure_device(int evm_id, struct evm_dev_cfg *dev_cfg,
	int profile)
{
	int i;

	/*
	* Only General Purpose & Industrial Auto Motro Control
	* EVM has profiles. So check if this evm has profile.
	* If not, ignore the profile comparison
	*/

	/*
	* If the device is on baseboard, directly configure it. Else (device on
	* Daughter board), check if the daughter card is detected.
	*/
	if (profile == PROFILE_NONE) {
		for (i = 0; dev_cfg->device_init != NULL; dev_cfg++) {
			if (dev_cfg->device_on == DEV_ON_BASEBOARD)
				dev_cfg->device_init(evm_id, profile);
			else if (daughter_brd_detected == true)
				dev_cfg->device_init(evm_id, profile);
		}
	} else {
		for (i = 0; dev_cfg->device_init != NULL; dev_cfg++) {
			if (dev_cfg->profile & profile) {
				if (dev_cfg->device_on == DEV_ON_BASEBOARD)
					dev_cfg->device_init(evm_id, profile);
				else if (daughter_brd_detected == true)
					dev_cfg->device_init(evm_id, profile);
			}
		}
	}
}


/* pinmux for usb0 drvvbus */
static struct pinmux_config usb0_pin_mux[] = {
	{"usb0_drvvbus.usb0_drvvbus",    OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT},
	{NULL, 0},
};

/* pinmux for usb1 drvvbus */
static struct pinmux_config usb1_pin_mux[] = {
	{"usb1_drvvbus.usb1_drvvbus",    OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT},
	{NULL, 0},
};

/* pinmux for profibus */
static struct pinmux_config profibus_pin_mux[] = {
	{"uart1_rxd.pr1_uart0_rxd_mux1", OMAP_MUX_MODE5 | AM33XX_PIN_INPUT},
	{"uart1_txd.pr1_uart0_txd_mux1", OMAP_MUX_MODE5 | AM33XX_PIN_OUTPUT},
	{"mcasp0_fsr.pr1_pru0_pru_r30_5", OMAP_MUX_MODE5 | AM33XX_PIN_OUTPUT},
	{NULL, 0},
};

#define BEAGLEBONE_W1_GPIO GPIO_TO_PIN(1, 3)

static struct w1_gpio_platform_data bone_w1_gpio_pdata = {
	.pin		= BEAGLEBONE_W1_GPIO,
	.is_open_drain	= 0,
};

static struct platform_device bone_w1_device = {
	.name			= "w1-gpio",
	.id			= -1,
	.dev.platform_data	= &bone_w1_gpio_pdata,
};

/* LEDS - gpio1_21 -> gpio1_24 */

#define BEAGLEBONE_USR1_LED  GPIO_TO_PIN(1, 21)
#define BEAGLEBONE_USR2_LED  GPIO_TO_PIN(1, 22)
#define BEAGLEBONE_USR3_LED  GPIO_TO_PIN(1, 23)
#define BEAGLEBONE_USR4_LED  GPIO_TO_PIN(1, 24)

static struct gpio_led bone_gpio_leds[] = {
	{
		.name			= "beaglebone::usr0",
		.default_trigger	= "heartbeat",
		.gpio			= BEAGLEBONE_USR1_LED,
	},
	{
		.name			= "beaglebone::usr1",
		.default_trigger	= "mmc0",
		.gpio			= BEAGLEBONE_USR2_LED,
	},
	{
		.name			= "beaglebone::usr2",
		.gpio			= BEAGLEBONE_USR3_LED,
	},
	{
		.name           = "beaglebone::usr3",
		.gpio           = BEAGLEBONE_USR4_LED,
	},
};

static struct gpio_led_platform_data bone_gpio_led_info = {
	.leds		= bone_gpio_leds,
	.num_leds	= ARRAY_SIZE(bone_gpio_leds),
};

static struct platform_device bone_leds_gpio = {
	.name	= "leds-gpio",
	.id	= -1,
	.dev	= {
		.platform_data	= &bone_gpio_led_info,
	},
};


#define BEAGLEBONEDVI_USR0_LED  GPIO_TO_PIN(1, 18)
#define BEAGLEBONEDVI_USR1_LED  GPIO_TO_PIN(1, 19)

static struct gpio_led dvi_gpio_leds[] = {
	{
		.name			= "beaglebone::usr0",
		.default_trigger	= "heartbeat",
		.gpio			= BEAGLEBONE_USR1_LED,
	},
	{
		.name			= "beaglebone::usr1",
		.default_trigger	= "mmc0",
		.gpio			= BEAGLEBONE_USR2_LED,
	},
	{
		.name			= "beaglebone::usr2",
		.gpio			= BEAGLEBONE_USR3_LED,
	},
	{
		.name           = "beaglebone::usr3",
		.gpio           = BEAGLEBONE_USR4_LED,
	},
	{
		.name			= "dvi::usr0",
		.default_trigger	= "heartbeat",
		.gpio			= BEAGLEBONEDVI_USR0_LED,
	},
	{
		.name			= "dvi::usr1",
		.default_trigger	= "mmc0",
		.gpio			= BEAGLEBONEDVI_USR1_LED,
	},
};

static struct gpio_led_platform_data dvi_gpio_led_info = {
	.leds		= dvi_gpio_leds,
	.num_leds	= ARRAY_SIZE(dvi_gpio_leds),
};

static struct platform_device dvi_leds_gpio = {
	.name	= "leds-gpio",
	.id	= -1,
	.dev	= {
		.platform_data	= &dvi_gpio_led_info,
	},
};

#define BEAGLEBONELCD3_USR0_LED  GPIO_TO_PIN(1, 18)

static struct gpio_led lcd3_gpio_leds[] = {
	{
		.name			= "beaglebone::usr0",
		.default_trigger	= "heartbeat",
		.gpio			= BEAGLEBONE_USR1_LED,
	},
	{
		.name			= "beaglebone::usr1",
		.default_trigger	= "mmc0",
		.gpio			= BEAGLEBONE_USR2_LED,
	},
	{
		.name			= "beaglebone::usr2",
		.gpio			= BEAGLEBONE_USR3_LED,
	},
	{
		.name           = "beaglebone::usr3",
		.gpio           = BEAGLEBONE_USR4_LED,
	},
	{
		.name			= "lcd3::usr0",
		.default_trigger	= "heartbeat",
		.gpio			= BEAGLEBONELCD3_USR0_LED,
	},
};

static struct gpio_led_platform_data lcd3_gpio_led_info = {
	.leds		= lcd3_gpio_leds,
	.num_leds	= ARRAY_SIZE(lcd3_gpio_leds),
};

static struct platform_device lcd3_leds_gpio = {
	.name	= "leds-gpio",
	.id	= -1,
	.dev	= {
		.platform_data	= &lcd3_gpio_led_info,
	},
};

#define BEAGLEBONELCD3A2_USR0_LED  GPIO_TO_PIN(3, 19)

static struct gpio_led lcd3a2_gpio_leds[] = {
	{
		.name			= "beaglebone::usr0",
		.default_trigger	= "heartbeat",
		.gpio			= BEAGLEBONE_USR1_LED,
	},
	{
		.name			= "beaglebone::usr1",
		.default_trigger	= "mmc0",
		.gpio			= BEAGLEBONE_USR2_LED,
	},
	{
		.name			= "beaglebone::usr2",
		.gpio			= BEAGLEBONE_USR3_LED,
	},
	{
		.name           = "beaglebone::usr3",
		.gpio           = BEAGLEBONE_USR4_LED,
	},
	{
		.name			= "lcd3::usr0",
		.default_trigger	= "heartbeat",
		.gpio			= BEAGLEBONELCD3A2_USR0_LED,
	},
};

static struct gpio_led_platform_data lcd3a2_gpio_led_info = {
	.leds		= lcd3a2_gpio_leds,
	.num_leds	= ARRAY_SIZE(lcd3a2_gpio_leds),
};

static struct platform_device lcd3a2_leds_gpio = {
	.name	= "leds-gpio",
	.id	= -1,
	.dev	= {
		.platform_data	= &lcd3a2_gpio_led_info,
	},
};
#define BEAGLEBONELCD7_USR_LED  GPIO_TO_PIN(1, 28)

static struct gpio_led lcd7_gpio_leds[] = {
	{
		.name			= "beaglebone::usr0",
		.default_trigger	= "heartbeat",
		.gpio			= BEAGLEBONE_USR1_LED,
	},
	{
		.name			= "beaglebone::usr1",
		.default_trigger	= "mmc0",
		.gpio			= BEAGLEBONE_USR2_LED,
	},
	{
		.name			= "beaglebone::usr2",
		.gpio			= BEAGLEBONE_USR3_LED,
	},
	{
		.name           = "beaglebone::usr3",
		.gpio           = BEAGLEBONE_USR4_LED,
	},
	{
		.name			= "lcd7::usr",
		.default_trigger	= "heartbeat",
		.gpio			= BEAGLEBONELCD7_USR_LED,
	},
};

static struct gpio_led_platform_data lcd7_gpio_led_info = {
	.leds		= lcd7_gpio_leds,
	.num_leds	= ARRAY_SIZE(lcd7_gpio_leds),
};

static struct platform_device lcd7_leds_gpio = {
	.name	= "leds-gpio",
	.id	= -1,
	.dev	= {
		.platform_data	= &lcd7_gpio_led_info,
	},
};

static struct pinmux_config bone_pin_mux[] = {
	/* User LED gpios (gpio1_21 to gpio1_24) */
    {"gpmc_a5.rgmii2_td0", OMAP_MUX_MODE7 | AM33XX_PIN_OUTPUT}, // USR0
    {"gpmc_a6.rgmii2_tclk", OMAP_MUX_MODE7 | AM33XX_PIN_OUTPUT}, // USR1
    {"gpmc_a7.rgmii2_rclk", OMAP_MUX_MODE7 | AM33XX_PIN_OUTPUT}, // USR2
    {"gpmc_a8.rgmii2_rd3", OMAP_MUX_MODE7 | AM33XX_PIN_OUTPUT}, // USR3
    /* Grounding gpio1_6 (pin 3 Conn A) signals bone tester to start diag tests */
    {"gpmc_ad6.gpio1_6", OMAP_MUX_MODE7 | AM33XX_PIN_INPUT_PULLUP},
	{NULL, 0},
};

/* Module pin mux for eCAP0 */
static struct pinmux_config ecap0_pin_mux[] = {
	{"ecap0_in_pwm0_out.ecap0_in_pwm0_out",
		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT},
	{NULL, 0},
};

static int ehrpwm_backlight_enable;
static int backlight_enable;

#define AM335XEVM_WLAN_PMENA_GPIO	GPIO_TO_PIN(1, 30)
#define AM335XEVM_WLAN_IRQ_GPIO		GPIO_TO_PIN(3, 17)

struct wl12xx_platform_data am335xevm_wlan_data = {
	.irq = OMAP_GPIO_IRQ(AM335XEVM_WLAN_IRQ_GPIO),
	.board_ref_clock = WL12XX_REFCLOCK_38_XTAL, /* 38.4Mhz */
};

/* Module pin mux for wlan and bluetooth */
static struct pinmux_config mmc2_wl12xx_pin_mux[] = {
	{"gpmc_a1.mmc2_dat0", OMAP_MUX_MODE3 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_a2.mmc2_dat1", OMAP_MUX_MODE3 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_a3.mmc2_dat2", OMAP_MUX_MODE3 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ben1.mmc2_dat3", OMAP_MUX_MODE3 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_csn3.mmc2_cmd", OMAP_MUX_MODE3 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_clk.mmc2_clk", OMAP_MUX_MODE3 | AM33XX_PIN_INPUT_PULLUP},
	{NULL, 0},
};

static struct pinmux_config uart1_wl12xx_pin_mux[] = {
	{"uart1_ctsn.uart1_ctsn", OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT},
	{"uart1_rtsn.uart1_rtsn", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT},
	{"uart1_rxd.uart1_rxd", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"uart1_txd.uart1_txd", OMAP_MUX_MODE0 | AM33XX_PULL_ENBL},
	{NULL, 0},
};

static struct pinmux_config wl12xx_pin_mux_evm_rev1_1a[] = {
	{"gpmc_a0.gpio1_16", OMAP_MUX_MODE7 | AM33XX_PIN_OUTPUT},
	{"mcasp0_ahclkr.gpio3_17", OMAP_MUX_MODE7 | AM33XX_PIN_INPUT},
	{"mcasp0_ahclkx.gpio3_21", OMAP_MUX_MODE7 | AM33XX_PIN_OUTPUT},
	{NULL, 0},
 };

static struct pinmux_config wl12xx_pin_mux_evm_rev1_0[] = {
	{"gpmc_csn1.gpio1_30", OMAP_MUX_MODE7 | AM33XX_PIN_OUTPUT},
	{"mcasp0_ahclkr.gpio3_17", OMAP_MUX_MODE7 | AM33XX_PIN_INPUT},
	{"gpmc_csn2.gpio1_31", OMAP_MUX_MODE7 | AM33XX_PIN_OUTPUT},
	{NULL, 0},
 };

static void enable_ecap0(int evm_id, int profile)
{
	backlight_enable = true;
	setup_pin_mux(ecap0_pin_mux);
}

/* Setup pwm-backlight */
static struct platform_device am335x_backlight = {
	.name           = "pwm-backlight",
	.id             = -1,
	.dev            = {
		.platform_data  = &am335x_backlight_data,
	}
};

static struct pwmss_platform_data  pwm_pdata[3] = {
	{
		.version = PWM_VERSION_1,
	},
	{
		.version = PWM_VERSION_1,
	},
	{
		.version = PWM_VERSION_1,
	},
};

static void register_all_pwms(void)
{
	am33xx_register_ehrpwm(0, &pwm_pdata[0]);
	am33xx_register_ehrpwm(1, &pwm_pdata[1]);
	am33xx_register_ehrpwm(2, &pwm_pdata[2]);
	am33xx_register_ecap(0, &pwm_pdata[0]);
	am33xx_register_ecap(1, &pwm_pdata[1]);
	am33xx_register_ecap(2, &pwm_pdata[2]);
}

static int __init ecap0_init(void)
{
	int status = 0;

	if (backlight_enable) {
		am33xx_register_ecap(0, &pwm_pdata[0]);
		platform_device_register(&am335x_backlight);
	}
	return status;
}
late_initcall(ecap0_init);

static void enable_ehrpwm1(int evm_id, int profile)
{
	ehrpwm_backlight_enable = true;
	/*am33xx_register_ehrpwm(1, &pwm_pdata[1]);
	 *TODO:no longer required as we are registering
	 *all pwms for the beaglebone
	 */
}

/* Setup pwm-backlight for bbtoys7lcd */
static struct platform_device bbtoys7lcd_backlight = {
	.name           = "pwm-backlight",
	.id             = -1,
	.dev            = {
		.platform_data  = &bbtoys7lcd_backlight_data,
	}
};

static int __init ehrpwm1_init(void)
{
	int status = 0;
	if (ehrpwm_backlight_enable) {
		platform_device_register(&bbtoys7lcd_backlight);
	}
	return status;
}
late_initcall(ehrpwm1_init);

static int __init conf_disp_pll(int rate)
{
	struct clk *disp_pll;
	int ret = -EINVAL;

	disp_pll = clk_get(NULL, "dpll_disp_ck");
	if (IS_ERR(disp_pll)) {
		pr_err("Cannot clk_get disp_pll\n");
		goto out;
	}

	ret = clk_set_rate(disp_pll, rate);
	clk_put(disp_pll);
out:
	return ret;
}

static void lcdc_init(int evm_id, int profile)
{

	setup_pin_mux(lcdc16_pin_mux);
	setup_pin_mux(lcdc_pin_mux);

	if (conf_disp_pll(300000000)) {
		pr_info("Failed configure display PLL, not attempting to"
				"register LCDC\n");
		return;
	}

	if (am33xx_register_lcdc(&TFC_S9700RTWV35TR_01B_pdata))
		pr_info("Failed to register LCDC device\n");
	return;
}

#define BEAGLEBONE_LCD_AVDD_EN GPIO_TO_PIN(0, 7)
#define BEAGLEBONE_LCD7A2_AVDD_EN GPIO_TO_PIN(1, 31)
#define BEAGLEBONE_LCD7A3_AVDD_EN GPIO_TO_PIN(0, 2)

static void bbtoys7lcd_init(int evm_id, int profile)
{
	setup_pin_mux(lcdc16_pin_mux);

	// we are being stupid and setting pixclock from here instead of da8xx-fb.c
	if (conf_disp_pll(300000000)) {
		pr_info("Failed to set pixclock to 300000000, not attempting to"
				"register LCD cape\n");
		return;
	}

	if (am33xx_register_lcdc(&bbtoys7_pdata))
		pr_info("Failed to register Beagleboardtoys 7\" LCD cape device\n");

	return;
}

static void bbtoys35lcd_init(int evm_id, int profile)
{
	setup_pin_mux(lcdc16_pin_mux);
	setup_pin_mux(bbtoys35_pin_mux);

	// we are being stupid and setting pixclock from here instead of da8xx-fb.c
	if (conf_disp_pll(16000000)) {
		pr_info("Failed to set pixclock to 16000000, not attempting to"
				"register LCD cape\n");
		return;
	}

	if (am33xx_register_lcdc(&bbtoys35_pdata))
		pr_info("Failed to register Beagleboardtoys 3.5\" LCD cape device\n");

	return;
}

static void bbtoys43lcd_init(int evm_id, int profile)
{
	setup_pin_mux(lcdc16_pin_mux);
	setup_pin_mux(lcdc_pin_mux);

	// we are being stupid and setting pixclock from here instead of da8xx-fb.c
	if (conf_disp_pll(18000000)) {
		pr_info("Failed to set pixclock to 18000000, not attempting to"
				"register LCD cape\n");
		return;
	}

	if (am33xx_register_lcdc(&bbtoys43_pdata))
		pr_info("Failed to register Beagleboardtoys 4.3\" LCD cape device\n");

	return;
}

#define BEAGLEBONEDVI_PDn_A1  GPIO_TO_PIN(1, 7)
#define BEAGLEBONEDVI_PDn_A2  GPIO_TO_PIN(1, 31)

static void dvi_init(int evm_id, int profile)
{
	setup_pin_mux(lcdc16_pin_mux);

	// we are being stupid and setting pixclock from here instead of da8xx-fb.c
	if (conf_disp_pll(560000000)) {
		pr_info("Failed to set pixclock to 56000000, not attempting to"
				"register DVI adapter\n");
		return;
	}

	if (am33xx_register_lcdc(&dvi_pdata))
		pr_info("Failed to register BeagleBoardToys DVI cape\n");
	return;
}

static void tsc_init(int evm_id, int profile)
{
	int err;

	if (gp_evm_revision == GP_EVM_REV_IS_1_1A) {
		am335x_touchscreen_data.analog_input = 1;
		pr_info("TSC connected to beta GP EVM\n");
	}
	if (gp_evm_revision == GP_EVM_REV_IS_1_1A) {
		am335x_touchscreen_data.analog_input = 0;
		pr_info("TSC connected to alpha GP EVM\n");
	}
	if( gp_evm_revision == GP_EVM_ACTUALLY_BEAGLEBONE) {
		am335x_touchscreen_data.analog_input = 1;
		pr_info("TSC connected to BeagleBone\n");
	}
	setup_pin_mux(tsc_pin_mux);

	err = am33xx_register_tsc(&am335x_touchscreen_data);
	if (err)
		pr_err("failed to register touchscreen device\n");
}

static void bone_tsc_init(int evm_id, int profile)
{
	int err;
	setup_pin_mux(tsc_pin_mux);
	err = am33xx_register_tsc(&bone_touchscreen_data);
	if (err)
		pr_err("failed to register touchscreen device\n");
}


static void boneleds_init(int evm_id, int profile )
{
	int err;
	setup_pin_mux(bone_pin_mux);
	err = platform_device_register(&bone_leds_gpio);
	if (err)
		pr_err("failed to register BeagleBone LEDS\n");
}

static void dvileds_init(int evm_id, int profile )
{
	int err;
	err = platform_device_register(&dvi_leds_gpio);
	if (err)
		pr_err("failed to register BeagleBone DVI cape LEDS\n");
}

static struct resource cssp_camera_resources[] = {
	{
		.name = "gpmc_phys_mem_slot",
		.flags = IORESOURCE_MEM,
	},
};

static struct mt9t112_camera_info mt9t111_cam_info = {
	/* divider calculated for 32Mhz CAM_MCLK */
	.divider = {
		.m = 24, .n = 1,
		.p1 = 0, .p2 = 7, .p3 = 0, .p4 = 11, .p5 = 15, .p6 = 7, .p7 = 0,
	},
};

static struct soc_camera_link mt9t111_camera_link =  {
	.priv = &mt9t111_cam_info,
	.i2c_adapter_id = 3,
};

static struct i2c_board_info i2c_camera = {
	I2C_BOARD_INFO("mt9t112", 0x3c),
	.platform_data = &mt9t111_camera_link,
};

struct cssp_cam_platform_data {
	struct i2c_board_info *cam_i2c_board_info;
	const char *cam_clk_name;
	int dma_ch;
	int cssp_reset_pin;
};

static struct cssp_cam_platform_data cssp_cam_platform_data = {
	.cam_i2c_board_info = &i2c_camera,
	.cam_clk_name = "clkout2_ck",
	.dma_ch = AM33XX_DMA_XDMA_EVENT_INTR2,
	.cssp_reset_pin = GPIO_TO_PIN(0, 4),
};

static struct platform_device cssp_camera = {
	.name  = "cssp-camera",
	.id    = -1,
	.dev   = {
		.platform_data = &cssp_cam_platform_data,
	},
	.num_resources = sizeof(cssp_camera_resources) / sizeof(cssp_camera_resources[0]),
	.resource = cssp_camera_resources,
};

static struct gpmc_timings cssp_timings = {
	/* Minimum clock period for synchronous mode (in picoseconds) */
	.sync_clk = 10000,

	/* CS signal timings corresponding to GPMC_CONFIG2 */
	.cs_on = 0,
	.cs_rd_off = 8 * 10,		/* Read deassertion time */
	.cs_wr_off = 20 * 10,		/* Write deassertion time */

	/* ADV signal timings corresponding to GPMC_CONFIG3 */
	.adv_on = 0,			/* Assertion time */
	.adv_rd_off = 2 * 10,		/* Read deassertion time */
	.adv_wr_off = 2 * 10,		/* Write deassertion time */

	/* WE signals timings corresponding to GPMC_CONFIG4 */
	.we_on = 3 * 10,		/* WE assertion time */
	.we_off = 8 * 10,		/* WE deassertion time */

	/* OE signals timings corresponding to GPMC_CONFIG4 */
	.oe_on = 3 * 10,		/* OE assertion time */
	.oe_off = 8 * 10,		/* OE deassertion time */

	/* Access time and cycle time timings corresponding to GPMC_CONFIG5 */
	.page_burst_access = 1 * 10,	/* Multiple access word delay */
	.access = 7 * 10,		/* Start-cycle to first data valid delay */
	.rd_cycle = 8 * 10,		/* Total read cycle time */
	.wr_cycle = 20 * 10,		/* Total write cycle time */

	/* The following are only on OMAP3430 */
	.wr_access = 7 * 10,		/* WRACCESSTIME */
	.wr_data_mux_bus = 3 * 10,	/* WRDATAONADMUXBUS */
};

static int gpmc_cssp_init(void)
{
	int cs = 1; /* Chip Select on GPMC bus */
	int val;
	long unsigned int cssp_gpmc_mem_base_phys;

	if (gpmc_cs_request(cs, SZ_16M, &cssp_gpmc_mem_base_phys) < 0) {
			printk(KERN_ERR "[cssp_cam platform init]: gpmc_cs_request failed\n");
			return -1;
	}

	cssp_camera_resources[0].start = cssp_gpmc_mem_base_phys;
	cssp_camera_resources[0].end = cssp_gpmc_mem_base_phys + 0x1ffff;

	if (gpmc_cs_configure(cs, GPMC_CONFIG_DEV_TYPE, GPMC_DEVICETYPE_NOR) < 0) {
			printk(KERN_ERR "[cssp_cam platform init]: gpmc_cs_configure failed\n");
			return -1;
	}

	val = GPMC_CONFIG1_READMULTIPLE_SUPP;
	val |= GPMC_CONFIG1_READTYPE_SYNC;
	val |= GPMC_CONFIG1_WRITETYPE_SYNC;
	val |= GPMC_CONFIG1_CLKACTIVATIONTIME(1);
	val |= GPMC_CONFIG1_PAGE_LEN(2);
	val |= GPMC_CONFIG1_DEVICESIZE_16;
	val |= GPMC_CONFIG1_DEVICETYPE_NOR;
	val |= GPMC_CONFIG1_MUXADDDATA;
	gpmc_cs_write_reg(cs, GPMC_CS_CONFIG1, val);

	if (gpmc_cs_set_timings(cs, &cssp_timings) < 0) {
		printk(KERN_ERR "Failed gpmc_cs_set_timings for QuickLogic CAMIF device\n");
		goto free;
	}

	val = gpmc_cs_read_reg(cs, GPMC_CS_CONFIG6);
	val &= 0xe0f0f030;
	val |= 0x07030481;
	gpmc_cs_write_reg(cs, GPMC_CS_CONFIG6, val);

	printk(KERN_INFO "gpmc_cssp_init for QuickLogic CAMIF device succeeded\n");

	return 0;

free:
	gpmc_cs_free(cs);

	printk(KERN_ERR "Could not initialize QuickLogic CAMIF device\n");

	return -1;
}

static void cssp_gpmc_init(void)
{
	struct gpmc_devices_info gpmc_device[2] = {
			{ NULL, GPMC_DEVICE_NOR },
		};

	setup_pin_mux(gpmc_pin_mux);
	setup_pin_mux(camera_cape_pin_mux);

	omap_init_gpmc(gpmc_device, sizeof(gpmc_device));
	gpmc_cssp_init();

	platform_device_register(&cssp_camera);

	printk(KERN_INFO "[cssp_cam platform init]: cssp_gpmc_init: DONE\n");
}


static void lcd3leds_init(int evm_id, int profile )
{
	int err;
	err = platform_device_register(&lcd3_leds_gpio);
	if (err)
		pr_err("failed to register BeagleBone LCD cape LEDs\n");
}

static void lcd7leds_init(int evm_id, int profile )
{
	int err;
	err = platform_device_register(&lcd7_leds_gpio);
	if (err)
		pr_err("failed to register BeagleBone LCD cape LED\n");
}

static void bonew1_gpio_init(int evm_id, int profile )
{
	beaglebone_w1gpio_free = 0;
	int err;
	setup_pin_mux(w1_gpio_pin_mux);
	err = platform_device_register(&bone_w1_device);
	if (err)
		pr_err("failed to register w1-gpio\n");
	else
		pr_info("w1-gpio connected to P8_6\n");
}

static void rgmii1_init(int evm_id, int profile)
{
	setup_pin_mux(rgmii1_pin_mux);
	return;
}

static void rgmii2_init(int evm_id, int profile)
{
	setup_pin_mux(rgmii2_pin_mux);
	return;
}

static void mii1_init(int evm_id, int profile)
{
	setup_pin_mux(mii1_pin_mux);
	return;
}

static void rmii1_init(int evm_id, int profile)
{
	setup_pin_mux(rmii1_pin_mux);
	return;
}

static void usb0_init(int evm_id, int profile)
{
	setup_pin_mux(usb0_pin_mux);
	return;
}

static void usb1_init(int evm_id, int profile)
{
	setup_pin_mux(usb1_pin_mux);
	return;
}

/* setup uart3 */
static void uart3_init(int evm_id, int profile)
{
	setup_pin_mux(uart3_pin_mux);
	return;
}

/* setup uart2 */
static void uart2_init(int evm_id, int profile)
{
	setup_pin_mux(uart2_pin_mux);
	return;
}

/* setup haptics */
#define HAPTICS_MAX_FREQ 250
static void haptics_init(int evm_id, int profile)
{
	setup_pin_mux(haptics_pin_mux);
	pwm_pdata[2].chan_attrib[1].max_freq = HAPTICS_MAX_FREQ;
	am33xx_register_ehrpwm(2, &pwm_pdata[2]);
}

/* NAND partition information */
static struct mtd_partition am335x_nand_partitions[] = {
/* All the partition sizes are listed in terms of NAND block size */
	{
		.name           = "SPL",
		.offset         = 0,			/* Offset = 0x0 */
		.size           = SZ_128K,
	},
	{
		.name           = "SPL.backup1",
		.offset         = MTDPART_OFS_APPEND,	/* Offset = 0x20000 */
		.size           = SZ_128K,
	},
	{
		.name           = "SPL.backup2",
		.offset         = MTDPART_OFS_APPEND,	/* Offset = 0x40000 */
		.size           = SZ_128K,
	},
	{
		.name           = "SPL.backup3",
		.offset         = MTDPART_OFS_APPEND,	/* Offset = 0x60000 */
		.size           = SZ_128K,
	},
	{
		.name           = "U-Boot",
		.offset         = MTDPART_OFS_APPEND,   /* Offset = 0x80000 */
		.size           = 15 * SZ_128K,
	},
	{
		.name           = "U-Boot Env",
		.offset         = MTDPART_OFS_APPEND,   /* Offset = 0x260000 */
		.size           = 1 * SZ_128K,
	},
	{
		.name           = "Kernel",
		.offset         = MTDPART_OFS_APPEND,   /* Offset = 0x280000 */
		.size           = 40 * SZ_128K,
	},
	{
		.name           = "File System",
		.offset         = MTDPART_OFS_APPEND,   /* Offset = 0x780000 */
		.size           = MTDPART_SIZ_FULL,
	},
};

/* SPI 0/1 Platform Data */
/* SPI flash information */
static struct mtd_partition am335x_spi_partitions[] = {
	/* All the partition sizes are listed in terms of erase size */
	{
		.name       = "SPL",
		.offset     = 0,			/* Offset = 0x0 */
		.size       = SZ_128K,
	},
	{
		.name       = "U-Boot",
		.offset     = MTDPART_OFS_APPEND,	/* Offset = 0x20000 */
		.size       = 2 * SZ_128K,
	},
	{
		.name       = "U-Boot Env",
		.offset     = MTDPART_OFS_APPEND,	/* Offset = 0x60000 */
		.size       = 2 * SZ_4K,
	},
	{
		.name       = "Kernel",
		.offset     = MTDPART_OFS_APPEND,	/* Offset = 0x62000 */
		.size       = 28 * SZ_128K,
	},
	{
		.name       = "File System",
		.offset     = MTDPART_OFS_APPEND,	/* Offset = 0x3E2000 */
		.size       = MTDPART_SIZ_FULL,		/* size ~= 4.1 MiB */
	}
};

static const struct flash_platform_data am335x_spi_flash = {
	.type      = "w25q64",
	.name      = "spi_flash",
	.parts     = am335x_spi_partitions,
	.nr_parts  = ARRAY_SIZE(am335x_spi_partitions),
};

/*
 * SPI Flash works at 80Mhz however SPI Controller works at 48MHz.
 * So setup Max speed to be less than that of Controller speed
 */
static struct spi_board_info am335x_spi0_slave_info[] = {
	{
		.modalias      = "m25p80",
		.platform_data = &am335x_spi_flash,
		.irq           = -1,
		.max_speed_hz  = 24000000,
		.bus_num       = 1,
		.chip_select   = 0,
	},
};

static struct spi_board_info am335x_spi1_slave_info[] = {
	{
		.modalias      = "m25p80",
		.platform_data = &am335x_spi_flash,
		.irq           = -1,
		.max_speed_hz  = 12000000,
		.bus_num       = 2,
		.chip_select   = 0,
	},
};

static struct spi_board_info bone_spidev2_info[] = {
	{
		.modalias      = "spidev",
		.irq           = -1,
		.max_speed_hz  = 12000000,
		.bus_num       = 2,
		.chip_select   = 0,
	},
};

static struct gpmc_timings am335x_nand_timings = {
	.sync_clk = 0,

	.cs_on = 0,
	.cs_rd_off = 44,
	.cs_wr_off = 44,

	.adv_on = 6,
	.adv_rd_off = 34,
	.adv_wr_off = 44,
	.we_off = 40,
	.oe_off = 54,

	.access = 64,
	.rd_cycle = 82,
	.wr_cycle = 82,

	.wr_access = 40,
	.wr_data_mux_bus = 0,
};

static void evm_nand_init(int evm_id, int profile)
{
	struct omap_nand_platform_data *pdata;
	struct gpmc_devices_info gpmc_device[2] = {
		{ NULL, 0 },
		{ NULL, 0 },
	};

	setup_pin_mux(nand_pin_mux);
	pdata = omap_nand_init(am335x_nand_partitions,
		ARRAY_SIZE(am335x_nand_partitions), 0, 0,
		&am335x_nand_timings);
	if (!pdata)
		return;
	pdata->ecc_opt =OMAP_ECC_BCH8_CODE_HW;
	pdata->elm_used = true;
	gpmc_device[0].pdata = pdata;
	gpmc_device[0].flag = GPMC_DEVICE_NAND;

	omap_init_gpmc(gpmc_device, sizeof(gpmc_device));
	omap_init_elm();
}

static struct regulator_consumer_supply bone_audio_supplies[] = {
        /* tlv320aic3x analog supplies */
        REGULATOR_SUPPLY("AVDD", "3-001b"),
        REGULATOR_SUPPLY("DRVDD", "3-001b"),
        /* tlv320aic3x digital supplies */
        REGULATOR_SUPPLY("IOVDD", "3-001b"),
        REGULATOR_SUPPLY("DVDD", "3-001b"),
};

/* TPS65217 voltage regulator support */

/* 1.8V */
static struct regulator_consumer_supply tps65217_dcdc1_consumers[] = {
	{
		.supply = "vdds_osc",
	},
	{
		.supply = "vdds_pll_ddr",
	},
	{
		.supply = "vdds_pll_mpu",
	},
	{
		.supply = "vdds_pll_core_lcd",
	},
	{
		.supply = "vdds_sram_mpu_bb",
	},
	{
		.supply = "vdds_sram_core_bg",
	},
	{
		.supply = "vdda_usb0_1p8v",
	},
	{
		.supply = "vdds_ddr",
	},
	{
		.supply = "vdds",
	},
	{
		.supply = "vdds_hvx_1p8v",
	},
	{
		.supply = "vdda_adc",
	},
	{
		.supply = "ddr2",
	},
};

/* 1.1V */
static struct regulator_consumer_supply tps65217_dcdc2_consumers[] = {
	{
		.supply = "vdd_mpu",
	},
};

/* 1.1V */
static struct regulator_consumer_supply tps65217_dcdc3_consumers[] = {
	{
		.supply = "vdd_core",
	},
};

/* 1.8V LDO */
static struct regulator_consumer_supply tps65217_ldo1_consumers[] = {
	{
		.supply = "vdds_rtc",
	},
};

/* 3.3V LDO */
static struct regulator_consumer_supply tps65217_ldo2_consumers[] = {
	{
		.supply = "vdds_any_pn",
	},
};

/* 3.3V LDO */
static struct regulator_consumer_supply tps65217_ldo3_consumers[] = {
	{
		.supply = "vdds_hvx_ldo3_3p3v",
	},
	{
		.supply = "vdda_usb0_3p3v",
	},
};

/* 3.3V LDO */
static struct regulator_consumer_supply tps65217_ldo4_consumers[] = {
	{
		.supply = "vdds_hvx_ldo4_3p3v",
	},
};

static struct regulator_init_data tps65217_regulator_data[] = {
	/* dcdc1 */
	{
		.constraints = {
			.min_uV = 900000,
			.max_uV = 1800000,
			.boot_on = 1,
			.always_on = 1,
		},
		.num_consumer_supplies = ARRAY_SIZE(tps65217_dcdc1_consumers),
		.consumer_supplies = tps65217_dcdc1_consumers,
	},

	/* dcdc2 */
	{
		.constraints = {
			.min_uV = 900000,
			.max_uV = 3300000,
			.valid_ops_mask = (REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS),
			.boot_on = 1,
			.always_on = 1,
		},
		.num_consumer_supplies = ARRAY_SIZE(tps65217_dcdc2_consumers),
		.consumer_supplies = tps65217_dcdc2_consumers,
	},

	/* dcdc3 */
	{
		.constraints = {
			.min_uV = 900000,
			.max_uV = 1500000,
			.valid_ops_mask = (REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS),
			.boot_on = 1,
			.always_on = 1,
		},
		.num_consumer_supplies = ARRAY_SIZE(tps65217_dcdc3_consumers),
		.consumer_supplies = tps65217_dcdc3_consumers,
	},

	/* ldo1 */
	{
		.constraints = {
			.min_uV = 1000000,
			.max_uV = 3300000,
			.valid_ops_mask = REGULATOR_CHANGE_STATUS,
			.boot_on = 1,
			.always_on = 1,
		},
		.num_consumer_supplies = ARRAY_SIZE(tps65217_ldo1_consumers),
		.consumer_supplies = tps65217_ldo1_consumers,
	},

	/* ldo2 */
	{
		.constraints = {
			.min_uV = 900000,
			.max_uV = 3300000,
			.valid_ops_mask = (REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS),
			.boot_on = 1,
			.always_on = 1,
		},
		.num_consumer_supplies = ARRAY_SIZE(tps65217_ldo2_consumers),
		.consumer_supplies = tps65217_ldo2_consumers,
	},

	/* ldo3 */
	{
		.constraints = {
			.min_uV = 1800000,
			.max_uV = 3300000,
			.valid_ops_mask = (REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS),
			.boot_on = 1,
			.always_on = 1,
		},
		.num_consumer_supplies = ARRAY_SIZE(tps65217_ldo3_consumers),
		.consumer_supplies = tps65217_ldo3_consumers,
	},

	/* ldo4 */
	{
		.constraints = {
			.min_uV = 1800000,
			.max_uV = 3300000,
			.valid_ops_mask = (REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS),
			.boot_on = 1,
			.always_on = 1,
		},
		.num_consumer_supplies = ARRAY_SIZE(tps65217_ldo4_consumers),
		.consumer_supplies = tps65217_ldo4_consumers,
	},
};

static struct tps65217_board beaglebone_tps65217_info = {
	.tps65217_init_data = &tps65217_regulator_data[0],
};

static struct lis3lv02d_platform_data lis331dlh_pdata = {
	.click_flags = LIS3_CLICK_SINGLE_X |
			LIS3_CLICK_SINGLE_Y |
			LIS3_CLICK_SINGLE_Z,
	.wakeup_flags = LIS3_WAKEUP_X_LO | LIS3_WAKEUP_X_HI |
			LIS3_WAKEUP_Y_LO | LIS3_WAKEUP_Y_HI |
			LIS3_WAKEUP_Z_LO | LIS3_WAKEUP_Z_HI,
	.irq_cfg = LIS3_IRQ1_CLICK | LIS3_IRQ2_CLICK,
	.wakeup_thresh	= 10,
	.click_thresh_x = 10,
	.click_thresh_y = 10,
	.click_thresh_z = 10,
	.g_range	= 2,
	.st_min_limits[0] = 120,
	.st_min_limits[1] = 120,
	.st_min_limits[2] = 140,
	.st_max_limits[0] = 550,
	.st_max_limits[1] = 550,
	.st_max_limits[2] = 750,
};

static struct i2c_board_info am335x_i2c_boardinfo1[] = {
	{
		I2C_BOARD_INFO("tlv320aic3x", 0x1b),
	},
	{
		I2C_BOARD_INFO("lis331dlh", 0x18),
		.platform_data = &lis331dlh_pdata,
	},
	{
		I2C_BOARD_INFO("tsl2550", 0x39),
	},
	{
		I2C_BOARD_INFO("tmp275", 0x48),
	},
};

static void i2c1_init(int evm_id, int profile)
{
	setup_pin_mux(i2c1_pin_mux);
	omap_register_i2c_bus(2, 100, am335x_i2c_boardinfo1,
			ARRAY_SIZE(am335x_i2c_boardinfo1));
	return;
}

static struct mcp251x_platform_data mcp251x_info = {
	.oscillator_frequency = 16000000,
};

static struct spi_board_info tt3201_spi_info[] = {
	{
		.modalias	= "mcp2515",
		.max_speed_hz	= 10000000,
		.bus_num	= 2,
		.chip_select	= 0,
		.mode		= SPI_MODE_0,
		.platform_data	= &mcp251x_info,
	},
	{
		.modalias	= "mcp2515",
		.max_speed_hz	= 10000000,
		.bus_num	= 2,
		.chip_select	= 1,
		.mode		= SPI_MODE_0,
		.platform_data	= &mcp251x_info,
	},
};

static void tt3201_init(int evm_id, int profile)
{
	pr_info("TowerTech TT3201 CAN cape\n");

	beaglebone_spi1_free = 0;
	setup_pin_mux(spi1_pin_mux);
	setup_pin_mux(tt3201_pin_mux);

	tt3201_spi_info[0].irq = gpio_to_irq(GPIO_TO_PIN(3, 19));
	tt3201_spi_info[1].irq = gpio_to_irq(GPIO_TO_PIN(3, 21));

	spi_register_board_info(tt3201_spi_info,
		ARRAY_SIZE(tt3201_spi_info));

	am33xx_d_can_init(1);
}

/* Setup McASP 1 */
static void mcasp1_init(int evm_id, int profile)
{
	/* Configure McASP */
	setup_pin_mux(mcasp1_pin_mux);
	am335x_register_mcasp(&am335x_evm_snd_data1, 1);
	return;
}

/* Setup McASP 0 */
static void mcasp0_init(int evm_id, int profile)
{
	printk("Beaglebone cape: mcasp0 init\n");
	/* Configure McASP */
	setup_pin_mux(mcasp0_pin_mux);
	am335x_register_mcasp(&bone_snd_data1, 0);
	return;
}

static void mmc0_init(int evm_id, int profile)
{
	setup_pin_mux(mmc0_pin_mux);

	omap2_hsmmc_init(am335x_mmc);
	return;
}

static void emmc_bone_init(int evm_id, int profile)
{
	printk("cape: emmc_bone_init\n");
	setup_pin_mux(mmc1_pin_mux);
	am335x_mmc[1].mmc = 2;
	am335x_mmc[1].caps = MMC_CAP_4_BIT_DATA;
	am335x_mmc[1].nonremovable = true;
	am335x_mmc[1].gpio_cd = -EINVAL;
	am335x_mmc[1].gpio_wp = -EINVAL;
	am335x_mmc[1].ocr_mask = MMC_VDD_32_33 | MMC_VDD_33_34; /* 3V3 */
	/* mmc will be initialized when mmc0_init is called */
	return;
}

static const char* cape_pins[] = {
/*
  From SRM RevA5.0.1:
*/
  /* offset  88 - P9-22 */ "uart2_rxd",
  /* offset  90 - P9-21 */ "uart2_txd",
  /* offset  92 - P9-18 */ "spi0_d1",
  /* offset  94 - P9-17 */ "spi0_cs0",
  /* offset  96 - P9-42 */ "ecap0_in_pwm0_out",
  /* offset  98 - P8-35 */ "lcd_data12",
  /* offset 100 - P8-33 */ "lcd_data13",
  /* offset 102 - P8-31 */ "lcd_data14",
  /* offset 104 - P8-32 */ "lcd_data15",
  /* offset 106 - P9-19 */ "uart1_rtsn",
  /* offset 108 - P9-20 */ "uart1_ctsn",
  /* offset 110 - P9-26 */ "uart1_rxd",
  /* offset 112 - P9-24 */ "uart1_txd",
  /* offset 114 - P9-41 */ "xdma_event_intr1",
  /* offset 116 - P8-19 */ "gpmc_ad8",
  /* offset 118 - P8-13 */ "gpmc_ad9",
  /* offset 120 - P8-14 */ "gpmc_ad10",
  /* offset 122 - P8-17 */ "gpmc_ad12",
  /* offset 124 - P9-11 */ "gpmc_wait0",
  /* offset 126 - P9-13 */ "gpmc_wpn",
  /* offset 128 - P8-25 */ "gpmc_ad0",
  /* offset 130 - P8-24 */ "gpmc_ad1",
  /* offset 132 - P8- 5 */ "gpmc_ad2",
  /* offset 134 - P8- 6 */ "gpmc_ad3",
  /* offset 136 - P8-23 */ "gpmc_ad4",
  /* offset 138 - P8-22 */ "gpmc_ad5",
  /* offset 140 - P8- 3 */ "gpmc_ad6",
  /* offset 142 - P8- 4 */ "gpmc_ad7",
  /* offset 144 - P8-12 */ "gpmc_ad12",
  /* offset 146 - P8-11 */ "gpmc_ad13",
  /* offset 148 - P8-16 */ "gpmc_ad14",
  /* offset 150 - P8-15 */ "gpmc_ad15",
  /* offset 152 - P9-15 */ "gpmc_a0",
  /* offset 154 - P9-23 */ "gpmc_a1",
  /* offset 156 - P9-14 */ "gpmc_a2",
  /* offset 158 - P9-16 */ "gpmc_a3",
  /* offset 160 - P9-12 */ "gpmc_be1n",
  /* offset 162 - P8-26 */ "gpmc_csn0",
  /* offset 164 - P8-21 */ "gpmc_csn1",
  /* offset 166 - P8-20 */ "gpmc_csn2",
  /* offset 168 - P8-18 */ "gpmc_clk",
  /* offset 170 - P8-7  */ "gpmc_advn_ale",
  /* offset 172 - P8-9  */ "gpmc_ben0_cle",
  /* offset 174 - P8-10 */ "gpmc_wen",
  /* offset 176 - P8-8  */ "gpmc_csn3",
  /* offset 178 - P8-45 */ "lcd_data0",
  /* offset 180 - P8-46 */ "lcd_data1",
  /* offset 182 - P8-43 */ "lcd_data2",
  /* offset 184 - P8-44 */ "lcd_data3",
  /* offset 186 - P8-41 */ "lcd_data4",
  /* offset 188 - P8-42 */ "lcd_data5",
  /* offset 190 - P8-39 */ "lcd_data6",
  /* offset 192 - P8-40 */ "lcd_data7",
  /* offset 194 - P8-37 */ "lcd_data8",
  /* offset 196 - P8-38 */ "lcd_data9",
  /* offset 198 - P8-36 */ "lcd_data10",
  /* offset 200 - P8-34 */ "lcd_data11",
  /* offset 202 - P8-27 */ "lcd_vsync",
  /* offset 204 - P8-29 */ "lcd_hsync",
  /* offset 206 - P8-28 */ "lcd_pclk",
  /* offset 208 - P8-30 */ "lcd_ac_bias_en",
  /* offset 210 - P9-29 */ "mcasp0_fsx",
  /* offset 212 - P9-30 */ "mcasp0_axr0",
  /* offset 214 - P9-28 */ "mcasp0_ahclkr",
  /* offset 216 - P9-27 */ "mcasp0_fsr",
  /* offset 218 - P9-31 */ "mcasp0_aclkx",
  /* offset 220 - P9-25 */ "mcasp0_ahclkx",
  /* offset 222 - P9-39 */ "ain0",
  /* offset 224 - P9-40 */ "ain1",
  /* offset 226 - P9-37 */ "ain2",
  /* offset 228 - P9-38 */ "ain3",
  /* offset 230 - P9-33 */ "ain4",
  /* offset 232 - P9-36 */ "ain5",
  /* offset 234 - P9-35 */ "ain6",
};

#define BIG_ENDIAN_16( i) ( ((i & 255) << 8) | ((i >> 8) & 255) )
#define NR_ITEMS( x) (sizeof( (x)) / sizeof( *(x)))

extern int am33xx_mux_get_entry( int index, struct omap_mux** mux);

typedef union {
/*
  From SRM RevA5.0.1:
  Bit 15     Pin is used or not: 0=Unused by Cape 1=Used by Cape
  Bit 14-13  Pin Direction: 10=Output 01=Input 11=BDIR
  Bit 12-7   Reserved
  Bit 6	     Slew Rate: 0=Fast 1=Slow
  Bit 5	     Rx Enable: 0=Disabled 1=Enabled
  Bit 4	     Pull Up/Dn Select: 0=Pulldown 1=PullUp
  Bit 3	     Pull Up/DN enabled: 0=Enabled 1=Disabled
  Bit 2-0    Mux Mode Selection: Mode 0-7
*/
	struct {
		uint16_t    mux		    : 3;
		uint16_t    pull_enable	    : 1;
		uint16_t    pull_up	    : 1;
		uint16_t    rx_enable	    : 1;
		uint16_t    slew_rate	    : 1;
		uint16_t    reserved	    : 6;
		uint16_t    direction	    : 2;
		uint16_t    used	    : 1;
	};
	uint16_t      value;
} pin_def;

#define DEBUG_EEPROM_CONFIG 0

static int bone_io_get_mux_setting( pin_def setting)
{
	int pin_setting;

	switch (setting.direction) {
	case 1:
	/* input */
		if (setting.pull_enable) {
			if (setting.pull_up) {
				pin_setting = AM33XX_PIN_INPUT_PULLUP;
			} else {
				pin_setting = AM33XX_PIN_INPUT_PULLDOWN;
			}
		} else {
			pin_setting = AM33XX_PIN_INPUT;
		}
		if (!setting.rx_enable) {
			pr_debug( "  pin is set as input but the receiver is not enabled!\n");
		}
		break;
	case 2:
	/* output */
		pin_setting = AM33XX_PIN_OUTPUT;
		break;
	case 3:
	/* bi-dir */
	default:
	/* reserved */
		pr_debug( "  pin ignored because it uses an unsupported mode: 0x%04x\n",
			setting.direction);
		return -1;
	}
	pr_debug("  pin is configured as %s\n",
		(pin_setting & AM33XX_PIN_INPUT) ? "input" : "output");
	switch (setting.mux) {
	case 0: pin_setting |= OMAP_MUX_MODE0; break;
	case 1: pin_setting |= OMAP_MUX_MODE1; break;
	case 2: pin_setting |= OMAP_MUX_MODE2; break;
	case 3: pin_setting |= OMAP_MUX_MODE3; break;
	case 4: pin_setting |= OMAP_MUX_MODE4; break;
	case 5: pin_setting |= OMAP_MUX_MODE5; break;
	case 6: pin_setting |= OMAP_MUX_MODE6; break;
	case 7: pin_setting |= OMAP_MUX_MODE7; break;
	}
	return pin_setting;
}

static struct omap_mux* bone_io_pin_lookup( const char* pin_name)
{
	int index = 0;
	struct omap_mux* mux;

	for (;;) {
		if (am33xx_mux_get_entry( index, &mux) < 0) {
			/* no more entries */
			pr_debug( "   configuration error, pin '%s' not found in mux database\n",
				pin_name);
			return NULL;
		}
		if (mux != NULL &&
		    mux->muxnames[ 0] != NULL &&
		    strcmp( mux->muxnames[ 0], pin_name) == 0)
		{
			/* entry found */
			pr_debug( "   found pin '%s' at index %d in mux database'\n",
				pin_name, index);
			return mux;
		}
		++index;
	}
}

static int bone_io_config_pin( const char* pin_name, pin_def eeprom_setting)
{
	struct omap_mux* mux;
	char* signal_name;
	int pin_setting = bone_io_get_mux_setting( eeprom_setting);
	int l1, l2;
	char full_name[ 50];

	if (pin_setting < 0) {
		return -1;
	}

	mux = bone_io_pin_lookup( pin_name);

	if (mux == NULL) {
		return -1;
	}

	signal_name = mux->muxnames[ eeprom_setting.mux];

	if (signal_name == NULL) {
		pr_debug( "    Configuration error, no signal found for pin '%s' in mode %d\n",
			pin_name, eeprom_setting.mux);
		return -1;
	}
	pr_debug( "    setting pin '%s' to signal '%s'\n",
		 pin_name, signal_name);
	l1 = strlen( pin_name);
	l2 = strlen( signal_name);

	if (l1 + 1 + l2 + 1 > sizeof( full_name)) {
		pr_debug( "    Internal error, combined signal name too long\n");
		return -1;
	} else {
		memcpy( full_name, pin_name, l1);
		full_name[ l1] = '.';
		memcpy( full_name + l1 + 1, signal_name, l2);
		full_name[ l1 + 1 + l2] = '\0';
		if (omap_mux_init_signal( full_name, pin_setting) < 0) {
			return -1;
		}
		pr_debug( "	mux '%s' was set to mode 0x%04x\n",
			 full_name, pin_setting);
	}
	// return 0 for input, 1 for output
	return (pin_setting & AM33XX_PIN_INPUT) ? 0 : 1;
}

#define RULER( x) \
	do { \
		char* p = status; \
		int i = 0; \
		int cnt = x; \
		status[ cnt] = '\0'; \
		while (cnt--) { \
			if (++i == 10) { \
				*p++ = '+'; \
				i = 0; \
			} else { \
				*p++ = '-'; \
			} \
		} \
		pr_info( "+%s+\n", status); \
	} while (0)

static int bone_io_config_from_cape_eeprom( void)
{
	int i;
	int cnt = BIG_ENDIAN_16( cape_config.numpins);
	u16* pmuxdata;
	char status[ NR_ITEMS( cape_config.muxdata) + 1];
	char revision[ NR_ITEMS( cape_config.format_revision) + 1];

	strncpy( revision, cape_config.format_revision, NR_ITEMS( revision) - 1);
	revision[ NR_ITEMS( revision) - 1] = '\0';
	if (strcmp( revision, "A0") != 0) {
		pr_warn( "BeagleBone cape: configuration revision '%s' is not supported\n",
			revision);
		return -1;
	}

	// Workaround for capes that have encoded this as ASCII
	if (cnt > 256) {
		pr_info( "BeagleBone cape: workaround for bad 'numpins' setting\n");
		cnt = (cape_config.numpins & 255) - '0';
		cnt = 10 * cnt + ((cape_config.numpins >> 8) & 255) - '0';
	}
	pr_info( "BeagleBone cape: revision %s format, configuring %2d out of %2d signals:\n",
		revision, cnt, NR_ITEMS( cape_config.muxdata));
	RULER( NR_ITEMS( cape_config.muxdata));
	for (i = 0, pmuxdata = cape_config.muxdata ; i < NR_ITEMS( cape_config.muxdata) ; ++i, ++pmuxdata) {
		const char* pin_name = cape_pins[ i];
		pin_def pin_setting = { .value = BIG_ENDIAN_16( *pmuxdata) };

		// Detect broken I2C configuration to prevent bus hangup!
		if (i >= 9 && i <= 10) {
			status[ i] = '-';
		} else if (pin_setting.used) {
			switch (bone_io_config_pin( pin_name, pin_setting)) {
			case 0:	 status[ i] = 'i'; break;
			case 1:	 status[ i] = 'o'; break;
			default: status[ i] = '#'; break;
			}
		} else {
			status[ i] = ' ';
		}
	}
	status[ NR_ITEMS( cape_config.muxdata)] = '\0';
	pr_info( "|%s|\n", status);
	RULER( NR_ITEMS( cape_config.muxdata));
	return 0;
}
static void beaglebone_cape_setup(struct memory_accessor *mem_acc, void *context)
{
	int ret;
	char tmp[32];
	char name[32];
	char version[4];
	char manufacturer[32];

	capecount++;
	/* get cape specific data */
	ret = mem_acc->read(mem_acc, (char *)&cape_config, 0, sizeof(cape_config));
	if (ret != sizeof(cape_config)) {
		pr_warning("BeagleBone cape EEPROM: could not read eeprom at address 0x%x\n", capecount + 0x53);
		goto out2;
	}

	if (cape_config.header != AM335X_EEPROM_HEADER) {
		pr_warning("BeagleBone cape EEPROM: wrong header 0x%x, expected 0x%x\n",
			cape_config.header, AM335X_EEPROM_HEADER);
		goto out;
	}

	pr_info("BeagleBone cape EEPROM: found eeprom at address 0x%x\n", capecount + 0x53);
	snprintf(name, sizeof(cape_config.name) + 1, "%s", cape_config.name);
	snprintf(version, sizeof(cape_config.version) + 1, "%s", cape_config.version);
	snprintf(manufacturer, sizeof(cape_config.manufacturer) + 1, "%s", cape_config.manufacturer);
	pr_info("BeagleBone cape: %s %s, revision %s\n", manufacturer, name, version);
	snprintf(tmp, sizeof(cape_config.partnumber) + 1, "%s", cape_config.partnumber);
	pr_info("BeagleBone cape partnumber: %s\n", tmp);

	if (!strncmp( "BEBOPR", cape_config.name, 6)) {
		pr_info( "BeagleBone cape: initializing BEBOPR cape\n");
		bone_io_config_from_cape_eeprom();
		return;	// if configured from eeprom, skip all other initialization
	}

	if (!strncmp("BB-BONE-DVID-01", cape_config.partnumber, 15)) {
		pr_info("BeagleBone cape: initializing DVI cape\n");

		beaglebone_leds_free = 0;
		dvileds_init(0,0);

		// Uncomment these for custom DVI capes with audio populated
		//beaglebone_spi1_free = 0;
		//mcasp0_init(0,0);

		if (!strncmp("00A1", cape_config.version, 4) || !strncmp("000A", cape_config.version, 4)) {
			pr_info("BeagleBone cape: DVI init for revision A1 or older\n");
			setup_pin_mux(dvia1_pin_mux);
			gpio_request(BEAGLEBONEDVI_PDn_A1, "DVI_PDn");
			gpio_direction_output(BEAGLEBONEDVI_PDn_A1, 1);
			dvi_init(0,0);
		} else {
			pr_info("BeagleBone cape: DVI init for revision A2 or newer\n");
			setup_pin_mux(dvia2_pin_mux);
			gpio_request(BEAGLEBONEDVI_PDn_A2, "DVI_PDn");
			gpio_direction_output(BEAGLEBONEDVI_PDn_A2, 1);
			dvi_init(0,0);
		}
	}

	if (!strncmp("BB-BONE-DVID-02", cape_config.partnumber, 15)) {
		pr_info("BeagleBone cape: initializing DVI+audio cape\n");

		beaglebone_leds_free = 0;
		dvileds_init(0,0);

		beaglebone_spi1_free = 0;
		mcasp0_init(0,0);

		setup_pin_mux(dvia2_pin_mux);
		gpio_request(BEAGLEBONEDVI_PDn_A2, "DVI_PDn");
		gpio_direction_output(BEAGLEBONEDVI_PDn_A2, 1);
		dvi_init(0,0);
	}

	if (!strncmp("BB-BONE-AUDI-01", cape_config.partnumber, 15)) {
		pr_info("BeagleBone cape: initializing audio cape\n");

		beaglebone_leds_free = 0;
		dvileds_init(0,0);

		beaglebone_spi1_free = 0;
		mcasp0_init(0,0);
	}

	if (!strncmp("BB-BONE-LCD7-01", cape_config.partnumber, 15)) {
		pr_info("BeagleBone cape: initializing LCD cape\n");

		if (!strncmp("00A2", cape_config.version, 4)) {
			setup_pin_mux(bbtoys7a2_pin_mux);
			gpio_request(BEAGLEBONE_LCD7A2_AVDD_EN, "BONE_LCD_AVDD_EN");
			gpio_direction_output(BEAGLEBONE_LCD7A2_AVDD_EN, 1);
		} else if(!strncmp("00A3", cape_config.version, 4)) {
			setup_pin_mux(bbtoys7a3_pin_mux);
			gpio_request(BEAGLEBONE_LCD7A3_AVDD_EN, "BONE_LCD_AVDD_EN");
			gpio_direction_output(BEAGLEBONE_LCD7A3_AVDD_EN, 1);
		} else if(!strncmp("00A4", cape_config.version, 4)) {
			setup_pin_mux(bbtoys7a3_pin_mux);
			gpio_request(BEAGLEBONE_LCD7A3_AVDD_EN, "BONE_LCD_AVDD_EN");
			gpio_direction_output(BEAGLEBONE_LCD7A3_AVDD_EN, 1);
		} else {
			setup_pin_mux(bbtoys7_pin_mux);
			gpio_request(BEAGLEBONE_LCD_AVDD_EN, "BONE_LCD_AVDD_EN");
			gpio_direction_output(BEAGLEBONE_LCD_AVDD_EN, 1);
			bbtoys7lcd_init(0,0);
		}

		bbtoys7lcd_init(0,0);
		pr_info("BeagleBone cape: initializing LCD cape touchscreen\n");
		tsc_init(0,0);
		pr_info("BeagleBone cape: Registering PWM backlight for LCD cape\n");
		enable_ehrpwm1(0,0);
		beaglebone_tsadcpins_free = 0;
		pr_info("BeagleBone cape: Registering gpio-keys for LCD cape\n");
		if (!strncmp("00A3", cape_config.version, 4)) {
			beaglebone_lcd7a3_keys_init(0,0);
		} else {
			beaglebone_lcd7_keys_init(0,0);
		}

		beaglebone_spi1_free = 0;
		// A1 or newer
		beaglebone_leds_free = 0;
		lcd7leds_init(0,0);
	}

	if (!strncmp("BB-BONE-LCD3-01", cape_config.partnumber, 15)) {
		pr_info("BeagleBone cape: initializing LCD cape\n");
		bbtoys35lcd_init(0,0);
		pr_info("BeagleBone cape: initializing LCD cape touchscreen\n");
		tsc_init(0,0);
		beaglebone_tsadcpins_free = 0;

		if (!strncmp("00A0", cape_config.version, 4)) {
			pr_info("BeagleBone cape: Registering gpio-keys for LCD3 rev A or earlier cape\n");
			int err;
			setup_pin_mux(lcd3_keys_pin_mux);
			err = platform_device_register(&beaglebone_lcd3_keys);
			if (err)
				pr_err("failed to register gpio keys for LCD3 rev A or earlier cape\n");
			lcd3leds_init(0,0);
		} else if (!strncmp("00A1", cape_config.version, 4)) {
			pr_info("BeagleBone cape: Registering gpio-keys for LCD rev A1 or later cape\n");
			int err;
			setup_pin_mux(lcd3a1_keys_pin_mux);
			err = platform_device_register(&lcd3a1_keys);
			if (err)
				pr_err("failed to register gpio keys for LCD3 rev A1 or later cape\n");
			lcd3leds_init(0,0);
		} else {
			pr_info("BeagleBone cape: Registering gpio-keys for LCD rev A2 or later cape\n");
			int err;
			setup_pin_mux(lcd3a2_keys_pin_mux);
			err = platform_device_register(&lcd3a2_keys);
			if (err)
				pr_err("failed to register gpio keys for LCD3 rev A2 or later cape\n");
			platform_device_register(&lcd3a2_leds_gpio);
			// uses PWM backlight instead of PMIC
			setup_pin_mux(bbtoys3_pin_mux);
			enable_ehrpwm1(0,0);
		}
		beaglebone_spi1_free = 0;
		beaglebone_leds_free = 0;
	}

	if (!strncmp("BB-BONE-LCD4-01", cape_config.partnumber, 15)) {
		pr_info("BeagleBone cape: initializing LCD cape\n");
		bbtoys43lcd_init(0,0);
		pr_info("BeagleBone cape: initializing LCD cape touchscreen\n");
		tsc_init(0,0);
		beaglebone_tsadcpins_free = 0;
	}

	if (!strncmp("BB-BONE-VGA-01", cape_config.partnumber, 14)) {
		pr_info("BeagleBone cape: initializing VGA cape\n");
		dvi_init(0,0);
	}

	if (!strncmp("BB-BONE-BATT-01", cape_config.partnumber, 15)) {
		pr_info("BeagleBone cape: initializing battery cape\n");
		// gpio1_6, P9_15 lowbat output
		// AIN4, P9_33 vbat
		/* LBO doesn't work, so disable it for now
		setup_pin_mux(batterycape_pin_mux);
		#if defined(CONFIG_CHARGER_GPIO)
			int err;
			err = platform_device_register(&beaglebone_charger_device);
			if (err)
				pr_err("failed to register BeagleBone battery cape gpio\n");

		#endif
		*/
	}

	if (!strncmp("BB-BONE-eMMC1-01", cape_config.partnumber, 15)) {
		pr_info("BeagleBone cape: initializing eMMC cape\n");
		beaglebone_w1gpio_free = 0;
		emmc_bone_init(0,0);
	}

	if (!strncmp("BB-BONE-SERL-01", cape_config.partnumber, 15)) {
		pr_info("BeagleBone cape: initializing CAN cape\n");
		setup_pin_mux(uart1dcan1_pin_mux);
		am33xx_d_can_init(1);
	}
		if (!strncmp("BB-BONE-SERL-02", cape_config.partnumber, 15)) {
		pr_info("BeagleBone cape: not initializing Profibus cape\n");
		// gpio1_0
		// gpio1_4
		//foo_init(0,0);
	}
		if (!strncmp("BB-BONE-SERL-03", cape_config.partnumber, 15)) {
		pr_info("BeagleBone cape: initializing RS232 cape\n");
		pr_info("BeagleBone RS232 cape: only uart2/ttyO2 is supported.\n");
		pr_info("BeagleBone RS232 cape: Jumper J27 and J23.\n");
		setup_pin_mux(uart2_pin_mux);
	}
		if (!strncmp("BB-BONE-SERL-04", cape_config.partnumber, 15)) {
		pr_info("BeagleBone cape: not initializing RS485 cape\n");
		//foo_init(0,0);
	}

	if (!strncmp("TT3201-001", cape_config.partnumber, 10)) {
		beaglebone_skip_mmc0_init = 1;

		pr_info("BeagleBone cape: initializing mmc");
		mmc0_init(0,0);

		pr_info("BeagleBone cape: initializing CAN cape\n");
		tt3201_init(0,0);
	}

	if (!strncmp("BB-BONE-CAM-01", cape_config.partnumber, 14) || !strncmp("BB-BONE-CAM3-01", cape_config.partnumber, 15)) {
		pr_info("BeagleBone cape: recognized Camera cape\n");
		beaglebone_w1gpio_free = 0;
		cssp_gpmc_init();
	}

	if (!strncmp("BB-BONE-MOTOR-01", cape_config.partnumber, 16)) {
		pr_info("BeagleBone cape: initializing Motor cape\n");
		beaglebone_w1gpio_free = 0;
		beaglebone_spi1_free = 0;
        }

	goto out2;
out:
	/*
	 * If the EEPROM hasn't been programed or an incorrect header
	 * or board name are read, assume this is an old beaglebone board
	 * (< Rev A3)
	 */
	pr_err("Could not detect BeagleBone cape properly\n");
	beaglebone_cape_detected = false;
out2:
	if (capecount > 3) {
		if (beaglebone_skip_mmc0_init == 0 ) {
			pr_info("BeagleBone cape: initializing mmc");
			mmc0_init(0,0);
		}
		if (beaglebone_tsadcpins_free == 1) {
			pr_info("BeagleBone cape: exporting ADC pins to sysfs\n");
			bone_tsc_init(0,0);
			beaglebone_tsadcpins_free = 0;
		}
		if (beaglebone_leds_free == 1) {
			pr_info("Beaglebone: initializing onboard LEDs");
			boneleds_init(0,0);
		}
		if(beaglebone_spi1_free == 1) {
			beaglebone_spi1_free = 0;
			pr_info("BeagleBone cape: exporting SPI pins as spidev\n");
			setup_pin_mux(spi1_pin_mux);
			spi_register_board_info(bone_spidev2_info, ARRAY_SIZE(bone_spidev2_info));
		}
		if(beaglebone_w1gpio_free == 1) {
			pr_info("BeagleBone cape: initializing w1-gpio\n");
			bonew1_gpio_init(0,0);
		}
	}
}

static struct at24_platform_data cape_eeprom_info = {
        .byte_len       = (256*1024) / 8,
        .page_size      = 64,
        .flags          = AT24_FLAG_ADDR16,
        .context        = (void *)NULL,
		.setup		  = beaglebone_cape_setup,
};

static struct i2c_board_info __initdata cape_i2c_boardinfo[] = {
	{
		I2C_BOARD_INFO("tlv320aic3x", 0x1b),
	},
        {
                I2C_BOARD_INFO("24c256", 0x54),
                .platform_data  = &cape_eeprom_info,
        },
        {
                I2C_BOARD_INFO("24c256", 0x55),
                .platform_data  = &cape_eeprom_info,
        },
        {
                I2C_BOARD_INFO("24c256", 0x56),
                .platform_data  = &cape_eeprom_info,
        },
        {
                I2C_BOARD_INFO("24c256", 0x57),
                .platform_data  = &cape_eeprom_info,
        },
};

static void i2c2_init(int evm_id, int profile)
{
        setup_pin_mux(i2c2_pin_mux);
        omap_register_i2c_bus(3, 100, cape_i2c_boardinfo,
                        ARRAY_SIZE(cape_i2c_boardinfo));
        return;
}

static void mmc1_init(int evm_id, int profile)
{
	setup_pin_mux(mmc1_pin_mux);

	am335x_mmc[1].mmc = 2;
	am335x_mmc[1].caps = MMC_CAP_4_BIT_DATA;
	am335x_mmc[1].gpio_cd = GPIO_TO_PIN(2, 2);
	am335x_mmc[1].gpio_wp = GPIO_TO_PIN(1, 29);
	am335x_mmc[1].ocr_mask = MMC_VDD_32_33 | MMC_VDD_33_34; /* 3V3 */

	/* mmc will be initialized when mmc0_init is called */
	return;
}

static void mmc2_wl12xx_init(int evm_id, int profile)
{
	setup_pin_mux(mmc2_wl12xx_pin_mux);

	am335x_mmc[1].mmc = 3;
	am335x_mmc[1].name = "wl1271";
	am335x_mmc[1].caps = MMC_CAP_4_BIT_DATA | MMC_CAP_POWER_OFF_CARD
				| MMC_PM_KEEP_POWER;
	am335x_mmc[1].nonremovable = true;
	am335x_mmc[1].gpio_cd = -EINVAL;
	am335x_mmc[1].gpio_wp = -EINVAL;
	am335x_mmc[1].ocr_mask = MMC_VDD_32_33 | MMC_VDD_33_34; /* 3V3 */

	/* mmc will be initialized when mmc0_init is called */
	return;
}

static void uart1_wl12xx_init(int evm_id, int profile)
{
	setup_pin_mux(uart1_wl12xx_pin_mux);
}

static void wl12xx_bluetooth_enable(void)
{
	int status = gpio_request(am335xevm_wlan_data.bt_enable_gpio,
		"bt_en\n");
	if (status < 0)
		pr_err("Failed to request gpio for bt_enable");

	pr_info("Configure Bluetooth Enable pin...\n");
	gpio_direction_output(am335xevm_wlan_data.bt_enable_gpio, 0);
}

static int wl12xx_set_power(struct device *dev, int slot, int on, int vdd)
{
	if (on) {
		gpio_set_value(am335xevm_wlan_data.wlan_enable_gpio, 1);
		mdelay(70);
	}
	else
		gpio_set_value(am335xevm_wlan_data.wlan_enable_gpio, 0);

	return 0;
}

static void wl12xx_init(int evm_id, int profile)
{
	struct device *dev;
	struct omap_mmc_platform_data *pdata;
	int ret;

	/* Register WLAN and BT enable pins based on the evm board revision */
	if (gp_evm_revision == GP_EVM_REV_IS_1_1A) {
		am335xevm_wlan_data.wlan_enable_gpio = GPIO_TO_PIN(1, 16);
		am335xevm_wlan_data.bt_enable_gpio = GPIO_TO_PIN(3, 21);
	}
	else {
		am335xevm_wlan_data.wlan_enable_gpio = GPIO_TO_PIN(1, 30);
		am335xevm_wlan_data.bt_enable_gpio = GPIO_TO_PIN(1, 31);
	}

	wl12xx_bluetooth_enable();

	if (wl12xx_set_platform_data(&am335xevm_wlan_data))
		pr_err("error setting wl12xx data\n");

	dev = am335x_mmc[1].dev;
	if (!dev) {
		pr_err("wl12xx mmc device initialization failed\n");
		goto out;
	}

	pdata = dev->platform_data;
	if (!pdata) {
		pr_err("Platfrom data of wl12xx device not set\n");
		goto out;
	}

	ret = gpio_request_one(am335xevm_wlan_data.wlan_enable_gpio,
		GPIOF_OUT_INIT_LOW, "wlan_en");
	if (ret) {
		pr_err("Error requesting wlan enable gpio: %d\n", ret);
		goto out;
	}

	if (gp_evm_revision == GP_EVM_REV_IS_1_1A)
		setup_pin_mux(wl12xx_pin_mux_evm_rev1_1a);
	else
		setup_pin_mux(wl12xx_pin_mux_evm_rev1_0);

	pdata->slots[0].set_power = wl12xx_set_power;
out:
	return;
}

static void d_can_init(int evm_id, int profile)
{
	switch (evm_id) {
	case IND_AUT_MTR_EVM:
		if ((profile == PROFILE_0) || (profile == PROFILE_1)) {
			setup_pin_mux(d_can_ia_pin_mux);
			/* Instance Zero */
			am33xx_d_can_init(0);
		}
		break;
	case GEN_PURP_EVM:
		if (profile == PROFILE_1) {
			setup_pin_mux(d_can_gp_pin_mux);
			/* Instance One */
			am33xx_d_can_init(1);
		}
		break;
	default:
		break;
	}
}

static struct i2c_board_info tps65217_i2c_boardinfo[] = {
	{
		I2C_BOARD_INFO("tps65217", TPS65217_I2C_ID),
		.platform_data  = &beaglebone_tps65217_info,
	},
};

static void tps65217_init(int evm_id, int profile)
{
	struct i2c_adapter *adapter;
	struct i2c_client *client;
	struct device *mpu_dev;
	struct tps65217 *tps;
	unsigned int val;
	int ret;

	mpu_dev = omap_device_get_by_hwmod_name("mpu");
	if (!mpu_dev)
		pr_warning("%s: unable to get the mpu device\n", __func__);

	/* I2C1 adapter request */
	adapter = i2c_get_adapter(1);
	if (!adapter) {
		pr_err("failed to get adapter i2c1\n");
		return;
	}

	client = i2c_new_device(adapter, tps65217_i2c_boardinfo);
	if (!client)
		pr_err("failed to register tps65217 to i2c1\n");

	i2c_put_adapter(adapter);

	tps = (struct tps65217 *)i2c_get_clientdata(client);

	ret = tps65217_reg_read(tps, TPS65217_REG_STATUS, &val);
	if (ret) {
		pr_err("failed to read tps65217 status reg\n");
		return;
	}

	if (!(val & TPS65217_STATUS_ACPWR)) {
		/* If powered by USB then disable OPP120 and OPPTURBO */
		pr_info("Maximum current provided by the USB port is 500mA"
			" which is not sufficient\nwhen operating @OPP120 and"
			" OPPTURBO. The current requirement for some\nuse-cases"
			" using OPP100 might also exceed the maximum current"
			" that the\nUSB port can provide. Unless you are fully"
			" confident that the current\nrequirements for OPP100"
			" use-case don't exceed the USB limits, switching\nto"
			" AC power is recommended.\n");
		opp_disable(mpu_dev, 600000000);
		opp_disable(mpu_dev, 720000000);
	}
}

static void mmc0_no_cd_init(int evm_id, int profile)
{
	setup_pin_mux(mmc0_no_cd_pin_mux);

	omap2_hsmmc_init(am335x_mmc);
	return;
}


/* setup spi0 */
static void spi0_init(int evm_id, int profile)
{
	setup_pin_mux(spi0_pin_mux);
	spi_register_board_info(am335x_spi0_slave_info,
			ARRAY_SIZE(am335x_spi0_slave_info));
	return;
}

/* setup spi1 */
static void spi1_init(int evm_id, int profile)
{
	setup_pin_mux(spi1_pin_mux);
	spi_register_board_info(am335x_spi1_slave_info,
			ARRAY_SIZE(am335x_spi1_slave_info));
	return;
}

static int beaglebone_phy_fixup(struct phy_device *phydev)
{
	phydev->supported &= ~(SUPPORTED_100baseT_Half |
				SUPPORTED_100baseT_Full);

	return 0;
}

#if defined(CONFIG_TLK110_WORKAROUND) || \
			defined(CONFIG_TLK110_WORKAROUND_MODULE)
static int am335x_tlk110_phy_fixup(struct phy_device *phydev)
{
	unsigned int val;

	/* This is done as a workaround to support TLK110 rev1.0 phy */
	val = phy_read(phydev, TLK110_COARSEGAIN_REG);
	phy_write(phydev, TLK110_COARSEGAIN_REG, (val | TLK110_COARSEGAIN_VAL));

	val = phy_read(phydev, TLK110_LPFHPF_REG);
	phy_write(phydev, TLK110_LPFHPF_REG, (val | TLK110_LPFHPF_VAL));

	val = phy_read(phydev, TLK110_SPAREANALOG_REG);
	phy_write(phydev, TLK110_SPAREANALOG_REG, (val | TLK110_SPANALOG_VAL));

	val = phy_read(phydev, TLK110_VRCR_REG);
	phy_write(phydev, TLK110_VRCR_REG, (val | TLK110_VRCR_VAL));

	val = phy_read(phydev, TLK110_SETFFE_REG);
	phy_write(phydev, TLK110_SETFFE_REG, (val | TLK110_SETFFE_VAL));

	val = phy_read(phydev, TLK110_FTSP_REG);
	phy_write(phydev, TLK110_FTSP_REG, (val | TLK110_FTSP_VAL));

	val = phy_read(phydev, TLK110_ALFATPIDL_REG);
	phy_write(phydev, TLK110_ALFATPIDL_REG, (val | TLK110_ALFATPIDL_VAL));

	val = phy_read(phydev, TLK110_PSCOEF21_REG);
	phy_write(phydev, TLK110_PSCOEF21_REG, (val | TLK110_PSCOEF21_VAL));

	val = phy_read(phydev, TLK110_PSCOEF3_REG);
	phy_write(phydev, TLK110_PSCOEF3_REG, (val | TLK110_PSCOEF3_VAL));

	val = phy_read(phydev, TLK110_ALFAFACTOR1_REG);
	phy_write(phydev, TLK110_ALFAFACTOR1_REG, (val | TLK110_ALFACTOR1_VAL));

	val = phy_read(phydev, TLK110_ALFAFACTOR2_REG);
	phy_write(phydev, TLK110_ALFAFACTOR2_REG, (val | TLK110_ALFACTOR2_VAL));

	val = phy_read(phydev, TLK110_CFGPS_REG);
	phy_write(phydev, TLK110_CFGPS_REG, (val | TLK110_CFGPS_VAL));

	val = phy_read(phydev, TLK110_FTSPTXGAIN_REG);
	phy_write(phydev, TLK110_FTSPTXGAIN_REG, (val | TLK110_FTSPTXGAIN_VAL));

	val = phy_read(phydev, TLK110_SWSCR3_REG);
	phy_write(phydev, TLK110_SWSCR3_REG, (val | TLK110_SWSCR3_VAL));

	val = phy_read(phydev, TLK110_SCFALLBACK_REG);
	phy_write(phydev, TLK110_SCFALLBACK_REG, (val | TLK110_SCFALLBACK_VAL));

	val = phy_read(phydev, TLK110_PHYRCR_REG);
	phy_write(phydev, TLK110_PHYRCR_REG, (val | TLK110_PHYRCR_VAL));

	return 0;
}
#endif

static void profibus_init(int evm_id, int profile)
{
	setup_pin_mux(profibus_pin_mux);
	return;
}

/* Low-Cost EVM */
static struct evm_dev_cfg low_cost_evm_dev_cfg[] = {
	{rgmii1_init,	DEV_ON_BASEBOARD, PROFILE_NONE},
	{usb0_init,	DEV_ON_BASEBOARD, PROFILE_NONE},
	{usb1_init,	DEV_ON_BASEBOARD, PROFILE_NONE},
	{evm_nand_init, DEV_ON_BASEBOARD, PROFILE_NONE},
	{NULL, 0, 0},
};

/* General Purpose EVM */
static struct evm_dev_cfg gen_purp_evm_dev_cfg[] = {
	{enable_ecap0,	DEV_ON_DGHTR_BRD, (PROFILE_0 | PROFILE_1 |
						PROFILE_2 | PROFILE_7) },
	{lcdc_init,	DEV_ON_DGHTR_BRD, (PROFILE_0 | PROFILE_1 |
						PROFILE_2 | PROFILE_7) },
	{tsc_init,	DEV_ON_DGHTR_BRD, (PROFILE_0 | PROFILE_1 |
						PROFILE_2 | PROFILE_7) },
	{rgmii1_init,	DEV_ON_BASEBOARD, PROFILE_ALL},
	{rgmii2_init,	DEV_ON_DGHTR_BRD, (PROFILE_1 | PROFILE_2 |
						PROFILE_4 | PROFILE_6) },
	{usb0_init,	DEV_ON_BASEBOARD, PROFILE_ALL},
	{usb1_init,	DEV_ON_BASEBOARD, PROFILE_ALL},
	{evm_nand_init, DEV_ON_DGHTR_BRD,
		(PROFILE_ALL & ~PROFILE_2 & ~PROFILE_3)},
	{i2c1_init,     DEV_ON_DGHTR_BRD, (PROFILE_ALL & ~PROFILE_2)},
	{mcasp1_init,	DEV_ON_DGHTR_BRD, (PROFILE_0 | PROFILE_3 | PROFILE_7)},
	{mmc1_init,	DEV_ON_DGHTR_BRD, PROFILE_2},
	{mmc2_wl12xx_init,	DEV_ON_BASEBOARD, (PROFILE_0 | PROFILE_3 |
								PROFILE_5)},
	{mmc0_init,	DEV_ON_BASEBOARD, (PROFILE_ALL & ~PROFILE_5)},
	{mmc0_no_cd_init,	DEV_ON_BASEBOARD, PROFILE_5},
	{spi0_init,	DEV_ON_DGHTR_BRD, PROFILE_2},
	{uart1_wl12xx_init,	DEV_ON_BASEBOARD, (PROFILE_0 | PROFILE_3 |
								PROFILE_5)},
	{wl12xx_init,	DEV_ON_BASEBOARD, (PROFILE_0 | PROFILE_3 | PROFILE_5)},
	{d_can_init,	DEV_ON_DGHTR_BRD, PROFILE_1},
	{matrix_keypad_init, DEV_ON_DGHTR_BRD, PROFILE_0},
	{volume_keys_init,  DEV_ON_DGHTR_BRD, PROFILE_0},
	{uart2_init,	DEV_ON_DGHTR_BRD, PROFILE_3},
	{haptics_init,	DEV_ON_DGHTR_BRD, (PROFILE_4)},
	{NULL, 0, 0},
};

/* Industrial Auto Motor Control EVM */
static struct evm_dev_cfg ind_auto_mtrl_evm_dev_cfg[] = {
	{mii1_init,	DEV_ON_DGHTR_BRD, PROFILE_ALL},
	{usb0_init,	DEV_ON_BASEBOARD, PROFILE_ALL},
	{usb1_init,	DEV_ON_BASEBOARD, PROFILE_ALL},
	{profibus_init, DEV_ON_DGHTR_BRD, PROFILE_ALL},
	{evm_nand_init, DEV_ON_DGHTR_BRD, PROFILE_ALL},
	{spi1_init,	DEV_ON_DGHTR_BRD, PROFILE_ALL},
	{uart3_init,	DEV_ON_DGHTR_BRD, PROFILE_ALL},
	{i2c1_init,	DEV_ON_BASEBOARD, PROFILE_ALL},
	{mmc0_no_cd_init,	DEV_ON_BASEBOARD, PROFILE_ALL},
	{NULL, 0, 0},
};

/* IP-Phone EVM */
static struct evm_dev_cfg ip_phn_evm_dev_cfg[] = {
	{enable_ecap0,	DEV_ON_DGHTR_BRD, PROFILE_NONE},
	{lcdc_init,	DEV_ON_DGHTR_BRD, PROFILE_NONE},
	{tsc_init,	DEV_ON_DGHTR_BRD, PROFILE_NONE},
	{rgmii1_init,	DEV_ON_BASEBOARD, PROFILE_NONE},
	{rgmii2_init,	DEV_ON_DGHTR_BRD, PROFILE_NONE},
	{usb0_init,	DEV_ON_BASEBOARD, PROFILE_NONE},
	{usb1_init,	DEV_ON_BASEBOARD, PROFILE_NONE},
	{evm_nand_init, DEV_ON_DGHTR_BRD, PROFILE_NONE},
	{i2c1_init,	DEV_ON_BASEBOARD, PROFILE_NONE},
	{mcasp1_init,	DEV_ON_DGHTR_BRD, PROFILE_NONE},
	{mmc0_init,	DEV_ON_BASEBOARD, PROFILE_NONE},
	{NULL, 0, 0},
};

/* Beaglebone < Rev A3 */
static struct evm_dev_cfg beaglebone_old_dev_cfg[] = {
	{rmii1_init,	DEV_ON_BASEBOARD, PROFILE_NONE},
	{usb0_init,	DEV_ON_BASEBOARD, PROFILE_NONE},
	{usb1_init,	DEV_ON_BASEBOARD, PROFILE_NONE},
	{i2c2_init,	DEV_ON_BASEBOARD, PROFILE_NONE},
	{mmc0_init,	DEV_ON_BASEBOARD, PROFILE_NONE},
	{boneleds_init,	DEV_ON_BASEBOARD, PROFILE_ALL},
	{NULL, 0, 0},
};

/* Beaglebone Rev A3 and after */
static struct evm_dev_cfg beaglebone_dev_cfg[] = {
	{tps65217_init,	DEV_ON_BASEBOARD, PROFILE_NONE},
	{i2c2_init,	DEV_ON_BASEBOARD, PROFILE_NONE},
	{mii1_init,	DEV_ON_BASEBOARD, PROFILE_NONE},
	{usb0_init,	DEV_ON_BASEBOARD, PROFILE_NONE},
	{usb1_init,	DEV_ON_BASEBOARD, PROFILE_NONE},
	{NULL, 0, 0},
};

static void setup_low_cost_evm(void)
{
	pr_info("The board is a AM335x Low Cost EVM.\n");

	_configure_device(LOW_COST_EVM, low_cost_evm_dev_cfg, PROFILE_NONE);
}

static void setup_general_purpose_evm(void)
{
	u32 prof_sel = am335x_get_profile_selection();
	pr_info("The board is general purpose EVM in profile %d\n", prof_sel);

	if (!strncmp("1.1A", config.version, 4)) {
		pr_info("EVM version is %s\n", config.version);
		gp_evm_revision = GP_EVM_REV_IS_1_1A;
	} else if (!strncmp("1.0", config.version, 3)) {
		gp_evm_revision = GP_EVM_REV_IS_1_0;
	} else {
		pr_err("Found invalid GP EVM revision, falling back to Rev1.1A");
		gp_evm_revision = GP_EVM_REV_IS_1_1A;
	}

	if (gp_evm_revision == GP_EVM_REV_IS_1_0)
		gigabit_enable = 0;
	else if (gp_evm_revision == GP_EVM_REV_IS_1_1A)
		gigabit_enable = 1;

	_configure_device(GEN_PURP_EVM, gen_purp_evm_dev_cfg, (1L << prof_sel));
}

static void setup_ind_auto_motor_ctrl_evm(void)
{
	u32 prof_sel = am335x_get_profile_selection();

	pr_info("The board is an industrial automation EVM in profile %d\n",
		prof_sel);

	/* Only Profile 0 is supported */
	if ((1L << prof_sel) != PROFILE_0) {
		pr_err("AM335X: Only Profile 0 is supported\n");
		pr_err("Assuming profile 0 & continuing\n");
		prof_sel = PROFILE_0;
	}

	_configure_device(IND_AUT_MTR_EVM, ind_auto_mtrl_evm_dev_cfg,
		PROFILE_0);

	/* Fillup global evmid */
	am33xx_evmid_fillup(IND_AUT_MTR_EVM);

	/* Initialize TLK110 PHY registers for phy version 1.0 */
	am335x_tlk110_phy_init();


}

static void setup_ip_phone_evm(void)
{
	pr_info("The board is an IP phone EVM\n");

	_configure_device(IP_PHN_EVM, ip_phn_evm_dev_cfg, PROFILE_NONE);
}

/* BeagleBone < Rev A3 */
static void setup_beaglebone_old(void)
{
	pr_info("The board is a AM335x Beaglebone < Rev A3.\n");

	/* Beagle Bone has Micro-SD slot which doesn't have Write Protect pin */
	am335x_mmc[0].gpio_wp = -EINVAL;

	_configure_device(LOW_COST_EVM, beaglebone_old_dev_cfg, PROFILE_NONE);

	phy_register_fixup_for_uid(BBB_PHY_ID, BBB_PHY_MASK,
					beaglebone_phy_fixup);

	/* Fill up global evmid */
	am33xx_evmid_fillup(BEAGLE_BONE_OLD);
}

/* BeagleBone after Rev A3 */
static void setup_beaglebone(void)
{
	pr_info("The board is a AM335x Beaglebone.\n");
	gp_evm_revision = GP_EVM_ACTUALLY_BEAGLEBONE;

	/* Beagle Bone has Micro-SD slot which doesn't have Write Protect pin */
	am335x_mmc[0].gpio_wp = -EINVAL;

	_configure_device(LOW_COST_EVM, beaglebone_dev_cfg, PROFILE_NONE);

	/* TPS65217 regulator has full constraints */
	regulator_has_full_constraints();

	/* Fill up global evmid */
	am33xx_evmid_fillup(BEAGLE_BONE_A3);

	/*TODO:We are registering all pwms for the beaglebone here
	 *this may effect power management in the future
	 */
	register_all_pwms();
}


static void am335x_setup_daughter_board(struct memory_accessor *m, void *c)
{
	int ret;

	/*
	 * Read from the EEPROM to see the presence of daughter board.
	 * If present, print the cpld version.
	 */

	ret = m->read(m, (char *)&config1, 0, sizeof(config1));
	if (ret == sizeof(config1)) {
		pr_info("Detected a daughter card on AM335x EVM..");
		daughter_brd_detected = true;
	}
	 else {
		pr_info("No daughter card found\n");
		daughter_brd_detected = false;
		return;
	}

	if (!strncmp("CPLD", config1.cpld_ver, 4))
		pr_info("CPLD version: %s\n", config1.cpld_ver);
	else
		pr_err("Unknown CPLD version found\n");
}

static void am335x_evm_setup(struct memory_accessor *mem_acc, void *context)
{
	int ret;
	char tmp[10];

	/* 1st get the MAC address from EEPROM */
	ret = mem_acc->read(mem_acc, (char *)&am335x_mac_addr,
		EEPROM_MAC_ADDRESS_OFFSET, sizeof(am335x_mac_addr));

	if (ret != sizeof(am335x_mac_addr)) {
		pr_warning("AM335X: EVM Config read fail: %d\n", ret);
		return;
	}

	/* Fillup global mac id */
	am33xx_cpsw_macidfillup(&am335x_mac_addr[0][0],
				&am335x_mac_addr[1][0]);

	/* get board specific data */
	ret = mem_acc->read(mem_acc, (char *)&config, 0, sizeof(config));
	if (ret != sizeof(config)) {
		pr_err("AM335X EVM config read fail, read %d bytes\n", ret);
		pr_err("This likely means that there either is no/or a failed EEPROM\n");
		goto out;
	}

	if (config.header != AM335X_EEPROM_HEADER) {
		pr_err("AM335X: wrong header 0x%x, expected 0x%x\n",
			config.header, AM335X_EEPROM_HEADER);
		goto out;
	}

	if (strncmp("A335", config.name, 4)) {
		pr_err("Board %s\ndoesn't look like an AM335x board\n",
			config.name);
		goto out;
	}

	snprintf(tmp, sizeof(config.name) + 1, "%s", config.name);
	pr_info("Board name: %s\n", tmp);
	snprintf(tmp, sizeof(config.version) + 1, "%s", config.version);
	pr_info("Board version: %s\n", tmp);

	if (!strncmp("A335BONE", config.name, 8)) {
		daughter_brd_detected = false;
		if(!strncmp("00A1", config.version, 4) ||
		   !strncmp("00A2", config.version, 4))
			setup_beaglebone_old();
		else
			setup_beaglebone();
	} else {
		/* only 6 characters of options string used for now */
		snprintf(tmp, 7, "%s", config.opt);
		pr_info("SKU: %s\n", tmp);

		if (!strncmp("SKU#00", config.opt, 6))
			setup_low_cost_evm();
		else if (!strncmp("SKU#01", config.opt, 6))
			setup_general_purpose_evm();
		else if (!strncmp("SKU#02", config.opt, 6))
			setup_ind_auto_motor_ctrl_evm();
		else if (!strncmp("SKU#03", config.opt, 6))
			setup_ip_phone_evm();
		else
			goto out;
	}
	/* Initialize cpsw after board detection is completed as board
	 * information is required for configuring phy address and hence
	 * should be call only after board detection
	 */
	am33xx_cpsw_init(gigabit_enable);

	return;

out:
	/*
	 * If the EEPROM hasn't been programed or an incorrect header
	 * or board name are read then the hardware details are unknown.
	 * Notify the user and call machine_halt to stop the boot process.
	 */
	pr_err("The error message above indicates that there is an issue with\n"
		   "the EEPROM or the EEPROM contents.  After verifying the EEPROM\n"
		   "contents, if any, refer to the %s function in the\n"
		   "%s file to modify the board\n"
		   "initialization code to match the hardware configuration\n",
		   __func__ , __FILE__);
	machine_halt();
}

static struct at24_platform_data am335x_daughter_board_eeprom_info = {
	.byte_len       = (256*1024) / 8,
	.page_size      = 64,
	.flags          = AT24_FLAG_ADDR16,
	.setup          = am335x_setup_daughter_board,
	.context        = (void *)NULL,
};

static struct at24_platform_data am335x_baseboard_eeprom_info = {
	.byte_len       = (256*1024) / 8,
	.page_size      = 64,
	.flags          = AT24_FLAG_ADDR16,
	.setup          = am335x_evm_setup,
	.context        = (void *)NULL,
};

static struct regulator_init_data am335x_dummy = {
	.constraints.always_on	= true,
};

static struct regulator_consumer_supply am335x_vdd1_supply[] = {
	REGULATOR_SUPPLY("vdd_mpu", NULL),
};

static struct regulator_init_data am335x_vdd1 = {
	.constraints = {
		.min_uV			= 600000,
		.max_uV			= 1500000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE,
		.always_on		= 1,
	},
	.num_consumer_supplies	= ARRAY_SIZE(am335x_vdd1_supply),
	.consumer_supplies	= am335x_vdd1_supply,
};

static struct regulator_consumer_supply am335x_vdd2_supply[] = {
	REGULATOR_SUPPLY("vdd_core", NULL),
};

static struct regulator_init_data am335x_vdd2 = {
	.constraints = {
		.min_uV			= 600000,
		.max_uV			= 1500000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE,
		.always_on		= 1,
	},
	.num_consumer_supplies	= ARRAY_SIZE(am335x_vdd2_supply),
	.consumer_supplies	= am335x_vdd2_supply,
};

static struct tps65910_board am335x_tps65910_info = {
	.tps65910_pmic_init_data[TPS65910_REG_VRTC]	= &am335x_dummy,
	.tps65910_pmic_init_data[TPS65910_REG_VIO]	= &am335x_dummy,
	.tps65910_pmic_init_data[TPS65910_REG_VDD1]	= &am335x_vdd1,
	.tps65910_pmic_init_data[TPS65910_REG_VDD2]	= &am335x_vdd2,
	.tps65910_pmic_init_data[TPS65910_REG_VDD3]	= &am335x_dummy,
	.tps65910_pmic_init_data[TPS65910_REG_VDIG1]	= &am335x_dummy,
	.tps65910_pmic_init_data[TPS65910_REG_VDIG2]	= &am335x_dummy,
	.tps65910_pmic_init_data[TPS65910_REG_VPLL]	= &am335x_dummy,
	.tps65910_pmic_init_data[TPS65910_REG_VDAC]	= &am335x_dummy,
	.tps65910_pmic_init_data[TPS65910_REG_VAUX1]	= &am335x_dummy,
	.tps65910_pmic_init_data[TPS65910_REG_VAUX2]	= &am335x_dummy,
	.tps65910_pmic_init_data[TPS65910_REG_VAUX33]	= &am335x_dummy,
	.tps65910_pmic_init_data[TPS65910_REG_VMMC]	= &am335x_dummy,
};

/*
* Daughter board Detection.
* Every board has a ID memory (EEPROM) on board. We probe these devices at
* machine init, starting from daughter board and ending with baseboard.
* Assumptions :
*	1. probe for i2c devices are called in the order they are included in
*	   the below struct. Daughter boards eeprom are probed 1st. Baseboard
*	   eeprom probe is called last.
*/
static struct i2c_board_info __initdata am335x_i2c_boardinfo[] = {
	{
		/* Daughter Board EEPROM */
		I2C_BOARD_INFO("24c256", DAUG_BOARD_I2C_ADDR),
		.platform_data  = &am335x_daughter_board_eeprom_info,
	},
	{
		/* Baseboard board EEPROM */
		I2C_BOARD_INFO("24c256", BASEBOARD_I2C_ADDR),
		.platform_data  = &am335x_baseboard_eeprom_info,
	},
	{
		I2C_BOARD_INFO("cpld_reg", 0x35),
	},
	{
		I2C_BOARD_INFO("tlc59108", 0x40),
	},
	{
		I2C_BOARD_INFO("tps65910", TPS65910_I2C_ID1),
		.platform_data  = &am335x_tps65910_info,
	},
};

static struct omap_musb_board_data musb_board_data = {
	.interface_type	= MUSB_INTERFACE_ULPI,
	/*
	 * mode[0:3] = USB0PORT's mode
	 * mode[4:7] = USB1PORT's mode
	 * AM335X beta EVM has USB0 in OTG mode and USB1 in host mode.
	 */
	.mode           = (MUSB_HOST << 4) | MUSB_OTG,
	.power		= 500,
	.instances	= 1,
};

static int cpld_reg_probe(struct i2c_client *client,
	    const struct i2c_device_id *id)
{
	cpld_client = client;
	return 0;
}

static int __devexit cpld_reg_remove(struct i2c_client *client)
{
	cpld_client = NULL;
	return 0;
}

static const struct i2c_device_id cpld_reg_id[] = {
	{ "cpld_reg", 0 },
	{ }
};

static struct i2c_driver cpld_reg_driver = {
	.driver = {
		.name	= "cpld_reg",
	},
	.probe		= cpld_reg_probe,
	.remove		= cpld_reg_remove,
	.id_table	= cpld_reg_id,
};

static void evm_init_cpld(void)
{
	i2c_add_driver(&cpld_reg_driver);
}

static void __init am335x_evm_i2c_init(void)
{
	/* Initially assume Low Cost EVM Config */
	am335x_evm_id = LOW_COST_EVM;

	evm_init_cpld();

	omap_register_i2c_bus(1, 100, am335x_i2c_boardinfo,
				ARRAY_SIZE(am335x_i2c_boardinfo));
}

static struct resource am335x_rtc_resources[] = {
	{
		.start		= AM33XX_RTC_BASE,
		.end		= AM33XX_RTC_BASE + SZ_4K - 1,
		.flags		= IORESOURCE_MEM,
	},
	{ /* timer irq */
		.start		= AM33XX_IRQ_RTC_TIMER,
		.end		= AM33XX_IRQ_RTC_TIMER,
		.flags		= IORESOURCE_IRQ,
	},
	{ /* alarm irq */
		.start		= AM33XX_IRQ_RTC_ALARM,
		.end		= AM33XX_IRQ_RTC_ALARM,
		.flags		= IORESOURCE_IRQ,
	},
};

static struct platform_device am335x_rtc_device = {
	.name           = "omap_rtc",
	.id             = -1,
	.num_resources	= ARRAY_SIZE(am335x_rtc_resources),
	.resource	= am335x_rtc_resources,
};

static int am335x_rtc_init(void)
{
	void __iomem *base;
	struct clk *clk;

	clk = clk_get(NULL, "rtc_fck");
	if (IS_ERR(clk)) {
		pr_err("rtc : Failed to get RTC clock\n");
		return -1;
	}

	if (clk_enable(clk)) {
		pr_err("rtc: Clock Enable Failed\n");
		return -1;
	}

	base = ioremap(AM33XX_RTC_BASE, SZ_4K);

	if (WARN_ON(!base))
		return -ENOMEM;

	/* Unlock the rtc's registers */
	writel(0x83e70b13, base + 0x6c);
	writel(0x95a4f1e0, base + 0x70);

	/*
	 * Enable the 32K OSc
	 * TODO: Need a better way to handle this
	 * Since we want the clock to be running before mmc init
	 * we need to do it before the rtc probe happens
	 */
	writel(0x48, base + 0x54);

	iounmap(base);

	return  platform_device_register(&am335x_rtc_device);
}

/* Enable clkout2 */
static struct pinmux_config clkout2_pin_mux[] = {
	{"xdma_event_intr1.clkout2", OMAP_MUX_MODE3 | AM33XX_PIN_OUTPUT},
	{NULL, 0},
};

static void __init clkout2_enable(void)
{
	void __iomem *base;
	unsigned int val;

	base = ioremap(0x44E00700, SZ_4K);
	val = (5 << 3) | (3 << 0); //32 MHz
	writel(val, base);
	iounmap(base);

	setup_pin_mux(clkout2_pin_mux);
}

void __iomem *am33xx_emif_base;

void __iomem * __init am33xx_get_mem_ctlr(void)
{

	am33xx_emif_base = ioremap(AM33XX_EMIF0_BASE, SZ_32K);

	if (!am33xx_emif_base)
		pr_warning("%s: Unable to map DDR2 controller",	__func__);

	return am33xx_emif_base;
}

void __iomem *am33xx_get_ram_base(void)
{
	return am33xx_emif_base;
}

static struct resource am33xx_cpuidle_resources[] = {
	{
		.start		= AM33XX_EMIF0_BASE,
		.end		= AM33XX_EMIF0_BASE + SZ_32K - 1,
		.flags		= IORESOURCE_MEM,
	},
};

/* AM33XX devices support DDR2 power down */
static struct am33xx_cpuidle_config am33xx_cpuidle_pdata = {
	.ddr2_pdown	= 1,
};

static struct platform_device am33xx_cpuidle_device = {
	.name			= "cpuidle-am33xx",
	.num_resources		= ARRAY_SIZE(am33xx_cpuidle_resources),
	.resource		= am33xx_cpuidle_resources,
	.dev = {
		.platform_data	= &am33xx_cpuidle_pdata,
	},
};

static void __init am33xx_cpuidle_init(void)
{
	int ret;

	am33xx_cpuidle_pdata.emif_base = am33xx_get_mem_ctlr();

	ret = platform_device_register(&am33xx_cpuidle_device);

	if (ret)
		pr_warning("AM33XX cpuidle registration failed\n");

}

static void __init am335x_evm_init(void)
{
	am33xx_cpuidle_init();
	am33xx_mux_init(board_mux);
	omap_serial_init();
	am335x_rtc_init();
	clkout2_enable();
	am335x_evm_i2c_init();
	omap_sdrc_init(NULL, NULL);
	usb_musb_init(&musb_board_data);
	omap_board_config = am335x_evm_config;
	omap_board_config_size = ARRAY_SIZE(am335x_evm_config);
	/* Create an alias for icss clock */
	if (clk_add_alias("pruss", NULL, "pruss_uart_gclk", NULL))
		pr_warn("failed to create an alias: icss_uart_gclk --> pruss\n");
	/* Create an alias for gfx/sgx clock */
	if (clk_add_alias("sgx_ck", NULL, "gfx_fclk", NULL))
		pr_warn("failed to create an alias: gfx_fclk --> sgx_ck\n");
}

static void __init am335x_evm_map_io(void)
{
	omap2_set_globals_am33xx();
	omapam33xx_map_common_io();
}

MACHINE_START(AM335XEVM, "am335xevm")
	/* Maintainer: Texas Instruments */
	.atag_offset	= 0x100,
	.map_io		= am335x_evm_map_io,
	.init_early	= am33xx_init_early,
	.init_irq	= ti81xx_init_irq,
	.handle_irq     = omap3_intc_handle_irq,
	.timer		= &omap3_am33xx_timer,
	.init_machine	= am335x_evm_init,
MACHINE_END

MACHINE_START(AM335XIAEVM, "am335xiaevm")
	/* Maintainer: Texas Instruments */
	.atag_offset	= 0x100,
	.map_io		= am335x_evm_map_io,
	.init_irq	= ti81xx_init_irq,
	.init_early	= am33xx_init_early,
	.timer		= &omap3_am33xx_timer,
	.init_machine	= am335x_evm_init,
MACHINE_END
