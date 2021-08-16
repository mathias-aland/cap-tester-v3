/*
 * delay.h
 *
 *  Created on: 17 feb. 2018
 *      Author: Mathias
 */

#ifndef INC_SYSTICK_H_
#define INC_SYSTICK_H_

#include <stdint.h>
#include <stdbool.h>

void Timer1_Delay (uint16_t us);


void SysTick_Delay (uint32_t delayTicks);
uint32_t SysTick_GetTicks(void);
uint8_t SysTick_GetRollovers(void);
bool SysTick_CheckElapsed(uint32_t initialTicks, uint32_t delayTicks);

uint8_t getButtonState(void);
uint8_t getButtonCnt(void);

#endif /* INC_SYSTICK_H_ */
