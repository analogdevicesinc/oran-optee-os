srcs-$(CFG_ADI_BOOT_PTA) += boot.c
srcs-$(CFG_ADI_ENFORCEMENT_COUNTER_PTA) += enforcement_counter.c
srcs-$(CFG_ADI_SECONDARY_LAUNCHER_PTA) += secondary_launcher.c
srcs-$(CFG_ADI_ADIMEM_PTA) += adimem.c
srcs-$(CFG_TE_MAILBOX_PTA) += te_mailbox.c
srcs-$(CFG_ADI_OTP_MACS_PTA) += otp_macs.c
srcs-$(CFG_ADI_I2C) += i2c.c

subdirs-y += memdump
