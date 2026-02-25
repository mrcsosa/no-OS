/***************************************************************************//**
 *   @file   basic_example.c
 *   @brief  Basic example source file for adalm-lsmspg project.
 *   @author Marc Paolo Sosa (MarcPaolo.Sosa@analog.com)
********************************************************************************
 * Copyright 2026(c) Analog Devices, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of Analog Devices, Inc. nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ANALOG DEVICES, INC. "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL ANALOG DEVICES, INC. BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/
#include "example.h"
#include "common_data.h"
#include "no_os_delay.h"
#include "no_os_print_log.h"
#include "ad5592r.h"
#include "ad5593r.h"
#include <stdlib.h>

#define DEFINE_AD5592R_IP() { \
	.int_ref = true, \
	.spi_init = &ad5592r_spi_ip, \
	.i2c_init = NULL, \
	.ss_init = &ad5592r_spi_ss_ip, \
	.channel_modes = { \
		CH_MODE_DAC,         /* channel 0 */ \
		CH_MODE_ADC,         /* channel 1 */ \
		CH_MODE_DAC_AND_ADC, /* channel 2 */ \
		CH_MODE_DAC_AND_ADC, /* channel 3 */ \
		CH_MODE_GPI, 	     /* channel 4 */ \
		CH_MODE_GPO,         /* channel 5 */ \
		CH_MODE_GPI,         /* channel 6 */ \
		CH_MODE_GPO,         /* channel 7 */ \
	}, \
	.channel_offstate = { \
		CH_OFFSTATE_OUT_TRISTATE, /* channel 0 */ \
		CH_OFFSTATE_OUT_TRISTATE, /* channel 1 */ \
		CH_OFFSTATE_OUT_TRISTATE, /* channel 2 */ \
		CH_OFFSTATE_OUT_TRISTATE, /* channel 3 */ \
		CH_OFFSTATE_PULLDOWN,     /* channel 4 */ \
		CH_OFFSTATE_OUT_LOW,      /* channel 5 */ \
		CH_OFFSTATE_PULLDOWN,     /* channel 6 */ \
		CH_OFFSTATE_OUT_LOW,      /* channel 7 */ \
	}, \
	.adc_range = ZERO_TO_VREF, \
	.dac_range = ZERO_TO_VREF, \
	.adc_buf = false, \
}

#define DEFINE_AD5593R_IP() { \
	.int_ref = true, \
	.spi_init = NULL, \
	.i2c_init = &ad5593r_i2c_ip, \
	.ss_init = NULL, \
	.channel_modes = { \
		CH_MODE_DAC,         /* channel 0 */ \
		CH_MODE_ADC,         /* channel 1 */ \
		CH_MODE_DAC_AND_ADC, /* channel 2 */ \
		CH_MODE_DAC_AND_ADC, /* channel 3 */ \
		CH_MODE_GPI, 	     /* channel 4 */ \
		CH_MODE_GPO,         /* channel 5 */ \
		CH_MODE_GPI,         /* channel 6 */ \
		CH_MODE_GPO,         /* channel 7 */ \
	}, \
	.channel_offstate = { \
		CH_OFFSTATE_OUT_TRISTATE, /* channel 0 */ \
		CH_OFFSTATE_OUT_TRISTATE, /* channel 1 */ \
		CH_OFFSTATE_OUT_TRISTATE, /* channel 2 */ \
		CH_OFFSTATE_OUT_TRISTATE, /* channel 3 */ \
		CH_OFFSTATE_PULLDOWN,     /* channel 4 */ \
		CH_OFFSTATE_OUT_LOW,      /* channel 5 */ \
		CH_OFFSTATE_PULLDOWN,     /* channel 6 */ \
		CH_OFFSTATE_OUT_LOW,      /* channel 7 */ \
	}, \
	.adc_range = ZERO_TO_VREF, \
	.dac_range = ZERO_TO_VREF, \
	.adc_buf = false, \
}

/**
 * @brief Display device configuration in a formatted way
 * @param dev - AD5592R device structure
 * @param device_name - Name to display for the device
 */
