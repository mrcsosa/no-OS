#include "parameters.h"

struct max_uart_init_param ade9153a_uart_extra = {
	.flow = MAX_UART_FLOW_DIS,
};

struct max_spi_init_param ade9153a_spi_extra = {
	.num_slaves = 1,
	.polarity = SPI_SS_POL_LOW,
	.vssel = MXC_GPIO_VSSEL_VDDIOH
};

struct max_gpio_init_param ade9153a_gpio_extra = {
	.vssel = MXC_GPIO_VSSEL_VDDIOH,
};

struct max_gpio_init_param gpio_extra = {
	.vssel = MXC_GPIO_VSSEL_VDDIOH,
};
