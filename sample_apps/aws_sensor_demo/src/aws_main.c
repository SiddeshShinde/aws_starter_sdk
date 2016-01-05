/*
 *  Copyright (C) 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */
/*
 * Device publishes the ADC value at GPIO-42 to AWS-IOT cloud.
 *
 * The serial console is set on UART-0.
 * A serial terminal program like HyperTerminal, putty, or
 * minicom can be used to see the program output.
 */

#include <wm_os.h>
#include <wmstdio.h>
#include <wmtime.h>
#include <wmsdk.h>
#include <led_indicator.h>
#include <board.h>
#include <push_button.h>
#include <aws_iot_mqtt_interface.h>
#include <aws_iot_shadow_interface.h>
#include <aws_utils.h>
/* configuration parameters */
#include <aws_iot_config.h>

#include "aws_starter_root_ca_cert.h"

enum state {
	AWS_CONNECTED = 1,
	AWS_RECONNECTED,
	AWS_DISCONNECTED
};

/*-----------------------Global declarations----------------------*/

/* These hold each pushbutton's count, updated in the callback ISR */
volatile uint32_t value_count_int;
volatile uint32_t value_count_frac;
static volatile uint32_t value_count_int_prev = -1;
static volatile uint32_t value_count_frac_prev = -1;

static MQTTClient_t mqtt_client;
static enum state device_state;

/* Thread handle */
static os_thread_t aws_starter_thread;
/* Buffer to be used as stack */
static os_thread_stack_define(aws_starter_stack, 8 * 1024);
/* aws iot url */
static char url[128];

#define AMAZON_ACTION_BUF_SIZE  100
#define VAR_ADC_VALUE_PROPERTY   "adc"
#define RESET_TO_FACTORY_TIMEOUT 5000
#define BUFSIZE                  128

/* callback function invoked on reset to factory */
static void device_reset_to_factory_cb()
{
	/* Clears device configuration settings from persistent memory
	 * and reboots the device.
	 */
	invoke_reset_to_factory();
}

/* board_button_2() is configured to perform reset to factory,
 * when pressed for more than 5 seconds.
 */
void configure_reset_to_factory()
{
	input_gpio_cfg_t pushbutton_reset_to_factory = {
		.gpio = board_button_2(),
		.type = GPIO_ACTIVE_LOW
	};
	push_button_set_cb(pushbutton_reset_to_factory,
			   device_reset_to_factory_cb,
			   RESET_TO_FACTORY_TIMEOUT, 0, NULL);
}

static char client_cert_buffer[AWS_PUB_CERT_SIZE];
static char private_key_buffer[AWS_PRIV_KEY_SIZE];
#define THING_LEN 126
#define REGION_LEN 16
static char thing_name[THING_LEN];

/* populate aws shadow configuration details */
static int aws_starter_load_configuration(ShadowParameters_t *sp)
{
	int ret = WM_SUCCESS;
	char region[REGION_LEN];
	memset(region, 0, sizeof(region));

	/* read configured thing name from the persistent memory */
	ret = read_aws_thing(thing_name, THING_LEN);
	if (ret == WM_SUCCESS) {
		sp->pMyThingName = thing_name;
	} else {
		/* if not found in memory, take the default thing name */
		sp->pMyThingName = AWS_IOT_MY_THING_NAME;
	}
	sp->pMqttClientId = AWS_IOT_MQTT_CLIENT_ID;

	/* read configured region name from the persistent memory */
	ret = read_aws_region(region, REGION_LEN);
	if (ret == WM_SUCCESS) {
		snprintf(url, sizeof(url), "data.iot.%s.amazonaws.com",
			 region);
	} else {
		snprintf(url, sizeof(url), "data.iot.%s.amazonaws.com",
			 AWS_IOT_MY_REGION_NAME);
	}
	sp->pHost = url;
	sp->port = AWS_IOT_MQTT_PORT;
	sp->pRootCA = rootCA;

	/* read configured certificate from the persistent memory */
	ret = read_aws_certificate(client_cert_buffer, AWS_PUB_CERT_SIZE);
	if (ret != WM_SUCCESS) {
		wmprintf("Failed to configure certificate. Returning!\r\n");
		return -WM_FAIL;
	}
	sp->pClientCRT = client_cert_buffer;

	/* read configured private key from the persistent memory */
	ret = read_aws_key(private_key_buffer, AWS_PRIV_KEY_SIZE);
	if (ret != WM_SUCCESS) {
		wmprintf("Failed to configure key. Returning!\r\n");
		return -WM_FAIL;
	}
	sp->pClientKey = private_key_buffer;

	return ret;
}
/* Update Status Call back */
void shadow_update_status_cb(const char *pThingName, ShadowActions_t action,
			     Shadow_Ack_Status_t status,
			     const char *pReceivedJsonDocument,
			     void *pContextData) {

	if (status == SHADOW_ACK_TIMEOUT) {
		wmprintf("Shadow publish state change timeout occured\r\n");
	} else if (status == SHADOW_ACK_REJECTED) {
		wmprintf("Shadow publish state change rejected\r\n");
	} else if (status == SHADOW_ACK_ACCEPTED) {
		wmprintf("Shadow publish state change accepted\r\n");
	}
}
/* Publish thing state to shadow */
int aws_publish_property_state(ShadowParameters_t *sp)
{
	char buf_out[BUFSIZE];
	char state[BUFSIZE];
	char *ptr = state;
	int ret = WM_SUCCESS;

	memset(state, 0, BUFSIZE);
	if (value_count_int_prev != value_count_int) {
		snprintf(buf_out, BUFSIZE, ",\"%s\":%d.%d",
		VAR_ADC_VALUE_PROPERTY,
		value_count_int,
		value_count_frac);
		strcat(state, buf_out);
		value_count_int_prev = value_count_int;
		value_count_frac_prev = value_count_frac;
	}
	if (*ptr == ',')
		ptr++;
	if (strlen(state)) {
		snprintf(buf_out, BUFSIZE, "{\"state\": {\"reported\":{%s}}}",
			 ptr);
		wmprintf("Publishing '%s' to AWS\r\n", buf_out);

		/* publish incremented value on pushbutton press on
		 * configured thing */
		ret = aws_iot_shadow_update(&mqtt_client,
					    sp->pMyThingName,
					    buf_out,
					    shadow_update_status_cb,
					    NULL,
					    10, true);
	}
	return ret;
}

