/***************************************************************************//**
 *   @file   iio_ad463x.c
 *   @brief  Implementation of iio_ad463x.c.
 *   @author Niel Acuna (niel.acuna@analog.com)
 *           Marc Paolo Sosa (MarcPaolo.Sosa@analog.com)
********************************************************************************
 * Copyright 2021(c) Analog Devices, Inc.
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
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include <no_os_alloc.h>

#include <iio_ad4080.h>

struct iio_ad4080_completion {
#define IIO_AD4080_COMPLETION_SIGNATURE 	0xdeadc0de
	uint32_t signature;
	bool done;
	int ret;
	uint32_t timeout;
};

typedef int (*attr_fn)(struct iio_ad4080_desc *iio_ad4080,
		       char *buf, 
		       uint32_t len,
		       const struct iio_ch_info *ch_info,
		       bool show);


static int init_completion(struct iio_ad4080_completion *completion)
{
	if (!completion)
		return -EINVAL;
	completion->signature = IIO_AD4080_COMPLETION_SIGNATURE;
	completion->done = false;
	return 0;
}

static int wait_for_completion(struct iio_ad4080_completion *completion)
{
	for(;;) {
		if (completion->timeout == 0)
			return -ETIMEDOUT;
		if (completion->done == true)
			break;
	}
	return completion->ret;
}

static int wait_for_completion_timeout(struct iio_ad4080_completion *completion,
		uint32_t timeout)
{
	if (!completion)
		return -EINVAL;
	assert(completion->signature == IIO_AD4080_COMPLETION_SIGNATURE);
	completion->timeout = timeout;
	return wait_for_completion(completion);
}

static int complete(struct iio_ad4080_completion *completion, int ret)
{
	if (!completion)
		return -EINVAL;
	assert(completion->signature == IIO_AD4080_COMPLETION_SIGNATURE);
	completion->done = true;
	completion->ret = ret;
	return 0;
}

static void ad4080_format_raw_data(uint32_t *data, uint8_t *raw_data, size_t count)
{
	size_t i;
	for (i = 0; i < count; i++) {
		uint8_t *p = &raw_data[3 * i + 1];
		data[i] = no_os_get_unaligned_be24(p);
	}
	return;
}

static int iio_ad4080_read_data(struct iio_ad4080_desc *iio_ad4080)
{
	struct ad4080_dev *ad4080 = iio_ad4080->ad4080;
	struct iio_ad4080_fifo_struct *fifo;
	int err;

	/* first read raw fifo data */
	fifo = &iio_ad4080->fifo;
	err = ad4080_read_data(ad4080, fifo->raw_fifo, fifo->bufsize);
	if (err)
		return err;

	/* then condition the raw data to make it readable */
	ad4080_format_raw_data(fifo->formatted_fifo, fifo->raw_fifo, fifo->formatted_bufsize >> 2);

	return err;
}

/* this is a blocking function */
static void iio_ad4080_immediate_trigger(struct iio_ad4080_desc *iio_ad4080)
{
	int err;

	/* declare completion on stack */
	struct iio_ad4080_completion completion_on_stack;

	init_completion(&completion_on_stack);
	iio_ad4080->ff_full_completion = &completion_on_stack;

	/* Arm the AD4080 FIFO and wait for done */
	ad4080_set_fifo_mode(iio_ad4080->ad4080, AD4080_IMMEDIATE_TRIGGER);

	/* wait till done */
	err = wait_for_completion_timeout(&completion_on_stack, 0xFFFF);
	if (err == -ETIMEDOUT) {
		/* no idea what to do here */
	}

	iio_ad4080->ff_full_completion = NULL;

	return;
}

static int32_t ad4080_reg_read(void *dev, uint32_t reg, uint32_t *readval)
{
	if (reg > AD4080_LAST_REG_ADDR)
		return -EINVAL;
	return ad4080_read(dev, reg, (uint8_t *)readval);
}

static int32_t ad4080_reg_write(void *dev, uint32_t reg, uint32_t writeval)
{
	if (reg > AD4080_LAST_REG_ADDR)
		return -EINVAL;
	return ad4080_write(dev, reg, writeval);
}

static int raw_attr_handler(struct iio_ad4080_desc *iio_ad4080,
			    char *buf, 
		            uint32_t len,
		            const struct iio_ch_info *ch_info,
		            bool show)
{
	int err = -1;
	struct iio_ad4080_fifo_struct *fifo;

	fifo = &iio_ad4080->fifo;
	if (show) {
		iio_ad4080_immediate_trigger(iio_ad4080);
		err = sprintf(buf, "%d", fifo->formatted_fifo[0]);	
	}
	return err;
}

static int scale_attr_handler(struct iio_ad4080_desc *iio_ad4080,
			      char *buf, 
		              uint32_t len,
		              const struct iio_ch_info *ch_info,
		              bool show)
{
	const double ad4080_scale = AD4080_DEFAULT_SCALE;
	return sprintf(buf, "%10f", ad4080_scale);
}

static uint16_t ad4080_read16(struct ad4080_dev *ad4080, uint16_t reg)
{
	uint16_t val = 0;
	if (ad4080) {
		uint8_t tmp;
		ad4080_read(ad4080, reg + 1, &tmp);
		val = tmp;

		ad4080_read(ad4080, reg, &tmp);
		val = (val << 8) | tmp;
	}
	return val;
}

