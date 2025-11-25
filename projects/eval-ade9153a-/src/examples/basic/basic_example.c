#include "basic_example.h"
#include "common_data.h"
#include "no_os_delay.h"
#include "no_os_print_log.h"
#include "no_os_irq.h"
#include "no_os_util.h"
#include "no_os_units.h"

/**
 * @brief ADE9153A example main execution
 * @return ret - Result of the example execution. If working correctly, will
 * 		 measure and print the ADE9153A measured values periodically.
 */
int ade9153a_example_main()
{
	struct ade9153a_dev *ade9153a_dev;
	int ret;

	/* GPIO Pin Interrupt Controller */
	struct no_os_irq_ctrl_desc *gpio_irq_desc;
	struct no_os_irq_init_param gpio_irq_desc_param = {
		.irq_ctrl_id = GPIO_IRQ_ID,
		.platform_ops = GPIO_IRQ_OPS,
		.extra = GPIO_IRQ_EXTRA
	};

	ret = no_os_irq_ctrl_init(&gpio_irq_desc, &gpio_irq_desc_param);
	if (ret)
		goto exit;

	ade9153a_ip.irq_ctrl = gpio_irq_desc;

	/* Initialize ADE9153A device */
	ret = ade9153a_init(&ade9153a_dev, &ade9153a_ip);
	if (ret)
		goto remove_irq;

	/* Perform device setup */
	ret = ade9153a_setup(ade9153a_dev, ade9153a_ip);
	if (ret)
		goto remove_ade9153a;

	/* Perform autocalibration */
	ret = autocalibration_read_vals(ade9153a_dev);
	if (ret)
		goto remove_ade9153a;

	/* Set temperature sample time */
	ret = ade9153a_temp_time(ade9153a_dev, ADE9153A_TEMP_TIME_SAMPLES_1024);
	if (ret)
		goto remove_ade9153a;

	while (1) {
		no_os_mdelay(500);

		/* Read and print ADE9153A measured values */
		ret = read_measurements(ade9153a_dev);
		if (ret)
			goto remove_ade9153a;

		ret = interface_toggle_led(gpio_led1_desc);
		if (ret)
			goto remove_ade9153a;
	}

remove_ade9153a:
	ade9153a_remove(ade9153a_dev);
remove_irq:
	no_os_irq_ctrl_remove(gpio_irq_desc);
exit:
	return ret;
}
