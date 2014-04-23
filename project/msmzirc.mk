# top level project rules for the msmzirc project
#
LOCAL_DIR := $(GET_LOCAL_DIR)

TARGET := msmzirc

DEBUG := 1
ENABLE_USB30_SUPPORT := 1
ENABLE_SDHCI_SUPPORT := 1
ENABLE_BOOT_CONFIG_SUPPORT := 1

MODULES += app/aboot

#DEFINES += WITH_DEBUG_DCC=1
DEFINES += WITH_DEBUG_UART=1
#DEFINES += WITH_DEBUG_FBCON=1
DEFINES += DEVICE_TREE=1
DEFINES += SPMI_CORE_V2=1

ifeq ($(ENABLE_USB30_SUPPORT),1)
DEFINES += USB30_SUPPORT=1
endif

ifeq ($(ENABLE_SDHCI_SUPPORT),1)
DEFINES += MMC_SDHCI_SUPPORT=1
endif

#disable Thumb mode for the codesourcery/arm-2011.03 toolchain
ENABLE_THUMB := false

#Override linker for mdm targets
LD := $(TOOLCHAIN_PREFIX)ld.bfd
