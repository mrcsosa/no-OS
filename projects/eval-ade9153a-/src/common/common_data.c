#include "common_data.h"

struct no_os_uart_init_param ade9153a_uart_ip = {
	.device_id = UART_DEV_ID,
	.baud_rate = UART_BAUD,
	.size = NO_OS_UART_CS_8,
	.parity = NO_OS_UART_PAR_NO,
	.stop = NO_OS_UART_STOP_1_BIT,
	.platform_ops = &max_uart_ops,
	.extra = &max_uart_extra_ip,
};

struct no_os_spi_init_param ade9153a_spi_ip = {
	.device_id = SPI_DEVICE_ID,
	.max_speed_hz = SPI_BAUDRATE,
	.bit_order = NO_OS_SPI_BIT_ORDER_MSB_FIRST,
	.mode = NO_OS_SPI_MODE_0,
	.platform_ops = SPI_OPS,
	.chip_select = SPI_CS,
	.extra = SPI_EXTRA,
};

struct no_os_gpio_init_param ade9153a_reset_ip = {
	.port = GPIO_RESET_PORT,
	.number = GPIO_RESET_PIN,
	.pull = NO_OS_PULL_NONE,
	.platform_ops = GPIO_OPS,
	.extra = GPIO_EXTRA,
};

struct no_os_gpio_init_param ade9153a_gpio_rdy_ip = {
	.port = GPIO_RDY_PORT,
	.number = GPIO_RDY_PIN,
	.pull = NO_OS_PULL_NONE,
	.platform_ops = GPIO_OPS,
	.extra = GPIO_EXTRA,
};

struct no_os_irq_init_param ade9153a_gpio_irq_ip = {
	.platform_ops = GPIO_IRQ_OPS,
	.irq_ctrl_id = GPIO_CTRL_IRQ_ID,
	.extra = GPIO_IRQ_EXTRA,
};

struct ade9153a_init_param ade9153a_ip = {
	.spi_init = &ade9153a_spi_ip,
	.gpio_rdy = &ade9153a_gpio_rdy_ip,
	.gpio_reset = &ade9153a_reset_ip,
	.irq_ctrl = &ade9153a_gpio_irq_desc,
	.ai_swap = ENABLE,
	.ai_pga_gain = ADE9153A_AI_GAIN_16,
	.hpf_crn = ADE9153A_HPF_CORNER_0_625_HZ,
	.freq = F_50_HZ,
	.vlevel = ADE9153A_VLEVEL,
	.rsmall = ADE9153A_RSMALL,
	.no_samples = ADE9153A_NO_SAMPLES,
	.ai_gain = ADE9153A_AIGAIN
};
