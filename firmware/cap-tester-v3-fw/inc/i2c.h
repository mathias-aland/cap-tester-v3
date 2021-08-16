/*
 * i2c.h
 *
 *  Created on: 13 okt. 2020
 *      Author: Mathias
 */

#ifndef INC_I2C_H_
#define INC_I2C_H_

#include <stdbool.h>

bool i2c_write(uint8_t devAddr, uint8_t memAddr, uint8_t *pData, uint8_t dataLen);
//uint32_t HAL_GetTick();
//void HAL_Delay(uint16_t ms);

#endif /* INC_I2C_H_ */