static void display_device_config(struct ad5592r_dev *dev, const char *device_name)
{
	uint32_t vref_mv;
	int ret;

	pr_info("=== %s Configuration ===\n\r", device_name);

	/* Display reference voltage */
	ret = ad5592r_get_ref(dev, &vref_mv);
	if (ret == 0) {
		pr_info("Reference Voltage: %ld.%03ld V (%s)\n\r",
			vref_mv / 1000, vref_mv % 1000,
			dev->int_ref ? "Internal" : "External");
	}

	/* Display ADC range */
	pr_info("ADC Range: %s\n\r",
		dev->adc_range == ZERO_TO_VREF ? "0 to VREF" : "0 to 2*VREF");

	/* Display DAC range */
	pr_info("DAC Range: %s\n\r",
		dev->dac_range == ZERO_TO_VREF ? "0 to VREF" : "0 to 2*VREF");

	/* Display ADC buffer status */
	pr_info("ADC Buffer: %s\n\r", dev->adc_buf ? "Enabled" : "Disabled");

	/* Display channel configuration */
	pr_info("Channel Configuration:\n\r");
	for (int i = 0; i < NUM_OF_CHANNELS; i++) {
		const char *mode_str;
		switch (dev->channel_modes[i]) {
		case CH_MODE_UNUSED:
			mode_str = "Unused";
			break;
		case CH_MODE_ADC:
			mode_str = "ADC";
			break;
		case CH_MODE_DAC:
			mode_str = "DAC";
			break;
		case CH_MODE_DAC_AND_ADC:
			mode_str = "DAC+ADC";
			break;
		case CH_MODE_GPI:
			mode_str = "GPIO Input";
			break;
		case CH_MODE_GPO:
			mode_str = "GPIO Output";
			break;
		default:
			mode_str = "Unknown";
		}
		pr_info("  Channel %d: %s\n\r", i, mode_str);
	}

	pr_info("\n\r");
}

/**
 * @brief Read and display ADC values
 * @param dev - AD5592R device structure
 * @param device_name - Name to display for the device
 */
static void display_adc_values(struct ad5592r_dev *dev, const char *device_name)
{
	uint16_t adc_value;
	uint32_t vref_mv;
	int ret;

	pr_info("=== %s ADC Readings ===\n\r", device_name);

	/* Get reference voltage for conversion */
	ret = ad5592r_get_ref(dev, &vref_mv);
	if (ret) {
		pr_err("Failed to get reference voltage: %d\n\r", ret);
		return;
	}

	for (int i = 0; i < NUM_OF_CHANNELS; i++) {
		/* Only read from channels configured as ADC or DAC+ADC */
		if (dev->channel_modes[i] == CH_MODE_ADC ||
		    dev->channel_modes[i] == CH_MODE_DAC_AND_ADC) {
			ret = ad5592r_read_adc(dev, i, &adc_value);
			if (ret == 0) {
				/* Convert to voltage: ADC_CODE * VREF / 4095 */
				uint32_t voltage_mv = (adc_value * vref_mv) / 4095;
				if (dev->adc_range == ZERO_TO_2VREF)
					voltage_mv *= 2;
				pr_info("  Channel %d: 0x%04X (%ld.%03ld V)\n\r",
					i, adc_value, voltage_mv / 1000, voltage_mv % 1000);
			} else {
				pr_err("  Channel %d: Read failed (%d)\n\r", i, ret);
			}
		}
	}

	pr_info("\n\r");
}

/**
 * @brief Write DAC values and display them
 * @param dev - AD5592R device structure
 * @param device_name - Name to display for the device
 */
static void write_and_display_dac_values(struct ad5592r_dev *dev,
					  const char *device_name,
					  uint16_t *dac_values)
{
	uint32_t vref_mv;
	int ret;

	pr_info("=== %s DAC Outputs ===\n\r", device_name);

	/* Get reference voltage for conversion */
	ret = ad5592r_get_ref(dev, &vref_mv);
	if (ret) {
		pr_err("Failed to get reference voltage: %d\n\r", ret);
		return;
	}

	for (int i = 0; i < NUM_OF_CHANNELS; i++) {
		/* Only write to channels configured as DAC or DAC+ADC */
		if (dev->channel_modes[i] == CH_MODE_DAC ||
		    dev->channel_modes[i] == CH_MODE_DAC_AND_ADC) {
			ret = ad5592r_write_dac(dev, i, dac_values[i]);
			if (ret == 0) {
				/* Convert to voltage: DAC_CODE * VREF / 4095 */
				uint32_t voltage_mv = (dac_values[i] * vref_mv) / 4095;
				if (dev->dac_range == ZERO_TO_2VREF)
					voltage_mv *= 2;
				pr_info("  Channel %d: 0x%04X (%ld.%03ld V)\n\r",
					i, dac_values[i], voltage_mv / 1000, voltage_mv % 1000);
			} else {
				pr_err("  Channel %d: Write failed (%d)\n\r", i, ret);
			}
		}
	}

	pr_info("\n\r");
}

/**
 * @brief Read and display GPIO status
 * @param dev - AD5592R device structure
 * @param device_name - Name to display for the device
 */