static void ad4080_write16(struct ad4080_dev *ad4080, uint16_t reg,
		uint16_t reg_val)
{
	if (ad4080) {
		uint8_t tmp;

		tmp = (reg_val >> 8) & 0xF;
		ad4080_write(ad4080, reg + 1, tmp);


		tmp = (uint8_t)(reg_val & 0xFF);
		ad4080_write(ad4080, reg, tmp);
	}
	return;
}

static int16_t ad4080_read_offset(struct ad4080_dev *ad4080)
{
	int16_t reg_val = 0;
	if (ad4080) {
		reg_val = ad4080_read16(ad4080, AD4080_REG_OFFSET);
	}
	/* sign extend if we need to */
	return reg_val & 0x800 ? -reg_val : reg_val;
}

static void ad4080_write_offset(struct ad4080_dev *ad4080, uint16_t offset)
{
	if (ad4080) {
		ad4080_write16(ad4080, AD4080_REG_OFFSET, offset);
	}
	return;
}

static int offset_attr_handler(struct iio_ad4080_desc *iio_ad4080,
			       char *buf, 
		               uint32_t len,
		               const struct iio_ch_info *ch_info,
		               bool show)
{
	const double lsb = 0.00572;
	struct ad4080_dev *ad4080;
	double offset_correction_coefficient;
	const double max_offset_correction = 2047 * lsb;
	const double least_offset_correction = -2048 * lsb;
	uint16_t offset;
	ad4080 = iio_ad4080->ad4080;
	if (show) {
		offset_correction_coefficient = (double)ad4080_read_offset(ad4080);
		offset_correction_coefficient *= lsb;
		return sprintf(buf, "%10f", offset_correction_coefficient);
	}

	offset_correction_coefficient = strtof(buf, NULL);
	
	if (offset_correction_coefficient > max_offset_correction)
		offset_correction_coefficient = max_offset_correction;
	if (offset_correction_coefficient < least_offset_correction)
		offset_correction_coefficient = least_offset_correction;

	offset = abs((int16_t)(offset_correction_coefficient / lsb));
	ad4080_write_offset(ad4080, offset);

	return 0;
}

static int gpx_glob_io_attr_handler(struct ad4080_dev *ad4080,
				    char *buf,
				    uint32_t len,
				    bool show, 
				    enum ad4080_gpio gpio)
{
	static const char *io_str[] = {  "input", "output" };
	uint8_t config_a;
	size_t index;
	size_t input_len;
	int base = 10;
	char *endptr;
	unsigned long val;
	uint8_t mask;

	mask = 1 << gpio;
	ad4080_read(ad4080, AD4080_REG_GPIO_CONFIG_A, &config_a);
	if (show) {
		index = ((config_a & mask) == mask) ? 1 : 0;
		return sprintf(buf, "%s", *(io_str + index));

	}
	
	/* we only tolerate a "0", "1" or 
	 * their hex counterparts "0x0" or "0x1" */
	input_len = strlen(buf);
	if ((input_len > 2) && (buf[0] == '0') && (buf[1] == 'x')) {
		base = 16;
	}
	val = strtoul(buf, &endptr, base);
	/* invalid characters detected */
	if (*endptr != '\0')
		return -1;

	if ((val != 0) && (val != 1))
		return -1;

	if (val == 0) 
		config_a &= ~mask;
	else 
		config_a |= mask;
	
	ad4080_write(ad4080, AD4080_REG_GPIO_CONFIG_A, config_a);

	return 0;
}

static int gp0_io_glob_attr_handler(struct iio_ad4080_desc *iio_ad4080,
			      	    char *buf,
				    uint32_t len,
				    const struct iio_ch_info *ch_info,
				    bool show)
{
	return gpx_glob_io_attr_handler(iio_ad4080->ad4080, buf, len, show, AD4080_GPIO_0);
}

static int gp1_io_glob_attr_handler(struct iio_ad4080_desc *iio_ad4080,
			      char *buf, 
		              uint32_t len,
		              const struct iio_ch_info *ch_info,
		              bool show)
{
	return gpx_glob_io_attr_handler(iio_ad4080->ad4080, buf, len, show, AD4080_GPIO_1);
}

static int gp2_io_glob_attr_handler(struct iio_ad4080_desc *iio_ad4080,
			      char *buf, 
		              uint32_t len,
		              const struct iio_ch_info *ch_info,
		              bool show)
{
	return gpx_glob_io_attr_handler(iio_ad4080->ad4080, buf, len, show, AD4080_GPIO_2);
}

static int gp3_io_glob_attr_handler(struct iio_ad4080_desc *iio_ad4080,
			      char *buf, 
		              uint32_t len,
		              const struct iio_ch_info *ch_info,
		              bool show)
{
	return gpx_glob_io_attr_handler(iio_ad4080->ad4080, buf, len, show, AD4080_GPIO_3);
}

static int gpx_glob_func_attr_handler(struct ad4080_dev *ad4080,
				      char *buf,
				      uint32_t len,
				      bool show, 
				      enum ad4080_gpio gpio)
{
	static const char *func_str[] = { "Cfg SPI SDO", 	   /* 0 */ 
					  "FIFO Full Flag", 	   /* 1 */
					  "FIFO Read Done Flag",   /* 2 */
					  "Filter Result Ready",   /* 3 */
					  "High Threshold Detect", /* 4 */
					  "Low Threshold Detect",  /* 5 */
					  "Status Alert", 	   /* 6 */
					  "GPIO Data", 		   /* 7 */
					  "Filter Synch Input",    /* 8 */
					  "Ext Evt Trigger Input", /* 9 */
					};
	unsigned long val;
	uint8_t config;
	uint16_t reg = AD4080_REG_GPIO_CONFIG_B;
	uint16_t shift = 0;
	uint16_t mask = 0x0F;
	char *endptr;
	int base = 10;
	size_t input_len;
	
