/* Copyright (c) 2014-2015 The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, nit
 * PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <debug.h>
#include <platform/iomap.h>
#include <platform/irqs.h>
#include <platform/gpio.h>
#include <reg.h>
#include <target.h>
#include <platform.h>
#include <dload_util.h>
#include <uart_dm.h>
#include <mmc.h>
#include <spmi.h>
#include <board.h>
#include <smem.h>
#include <baseband.h>
#include <regulator.h>
#include <dev/keys.h>
#include <pm8x41.h>
#include <crypto5_wrapper.h>
#include <clock.h>
#include <partition_parser.h>
#include <scm.h>
#include <platform/clock.h>
#include <platform/gpio.h>
#include <platform/timer.h>
#include <stdlib.h>
#include <ufs.h>
#include <boot_device.h>
#include <qmp_phy.h>
#include <sdhci_msm.h>
#include <qusb2_phy.h>
#include <secapp_loader.h>
#include <rpmb.h>
#include <rpm-glink.h>
#if ENABLE_WBC
#include <pm_app_smbchg.h>
#endif

#if LONG_PRESS_POWER_ON
#include <shutdown_detect.h>
#endif

#if PON_VIB_SUPPORT
#include <vibrator.h>
#define VIBRATE_TIME 250
#endif

#define CE_INSTANCE             1
#define CE_EE                   0
#define CE_FIFO_SIZE            64
#define CE_READ_PIPE            3
#define CE_WRITE_PIPE           2
#define CE_READ_PIPE_LOCK_GRP   0
#define CE_WRITE_PIPE_LOCK_GRP  0
#define CE_ARRAY_SIZE           20

#define PMIC_ARB_CHANNEL_NUM    0
#define PMIC_ARB_OWNER_ID       0

static void set_sdc_power_ctrl(void);
static uint32_t mmc_pwrctl_base[] =
	{ MSM_SDC1_BASE, MSM_SDC2_BASE };

static uint32_t mmc_sdhci_base[] =
	{ MSM_SDC1_SDHCI_BASE, MSM_SDC2_SDHCI_BASE };

static uint32_t  mmc_sdc_pwrctl_irq[] =
	{ SDCC1_PWRCTL_IRQ, SDCC2_PWRCTL_IRQ };

struct mmc_device *dev;
struct ufs_dev ufs_device;

void target_early_init(void)
{
#if WITH_DEBUG_UART
	uart_dm_init(8, 0, BLSP2_UART1_BASE);
#endif
}

/* Return 1 if vol_up pressed */
int target_volume_up()
{
	static uint8_t first_time = 0;
	uint8_t status = 0;
	struct pm8x41_gpio gpio;

	if (!first_time) {
		/* Configure the GPIO */
		gpio.direction = PM_GPIO_DIR_IN;
		gpio.function  = 0;
		gpio.pull      = PM_GPIO_PULL_UP_30;
		gpio.vin_sel   = 2;

		pm8x41_gpio_config(2, &gpio);

		/* Wait for the pmic gpio config to take effect */
		udelay(10000);

		first_time = 1;
	}

	/* Get status of P_GPIO_5 */
	pm8x41_gpio_get(2, &status);

	return !status; /* active low */
}

/* Return 1 if vol_down pressed */
uint32_t target_volume_down()
{
	return pm8x41_resin_status();
}

static void target_keystatus()
{
	keys_init();

	if(target_volume_down())
		keys_post_event(KEY_VOLUMEDOWN, 1);

	if(target_volume_up())
		keys_post_event(KEY_VOLUMEUP, 1);
}

void target_uninit(void)
{
	if (platform_boot_dev_isemmc())
	{
		mmc_put_card_to_sleep(dev);
	}

	if (is_sec_app_loaded())
	{
		if (send_milestone_call_to_tz() < 0)
		{
			dprintf(CRITICAL, "Failed to unload App for rpmb\n");
			ASSERT(0);
		}
	}

#if ENABLE_WBC
	if (board_hardware_id() == HW_PLATFORM_MTP)
		pm_appsbl_set_dcin_suspend(1);
#endif


	if (crypto_initialized())
	{
		crypto_eng_cleanup();
		clock_ce_disable(CE_INSTANCE);
	}

	/* Tear down glink channels */
	rpm_glink_uninit();

	if (rpmb_uninit() < 0)
	{
		dprintf(CRITICAL, "RPMB uninit failed\n");
		ASSERT(0);
	}

}

