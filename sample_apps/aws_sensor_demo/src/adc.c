/*
 *  Copyright (C) 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */
#include "adc.h"
static mdev_t *adc_dev;
/* Function to get the value from the configured ADC */
float get_sensor_raw_value()
{
	uint16_t value, rv;
	float result;
	/* ADC initialization */
	if (adc_drv_init(ADC0_ID) != WM_SUCCESS) {
		wmprintf("Error: Cannot init ADC\n\r");
		return -1;
	}
	/* Open ADC channel*/
	adc_dev = adc_drv_open(ADC0_ID, ADC_CH0);
	/* Calibrate */
	rv = adc_drv_selfcalib(adc_dev, vref_internal);
	if (rv != WM_SUCCESS)
		wmprintf("Calibration failed!\r\n");
	/* Close ADC channel */
	adc_drv_close(adc_dev);
	ADC_CFG_Type config;
	/* Get default ADC gain value */
	adc_get_config(&config);
	/* Modify ADC gain to 1 */
	adc_modify_default_config(adcGainSel, ADC_GAIN);

	adc_get_config(&config);
	adc_dev = adc_drv_open(ADC0_ID, ADC_CH0);
	value = adc_drv_result(adc_dev);
	result = ((float)value / BIT_RESOLUTION_FACTOR) *
	    VMAX_IN_mV * ((float)1/(float)(config.adcGainSel != 0 ?
			config.adcGainSel : 0.5));
	adc_drv_close(adc_dev);
	adc_drv_deinit(ADC0_ID);
	return result;
}