	if (gpio > AD4080_GPIO_1)
		reg = AD4080_REG_GPIO_CONFIG_C;

	if ((gpio == AD4080_GPIO_1) || (gpio == AD4080_GPIO_3)) {
		shift = 4;
		mask = 0xF0;
	}
	
	ad4080_read(ad4080, reg, &config);
	if (show) {
		config = (config >> shift) & 0xF;

		if (config < 10)
			return sprintf(buf, "%s", *(func_str + config));
		else 
			return sprintf(buf, "%s", "Invalid Function");

	}

	/* max range is 0 - 9*/
	input_len = strlen(buf);
	if ((input_len > 2) && (buf[0] == '0') && (buf[1] == 'x')) {
		base = 16;
	}
	val = strtoul(buf, &endptr, base);
	/* invalid characters detected */
	if (*endptr != '\0')
		return -1;

	if (val > 9)
		return -1;

	config &= ~mask;
	config |= (val << shift);
	
	return ad4080_write(ad4080, reg, config);
}

static int gp0_func_glob_attr_handler(struct iio_ad4080_desc *iio_ad4080,
			      char *buf, 
		              uint32_t len,
		              const struct iio_ch_info *ch_info,
		              bool show)
{
	return gpx_glob_func_attr_handler(iio_ad4080->ad4080, buf, len, show, AD4080_GPIO_0);
}

static int gp1_func_glob_attr_handler(struct iio_ad4080_desc *iio_ad4080,
			      char *buf, 
		              uint32_t len,
		              const struct iio_ch_info *ch_info,
		              bool show)
{
	return gpx_glob_func_attr_handler(iio_ad4080->ad4080, buf, len, show, AD4080_GPIO_1);
}

static int gp2_func_glob_attr_handler(struct iio_ad4080_desc *iio_ad4080,
			      char *buf, 
		              uint32_t len,
		              const struct iio_ch_info *ch_info,
		              bool show)
{
	return gpx_glob_func_attr_handler(iio_ad4080->ad4080, buf, len, show, AD4080_GPIO_2);
}

static int gp3_func_glob_attr_handler(struct iio_ad4080_desc *iio_ad4080,
			      char *buf, 
		              uint32_t len,
		              const struct iio_ch_info *ch_info,
		              bool show)
{
	return gpx_glob_func_attr_handler(iio_ad4080->ad4080, buf, len, show, AD4080_GPIO_3);
}

static int fifo_mode_glob_attr_handler(struct iio_ad4080_desc *iio_ad4080,
				       char *buf,
				       uint32_t len,
				       const struct iio_ch_info *ch_info,
				       bool show)
{
	static const char *fifo_mode[] = {
		"FIFO disabled",
		"Immediate trigger mode",
		"Evt trigger capture, read lastet watermark",
		"Evt trigger capture, read all FIFO",
	};
	enum ad4080_fifo_mode mode;
	unsigned long val;
	int base = 10;
	char *endptr;
	size_t input_len;

	ad4080_get_fifo_mode(iio_ad4080->ad4080, &mode);
	if (show) {
		return sprintf(buf, "%s", *(fifo_mode + mode));
	}

	/* max range is 0 - 3*/
	input_len = strlen(buf);
	if ((input_len > 2) && (buf[0] == '0') && (buf[1] == 'x')) {
		base = 16;
	}
	val = strtoul(buf, &endptr, base);
	/* invalid characters detected */
	if (*endptr != '\0')
		return -1;

	if (val > AD4080_EVENT_TRIGGER)
		return -1;

	/* insert new fifo mode */
	return ad4080_set_fifo_mode(iio_ad4080->ad4080, mode);
}

static int fifo_watermark_glob_attr_handler(struct iio_ad4080_desc *iio_ad4080,
					    char *buf,
					    uint32_t len,
					    const struct iio_ch_info *ch_info,
					    bool show)
{
	uint16_t watermark;
	unsigned long val;
	char *endptr;
	int base = 10;
	size_t input_len;

	if (show) {
		ad4080_get_fifo_watermark(iio_ad4080->ad4080, &watermark);
		return sprintf(buf, "%hd", watermark);
	}

	/* max range is 1 - AD4080_FIFO_DEPT */
	input_len = strlen(buf);
	if ((input_len > 2) && (buf[0] == '0') && (buf[1] == 'x')) {
		base = 16;
	}
	val = strtoul(buf, &endptr, base);
	/* invalid characters detected */
	if (*endptr != '\0')
		return -1;

	if ((val < 1 || val > AD4080_FIFO_DEPTH))
		return -1;

	return ad4080_set_fifo_watermark(iio_ad4080->ad4080, (uint16_t)val);
}

static uint16_t ad4080_read_hysteresis(struct ad4080_dev *ad4080)
{
	uint16_t hyst = 0;
	uint8_t tmp;
	if (ad4080) {
		ad4080_read(ad4080, AD4080_REG_EVENT_HYSTERESIS + 1, &tmp);
		hyst = tmp << 8;

		ad4080_read(ad4080, AD4080_REG_EVENT_HYSTERESIS, &tmp);
		hyst = hyst | (tmp & 0xFF);
	}
	return hyst & 0x3FF;
}

