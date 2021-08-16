/*
 * i2c.c
 *
 *  Created on: 13 okt. 2020
 *      Author: Mathias
 */

#include <stdint.h>
#include <stdbool.h>
#include <SI_EFM8UB3_Register_Enums.h>                // SFR declarations

bool i2c_write(uint8_t devAddr, uint8_t memAddr, uint8_t *pData, uint8_t dataLen)
{
	uint8_t SFR_save;

	if (!dataLen)
		return false;

	SFR_save = SFRPAGE;
	SFRPAGE = 0x20;

	SMB0CN0_STA = 1;		// send start condition
	while (!SMB0CN0_SI);	// wait for completion
	SMB0CN0_STA = 0;
	SMB0CN0_STO = 0;
	SMB0DAT = devAddr;	// Send address, write mode
	SMB0CN0_SI = 0;
	while (!SMB0CN0_SI);	// wait for completion

	if (!SMB0CN0_ACK)
	{
		// NACK
		SMB0CN0_STO = 1;	// send stop condition
		SMB0CN0_SI = 0;
		while (SMB0CN0_STO);	// wait for completion
		SFRPAGE = SFR_save;
		return false;
	}


	SMB0DAT = memAddr;
	SMB0CN0_SI = 0;
	while (!SMB0CN0_SI);	// wait for completion

	if (!SMB0CN0_ACK)
	{
		// NACK
		SMB0CN0_STO = 1;	// send stop condition
		SMB0CN0_SI = 0;
		while (SMB0CN0_STO);	// wait for completion
		SFRPAGE = SFR_save;
		return false;
	}


	do
	{
		SMB0DAT = *pData++;
		SMB0CN0_SI = 0;
		while (!SMB0CN0_SI);	// wait for completion

		if (!SMB0CN0_ACK)
		{
			// NACK
			SMB0CN0_STO = 1;	// send stop condition
			SMB0CN0_SI = 0;
			while (SMB0CN0_STO);	// wait for completion
			SFRPAGE = SFR_save;
			return false;
		}


	} while (--dataLen);

	SMB0CN0_STO = 1;	// send stop condition
	SMB0CN0_SI = 0;
	while (SMB0CN0_STO);	// wait for completion

	SFRPAGE = SFR_save;
	return true;


}


//uint32_t HAL_GetTick()
//{
//	return 0;
//}


//void HAL_Delay(uint16_t ms)
//{

//}