static void set_sdc_power_ctrl()
{
	/* Drive strength configs for sdc pins */
	struct tlmm_cfgs sdc1_hdrv_cfg[] =
	{
		{ SDC1_CLK_HDRV_CTL_OFF,  TLMM_CUR_VAL_16MA, TLMM_HDRV_MASK, SDC1_HDRV_PULL_CTL },
		{ SDC1_CMD_HDRV_CTL_OFF,  TLMM_CUR_VAL_10MA, TLMM_HDRV_MASK, SDC1_HDRV_PULL_CTL },
		{ SDC1_DATA_HDRV_CTL_OFF, TLMM_CUR_VAL_10MA, TLMM_HDRV_MASK, SDC1_HDRV_PULL_CTL },
	};

	/* Pull configs for sdc pins */
	struct tlmm_cfgs sdc1_pull_cfg[] =
	{
		{ SDC1_CLK_PULL_CTL_OFF,  TLMM_NO_PULL, TLMM_PULL_MASK, SDC1_HDRV_PULL_CTL },
		{ SDC1_CMD_PULL_CTL_OFF,  TLMM_PULL_UP, TLMM_PULL_MASK, SDC1_HDRV_PULL_CTL },
		{ SDC1_DATA_PULL_CTL_OFF, TLMM_PULL_UP, TLMM_PULL_MASK, SDC1_HDRV_PULL_CTL },
	};

	struct tlmm_cfgs sdc1_rclk_cfg[] =
	{
		{ SDC1_RCLK_PULL_CTL_OFF, TLMM_PULL_DOWN, TLMM_PULL_MASK, SDC1_HDRV_PULL_CTL },
	};

	/* Set the drive strength & pull control values */
	tlmm_set_hdrive_ctrl(sdc1_hdrv_cfg, ARRAY_SIZE(sdc1_hdrv_cfg));
	tlmm_set_pull_ctrl(sdc1_pull_cfg, ARRAY_SIZE(sdc1_pull_cfg));
	tlmm_set_pull_ctrl(sdc1_rclk_cfg, ARRAY_SIZE(sdc1_rclk_cfg));
}

uint32_t target_is_pwrkey_pon_reason()
{
	uint8_t pon_reason = pm8950_get_pon_reason();
	if (pm8x41_get_is_cold_boot() && ((pon_reason == KPDPWR_N) || (pon_reason == (KPDPWR_N|PON1))))
		return 1;
	else
		return 0;
}


void target_sdc_init()
{
	struct mmc_config_data config = {0};

	/* Set drive strength & pull ctrl values */
	set_sdc_power_ctrl();

	config.bus_width = DATA_BUS_WIDTH_8BIT;
	config.max_clk_rate = MMC_CLK_192MHZ;
	config.hs400_support = 1;

	/* Try slot 1*/
	config.slot = 1;
	config.sdhc_base = mmc_sdhci_base[config.slot - 1];
	config.pwrctl_base = mmc_pwrctl_base[config.slot - 1];
	config.pwr_irq     = mmc_sdc_pwrctl_irq[config.slot - 1];

	if (!(dev = mmc_init(&config)))
	{
		/* Try slot 2 */
		config.slot = 2;
		config.max_clk_rate = MMC_CLK_200MHZ;
		config.sdhc_base = mmc_sdhci_base[config.slot - 1];
		config.pwrctl_base = mmc_pwrctl_base[config.slot - 1];
		config.pwr_irq     = mmc_sdc_pwrctl_irq[config.slot - 1];

		if (!(dev = mmc_init(&config)))
		{
			dprintf(CRITICAL, "mmc init failed!");
			ASSERT(0);
		}
	}
}

