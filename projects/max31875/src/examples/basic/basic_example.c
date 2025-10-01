/*******************************************************************************
 *   @file   basic_example.c
 *   @brief  Basic example code for max31875 project
 *   @author 
 ********************************************************************************
 * Copyright 2024(c) Analog Devices, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY ANALOG DEVICES, INC. “AS IS” AND ANY EXPRESS OR
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
#include "common_data.h"
#include "max31875.h"
#include "no_os_delay.h"
#include "no_os_print_log.h"
#include "no_os_units.h"
/*****************************************************************************
 * @brief Basic example main execution.
 *
 * @return ret - Result of the example execution. If working correctly, will
 *               execute continuously the while(1) loop and will not return.
 *******************************************************************************/
static int32_t max31875_get_single_temp(struct max31875_dev *dev,
        uint8_t extended, float *temp)
{
        float lsb_value = 0.0625;
        uint32_t temp_reg_val = 0;
        int16_t temp_val;
        int32_t ret;

        ret = max31875_reg_read(dev, MAX31875_TEMPERATURE_REG, &temp_reg_val);
        if (ret)
        return ret;

        if (extended) {
        temp_reg_val = no_os_field_get(NO_OS_GENMASK(15, 3), temp_reg_val);
        temp_val = no_os_sign_extend16(temp_reg_val, 12);
        } else {
        temp_reg_val = no_os_field_get(NO_OS_GENMASK(15, 4), temp_reg_val);
        temp_val = no_os_sign_extend16(temp_reg_val, 11);
        }

        *temp = temp_val * lsb_value;

        return 0;
}

 int example_main()
{
        struct max31875_dev *dev;
        int ret;
        float temperature;
        int extended = 1; // 0 - 12-bit mode, 1 - 13-bit mode

        struct no_os_uart_desc *uart;

        ret = no_os_uart_init(&uart, &uip);
        if (ret)
                goto error;
        
        no_os_uart_stdio(uart);

        pr_info("\r\nRunning max31875 Basic Example\r\n");

        ret = max31875_init(&dev, &max31875_ip);
        if (ret)
                goto free_uart;
        
        while (1) {

                ret = max31875_get_single_temp(dev, extended, &temperature);
                if (ret)
                        goto free_dev;
                
                pr_info("Temperature: %.4f C\r\n", temperature);
                no_os_mdelay(1000);
        }
free_dev:
	max31875_remove(dev);
free_uart:
	no_os_uart_remove(uart);
error:
	pr_info("Error!\r\n");
	return ret;
}
