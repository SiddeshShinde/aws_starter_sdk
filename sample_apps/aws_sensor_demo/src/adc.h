/*
 *  Copyright (C) 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */
#ifndef _ADC_H_
#define _ADC_H_
#include <wmstdio.h>
#include <wm_os.h>
#include <mdev_gpio.h>
#include <mdev_adc.h>
#include <mdev_pinmux.h>
#include <lowlevel_drivers.h>
/*------------------Macro Definitions ------------------*/
#define SAMPLES	500
#define ADC_GAIN	ADC_GAIN_1
#define BIT_RESOLUTION_FACTOR 32768	/* For 16 bit resolution (2^15-1) */
#define VMAX_IN_mV	1200	/* Max input voltage in milliVolts */
float get_sensor_raw_value();
#endif
