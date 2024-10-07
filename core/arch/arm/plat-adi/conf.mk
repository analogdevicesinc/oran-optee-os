PLATFORM_FLAVOR ?= adrv906x_eval

CFG_ARM64_core ?= y

include core/arch/arm/cpu/cortex-armv8-0.mk

# Enable warnings-as-errors
CFG_WERROR ?= y

$(call force,CFG_ARM_GICV3,y)
$(call force,CFG_GIC,y)

$(call force,CFG_PL011,y)

$(call force,CFG_SECURE_TIME_SOURCE_CNTPCT,y)
$(call force,CFG_WITH_ARM_TRUSTED_FW,y)
$(call force,CFG_WITH_LPAE,y)

# Disable software PRNG implementation
$(call force,CFG_WITH_SOFTWARE_PRNG,n)

# Enable pseudo TA to export hardware RNG output to Normal World
$(call force,CFG_HWRNG_PTA,y)

# Set output rate and quality of hw_get_random_bytes()
$(call force,CFG_HWRNG_RATE,1250)
$(call force,CFG_HWRNG_QUALITY,1024)

# Disable backwards compatible derivation of RPMB and SSK keys
CFG_CORE_HUK_SUBKEY_COMPAT ?= n

CFG_USER_TA_TARGETS ?= ta_arm64

CFG_CRYPTO_WITH_CE ?= y

$(call force,CFG_DT,y)
CFG_DTB_MAX_SIZE ?= 0x100000

# Enable boot pseudo TA
CFG_ADI_BOOT_PTA ?= y

# Enable enforcement_counter TA
CFG_ADI_ENFORCEMENT_COUNTER_PTA ?= y

# Enable TE interface driver module
CFG_ADI_TE_INTERFACE ?= y

# Enable OTP driver module
CFG_ADI_OTP ?= y

# Enable TE Mailbox pseudo TA
CFG_TE_MAILBOX_PTA ?= y

# Enable I2C interface driver module
CFG_ADI_I2C ?= y

# Enable MACs pseudo TA
CFG_ADI_OTP_MACS_PTA ?= y

#
# Platform-flavor-specific configurations
#
ifeq ($(PLATFORM_FLAVOR),adrv906x_denali)
include core/arch/arm/plat-adi/adrv906x_denali.mk
else ifeq ($(PLATFORM_FLAVOR),adrv906x_titan)
include core/arch/arm/plat-adi/adrv906x_titan.mk
endif

#
# Unused features
#
CFG_GP_SOCKETS ?= n

#
# Set DEBUG flag for debug builds
#
ifeq ($(DEBUG),1)
platform-cflags-debug-info = -DDEBUG=1
endif

#
# Configuration for production
#
ifneq ($(DEBUG),1)
CFG_WARN_INSECURE = n
# Stack trace
CFG_LOCKDEP_RECORD_STACK = n
CFG_UNWIND = n
# Logs
CFG_DEBUG_INFO = n
CFG_TEE_CORE_DEBUG = n
CFG_TEE_CORE_TA_TRACE = n
endif

