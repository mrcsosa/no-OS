#include <common_data.h>

struct no_os_gpio_init_param gp3_init_param = {
	.port = GP3_PORT,
	.number = GP3_NUMBER,
	.pull = GP3_PULL,
	.platform_ops = GP3_OPS,
	.extra = GP3_EXTRA,
};

struct no_os_gpio_init_param gp2_init_param = {
	.port = GP2_PORT,
	.number = GP2_NUMBER,
	.pull = GP2_PULL,
	.platform_ops = GP2_OPS,
	.extra = GP2_EXTRA,
};

struct no_os_gpio_init_param gp1_init_param = {
	.port = GP1_PORT,
	.number = GP1_NUMBER,
	.pull = GP1_PULL,
	.platform_ops = GP1_OPS,
	.extra = GP1_EXTRA,
};

struct no_os_gpio_init_param data_ss_init_param = {
	.port = DATA_SWSS_PORT,
	.number = DATA_SWSS_NUMBER,
	.pull = DATA_SWSS_PULL,
	.platform_ops = DATA_SWSS_OPS,
	.extra = DATA_SWSS_EXTRA,
};

struct no_os_spi_init_param data_spi_init_param = {
	.device_id = DATA_SPI_DEVICE_ID,
	.max_speed_hz = DATA_SPI_SPEED,
	.chip_select = DATA_SPI_SS,
	.mode= DATA_SPI_MODE,
	.bit_order = DATA_SPI_BIT_ORDER,
	.lanes = DATA_SPI_LANES,
	.platform_ops = DATA_SPI_OPS,
	.extra = DATA_SPI_EXTRA,
};

struct no_os_gpio_init_param cfg_ss_init_param = {
	.port = CFG_SWSS_PORT,
	.number = CFG_SWSS_NUMBER,
	.pull = CFG_SWSS_PULL,
	.platform_ops = CFG_SWSS_OPS,
	.extra = CFG_SWSS_EXTRA,
};

struct no_os_spi_init_param cfg_spi_init_param = {
	.device_id = CFG_SPI_DEVICE_ID,
	.max_speed_hz = CFG_SPI_SPEED,
	.chip_select = CFG_SPI_SS,
	.mode= CFG_SPI_MODE,
	.bit_order = CFG_SPI_BIT_ORDER,
	.lanes = CFG_SPI_LANES,
	.platform_ops = CFG_SPI_OPS,
	.extra = CFG_SPI_EXTRA,
};

struct no_os_uart_init_param iio_uart_init_param = {
	.device_id = IIO_UART_DEVICE_ID,
	.irq_id = IIO_UART_IRQ_ID,
	.asynchronous_rx = IIO_UART_ASYNC_RX,
	.baud_rate = IIO_UART_BAUD_RATE,
	.size = IIO_UART_SIZE,
	.parity = IIO_UART_PARITY,
	.stop = IIO_UART_STOP_BIT,
	.platform_ops = IIO_UART_OPS,
	.extra = IIO_UART_EXTRA,
};

