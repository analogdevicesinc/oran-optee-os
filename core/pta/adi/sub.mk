srcs-$(CFG_ADI_BOOT_PTA) += boot.c
srcs-$(CFG_ADI_ENFORCEMENT_COUNTER_PTA) += enforcement_counter.c
srcs-$(CFG_ADI_ENFORCEMENT_COUNTER_UPDATE_PTA) += enforcement_counter_update.c
srcs-$(CFG_ADI_SECONDARY_LAUNCHER_PTA) += secondary_launcher.c
srcs-$(CFG_TE_MAILBOX_PTA) += te_mailbox.c
srcs-$(CFG_ADI_OTP_MACS_PTA) += otp_macs.c
srcs-$(CFG_ADI_ALIVE_REPLY) += alive_reply.c
srcs-$(CFG_ADI_OTP_TEMP_PTA) += otp_temp.c

subdirs-y += adimem
subdirs-y += memdump
subdirs-y += i2c