static void display_gpio_status(struct ad5592r_dev *dev, const char *device_name)
{
	uint8_t gpio_value;
	int ret;

	pr_info("=== %s GPIO Status ===\n\r", device_name);

	ret = ad5592r_gpio_read(dev, &gpio_value);
	if (ret) {
		pr_err("Failed to read GPIO: %d\n\r", ret);
		return;
	}

	for (int i = 0; i < NUM_OF_CHANNELS; i++) {
		/* Only display GPIO channels */
		if (dev->channel_modes[i] == CH_MODE_GPI ||
		    dev->channel_modes[i] == CH_MODE_GPO) {
			const char *direction = dev->channel_modes[i] == CH_MODE_GPI ? "Input" : "Output";
			const char *state = (gpio_value & (1 << i)) ? "High" : "Low";
			pr_info("  Channel %d (%s): %s\n\r", i, direction, state);
		}
	}

	pr_info("\n\r");
}

/**
 * @brief Toggle GPIO outputs
 * @param dev - AD5592R device structure
 * @param device_name - Name to display for the device
 */
static void toggle_gpio_outputs(struct ad5592r_dev *dev, const char *device_name)
{
	int ret;
	static bool toggle_state = false;

	for (int i = 0; i < NUM_OF_CHANNELS; i++) {
		/* Toggle GPIO output channels */
		if (dev->channel_modes[i] == CH_MODE_GPO) {
			ret = ad5592r_gpio_set(dev, i, toggle_state ? 1 : 0);
			if (ret) {
				pr_err("%s: Failed to set GPIO %d: %d\n\r", device_name, i, ret);
			}
		}
	}

	toggle_state = !toggle_state;
}

int basic_example(void)
{
	struct ad5592r_dev *ad5592r_dev = NULL;
	struct ad5592r_dev *ad5593r_dev = NULL;
	struct no_os_uart_desc *uart_desc = NULL;
	static uint16_t dac_values[NUM_OF_CHANNELS] = {1024, 0, 2048, 3072, 0, 0, 0, 0};
	int ret;
	int loop_count = 0;

	/* Initialize UART for logging */
	ret = no_os_uart_init(&uart_desc, &uart_ip);
	if (ret) {
		return ret;
	}

	no_os_uart_stdio(uart_desc);
	pr_info("\e[2J\e[H");
	pr_info("ADALM-LSMSPG Basic Example\n\r");
	pr_info("AD5592R and AD5593R Mixed-Signal Devices\n\r");
	pr_info("==========================================\n\r\n\r");

	/* Initialize AD5592R (SPI device) */
	struct ad5592r_init_param ad5592r_ip = DEFINE_AD5592R_IP();
	ret = ad5592r_init(&ad5592r_dev, &ad5592r_ip);
	if (ret) {
		pr_err("Failed to initialize AD5592R (SPI): %d\n\r", ret);
		goto cleanup;
	}
	pr_info("AD5592R (SPI) initialized successfully\n\r");

	/* Initialize AD5593R (I2C device) */
	struct ad5592r_init_param ad5593r_ip = DEFINE_AD5593R_IP();
	ret = ad5593r_init(&ad5593r_dev, &ad5593r_ip);
	if (ret) {
		pr_err("Failed to initialize AD5593R (I2C): %d\n\r", ret);
		goto cleanup;
	}
	pr_info("AD5593R (I2C) initialized successfully\n\r\n\r");

	/* Display device configurations once */
	display_device_config(ad5592r_dev, "AD5592R (SPI)");
	display_device_config(ad5593r_dev, "AD5593R (I2C)");

	/* Main loop - continuously read ADC, write DAC, and check GPIO */
	while (1) {
		pr_info("===== Loop %d =====\n\r", ++loop_count);

		/* Write DAC values */
		write_and_display_dac_values(ad5592r_dev, "AD5592R (SPI)", dac_values);
		write_and_display_dac_values(ad5593r_dev, "AD5593R (I2C)", dac_values);

		/* Read ADC values */
		display_adc_values(ad5592r_dev, "AD5592R (SPI)");
		display_adc_values(ad5593r_dev, "AD5593R (I2C)");

		/* Display GPIO status */
		display_gpio_status(ad5592r_dev, "AD5592R (SPI)");
		display_gpio_status(ad5593r_dev, "AD5593R (I2C)");

		/* Toggle GPIO outputs */
		toggle_gpio_outputs(ad5592r_dev, "AD5592R (SPI)");
		toggle_gpio_outputs(ad5593r_dev, "AD5593R (I2C)");

		/* Update DAC values for next iteration (create a simple ramp) */
		for (int i = 0; i < NUM_OF_CHANNELS; i++) {
			dac_values[i] = (dac_values[i] + 100) % 4096;
		}

		pr_info("Waiting 2 seconds...\n\r\n\r");
		no_os_mdelay(2000);
	}

cleanup:
	if (ad5593r_dev)
		ad5593r_remove(ad5593r_dev);
	if (ad5592r_dev)
		ad5592r_remove(ad5592r_dev);
	if (uart_desc)
		no_os_uart_remove(uart_desc);

	return ret;
}

example_main("basic_example");