static void ad4080_write_hysteresis(struct ad4080_dev *ad4080, uint16_t val)
{
	uint8_t tmp;
	val = val & 0x3FF; /* zero out the upper 5 bits */
	if (ad4080) {
		tmp = val >> 8;
		ad4080_write(ad4080, AD4080_REG_EVENT_HYSTERESIS + 1, tmp);

		tmp = val & 0xFF;
		ad4080_write(ad4080, AD4080_REG_EVENT_HYSTERESIS, tmp);
	}
	return;
}

static int evt_detection_hysteresis_glob_attr_handler(struct iio_ad4080_desc *iio_ad4080,
					    	      char *buf,
						      uint32_t len,
						      const struct iio_ch_info *ch_info,
						      bool show)
{
	double hyst;
	const double lsb = 1.46484;
	const double max_hyst = 0x3FF * lsb; 
	uint16_t val;
	
	if (show) {
		hyst = (double)ad4080_read_hysteresis(iio_ad4080->ad4080);
		hyst = hyst * lsb;				
		return sprintf(buf, "%10f", hyst);
	}

	/* hysteresis in mV, clip value to maximum possible mV only */
	hyst = strtof(buf, NULL);
	if (hyst > max_hyst)
		hyst = max_hyst;
	val = (uint16_t)(hyst / lsb);
	ad4080_write_hysteresis(iio_ad4080->ad4080, val);
	return 0;
}



static int16_t ad4080_read_evt_detection(struct ad4080_dev *ad4080, bool hi)
{
	uint16_t evt_detection_reg = AD4080_REG_EVENT_DETECTION_HI;
	int16_t reg_val;
	
	if (!ad4080)
		return 0;

	if (!hi)
		evt_detection_reg = AD4080_REG_EVENT_DETECTION_LO;
	
	reg_val = ad4080_read16(ad4080, evt_detection_reg);

	/* sign extend if we need to */
	return reg_val & 0x800 ? -reg_val : reg_val;
}

static void ad4080_write_evt_detection(struct ad4080_dev *ad4080, uint16_t reg_val,
		bool hi)
{
	uint16_t evt_detection_reg = AD4080_REG_EVENT_DETECTION_HI;
	if (!ad4080)
		return;
	if (!hi)
		evt_detection_reg = AD4080_REG_EVENT_DETECTION_LO;
	ad4080_write16(ad4080, evt_detection_reg, reg_val);
	return ;
}

static int evt_detection_hi_glob_attr_handler(struct iio_ad4080_desc *iio_ad4080,
					      char *buf,
					      uint32_t len,
					      const struct iio_ch_info *ch_info,
					      bool show)
{
	int16_t reg_val;
	const double lsb = 1.46484;
	const double least_value = -2048.0l * lsb;
	const double max_value = 2047.0l * lsb;
	double evt_detection_hi;
	if (show) {
		reg_val = ad4080_read_evt_detection(iio_ad4080->ad4080, true);
		evt_detection_hi = reg_val * lsb;
		return sprintf(buf, "%10f", evt_detection_hi);
	}

	evt_detection_hi = strtof(buf, NULL);
	if (evt_detection_hi > max_value)
		evt_detection_hi = max_value;
	else if (evt_detection_hi < least_value)
		evt_detection_hi = least_value;
	
	reg_val = abs((int16_t)(evt_detection_hi / lsb));
	ad4080_write_evt_detection(iio_ad4080->ad4080, reg_val, true);
	return 0;
}

static int evt_detection_lo_glob_attr_handler(struct iio_ad4080_desc *iio_ad4080,
					      char *buf,
					      uint32_t len,
					      const struct iio_ch_info *ch_info,
					      bool show)
{
	int16_t reg_val;
	const double lsb = 1.46484;
	const double least_value = -2048.0l * lsb;
	const double max_value = 2047.0l * lsb;
	double evt_detection_lo;
	if (show) {
		reg_val = ad4080_read_evt_detection(iio_ad4080->ad4080, false);
		evt_detection_lo = reg_val * lsb;
		return sprintf(buf, "%10f", evt_detection_lo);
	}

	evt_detection_lo = strtof(buf, NULL);
	if (evt_detection_lo > max_value)
		evt_detection_lo = max_value;
	else if (evt_detection_lo < least_value)
		evt_detection_lo = least_value;
	
	reg_val = abs((int16_t)(evt_detection_lo / lsb));
	ad4080_write_evt_detection(iio_ad4080->ad4080, reg_val, false);
	return 0;
}

static int filter_sel_glob_attr_handler(struct iio_ad4080_desc *iio_ad4080,
					char *buf,
					uint32_t len,
					const struct iio_ch_info *ch_info,
					bool show)
{
	uint8_t reg_val;
	uint8_t mask = 0b11;
	static const char *filter_select[] = {
		"Disabled",
		"Sinc1",
		"Sinc5",
		"Sinc5 Compensation"
	};
	long filter;
	char *endptr;
	
	ad4080_read(iio_ad4080->ad4080, AD4080_REG_FILTER_CONFIG, &reg_val);
	if (show) {
		reg_val &= mask;
		return sprintf(buf, "%s", filter_select[reg_val]);
		
	}

	filter = strtol(buf, &endptr, 10);
	if (*endptr != '\0')
		return -1;

	if (filter > 3)
		return -1;

	reg_val &= ~mask;
	reg_val |= filter;
	return ad4080_write(iio_ad4080->ad4080, AD4080_REG_FILTER_CONFIG, reg_val);
}

