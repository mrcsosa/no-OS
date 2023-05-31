#include <string.h>
#include "no_os_delay.h"
#include "no_os_print_log.h"
#include "no_os_util.h"
#include "no_os_rtc.h"
#include "no_os_crc8.h"
#include "parameters.h"
#include "hmc630x.h"
#include "iio_hmc630x.h"
#include "mwc.h"
#include "iio_app.h"
#include "led.h"
#include "net.h"

volatile bool heartbeat_pulse;

static int mwc_step(void *arg)
{
	uint8_t lock;
	struct mwc_iio_dev *mwc = arg;
	if (!heartbeat_pulse)
		return 0;

	lock = 0;
	hmc630x_read(mwc->tx_iiodev->dev, HMC630X_LOCKDET, &lock);
	led_tx_lock(lock);
	
	lock = 0;
	hmc630x_read(mwc->rx_iiodev->dev, HMC630X_LOCKDET, &lock);
	led_rx_lock(lock);

	mwc_algorithms(mwc);

	heartbeat_pulse = false;
	return 0;
}

void heartbeat(void *context)
{
	heartbeat_pulse = true;
	no_os_rtc_set_cnt(context, 0);
}

static int heartbeat_prepare(void)
{
	int ret;
	struct no_os_rtc_init_param rtcip = {
		.id = 0,
	};

	struct no_os_rtc_desc *rtc;
	ret = no_os_rtc_init(&rtc, &rtcip);
	if (ret)
		return ret;

	ret = no_os_rtc_set_irq_time(rtc, 1);
	if (ret)
		return ret;

	struct no_os_irq_ctrl_desc *nvic;
	struct no_os_irq_init_param nvic_param = {
		.irq_ctrl_id = 0,
		.platform_ops = &max_irq_ops,
	};
	ret = no_os_irq_ctrl_init(&nvic, &nvic_param);


	struct no_os_callback_desc rtc_cb = {
		.callback = heartbeat,
		.event = NO_OS_EVT_RTC,
		.peripheral = NO_OS_RTC_IRQ,
		.ctx = rtc,
	};
	ret = no_os_irq_register_callback(nvic, RTC_IRQn, &rtc_cb);
	if (ret)
		return ret;
	ret = no_os_irq_set_priority(nvic, RTC_IRQn, 1);
	if (ret)
		return ret;
	ret = no_os_irq_enable(nvic, RTC_IRQn);
	if (ret)
		return ret;
	ret = no_os_rtc_start(rtc);
	if (ret)
		return ret;

	return 0;
}

#define NO_OS_STRINGIFY(x) #x
#define NO_OS_TOSTRING(x) NO_OS_STRINGIFY(x)

