/***************************************************************************//**
 *   @file   iio_max31865.c
 *   @brief  Implementation of IIO MAX31865 driver.
 *   @author 
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
#include "iio_max31865.h"
#include "max31865.h"
#include "no_os_util.h"
#include "iio.h"

/******************************************************************************/
/************************ Functions Declarations ******************************/
/******************************************************************************/

static int max31865_iio_read_raw(void *dev, char *buf, uint32_t len,
				 const struct iio_ch_info *channel, intptr_t priv);
static int max31865_iio_read_scale(void *dev, char *buf, uint32_t len,
				   const struct iio_ch_info *channel, intptr_t priv);
static int max31865_iio_update_channels(void *dev, uint32_t mask);
static int max31865_iio_reg_read(struct max31865_iio_dev *dev, uint32_t reg,
				 uint32_t *readval);
static int max31865_iio_read_samples(void* dev, uint16_t* buff,
				     uint32_t samples);

/******************************************************************************/
/************************ Variable Declarations ******************************/
/******************************************************************************/

static struct iio_attribute max31865_iio_rtd_attrs[] = {
	{
		.name = "raw",
		.show = max31865_iio_read_raw
	},
	{
		.name = "scale",
		.show = max31865_iio_read_scale
	},
	END_ATTRIBUTES_ARRAY
};

static struct scan_type scan_RTD = {
	.sign = 's',
	.realbits = 16,
	.storagebits = 15,
	.shift = 1,
	.is_big_endian = false
};


static struct iio_channel max31865_channels[] = {
	{
		.name = "i_rtd",
		.ch_type = IIO_TEMP,
		.address = 0,
		.channel2 = IIO_MOD_TEMP_AMBIENT,
		.modified = true,
		.scan_index = 1,
		.scan_type = &scan_RTD,
		.attributes = max31865_iio_rtd_attrs
	},
	{
		.name = "t_rtd",
		.ch_type = IIO_TEMP,
		.address = 2,
		.scan_index = 0,
		.scan_type = &scan_RTD,
		.attributes = max31865_iio_rtd_attrs
	}
};

static struct iio_device max31865_iio_dev = {
	.num_ch = NO_OS_ARRAY_SIZE(max31865_channels),
	.channels = max31865_channels,
	.pre_enable = (int32_t (*)())max31865_iio_update_channels,
	.read_dev = (int32_t (*)())max31865_iio_read_samples,
	.debug_reg_read = (int32_t (*)())max31865_iio_reg_read
};

/******************************************************************************/
/************************ Functions Definitions *******************************/
/******************************************************************************/
/******************************************************************************/

/**
 * @brief Initializes the MAX3165 IIO driver
 * @param iio_dev - The iio device structure.
 * @param init_param - Parameters for the initialization of iio_dev
 * @return 0 in case of success, errno errors otherwise
 */
int max31865_iio_init(struct max31865_iio_dev **iio_dev,
		  struct max31865_iio_dev_init_param *init_param)
{
	int ret;
	struct max31865_iio_dev *descriptor;

	if (!init_param)
		return -EINVAL;

	descriptor = calloc(1, sizeof(*descriptor));
	if (!descriptor)
		return -ENOMEM;

	ret = max31865_init(&descriptor->max31865_desc, init_param->max31865_dev_init);
	if (ret)
		goto init_err;

	descriptor->iio_dev = &max31865_iio_dev;

	*iio_dev = descriptor;

	return 0;
init_err:
	free(descriptor);

	return ret;
}

/**
 * @brief Free resources allocated by the init function
 * @param desc - The iio device structure.
 * @return 0 in case of success, errno errors otherwise
 */
int max31865_iio_remove(struct max31865_iio_dev *desc)
{
	int ret;

	ret = max31865_remove(desc->max31865_desc);
	if (ret)
		return ret;

	free(desc);

	return 0;
}

/**
 * @brief Updates the number of active channels
 * @param dev - The iio device structure.
 * @param mask - Bit mask containing active channels
 * @return 0 in case of success, errno errors otherwise
 */
static int max31865_iio_update_channels(void *dev, uint32_t mask)
{
	struct max31865_iio_dev *iio_max31865;

	if (!dev)
		return -EINVAL;

	iio_max31865 = dev;

	iio_max31865->active_channels = mask;
	iio_max31865->no_of_active_channels = no_os_hweight8((uint8_t)mask);

	return 0;
}

/**
 * @brief Handles the read request for scale attribute.
 * @param dev     - The iio device structure.
 * @param buf	  - Command buffer to be filled with requested data.
 * @param len     - Length of the received command buffer in bytes.
 * @param channel - Command channel info.
 * @param priv    - Command attribute id.
 * @return ret    - 0 in case of success, errno errors otherwise
*/
static int max31865_iio_read_scale(void *dev, char *buf, uint32_t len,
				   const struct iio_ch_info *channel, intptr_t priv)
{
	int32_t val[2] = {0};

	switch (channel->type) {
	case IIO_TEMP:
		switch (channel->address) {
		case 0:
			val[0] = 62;
			val[1] = 500000;
			return iio_format_value(buf, len, IIO_VAL_INT_PLUS_MICRO, 2, val);
		case 2:
			val[0] = 250;
			return iio_format_value(buf, len, IIO_VAL_INT, 1, val);
		default:
			return -EINVAL;
		}
	default:
		return -EINVAL;
	}
}

/**
 * @brief Reads the number of given samples for the selected channels
 * @param dev     - The iio device structure.
 * @param buf	  - Command buffer to be filled with requested data.
 * @param samples - Number of samples to be returned
 * @return ret    - 0 in case of success, errno errors otherwise
*/
static int max31865_iio_read_samples(void* dev, uint16_t* buff,
				     uint32_t samples)
{
	int ret;
	int16_t i_temp;
	int16_t t_temp;
	uint32_t raw_buff;
	struct max31865_iio_dev *max31865_iio;
	struct max31865_dev *max31865_dev;

	if (!dev)
		return -EINVAL;

	max31865_iio = dev;
	max31865_dev = max31865_iio->max31865_desc;

	for (uint32_t i = 0; i < samples * max31865_iio->no_of_active_channels;) {
		ret = max31865_read_raw(max31865_dev, &raw_buff);
		if (ret)
			return ret;

		if (max31865_iio->active_channels & NO_OS_BIT(0)) {
			t_temp = no_os_field_get(NO_OS_GENMASK(31, 16), raw_buff);
			buff[i] = t_temp;
			i++;
		}

		if (max31865_iio->active_channels & NO_OS_BIT(1)) {
			i_temp = no_os_field_get(NO_OS_GENMASK(15, 0), raw_buff);
			buff[i] = i_temp;
			i++;
		}
	}

	return samples;
}