static int filter_sinc_dec_glob_attr_handler(struct iio_ad4080_desc *iio_ad4080,
					     char *buf,
					     uint32_t len,
					     const struct iio_ch_info *ch_info,
					     bool show)
{
	static const char *decimation_factor[] = {
		"2",
		"4",
		"8",
		"16",
		"32",
		"64",
		"128",
		"256",
		"512",
		"1024",
	};
	uint8_t reg_val;
	uint8_t mask = 0b01111000;
	uint8_t shift = 3;
	long sinc_dec;
	char *endptr;

	ad4080_read(iio_ad4080->ad4080, AD4080_REG_FILTER_CONFIG, &reg_val);				
	if (show) {
		reg_val &= mask;
		reg_val >>= shift;
		return sprintf(buf, "%s", decimation_factor[reg_val]);
	}

	sinc_dec = strtol(buf, &endptr, 10);
	if (*endptr != '\0')
		return -1;

	if (sinc_dec > 9)
		return -1;

	reg_val &= ~mask;
	reg_val |= (sinc_dec << shift);
	return ad4080_write(iio_ad4080->ad4080, AD4080_REG_FILTER_CONFIG, reg_val);
}

static int device_mode_glob_attr_handler(struct iio_ad4080_desc *iio_ad4080,
					 char *buf,
					 uint32_t len,
					 const struct iio_ch_info *ch_info,
					 bool show)
{
	uint8_t reg_val;
	uint8_t mask = 0b11;
	static const char *operating_modes[] = {
		"Normal",
		"unknown", /* 1 is underfined operating mode */
		"Standby",
		"Sleep",
	};
	char *endptr;
	uint8_t opmode;

	ad4080_read(iio_ad4080->ad4080, AD4080_REG_DEVICE_CONFIG, &reg_val);
	if (show) {
		reg_val &= mask;
		return sprintf(buf, "%s", operating_modes[reg_val]);
	}

	opmode = strtol(buf, &endptr, 10);
	if (*endptr != '\0')
		return -1;

	if ((opmode == 1) || (opmode > 3))
		return -1;

	reg_val &= ~mask;
	reg_val |= opmode;
	ad4080_write(iio_ad4080->ad4080, AD4080_REG_DEVICE_CONFIG, reg_val);
	return 0;
}

static attr_fn attr_handlers[] = {
	raw_attr_handler,
	scale_attr_handler,
	offset_attr_handler,
	gp0_io_glob_attr_handler,
	gp0_func_glob_attr_handler,
	gp1_io_glob_attr_handler,
	gp1_func_glob_attr_handler,
	gp2_io_glob_attr_handler,
	gp2_func_glob_attr_handler,
	gp3_io_glob_attr_handler,
	gp3_func_glob_attr_handler,
	fifo_mode_glob_attr_handler,
	fifo_watermark_glob_attr_handler,
	evt_detection_hysteresis_glob_attr_handler,
	evt_detection_hi_glob_attr_handler,
	evt_detection_lo_glob_attr_handler,
	filter_sel_glob_attr_handler,
	filter_sinc_dec_glob_attr_handler,
	device_mode_glob_attr_handler,
};

static int ad4080_attr_store(void *device,
			     char *buf,
			     uint32_t len,
			     const struct iio_ch_info *ch_info,
			     intptr_t priv)
{
	int err;
	struct iio_ad4080_desc *iio_ad4080;
	iio_ad4080 = ad4080_privdata(device);
	if (priv < MAX_ATTR_ID)
		err = attr_handlers[priv](iio_ad4080, buf, len, ch_info, false);
	return err;
}

static int ad4080_attr_show(void *device,
			    char *buf,
			    uint32_t len,
			    const struct iio_ch_info *ch_info,
			    intptr_t priv)
{
	int err;
	struct iio_ad4080_desc *iio_ad4080;
	iio_ad4080 = ad4080_privdata(device);
	if (priv < MAX_ATTR_ID)
		err = attr_handlers[priv](iio_ad4080, buf, len, ch_info, true);
	return err;
}

static int32_t iio_ad4080_prepare_transfer(void *dev, uint32_t mask)
{
	return 0;
}

static int32_t iio_ad4080_end_transfer(void *dev)
{
	return 0;
}

/* some clarifying points here. 
 * 1) 1 channel (zero possibility to for any interleave to happen)
 * 2) 20-bit is extended to 32-bits (4 bytes)
 * 3) Raw fifo data needs to be formatted before it's usable. 
 *    3.1) as simple as to just remove the 0xAA synchro starting byte
 * 4) buffer is passed by the upper layer as a circular buffer.
 *    4.1) cannot be memcpy'd directly.
 *    4.2) need to use no os circular buffer facilities to transfer data 
 */
static int32_t ad4080_submit(struct iio_device_data *iio_device_data)
{
	struct ad4080_dev *ad4080 = iio_device_data->dev;
	struct iio_ad4080_desc *iio_ad4080 = ad4080_privdata(ad4080);
	struct iio_ad4080_fifo_struct *fifo = &iio_ad4080->fifo;
	uint32_t samples = iio_device_data->buffer->samples;
	int err;

	iio_device_data->buffer->buf->size = iio_device_data->buffer->size;
	
	/* see if we need to update our watermark based on sample size 
	 * request from host */
	if (fifo->watermark != samples) {
		int err;
		iio_ad4080_fifo_unset_watermark(fifo);
		err = iio_ad4080_fifo_set_watermark(fifo, samples);
		if (err)
			return err;
	}

	iio_ad4080_immediate_trigger(iio_ad4080);
	err = no_os_cb_write(iio_device_data->buffer->buf, 
			     fifo->formatted_fifo,
			     iio_device_data->buffer->size);
	if (err)
		return err;
	return 0;
}