void *target_mmc_device()
{
	if (platform_boot_dev_isemmc())
		return (void *) dev;
	else
		return (void *) &ufs_device;
}

void target_init(void)
{
	int ret = 0;
	dprintf(INFO, "target_init()\n");

	pmic_info_populate();

	spmi_init(PMIC_ARB_CHANNEL_NUM, PMIC_ARB_OWNER_ID);

	/* Initialize Glink */
	rpm_glink_init();

	target_keystatus();

#if defined(LONG_PRESS_POWER_ON) || defined(PON_VIB_SUPPORT)
	switch(board_hardware_id())
	{
		case HW_PLATFORM_QRD:
#if LONG_PRESS_POWER_ON
			shutdown_detect();
#endif
#if PON_VIB_SUPPORT
			vib_timed_turn_on(VIBRATE_TIME);
#endif
			break;
	}
#endif

	if (target_use_signed_kernel())
		target_crypto_init_params();

	platform_read_boot_config();

#ifdef MMC_SDHCI_SUPPORT
	if (platform_boot_dev_isemmc())
	{
		target_sdc_init();
	}
#endif
#ifdef UFS_SUPPORT
	if (!platform_boot_dev_isemmc())
	{
		ufs_device.base = UFS_BASE;
		ufs_init(&ufs_device);
	}
#endif

	/* Storage initialization is complete, read the partition table info */
	mmc_read_partition_table(0);

#if ENABLE_WBC
	/* Look for battery voltage and make sure we have enough to bootup
	 * Otherwise initiate battery charging
	 * Charging should happen as early as possible, any other driver
	 * initialization before this should consider the power impact
	 */
	switch(board_hardware_id())
	{
		case HW_PLATFORM_MTP:
		case HW_PLATFORM_FLUID:
		case HW_PLATFORM_QRD:
			pm_appsbl_chg_check_weak_battery_status(1);
			break;
		default:
			/* Charging not supported */
			break;
	};
#endif

	/* Initialize Qseecom */
	ret = qseecom_init();

	if (ret < 0)
	{
		dprintf(CRITICAL, "Failed to initialize qseecom, error: %d\n", ret);
		ASSERT(0);
	}

	/* Start Qseecom */
	ret = qseecom_tz_init();

	if (ret < 0)
	{
		dprintf(CRITICAL, "Failed to start qseecom, error: %d\n", ret);
		ASSERT(0);
	}

	if (rpmb_init() < 0)
	{
		dprintf(CRITICAL, "RPMB init failed\n");
		ASSERT(0);
	}

	/*
	 * Load the sec app for first time
	 */
	if (load_sec_app() < 0)
	{
		dprintf(CRITICAL, "Failed to load App for verified\n");
		ASSERT(0);
	}
}

unsigned board_machtype(void)
{
	return LINUX_MACHTYPE_UNKNOWN;
}

/* Detect the target type */
void target_detect(struct board_data *board)
{
	/* This is filled from board.c */
}

static uint8_t splash_override;
/* Returns 1 if target supports continuous splash screen. */
int target_cont_splash_screen()
{
	uint8_t splash_screen = 0;
	if(!splash_override && !pm_appsbl_charging_in_progress()) {
		switch(board_hardware_id())
		{
			case HW_PLATFORM_SURF:
			case HW_PLATFORM_MTP:
			case HW_PLATFORM_FLUID:
			case HW_PLATFORM_QRD:
			case HW_PLATFORM_LIQUID:
				dprintf(SPEW, "Target_cont_splash=1\n");
				splash_screen = 1;
				break;
			default:
				dprintf(SPEW, "Target_cont_splash=0\n");
				splash_screen = 0;
		}
	}
	return splash_screen;
}

void target_force_cont_splash_disable(uint8_t override)
{
        splash_override = override;
}

