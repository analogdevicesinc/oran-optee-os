// SPDX-License-Identifier: BSD-2-Clause
/*
 * ADI GPIO Driver
 *
 * Copyright (c) 2023, Analog Devices Incorporated. All rights reserved.
 */

#include <assert.h>
#include <drivers/adi/adrv906x/adi_adrv906x_pinmux_source_def.h>
#include <initcall.h>
#include <io.h>
#include <mm/core_memprot.h>
#include <gpio.h>

#include "adi_adrv906x_gpio.h"

#define ADI_ADRV906X_GPIO_COUNT ADRV906X_PIN_COUNT
#define ADI_GPIO_START_NUM (0U)
#define ADI_GPIO_CONTROLLER_REG_SIZE 0x800

/* Calculate the GPIO register and bit offset based on the pin number (0-115) */
#define GET_GPIO_REG(pin)       (pin / 32)
#define GET_GPIO_OFFSET(pin)    (pin % 32)

#define GPIO_DIR_CONTROL_SIZE    (4)    /* Size of each GPIO mode direction control register */
#define GPIO_REG_NUM    (4)             /* Number of GPIO registers */
#define GPIO_FUNCTION_NUM       (5)     /* Number of GPIO functions (write, clear, set, toggle, read) */


static vaddr_t adrv906x_reg_base_s[GPIO_REG_NUM][GPIO_FUNCTION_NUM] =
{
	{ GPIO_WRITE_REG0_OFFSET, GPIO_WRITE_REG0_CLEAR_OFFSET, GPIO_WRITE_REG0_SET_OFFSET,
	  GPIO_WRITE_REG0_TOGGLE_OFFSET, GPIO_READ_REG0_OFFSET },
	{ GPIO_WRITE_REG1_OFFSET, GPIO_WRITE_REG1_CLEAR_OFFSET, GPIO_WRITE_REG1_SET_OFFSET,
	  GPIO_WRITE_REG1_TOGGLE_OFFSET, GPIO_READ_REG1_OFFSET },
	{ GPIO_WRITE_REG2_OFFSET, GPIO_WRITE_REG2_CLEAR_OFFSET, GPIO_WRITE_REG2_SET_OFFSET,
	  GPIO_WRITE_REG2_TOGGLE_OFFSET, GPIO_READ_REG2_OFFSET },
	{ GPIO_WRITE_REG3_OFFSET, GPIO_WRITE_REG3_CLEAR_OFFSET, GPIO_WRITE_REG3_SET_OFFSET,
	  GPIO_WRITE_REG3_TOGGLE_OFFSET, GPIO_READ_REG3_OFFSET }
};

/* This enum must match the order of actions in adrv906x_reg_base_s */
typedef enum {
	GPIO_WRITE	= 0,
	GPIO_CLEAR	= 1,
	GPIO_SET	= 2,
	GPIO_TOGGLE	= 3,
	GPIO_READ	= 4
} gpio_mode_action_t;

/*
 * struct adi_adrv906x_gpio_chip_data describes GPIO controller chip instance
 * @chip:       generic GPIO chip handle.
 * @gpio_base:  starting GPIO number managed by this GPIO controller.
 * @ngpios:     number of GPIOs managed by this GPIO controller.
 * @base:       virtual base address of the GPIO controller registers.
 */
struct adi_adrv906x_gpio_chip_data {
	struct gpio_chip chip;
	unsigned int gpio_base;
	uint8_t gpio_controller;
	uint32_t ngpio;
	vaddr_t base;
};

/*
 * Get value from GPIO controller
 * chip:        pointer to GPIO controller chip instance
 * gpio_pin:    pin from which value needs to be read
 */
static enum gpio_level gpio_get_value(struct gpio_chip *chip,
				      unsigned int gpio_pin)
{
	vaddr_t gpio_data_addr = 0;
	uint32_t data = 0;
	uint32_t offset, bitmask;
	struct adi_adrv906x_gpio_chip_data *gc_data = container_of(chip,
								   struct adi_adrv906x_gpio_chip_data,
								   chip);

	assert(gpio_pin < gc_data->ngpio);

	gpio_data_addr = gc_data->base + adrv906x_reg_base_s[GET_GPIO_REG(gpio_pin)][GPIO_READ];
	offset = GET_GPIO_OFFSET(gpio_pin);
	bitmask = 0x1U << offset;

	data = io_read32(gpio_data_addr) & bitmask;

	if (data)
		return GPIO_LEVEL_HIGH;

	return GPIO_LEVEL_LOW;
}

/*
 * Set value for GPIO controller
 * chip:        pointer to GPIO controller chip instance
 * gpio_pin:    pin to which value needs to be write
 * value:       value needs to be written to the pin
 */