#define IIO_AD4080_CH_ATTR(_name, _priv) 	\
	{ 					\
		.name = _name, 			\
		.priv = _priv, 			\
		.show = ad4080_attr_show, 	\
		.store = ad4080_attr_store, 	\
	}

#define IIO_AD4080_GLOB_ATTR(_name, _priv) 	\
	{ 					\
		.name = _name, 			\
		.priv = _priv, 			\
		.show = ad4080_attr_show, 	\
		.store = ad4080_attr_store, 	\
	}

static struct iio_attribute ad4080_ch_attr[] = {
	IIO_AD4080_CH_ATTR("raw", RAW_ATTR_ID),
	IIO_AD4080_CH_ATTR("scale", SCALE_ATTR_ID),
	IIO_AD4080_CH_ATTR("offset", OFFSET_ATTR_ID),
	{0},
};

static struct iio_attribute ad4080_global_attr[] = {
	IIO_AD4080_GLOB_ATTR("gp0_output_enable", GP0_IO_GLOB_ATTR_ID),
	IIO_AD4080_GLOB_ATTR("gp0_func", GP0_FUNC_GLOB_ATTR_ID),
	IIO_AD4080_GLOB_ATTR("gp1_output_enable", GP1_IO_GLOB_ATTR_ID),
	IIO_AD4080_GLOB_ATTR("gp1_func", GP1_FUNC_GLOB_ATTR_ID),
	IIO_AD4080_GLOB_ATTR("gp2_output_enable", GP2_IO_GLOB_ATTR_ID),
	IIO_AD4080_GLOB_ATTR("gp2_func", GP2_FUNC_GLOB_ATTR_ID),
	IIO_AD4080_GLOB_ATTR("gp3_output_enable", GP3_IO_GLOB_ATTR_ID),
	IIO_AD4080_GLOB_ATTR("gp3_func", GP3_FUNC_GLOB_ATTR_ID),
	IIO_AD4080_GLOB_ATTR("fifo_mode", FIFO_MODE_GLOB_ATTR_ID),
	IIO_AD4080_GLOB_ATTR("fifo_watermark", FIFO_WATERMARK_GLOB_ATTR_ID),
	IIO_AD4080_GLOB_ATTR("evt_detection_hysteresis", EVT_DETECTION_HYSTERESIS_GLOB_ATTR_ID),
	IIO_AD4080_GLOB_ATTR("evt_detection_high", EVT_DETECTION_HI_GLOB_ATTR_ID),
	IIO_AD4080_GLOB_ATTR("evt_detection_low", EVT_DETECTION_LO_GLOB_ATTR_ID),
	IIO_AD4080_GLOB_ATTR("filter_select", FILTER_SEL_GLOB_ATTR_ID),
	IIO_AD4080_GLOB_ATTR("filter_sinc_dec", FILTER_SINC_DEC_RATE_GLOB_ATTR_ID),
	IIO_AD4080_GLOB_ATTR("device_mode", DEVICE_MODE_GLOB_ATTR_ID),
	{0},
};

static struct scan_type ad4080_scan_type = {
	.sign = 's',
	.realbits = AD4080_ADC_GRANULARITY,
	.storagebits = 32, /* round up to 4 bytes */
	.shift = 0,
	.is_big_endian = false
};

static struct iio_channel ad4080_ch = {
	.name = "voltage",
	.ch_type = IIO_VOLTAGE,
	.channel = 0,
	.scan_index = 0,
	.indexed = true,
	.scan_type = &ad4080_scan_type,
	.attributes = ad4080_ch_attr,
	.ch_out = false,
};

static void iio_ad4080_fifo_full_handler(void *isr_data)
{
	int err;
	struct iio_ad4080_desc *iio_ad4080 = isr_data;

	err = iio_ad4080_read_data(iio_ad4080);

	/* mark our operation done so waiter can proceed */
	complete(iio_ad4080->ff_full_completion, err);

	return;
}

const struct iio_app_device *iio_ad4080_get_device_descriptors(struct iio_ad4080_desc *iio_ad4080,
							       uint32_t *app_device_count)
{
	if (!iio_ad4080 || !app_device_count)
		return NULL;
	*app_device_count = 1;
	return &iio_ad4080->app_device;
}

int ad4080_device(struct iio_ad4080_desc *iio_ad4080,
		  struct ad4080_dev **ad4080)
{
	if (!iio_ad4080 || !ad4080)
		return -EINVAL;
	*ad4080 = iio_ad4080->ad4080;
	return 0;
}

int ad4080_iio_device(struct iio_ad4080_desc *iio_ad4080,
		      struct iio_device *iio_device)
{
	if (!iio_ad4080 || !iio_device)
		return -EINVAL;

	iio_device->num_ch = 1;
	iio_device->channels = &ad4080_ch;
	iio_device->attributes = ad4080_global_attr;
	iio_device->debug_attributes = NULL;
	iio_device->buffer_attributes = NULL;
	iio_device->pre_enable = iio_ad4080_prepare_transfer;
	iio_device->post_disable = iio_ad4080_end_transfer;
	iio_device->submit = ad4080_submit;
	iio_device->debug_reg_read = ad4080_reg_read;
	iio_device->debug_reg_write = ad4080_reg_write;
	
	return 0;
}

int iio_ad4080_init(struct iio_ad4080_desc **iio_ad4080,
		    struct iio_ad4080_init_param *iio_ad4080_init_param)
{
	int err;
	struct ad4080_dev *ad4080;
	struct ad4080_init_param *ad4080_init_param;
	struct iio_ad4080_desc *ad4080_iio;
	struct iio_ad4080_fifo_struct *fifo;

