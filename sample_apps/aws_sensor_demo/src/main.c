/*
 *  Copyright (C) 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */
#include <wmstdio.h>
#include <wmsdk.h>
#include <wm_os.h>
#include <mdev_gpio.h>
#include <mdev_adc.h>
#include <mdev_pinmux.h>
#include <lowlevel_drivers.h>
#include <aws_iot_config.h>
#include <aws_iot_mqtt_interface.h>
#include <aws_iot_shadow_interface.h>
#include <aws_utils.h>
#include "adc.h"
/*
 * Simple Application which publishes the ADC value to AWS-IOT cloud.
 *
 * Summary:
 * This application uses ADC (analog to digital converter) to sample
 * an analog signal on GPIO-42. Self calibration is performed before conversion.
 * This value is contiuoulsy published to AWS-IOT cloud.
 * Any analog sensor can be used to publish its value continuously
 * the AWS-IOT cloud.
 */

#define MICRO_AP_SSID                "aws_starter"
#define MICRO_AP_PASSPHRASE          "marvellwm"
extern uint32_t value_count_int;
extern uint32_t value_count_frac;
void configure_reset_to_factory();
/* This is an entry point for the application.
   All application specific initialization is performed here. */
int main(void)
{
	/* initialize the standard input output facility over uart */
	if (wmstdio_init(UART0_ID, 0) != WM_SUCCESS) {
		return -WM_FAIL;
	}
	wmprintf("Build Time: " __DATE__ " " __TIME__ "\r\n");
	wmprintf("\r\n#### AWS SENSOR DEMO ####\r\n\r\n");
	/* initialize gpio driver */
	if (gpio_drv_init() != WM_SUCCESS) {
		wmprintf("gpio_drv_init failed\r\n");
		return -WM_FAIL;
	}
	/* configure pushbutton on device to perform reset to factory */
	configure_reset_to_factory();

	/* This api adds aws iot configuration support in web application.
	 * Configuration details are then stored in persistent memory.
	 */
	enable_aws_config_support();

	/* This api starts micro-AP if device is not configured, else connects
	 * to configured network stored in persistent memory. Function
	 * wlan_event_normal_connected() is invoked on successful connection.
	 */
	wm_wlan_start(MICRO_AP_SSID, MICRO_AP_PASSPHRASE);

	/* Continuously publish data to AWS-IOT cloud */
	while (1) {
		float result = get_sensor_raw_value();
		value_count_int = wm_int_part_of(result);
		value_count_frac = wm_frac_part_of(result, 6);
		os_thread_sleep(5000);
	}
	return 0;
}
