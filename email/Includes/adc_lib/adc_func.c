/*
 * adc_measurement.c
 *
 *  Created on: Aug 21, 2014
 *  Author: sstankovic
 */

#include "hw_memmap.h"
#include "hw_common_reg.h"
#include "hw_types.h"
#include "hw_adc.h"
#include "hw_ints.h"
#include "hw_gprcm.h"
#include "rom.h"
#include "rom_map.h"
#include "uart.h"
#include "pin.h"
#include "adc.h"
#include "uart_if.h"

//*****************************************************************************
//
//! Initialization ADC
//!
//! This function
//!		1. Decide which ADC pin used
//!
//! \return none
//
//*****************************************************************************
void initADC(void)
{
    Message("\n\r...start ADC init...");

#ifdef CC3200_ES_1_2_1
    //
    // Enable ADC clocks.###IMPORTANT###Need to be removed for PG 1.32
    //
    HWREG(GPRCM_BASE + GPRCM_O_ADC_CLK_CONFIG) = 0x00000043;
    HWREG(ADC_BASE + ADC_O_ADC_CTRL) = 0x00000004;
    HWREG(ADC_BASE + ADC_O_ADC_SPARE0) = 0x00000100;
    HWREG(ADC_BASE + ADC_O_ADC_SPARE1) = 0x0355AA00;
#endif

    //
    // Pinmux for the selected ADC input pin
    //
    MAP_PinTypeADC(PIN_58,0xFF);
    MAP_PinTypeADC(PIN_59,0xFF);
    MAP_PinTypeADC(PIN_60,0xFF);//(obdaberes zeljeni fizicki pin, i njegov mod-ADC mode)

	//
	// Enable ADC channel
	//
	MAP_ADCChannelEnable(ADC_BASE, ADC_CH_1);
	MAP_ADCChannelEnable(ADC_BASE, ADC_CH_2);
	MAP_ADCChannelEnable(ADC_BASE, ADC_CH_3);//enables specified ADC channel and configures the pin as analog pin

	//
	// Configure ADC timer which is used to timestamp the ADC data samples
	//
	MAP_ADCTimerReset(ADC_BASE);
	MAP_ADCTimerConfig(ADC_BASE,2^17);//The ADC timer is a 17 bit used to timestamp the ADC data samples internally
	MAP_ADCTimerEnable(ADC_BASE);

	//
	// Enable ADC module
	//
	MAP_ADCEnable(ADC_BASE);
    Message("\n\r...finish...");
}

//*****************************************************************************
//
//! batteryVoltage
//!
//! This function measure battery voltage
//!
//! \return battery voltage, type int
//
//*****************************************************************************
char batteryVoltage(void)
{
	unsigned long ulSample = 0x00000000;
	unsigned long valLong = 0x00000000;
	unsigned int CH_1 = 0x00;
	float voltage = 0;
	char volInt;

	if(MAP_ADCFIFOLvlGet(ADC_BASE, ADC_CH_1))
	{
		ulSample = MAP_ADCFIFORead(ADC_BASE, ADC_CH_1);//give FIFO value from channel
		valLong = ulSample;
		//put (14-2)bits from ulSample to CH_1
		valLong =valLong>>2;
		valLong &= ~((1<<31)|(1<<30)|(1<<29)|(1<<28)|(1<<27)|(1<<26)|(1<<25)|(1<<24)|(1<<23)|(1<<22)|(1<<21)|(1<<20)|(1<<19)|(1<<18)|(1<<17)|(1<<16)|(1<<15)|(1<<14)|(1<<13));
		CH_1 |= valLong;

		voltage = (CH_1*1.48)/4095;//convert samples to voltage on ADC input pin
		voltage = voltage * 3.201;//scale to real voltage level
		volInt = (char) ((voltage/3.291) * 100);//scale (0-100)%
	}

	return volInt;
}

//*****************************************************************************
//
//! getCurrent
//!
//! This function measure current on J12
//!
//! \return current value, type int
//
//*****************************************************************************
int getCurrent(void)
{
    unsigned long ulSample = 0x00000000;
    unsigned long valLong = 0x00000000;
    unsigned int valInt = 0x0000;
    unsigned int CH_2 = 0x00;
    unsigned int CH_3 = 0x00;
    float voltage = 0;
    float current = 0;
    int currentInt = 0x00;

	if(MAP_ADCFIFOLvlGet(ADC_BASE, ADC_CH_3))
	{
		ulSample = MAP_ADCFIFORead(ADC_BASE, ADC_CH_3);//give FIFO value from channel
		valLong = ulSample;
		//put (14-2)bits from ulSample to CH_3
		valLong =valLong>>2;
		valLong &= ~((1<<31)|(1<<30)|(1<<29)|(1<<28)|(1<<27)|(1<<26)|(1<<25)|(1<<24)|(1<<23)|(1<<22)|(1<<21)|(1<<20)|(1<<19)|(1<<18)|(1<<17)|(1<<16)|(1<<15)|(1<<14)|(1<<13));
		CH_3 |= valLong;
	}

	ulSample = 0x00000000;
	valLong = 0x00000000;
	voltage = 0;

	if(MAP_ADCFIFOLvlGet(ADC_BASE, ADC_CH_2))
	{
		ulSample = MAP_ADCFIFORead(ADC_BASE, ADC_CH_2);//give FIFO value from channel
		valLong = ulSample;
		//put (14-2)bits from ulSample to CH_3
		valLong =valLong>>2;
		valLong &= ~((1<<31)|(1<<30)|(1<<29)|(1<<28)|(1<<27)|(1<<26)|(1<<25)|(1<<24)|(1<<23)|(1<<22)|(1<<21)|(1<<20)|(1<<19)|(1<<18)|(1<<17)|(1<<16)|(1<<15)|(1<<14)|(1<<13));
		CH_2 |= valLong;
	}

	valInt = CH_2 - CH_3;//voltage difference in Samples
	voltage = (valInt*1.45)/4095;//convert samples to voltage on ADC input pin
	current = voltage /0.1 ;//convert voltage to current; R = 0,1Ohm
	current *= 1000;//convert to mA

	currentInt = current * 10;//10x value higher for MQTT packet

	ulSample = 0x00000000;
	valLong = 0x00000000;
	voltage = 0;
	current = 0;

	return currentInt;
}