	if (!iio_ad4080)
		return -EINVAL;

	if (!iio_ad4080_init_param)
		return -EINVAL;

	ad4080_init_param = iio_ad4080_init_param->ad4080_init_param;

	ad4080_init_param->privdata_len = sizeof(struct iio_ad4080_desc) + AD4080_ADC_DATA_BUFFER_LEN;
	/* iio ad4080 exposes just 1 iio_app_device */
	ad4080_init_param->privdata_len += (1 * sizeof(struct iio_app_device));
	err = ad4080_init(&ad4080, *ad4080_init_param);
	if (err)
		return err;

	ad4080_iio = ad4080_privdata(ad4080);
	ad4080_iio->ad4080 = ad4080;
	fifo = &ad4080_iio->fifo;
	err = iio_ad4080_fifo_init(fifo, ad4080);
	if (err)
		goto err_iio_ad4080_fifo_init;

	err = iio_ad4080_fifo_set_watermark(fifo, iio_ad4080_init_param->watermark);
	if (err)
		goto err_set_watermark;

	err = iio_ad4080_fifo_register_irq(fifo,
					   iio_ad4080_init_param->ff_full_init_param,
					   iio_ad4080_init_param->gpio_irq_platform_ops,
					   iio_ad4080_init_param->i_gp,
					   iio_ad4080_fifo_full_handler,
					   ad4080_iio);
	if (err)
		goto err_register_irq;
	
	*iio_ad4080 = ad4080_iio;

	return 0;

err_register_irq:
	iio_ad4080_fifo_unset_watermark(fifo);
err_set_watermark:
	iio_ad4080_fifo_fini(fifo);
err_iio_ad4080_fifo_init:
	ad4080_remove(ad4080);
	return err;
}

void iio_ad4080_fini(struct iio_ad4080_desc *iio_ad4080)
{
	struct ad4080_dev *ad4080;
	struct iio_ad4080_fifo_struct *fifo;

	if (!iio_ad4080)
		return;

	ad4080 = iio_ad4080->ad4080;
	fifo = &iio_ad4080->fifo;
	
	iio_ad4080_fifo_unset_watermark(fifo);
	iio_ad4080_fifo_fini(fifo);
	ad4080_remove(ad4080);
	return;
}


/* AD4080 FIFO API */

static bool is_iio_ad4080_fifo(struct iio_ad4080_fifo_struct *fifo)
{
	int err;
	err = memcmp(fifo->signature, IIO_AD4080_FIFO_SIGNATURE, IIO_AD4080_FIFO_SIGNATURE_LEN);
	return err == 0 ? true : false;
}

static void fifo_irq_handler(void *context)
{
	struct iio_ad4080_fifo_struct *fifo = context;
	struct ad4080_dev *ad4080;
	uint8_t status;
	assert(is_iio_ad4080_fifo(fifo) == true);
	ad4080 = fifo->ad4080;

	/* check if spurious FIFO Full */
	ad4080_read(ad4080, AD4080_REG_DEVICE_STATUS, &status);
	if (status & (1 << 7)) {
		if (fifo->isr)
			fifo->isr(fifo->isr_data);

		/* turn off the sampling */
		ad4080_set_fifo_mode(ad4080, AD4080_FIFO_DISABLE);
	}

	return;
}

int iio_ad4080_fifo_init(struct iio_ad4080_fifo_struct *fifo, 
			 struct ad4080_dev *ad4080)
{
	int err = -EINVAL;
	if (fifo) {
		memcpy(fifo->signature, IIO_AD4080_FIFO_SIGNATURE, IIO_AD4080_FIFO_SIGNATURE_LEN);
		fifo->ad4080 = ad4080;
		fifo->raw_fifo = NULL;
		fifo->bufsize = 0;
		fifo->formatted_fifo = NULL;
		fifo->formatted_bufsize = 0;
		err = 0;
	}
	return err;
}

void iio_ad4080_fifo_fini(struct iio_ad4080_fifo_struct *fifo)
{
	if (fifo) {
		memset(fifo->signature, 0, IIO_AD4080_FIFO_SIGNATURE_LEN);
		fifo->ad4080 = NULL;
	}
	return;
}