/* Detect the modem type */
void target_baseband_detect(struct board_data *board)
{
	uint32_t platform;

	platform = board->platform;

	switch(platform) {
	case APQ8096:
		board->baseband = BASEBAND_APQ;
		break;
	case MSM8996:
		if (board->platform_version == 0x10000)
			board->baseband = BASEBAND_APQ;
		else
			board->baseband = BASEBAND_MSM;
		break;
	default:
		dprintf(CRITICAL, "Platform type: %u is not supported\n",platform);
		ASSERT(0);
	};
}
unsigned target_baseband()
{
	return board_baseband();
}

void target_serialno(unsigned char *buf)
{
	unsigned int serialno;
	if (target_is_emmc_boot()) {
		serialno = mmc_get_psn();
		snprintf((char *)buf, 13, "%x", serialno);
	}
}

int emmc_recovery_init(void)
{
	return _emmc_recovery_init();
}

void target_usb_phy_reset()
{
	usb30_qmp_phy_reset();
	qusb2_phy_reset();
}

target_usb_iface_t* target_usb30_init()
{
	target_usb_iface_t *t_usb_iface;

	t_usb_iface = calloc(1, sizeof(target_usb_iface_t));
	ASSERT(t_usb_iface);

	t_usb_iface->phy_init   = usb30_qmp_phy_init;
	t_usb_iface->phy_reset  = target_usb_phy_reset;
	t_usb_iface->clock_init = clock_usb30_init;
	t_usb_iface->vbus_override = 1;

	return t_usb_iface;
}

/* identify the usb controller to be used for the target */
const char * target_usb_controller()
{
	return "dwc";
}

uint32_t target_override_pll()
{
	if (board_soc_version() >= 0x20000)
		return 0;
	else
		return 1;
}

crypto_engine_type board_ce_type(void)
{
	return CRYPTO_ENGINE_TYPE_HW;
}

/* Set up params for h/w CE. */
void target_crypto_init_params()
{
	struct crypto_init_params ce_params;

	/* Set up base addresses and instance. */
	ce_params.crypto_instance  = CE_INSTANCE;
	ce_params.crypto_base      = MSM_CE_BASE;
	ce_params.bam_base         = MSM_CE_BAM_BASE;

	/* Set up BAM config. */
	ce_params.bam_ee               = CE_EE;
	ce_params.pipes.read_pipe      = CE_READ_PIPE;
	ce_params.pipes.write_pipe     = CE_WRITE_PIPE;
	ce_params.pipes.read_pipe_grp  = CE_READ_PIPE_LOCK_GRP;
	ce_params.pipes.write_pipe_grp = CE_WRITE_PIPE_LOCK_GRP;

	/* Assign buffer sizes. */
	ce_params.num_ce           = CE_ARRAY_SIZE;
	ce_params.read_fifo_size   = CE_FIFO_SIZE;
	ce_params.write_fifo_size  = CE_FIFO_SIZE;

	/* BAM is initialized by TZ for this platform.
	 * Do not do it again as the initialization address space
	 * is locked.
	 */
	ce_params.do_bam_init      = 0;

	crypto_init_params(&ce_params);
}

unsigned target_pause_for_battery_charge(void)
{
	uint8_t pon_reason = pm8x41_get_pon_reason();
	uint8_t is_cold_boot = pm8x41_get_is_cold_boot();
	dprintf(INFO, "%s : pon_reason is %d cold_boot:%d\n", __func__,
		pon_reason, is_cold_boot);
	/* In case of fastboot reboot,adb reboot or if we see the power key
	* pressed we do not want go into charger mode.
	* fastboot reboot is warm boot with PON hard reset bit not set
	* adb reboot is a cold boot with PON hard reset bit set
	*/
	if (is_cold_boot &&
			(!(pon_reason & HARD_RST)) &&
			(!(pon_reason & KPDPWR_N)) &&
			((pon_reason & PON1)))
		return 1;
	else
		return 0;
}

int set_download_mode(enum dload_mode mode)
{
	int ret = 0;
	ret = scm_dload_mode(mode);

	return ret;
}

void pmic_reset_configure(uint8_t reset_type)
{
	pm8994_reset_configure(reset_type);
}

uint32_t target_get_pmic()
{
	return PMIC_IS_PMI8996;
}