/* application thread */
static void aws_starter_demo(os_thread_arg_t data)
{
	int ret;
	ShadowParameters_t sp;

	aws_iot_mqtt_init(&mqtt_client);

	ret = aws_starter_load_configuration(&sp);
	if (ret != WM_SUCCESS) {
		wmprintf("aws shadow configuration failed : %d\r\n", ret);
		goto out;
	}

	ret = aws_iot_shadow_init(&mqtt_client);
	if (ret != WM_SUCCESS) {
		wmprintf("aws shadow init failed : %d\r\n", ret);
		goto out;
	}

	ret = aws_iot_shadow_connect(&mqtt_client, &sp);
	if (ret != WM_SUCCESS) {
		wmprintf("aws shadow connect failed : %d\r\n", ret);
		goto out;
	}

	/* indication that device is connected and cloud is started */
	led_on(board_led_2());
	wmprintf("Cloud Started\r\n");

	/* creates a thread which will wait for incoming messages, ensuring the
	 * connection is kept alive with the AWS Service
	 */
	while (1) {
		/* Implement application logic here */

		if (device_state == AWS_RECONNECTED) {
			ret = aws_iot_shadow_connect(&mqtt_client, &sp);
			if (ret != WM_SUCCESS) {
				wmprintf("aws shadow reconnect failed: "
					 "%d\r\n", ret);
				goto out;
			} else {
				device_state = AWS_CONNECTED;
				led_on(board_led_2());
			}
		}

		ret = aws_publish_property_state(&sp);
		if (ret != WM_SUCCESS)
			wmprintf("Sending property failed\r\n");

		os_thread_sleep(5000);
	}

	ret = aws_iot_shadow_disconnect(&mqtt_client);
	if (NONE_ERROR != ret) {
		wmprintf("aws iot shadow error %d\r\n", ret);
	}

out:
	os_thread_self_complete(NULL);
	return;
}

void wlan_event_normal_link_lost(void *data)
{
	/* led indication to indicate link loss */
	aws_iot_shadow_disconnect(&mqtt_client);
	device_state = AWS_DISCONNECTED;
}

void wlan_event_normal_connect_failed(void *data)
{
	/* led indication to indicate connect failed */
	aws_iot_shadow_disconnect(&mqtt_client);
	device_state = AWS_DISCONNECTED;
}

/* This function gets invoked when station interface connects to home AP.
 * Network dependent services can be started here.
 */
void wlan_event_normal_connected(void *data)
{
	int ret;
	/* Default time set to 1 October 2015 */
	time_t time = 1443657600;

	wmprintf("Connected successfully to the configured network\r\n");

	if (!device_state) {
		/* set system time */
		wmtime_time_set_posix(time);

		/* create cloud thread */
		ret = os_thread_create(
			/* thread handle */
			&aws_starter_thread,
			/* thread name */
			"awsStarterDemo",
			/* entry function */
			aws_starter_demo,
			/* argument */
			0,
			/* stack */
			&aws_starter_stack,
			/* priority */
			OS_PRIO_3);
		if (ret != WM_SUCCESS) {
			wmprintf("Failed to start cloud_thread: %d\r\n", ret);
			return;
		}
	}

	if (!device_state)
		device_state = AWS_CONNECTED;
	else if (device_state == AWS_DISCONNECTED)
		device_state = AWS_RECONNECTED;
}