int iio_ad4080_fifo_register_irq(struct iio_ad4080_fifo_struct *fifo, 
		struct no_os_gpio_init_param *gpio_init_param,
		struct no_os_irq_platform_ops *gpio_irq_platform_ops,
		const size_t i_gp,
		iio_ad4080_fifo_isr isr,
		void *isr_data)
{
	int err;
	
	if (fifo == NULL)
		return -EINVAL;

	if (gpio_init_param == NULL)
		return -EINVAL;

	if (i_gp > AD4080_GPIO_3)
		return -EINVAL;

	if (isr == NULL)
		return -EINVAL;

	if (gpio_irq_platform_ops == NULL)
		return -EINVAL;

	if (!is_iio_ad4080_fifo(fifo)) {
		return -EINVAL;
	}

	/* configure the GPIO line */
	err = no_os_gpio_get(&fifo->ff_full, gpio_init_param);
	if (err)
		return err;

	err = no_os_gpio_direction_input(fifo->ff_full);
	if (err)
		goto err_gpio_direction_input;

	/* configure the GPIO interrupt */
	struct no_os_irq_init_param irq_init_param = {
		.platform_ops = gpio_irq_platform_ops,
	};
	err = no_os_irq_ctrl_init(&fifo->irq_desc, &irq_init_param);
	if (err)
		goto err_gpio_direction_input;
	
	/* register the callback */
	struct no_os_callback_desc fifo_full_cb = {
		.callback = fifo_irq_handler,
		.ctx = fifo,
		.event = NO_OS_EVT_GPIO,
		.peripheral = NO_OS_GPIO_IRQ,
	};
	err = no_os_irq_register_callback(fifo->irq_desc, 
					  fifo->ff_full->number, 
					  &fifo_full_cb);
	if (err)
		goto err_irq_register;

	/* set trigger level */
	err = no_os_irq_trigger_level_set(fifo->irq_desc, 
					  fifo->ff_full->number,
					  NO_OS_IRQ_LEVEL_HIGH);
	if (err)
		goto err_irq_trigger_level;

	/* enable the GPIO interrupt */
	err = no_os_irq_enable(fifo->irq_desc, fifo->ff_full->number);
	if (err)
		goto err_irq_trigger_level;

	/* now we can configure the FIFO full on the AD4080 side */
	err = ad4080_set_gpio_output_enable(fifo->ad4080,
					    i_gp,
					    AD4080_GPIO_OUTPUT);
	if (err)
		goto err_output_enable;

	err = ad4080_set_gpio_output_func(fifo->ad4080, i_gp,
					  AD4080_GPIO_FIFO_FULL);
	if (err)
		goto err_output_enable;

	fifo->i_gp = i_gp;
	fifo->isr = isr;
	fifo->isr_data = isr_data;

	return 0;

err_output_enable:
	no_os_irq_disable(fifo->irq_desc, fifo->ff_full->number);
err_irq_trigger_level:
	no_os_irq_unregister_callback(fifo->irq_desc,
				      fifo->ff_full->number,
				      &fifo_full_cb);
err_irq_register:
	no_os_irq_ctrl_remove(fifo->irq_desc);
err_gpio_direction_input:
	no_os_gpio_remove(fifo->ff_full);
	fifo->ff_full = NULL;
	return err;
}

void iio_ad4080_fifo_unregister_irq(struct iio_ad4080_fifo_struct *fifo,
		const size_t i_gp)
{
	/* disable the FIFO full on the AD4080 side */
	ad4080_set_gpio_output_func(fifo->ad4080, i_gp,
			AD4080_GPIO_ADI_NSPI_SDO_DATA);

	/* disable the IRQ */
	no_os_irq_disable(fifo->irq_desc, fifo->ff_full->number);

	/* unregister the callback */
	struct no_os_callback_desc fifo_full_cb = {
		.callback = fifo_irq_handler,
		.ctx = fifo,
		.event = NO_OS_EVT_GPIO,
		.peripheral = NO_OS_GPIO_IRQ,
	};
	no_os_irq_unregister_callback(fifo->irq_desc,
				      fifo->ff_full->number,
				      &fifo_full_cb);
	
	/* remove the gpio irq desc */
	no_os_irq_ctrl_remove(fifo->irq_desc);
	fifo->irq_desc = NULL;

	/* remove the GPIO line */
	no_os_gpio_remove(fifo->ff_full);
	fifo->ff_full = NULL;

	return;
}

/* set the watermark on the AD4080. 
 * if a raw fifo buffer is not available, create it.
 * if one already exists, then resize it to fit the new watermark level.
 */
int iio_ad4080_fifo_set_watermark(struct iio_ad4080_fifo_struct *fifo,
		const size_t watermark)
{
	int err;
	size_t fifo_size;
	size_t formatted_bufsize;
	uint8_t *raw_fifo;
	uint32_t *formatted_fifo;

	if (fifo == NULL)
		return -EINVAL;

	if (!is_iio_ad4080_fifo(fifo)) {
		return -EINVAL;
	}

	if ((watermark < 1) || (watermark > 16384)) {
		return -EINVAL;
	}

	iio_ad4080_fifo_unset_watermark(fifo);

	fifo_size = NO_OS_DIV_ROUND_UP(AD4080_ADC_GRANULARITY, 8);
	fifo_size = (fifo_size * watermark);
	fifo_size = fifo_size + 1; /* account for the 0xAA synchro byte */

	raw_fifo = no_os_malloc(fifo_size);
	if (!raw_fifo) {
		return -ENOMEM;
	}
	fifo->raw_fifo = raw_fifo;
	fifo->watermark = watermark;
	fifo->bufsize = fifo_size;

	formatted_bufsize = watermark * sizeof(uint32_t);
	formatted_fifo = no_os_malloc(formatted_bufsize);
	if (!formatted_fifo)
		goto err_malloc_formatted_fifo;
	fifo->formatted_fifo = formatted_fifo;
	fifo->formatted_bufsize = formatted_bufsize;

	err = ad4080_set_fifo_watermark(fifo->ad4080, fifo->watermark);
	if (err) {
		goto err_set_fifo_watermark;
	}

	return 0;
err_set_fifo_watermark:
	no_os_free(formatted_fifo);
err_malloc_formatted_fifo:
	no_os_free(raw_fifo);
	return err;
}

void iio_ad4080_fifo_unset_watermark(struct iio_ad4080_fifo_struct *fifo)
{
	/* turn off the fifo */
	ad4080_set_fifo_mode(fifo->ad4080,
			AD4080_FIFO_DISABLE);

	if (fifo->formatted_fifo) {
		no_os_free(fifo->formatted_fifo);
		fifo->formatted_fifo = NULL;
		fifo->formatted_bufsize = 0;
	}

	if (fifo->raw_fifo) {
		no_os_free(fifo->raw_fifo);
		fifo->raw_fifo = NULL;
		fifo->bufsize = 0;
	}

	return;
}

