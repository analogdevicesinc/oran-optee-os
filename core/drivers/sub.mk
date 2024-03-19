srcs-$(CFG_CDNS_UART) += cdns_uart.c
srcs-$(CFG_PL011) += pl011.c
srcs-$(CFG_TZC400) += tzc400.c
srcs-$(CFG_TZC380) += tzc380.c
srcs-$(CFG_GIC) += gic.c
srcs-$(CFG_PL061) += pl061_gpio.c
srcs-$(CFG_PL022) += pl022_spi.c
srcs-$(CFG_SP805_WDT) += sp805_wdt.c
srcs-$(CFG_8250_UART) += serial8250_uart.c
srcs-$(CFG_16550_UART) += ns16550.c
srcs-$(CFG_IMX_SNVS) += imx_snvs.c
srcs-$(CFG_IMX_UART) += imx_uart.c
srcs-$(CFG_IMX_I2C) += imx_i2c.c
srcs-$(CFG_IMX_LPUART) += imx_lpuart.c
srcs-$(CFG_IMX_WDOG) += imx_wdog.c
srcs-$(CFG_SPRD_UART) += sprd_uart.c
srcs-$(CFG_HI16XX_UART) += hi16xx_uart.c
srcs-$(CFG_HI16XX_RNG) += hi16xx_rng.c
srcs-$(CFG_SCIF) += scif.c
srcs-$(CFG_DRA7_RNG) += dra7_rng.c
srcs-$(CFG_STIH_UART) += stih_asc.c
srcs-$(CFG_ATMEL_UART) += atmel_uart.c
srcs-$(CFG_ATMEL_TRNG) += atmel_trng.c
srcs-$(CFG_ATMEL_RSTC) += atmel_rstc.c
srcs-$(CFG_ATMEL_SHDWC) += atmel_shdwc.c atmel_shdwc_a32.S
srcs-$(CFG_AMLOGIC_UART) += amlogic_uart.c
srcs-$(CFG_MVEBU_UART) += mvebu_uart.c
srcs-$(CFG_STM32_BSEC) += stm32_bsec.c
srcs-$(CFG_STM32_ETZPC) += stm32_etzpc.c
srcs-$(CFG_STM32_GPIO) += stm32_gpio.c
srcs-$(CFG_STM32_I2C) += stm32_i2c.c
srcs-$(CFG_STM32_RNG) += stm32_rng.c
srcs-$(CFG_STM32_UART) += stm32_uart.c
srcs-$(CFG_STPMIC1) += stpmic1.c
srcs-$(CFG_BCM_HWRNG) += bcm_hwrng.c
srcs-$(CFG_BCM_SOTP) += bcm_sotp.c
srcs-$(CFG_BCM_GPIO) += bcm_gpio.c
srcs-$(CFG_LS_I2C) += ls_i2c.c
srcs-$(CFG_LS_GPIO) += ls_gpio.c
srcs-$(CFG_LS_DSPI) += ls_dspi.c
srcs-$(CFG_IMX_RNGB) += imx_rngb.c
srcs-$(CFG_IMX_OCOTP) += imx_ocotp.c
srcs-$(CFG_IMX_SC) += imx_mu.c imx_sc_api.c
srcs-$(CFG_ZYNQMP_CSU_PUF) += zynqmp_csu_puf.c
srcs-$(CFG_ZYNQMP_CSUDMA) += zynqmp_csudma.c
srcs-$(CFG_ZYNQMP_CSU_AES) += zynqmp_csu_aes.c
srcs-$(CFG_ZYNQMP_PM) += zynqmp_pm.c
srcs-$(CFG_ZYNQMP_HUK) += zynqmp_huk.c

subdirs-y += crypto
subdirs-$(CFG_BNXT_FW) += bnxt
subdirs-$(CFG_DRIVERS_CLK) += clk
subdirs-$(CFG_DRIVERS_RSTCTRL) += rstctrl
subdirs-$(CFG_SCMI_MSG_DRIVERS) += scmi-msg
subdirs-y += imx
subdirs-y += adi
