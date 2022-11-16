/***************************************************************************//**
 *   @file   max31855.c
 *   @brief  Implementation of MAX31865 Driver.
 *   @author Ciprian Regus (ciprian.regus@analog.com)
********************************************************************************
 * Copyright 2022(c) Analog Devices, Inc.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *  - Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  - Neither the name of Analog Devices, Inc. nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *  - The use of this software may or may not infringe the patent rights
 *    of one or more patent holders.  This license does not release you
 *    from the requirement that you obtain separate licenses from these
 *    patent holders to use this software.
 *  - Use of the software either in source or binary form, must be run
 *    on or directly connected to an Analog Devices Inc. component.
 *
 * THIS SOFTWARE IS PROVIDED BY ANALOG DEVICES "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, NON-INFRINGEMENT,
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL ANALOG DEVICES BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, INTELLECTUAL PROPERTY RIGHTS, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/

/******************************************************************************/
/***************************** Include Files **********************************/
/******************************************************************************/

#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include "max31865.h"
#include "no_os_spi.h"
#include "no_os_util.h"

/******************************************************************************/
/************************ Functions Definitions *******************************/
/******************************************************************************/
/******************************************************************************/
int max31865_init(struct max31865_dev **device,
		  struct max31865_init_param *init_param)
{
	int ret;
	struct max31865_dev *descriptor;

	descriptor = calloc(1, sizeof(*descriptor));
	if (!descriptor)
		return -ENOMEM;

	ret = no_os_spi_init(&descriptor->comm_desc, &init_param->spi_init);
	if (ret)
		goto spi_err;

	*device = descriptor;

	return 0;
spi_err:
	free(descriptor);

	return ret;
}


int max31865_remove(struct max31865_dev *device)
{
	int ret;
	if (!device)
		return -ENODEV;

	ret = no_os_spi_remove(device->comm_desc);
	if (ret)
		return -EINVAL;

	free(device);

	return 0;
}


int max31865_read_raw(struct max31865_dev *device, uint8_t *val)
{
	int ret;
	uint8_t raw_array[1];
	raw_array[0] = *val;

	ret = no_os_spi_write_and_read(device->comm_desc, raw_array, 1);
	if (ret)
		return ret;
	*val = raw_array[0];
	return 0;
}


int max31865_write_raw(struct max31865_dev *device, uint8_t *val, uint8_t *val2)
{
	int ret;
	uint8_t raw_array[2];
	raw_array[0] = *val;
	raw_array[1] = *val2;

	ret = no_os_spi_write_and_read(device->comm_desc, raw_array, 1);
	if (ret)
		return ret;
	return 0;
}

uint8_t max31865_read_fault(struct max31865_dev *device)
{
	uint8_t faultReg = 0x07;
	max31865_read_raw(device, &faultReg);
	return faultReg;
}

void max31865_clear_fault(struct max31865_dev *device)
{
	uint8_t faultReg= 0x00;
	max31865_read_raw(device, &faultReg);
	faultReg &= ~0x2C;
	faultReg |= 0x02;
	uint8_t clrReg = 0x00;
	max31865_write_raw(device, &faultReg, &clrReg);
}

void max31865_enable_bias(struct max31865_dev *device, bool b)
{
	uint8_t confReg = 0x00;
	uint8_t biasReg = 0x00;
	max31865_read_raw(device, &confReg);
	if (b) {
		biasReg |= 0x80;	
	} else {
		biasReg &= ~0x80;
	}
	max31865_write_raw(device, &confReg, &biasReg);
}

void max31865_auto_convert(struct max31865_dev *device, bool b)
{
	uint8_t confReg = 0x00;
	uint8_t convReg = 0x00;
	max31865_read_raw(device, &confReg);
	if (b) {
		convReg |= 0x40;
	} else {
		convReg &= ~0x40;
	}
	max31865_write_raw(device, &confReg, &convReg);
}

void max31865_enable_50Hz(struct max31865_dev *device, bool b)
{
	uint8_t confReg = 0x00;
	uint8_t filReg = 0x00;
	max31865_read_raw(device, &confReg);
	if (b) {
		filReg |= 0x01;
	} else {
		filReg &= ~0x01;
	}
	max31865_write_raw(device, &confReg, &filReg);
}


void max31865_set_threshold(struct max31865_dev *device, uint16_t *lower, uint16_t *upper)
 {
	max31865_write_raw(MAX31865_LFAULTLSB_REG, lower & 0xFF);
  max31865_write_raw(MAX31865_LFAULTMSB_REG, lower >> 8);
  max31865_write_raw(MAX31865_HFAULTLSB_REG, upper & 0xFF);
 max31865_write_raw(MAX31865_HFAULTMSB_REG, upper >> 8);
}


uint16_t max31865_get_lower_threshold(struct max31865_dev *device)
{
	uint8_t lowmsb = 0x05;
	uint8_t lowlsb = 0x06;
	max31865_read_raw(device, &lowmsb);
	max31865_read_raw(device, &lowlsb);
	
	uint16_t lowthresh = 0x00;

	lowthresh = (((uint16_t)lowmsb << 8) | (uint16_t)lowlsb);

	return lowthresh;
}

uint16_t max31865_get_upper_threshold(struct max31865_dev *device)
{
	uint8_t highmsb = 0x03;
	uint8_t highlsb = 0x04;
	max31865_read_raw(device, &highmsb);
	max31865_read_raw(device, &highlsb);
	
	uint16_t highthresh = 0x00;

	highthresh = (((uint16_t)highmsb << 8) | (uint16_t)highlsb);

	return highthresh;
}

void max31865_set_wires(struct max31865_dev *device)
{	max31865_numwires_t wires;
	uint8_t wireReg = 0x00;
	if (wires == MAX31865_3WIRE)
	{
		wireReg |= 0x10;
	} else
	{
		wireReg &= ~0x10;
	}
	max31865_write_raw(device, &wireReg, &wires);
}

uint16_t max31865_read_RTD(struct max31865_dev *device)
{
	max31865_clear_fault(device);
	max31865_enable_bias(device, true);
	delay(10);

	uint8_t confReg = 0x00;
	uint8_t rtdReg = 0x00;
	max_read_raw(device, &confReg);
	if (confReg = 0x00)
	{
		rtdReg |= 0x20;
	} else 
	{
		rtdReg &= ~0x20;
	}
	max31865_write_raw(device, &confReg, &rtdReg);
	delay(65);

	uint8_t rtd_MSBReg = 0x01;
	uint8_t rtd_LSBReg = 0x02;
	max31865_read_raw(device, &rtd_MSBReg);
	max31865_read_raw(device, &rtd_LSBReg);

	uint16_t rtd = 0x00;

	rtd = (((uint16_t)rtd_MSBReg << 8) | (uint16_t)rtd_LSBReg);

	max31865_enable_bias(device, false);

	rtd >>=1;

	return rtd;
}