static void gpio_set_value(struct gpio_chip *chip, unsigned int gpio_pin,
			   enum gpio_level value)
{
	vaddr_t gpio_data_addr = 0;
	uint32_t offset, bitmask, reg;
	struct adi_adrv906x_gpio_chip_data *gc_data = container_of(chip,
								   struct adi_adrv906x_gpio_chip_data,
								   chip);

	assert(gpio_pin < gc_data->ngpio);

	offset = GET_GPIO_OFFSET(gpio_pin);
	bitmask = 0x1U << offset;

	if (value == GPIO_LEVEL_HIGH)
		gpio_data_addr = gc_data->base + adrv906x_reg_base_s[GET_GPIO_REG(gpio_pin)][GPIO_SET];
	else
		gpio_data_addr = gc_data->base + adrv906x_reg_base_s[GET_GPIO_REG(gpio_pin)][GPIO_CLEAR];

	reg = io_read32(gpio_data_addr);
	reg |= bitmask;
	io_write32(gpio_data_addr, reg);
}

/*
 * Get direction from GPIO controller
 * chip:        pointer to GPIO controller chip instance
 * gpio_pin:    pin from which direction needs to be read
 */
static enum gpio_dir gpio_get_direction(struct gpio_chip *chip,
					unsigned int gpio_pin)
{
	vaddr_t gpio_dir_addr = 0;
	uint32_t offset, data;
	struct adi_adrv906x_gpio_chip_data *gc_data = container_of(chip,
								   struct adi_adrv906x_gpio_chip_data,
								   chip);

	assert(gpio_pin < gc_data->ngpio);

	gpio_dir_addr = gc_data->base + GPIO_DIR_CONTROL_OFFSET;
	offset = gpio_pin * GPIO_DIR_CONTROL_SIZE;

	data = io_read32(gpio_dir_addr + offset) & GPIO_DIR_SEL_MASK;

	if (data == 0x1)
		return GPIO_DIR_OUT;

	return GPIO_DIR_IN;
}

/*
 * Set direction for GPIO controller
 * chip:        pointer to GPIO controller chip instance
 * gpio_pin:    pin on which direction needs to be set
 * direction:   direction which needs to be set on pin
 */
static void gpio_set_direction(struct gpio_chip *chip, unsigned int gpio_pin,
			       enum gpio_dir direction)
{
	vaddr_t gpio_dir_addr = 0;
	uint32_t offset, data, cleared_data;
	struct adi_adrv906x_gpio_chip_data *gc_data = container_of(chip,
								   struct adi_adrv906x_gpio_chip_data,
								   chip);

	assert(gpio_pin < gc_data->ngpio);

	gpio_dir_addr = gc_data->base + GPIO_DIR_CONTROL_OFFSET;
	offset = gpio_pin * GPIO_DIR_CONTROL_SIZE;

	data = io_read32(gpio_dir_addr + offset);
	cleared_data = data & ~GPIO_DIR_SEL_MASK;        /* clear OE and IE bits */

	if (direction == GPIO_DIR_OUT)
		io_write32(gpio_dir_addr + offset, cleared_data | (0x1U << GPIO_DIR_SEL_POS));
	else
		io_write32(gpio_dir_addr + offset, cleared_data | (0x1U << (GPIO_DIR_SEL_POS + 1)));
}

static const struct gpio_ops adi_adrv906x_gpio_ops = {
	.get_direction	= gpio_get_direction,
	.set_direction	= gpio_set_direction,
	.get_value	= gpio_get_value,
	.set_value	= gpio_set_value,
};

static TEE_Result adi_adrv906x_gpio_init(void)
{
	struct adi_adrv906x_gpio_chip_data *gc = NULL;

#ifndef GPIO_MODE_SECURE_BASE
	EMSG("Error GPIO driver init fail: File="__FILE__ "Line= " __LINE__ "\n");
	return TEE_ERROR_ITEM_NOT_FOUND;
#endif

	gc = malloc(sizeof(*gc));
	if (gc == NULL)
		return TEE_ERROR_OUT_OF_MEMORY;

	gc->base = (vaddr_t)phys_to_virt_io(GPIO_MODE_SECURE_BASE, ADI_GPIO_CONTROLLER_REG_SIZE);
	assert(gc->base);
	gc->chip.ops = &adi_adrv906x_gpio_ops;
	gc->gpio_base = ADI_GPIO_START_NUM;
	gc->ngpio = ADI_ADRV906X_GPIO_COUNT;

	DMSG("ADI GPIO gpio init SUCCESS\n");

	return TEE_SUCCESS;
}
driver_init(adi_adrv906x_gpio_init);
