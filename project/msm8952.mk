# top level project rules for the MSM8952 project
#
LOCAL_DIR := $(GET_LOCAL_DIR)

TARGET := msm8952

MODULES += app/aboot


#ifeq ($(TARGET_BUILD_VARIANT),user)
#DEBUG := 0
#else
#DEBUG := 1
#endif

EMMC_BOOT := 1


ifeq ($(VERIFIED_BOOT),1)
ifeq ($(ENABLE_VBOOT_MOTA_SUPPORT),1)
DEFINES += VBOOT_MOTA=1
else
ENABLE_SECAPP_LOADER := 1
ENABLE_RPMB_SUPPORT := 1
#enable fbcon display menu
ENABLE_FBCON_DISPLAY_MSG := 1
endif
endif



ENABLE_SMD_SUPPORT := 1
#ENABLE_PWM_SUPPORT := true

#DEFINES += WITH_DEBUG_DCC=1
DEFINES += WITH_DEBUG_LOG_BUF=1
DEFINES += WITH_DEBUG_UART=1
#DEFINES += WITH_DEBUG_FBCON=1
DEFINES += DEVICE_TREE=1
#DEFINES += MMC_BOOT_BAM=1
DEFINES += CRYPTO_BAM=1
DEFINES += SPMI_CORE_V2=1
DEFINES += ABOOT_IGNORE_BOOT_HEADER_ADDRS=1
DEFINES += TARGET_MAX_WLED_STRINGS=2

DEFINES += BAM_V170=1

#Enable the feature of long press power on
DEFINES += LONG_PRESS_POWER_ON=1

ifeq ($(ENABLE_RPMB_SUPPORT),1)
DEFINES += USE_RPMB_FOR_DEVINFO=1
endif

#Disable thumb mode
ENABLE_THUMB := false

ENABLE_SDHCI_SUPPORT := 1

ifeq ($(ENABLE_SDHCI_SUPPORT),1)
DEFINES += MMC_SDHCI_SUPPORT=1
endif



ifeq ($(ENABLE_FBCON_DISPLAY_MSG),1)
DEFINES += FBCON_DISPLAY_MSG=1
endif

#enable power on vibrator feature
ENABLE_HAP_VIB_SUPPORT := true

ifeq ($(EMMC_BOOT),1)
DEFINES += _EMMC_BOOT=1
endif

ifeq ($(ENABLE_PON_VIB_SUPPORT),true)
DEFINES += PON_VIB_SUPPORT=1
endif

ifeq ($(ENABLE_HAP_VIB_SUPPORT),true)
DEFINES += PON_VIB_SUPPORT=1
endif

ifeq ($(ENABLE_SMD_SUPPORT),1)
DEFINES += SMD_SUPPORT=1
endif

ifeq ($(ENABLE_MDTP_SUPPORT),1)
DEFINES += MDTP_SUPPORT=1
endif

#Disable the dload mode
DEFINES += DISABLE_DLOAD_MODE=0

ENABLE_WDOG_SUPPORT := 0
ifeq ($(ENABLE_WDOG_SUPPORT),1)
DEFINES += WDOG_SUPPORT=1
endif

#SCM call before entering DLOAD mode
DEFINES += PLATFORM_USE_SCM_DLOAD=1

#enable user force reset feature
DEFINES += USER_FORCE_RESET_SUPPORT=1

DEFINES += USE_TARGET_HS200_DELAY=1

#enable battery voltage check
DEFINES += CHECK_BAT_VOLTAGE=1

ENABLE_SECONDARY_CPU_SUPPORT := 0
ifeq ($(ENABLE_SECONDARY_CPU_SUPPORT),0)
DEFINES += SECONDARY_CPU_SUPPORT=0
else
DEFINES += SECONDARY_CPU_SUPPORT=1
ENABLE_PSCI_SUPPORT := 1
endif

ENABLE_PSCI_SUPPORT := 0
ifeq ($(ENABLE_PSCI_SUPPORT),0)
DEFINES += PSCI_SUPPORT=0
else
DEFINES += PSCI_SUPPORT=1
endif

ENABLE_EARLY_CAMERA_SUPPORT := 0
ifeq ($(ENABLE_EARLY_CAMERA_SUPPORT),0)
DEFINES += EARLY_CAMERA_SUPPORT=0
else
DEFINES += EARLY_CAMERA_SUPPORT=1
endif

#enable battery voltage check
DEFINES += CHECK_BAT_VOLTAGE=1