int main(void)
{
	int ret;
	struct no_os_uart_desc *console;
	char hw_model_str[10];
	enum admv96xx_id id = ID_ADMV96X5;
	int speed;
	uint8_t hbtx;
	struct no_os_gpio_desc *brd_select;
	struct no_os_eeprom_desc *eeprom;
	const uint16_t nvmpsz = sizeof(struct nvmp);
	uint8_t eebuf[nvmpsz + 1];
	struct nvmp *nvmp;
	struct adin1300_iio_desc *iio_adin1300;
	struct max24287_iio_desc *iio_max24287;
	NO_OS_DECLARE_CRC8_TABLE(crc8);
	
	no_os_crc8_populate_msb(crc8, 0x7);

	// Greeting
	ret = no_os_uart_init(&console, &uart_console_ip);
	if (ret)
		return ret;
	no_os_uart_stdio(console);
	printf("\nwethlink-firmware %s\n", NO_OS_TOSTRING(NO_OS_VERSION));

	// Detect board type switch state
	ret = no_os_gpio_get(&brd_select, &brd_select_gpio_ip);
	if (ret)
		goto end;
	ret = no_os_gpio_direction_input(brd_select);
	if (ret)
		goto end;
	ret = no_os_gpio_get_value(brd_select, &hbtx);
	if (ret)
		goto end;

	sprintf(hw_model_str, "admv96%d%d", hbtx ? 1 : 2, id);

	printf("Board: %s\n", hw_model_str);

	ret = led_init();
	if (ret)
		goto end;
	
	ret = no_os_eeprom_init(&eeprom, &eeprom_ip);
	if (ret)
		goto end;
	
	ret = no_os_eeprom_read(eeprom, 0, eebuf, nvmpsz+1);
	if (ret)
		return ret;
	
	uint8_t crc = no_os_crc8(crc8, eebuf, nvmpsz, 0xa5);
	if (crc != eebuf[nvmpsz]) {
		printf("EEPROM: CRC mismatch, read 0x%x, computed 0x%x\n", eebuf[nvmpsz], crc);

		memcpy(eebuf, &factory_defaults, nvmpsz);
		eebuf[nvmpsz] = no_os_crc8(crc8, eebuf, nvmpsz, 0xa5);
		ret = no_os_eeprom_write(eeprom, 0, eebuf, nvmpsz+1);
		if (ret)
			return ret;

		ret = no_os_eeprom_read(eeprom, 0, eebuf, nvmpsz+1);
		if (ret)
			return ret;
		
		crc = no_os_crc8(crc8, eebuf, nvmpsz, 0xa5);
		if (crc != eebuf[nvmpsz]) {
			printf("EEPROM: failed to store factory defaults.\n");
			return -EFAULT;
		}

		printf("EEPROM: stored factory defaults.\n");
	}

	nvmp = (struct nvmp *)eebuf;

	struct mwc_iio_dev *mwc;
	struct mwc_iio_init_param mwc_ip = {
		.reset_gpio_ip = &xcvr_reset_gpio_ip,
		.tx_autotuning = nvmp->tx_autotuning,
		.tx_target = nvmp->tx_target,
		.tx_tolerance = nvmp->tx_tolerance,
		.rx_autotuning = nvmp->rx_autotuning,
		.rx_target = nvmp->rx_target,
		.rx_tolerance = nvmp->rx_tolerance,
		.tx_auto_ifvga = nvmp->tx_auto_ifvga,
		.rx_auto_ifvga_rflna = nvmp->rx_auto_ifvga_rflna,
		.temp_correlation = &nvmp->temp_correlation[hbtx],
		.id = id,
		.hbtx = hbtx,
		.crc8 = crc8,
		.eeprom = eeprom,
	};
	ret = mwc_iio_init(&mwc, &mwc_ip);
	if (ret)
		goto end;

	ret = mwc_tx_rx_reset(mwc);
	if (ret)
		goto end;

	struct hmc630x_iio_dev *iio_tx;
	struct hmc630x_init_param txip = {0};
	txip.type = HMC6300;
	txip.ref_clk = HMC6300_REF_CLK_75MHz;
	txip.en = xcvr_en_gpio_ip;
	txip.clk = xcvr_clk_gpio_ip;
	txip.data = xcvr_data_gpio_ip;
	txip.scanout = xcvr_scanout_tx_gpio_ip;
	txip.vco = nvmp->hmc6300_vco[hbtx];
	txip.enabled = nvmp->hmc6300_enabled;
	txip.temp_en = true;
	txip.if_attn = nvmp->hmc6300_if_attn;
	txip.tx.rf_attn = nvmp->hmc6300_rf_attn;
	struct hmc630x_iio_init_param iio_txip = {
		.ip = &txip,
	};
	ret = hmc630x_iio_init(&iio_tx, &iio_txip);
	if (ret)
		goto end;

	if (id == ID_ADMV96X5) {
		ret = hmc630x_write(iio_tx->dev, HMC6300_PA_SEL_VREF, 0x8);
		if (ret)
			goto end;
		ret = hmc630x_write(iio_tx->dev, HMC6300_PA_PWRDWN_FAST, 0);
		if (ret)
			goto end;
		ret = hmc630x_write(iio_tx->dev, HMC6300_PA_SE_SEL, 0);
		if (ret)
			goto end;
	}

	struct hmc630x_iio_dev *iio_rx;
	struct hmc630x_init_param rxip = {0};
	rxip.type = HMC6301;
	rxip.ref_clk = HMC6300_REF_CLK_75MHz;
	rxip.en = xcvr_en_gpio_ip;
	rxip.clk = xcvr_clk_gpio_ip;
	rxip.data = xcvr_data_gpio_ip;
	rxip.scanout = xcvr_scanout_rx_gpio_ip;
	rxip.vco = nvmp->hmc6301_vco[hbtx];
	rxip.enabled = nvmp->hmc6301_enabled;
	rxip.temp_en = true;
	rxip.if_attn = nvmp->hmc6301_if_attn;
	rxip.rx.bb_attn1 = nvmp->hmc6301_bb_attn1;
	rxip.rx.bb_attn2 = nvmp->hmc6301_bb_attn2;
	rxip.rx.bb_attni_fine = nvmp->hmc6301_bb_attni_fine;
	rxip.rx.bb_attnq_fine = nvmp->hmc6301_bb_attnq_fine;
	rxip.rx.lna_attn = nvmp->hmc6301_lna_attn;
	rxip.rx.bb_lpc = HMC6301_BB_LPC_1400MHz;
	rxip.rx.bb_hpc = HMC6301_BB_HPC_45kHz;
	struct hmc630x_iio_init_param iio_rxip = {
		.ip = &rxip,
	};
	ret = hmc630x_iio_init(&iio_rx, &iio_rxip);
	if (ret)
		goto end;

	// TODO: do this in a better way.
	mwc->tx_iiodev = iio_tx;
	mwc->rx_iiodev = iio_rx;

	ret = heartbeat_prepare();
	if (ret)
		goto end;

	switch(id) {
	case ID_ADMV96X1:
		speed = 100;
		break;
	case ID_ADMV96X3:
		speed = 100;
		break;
	default:
	case ID_ADMV96X5:
		speed = 1000;
		break;
	};

	ret = net_init(&iio_adin1300, &iio_max24287, speed);
	if (ret)
		goto end;

	struct iio_app_device iio_devices[] = {
		{
			.name = "hmc6300",
			.dev = iio_tx,
			.dev_descriptor = iio_tx->iio_dev,
		},
		{
			.name = "hmc6301",
			.dev = iio_rx,
			.dev_descriptor = iio_rx->iio_dev,
		},
		{
			.name = "mwc",
			.dev = mwc,
			.dev_descriptor = mwc->iio_dev,
		},
		{
			.name = "adin1300",
			.dev = iio_adin1300,
			.dev_descriptor = iio_adin1300->iio_dev,
		},
		{
			.name = "max24287",
			.dev = iio_max24287,
			.dev_descriptor = iio_max24287->iio_dev,
		}
	};

	struct iio_ctx_attr iio_ctx_attrs[] = {
		{
			.name = "hw_model",
			.value = hw_model_str,
		},
		{
			.name = "hw_version",
			.value = nvmp->hw_version
		},
		{
			.name = "hw_serial",
			.value = nvmp->hw_serial
		},
		{
			.name = "carrier_model",
			.value = nvmp->carrier_model
		},
		{
			.name = "carrier_version",
			.value = nvmp->carrier_version
		},
		{
			.name = "carrier_serial",
			.value = nvmp->carrier_serial
		},
	};

	struct iio_app_init_param aip = {
		.ctx_attrs = iio_ctx_attrs,
		.nb_ctx_attr = NO_OS_ARRAY_SIZE(iio_ctx_attrs),
		.devices = iio_devices,
		.nb_devices = NO_OS_ARRAY_SIZE(iio_devices),
		.uart_init_params = uart_console_ip,
		.post_step_callback = mwc_step,
		.arg = mwc,
	};

	struct iio_app_desc *app;
	ret = iio_app_init(&app, aip);
	if (ret)
		goto end;

	iio_app_run(app);
end:
	printf("End of program: %d\n", ret);
	iio_app_remove(app);
	hmc630x_iio_remove(iio_tx);
	hmc630x_iio_remove(iio_rx);
	
	return 0;
}