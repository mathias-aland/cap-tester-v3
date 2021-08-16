/*
 * spi.c
 *
 *  Created on: 15 feb. 2018
 *      Author: Mathias
 */


#include <SI_EFM8UB3_Register_Enums.h>
#include <stdint.h>
#include <systick.h>
#include "efm8_device.h"
#include "pga.h"
#include "delay.h"


#define PGA_CS_PIN P0_B5
#define PGA_CK_PIN P0_B3
#define PGA_DO_PIN P0_B4

void spi_writebyte(uint8_t dat)
{
	uint8_t i;

	for (i=0;i<8;i++)
	{
		PGA_CK_PIN = 0;

		// set data pin
		if (dat & 0x80)
			PGA_DO_PIN = 1;
		else
			PGA_DO_PIN = 0;
		delay_us(1);

		PGA_CK_PIN = 1;

		delay_us(1);

		dat = dat << 1;
	}

}


void pga_SetGain(uint8_t gain)
{
	PGA_CS_PIN = 0;
	spi_writebyte(PGA_CMD_GAIN);
	spi_writebyte(gain);
	PGA_CS_PIN = 1;
}


