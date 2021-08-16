/*
 * delay.c
 *
 *  Created on: 17 feb. 2018
 *      Author: Mathias
 */

#include <SI_EFM8UB3_Register_Enums.h>
#include <stdint.h>
#include <stdbool.h>


uint32_t systickTicks = 0;
uint8_t systickRollovers = 0;

#define BTN_DEBOUNCE			20	// milliseconds, also used as base for below counts

#define BTN_DELAY_REPEAT			25	// start repeating after 25*20 = 500 ms
#define BTN_DELAY_LONGPRESS			15	// interpret as long press after 15*20 = 300 ms
#define BTN_DELAY_REPEATFAST		250	// increase speed after 250*20 = 5000 ms

#define BTN_REPEAT		12	// 12*20 = 240 ms
#define BTN_REPEATFAST	5	// 5*20 = 100 ms




#define BTN_1				0x01
#define BTN_2				0x02
#define BTN_3				0x04
#define BTN_EVT_REPEAT		0x20
#define BTN_EVT_LONGPRESS	0x40
#define BTN_EVT_RELEASE		0x80

uint8_t debounce_cnt = 0;
uint8_t delay_cnt = 0;
uint8_t repeat_cnt = 0;

uint8_t lastBtnState = 0;
uint8_t newBtnState;
uint8_t btnEvent = 0;
uint8_t btnLastReg = 0;

uint8_t btnDownCounter = 0;

// Systick ISR
SI_INTERRUPT(TIMER2_ISR, TIMER2_IRQn)
{
    systickTicks++;

    // Increment rollover counter
    if (!systickTicks) systickRollovers++;

    SFRPAGE = 0x00;
    TMR2CN0_TF2H = 0;


    // read buttons
    newBtnState  = P2_B1 ? 0x00 : BTN_1;	// SW1
    newBtnState |= P2_B0 ? 0x00 : BTN_2;	// SW2
    newBtnState |= P0_B6 ? 0x00 : BTN_3;	// SW3


    if ((newBtnState != lastBtnState))
    {
    	// reset debounce counter on button state change
    	debounce_cnt = 0;
    }

    if ((!newBtnState) && (delay_cnt == 0))	// no button down AND delay_cnt == 0
    {
		// reset all counters
		repeat_cnt = 0;
		delay_cnt = 0;
		btnDownCounter = 0;
    }
    else if (debounce_cnt < BTN_DEBOUNCE)			// debounce
       	debounce_cnt++;
    else
    {
    	// debounced

    	if (!newBtnState)
    	{
    		// wait for btnEvent == 0
    		if (!btnEvent)
    		{
    			// reset counter and send button up event
    			btnEvent = btnLastReg | BTN_EVT_RELEASE;
    			btnLastReg = newBtnState;
				delay_cnt = 0;
				repeat_cnt = 0;
				debounce_cnt = 0;
    		}
    	}
    	if ((btnLastReg & (~BTN_EVT_LONGPRESS)) & (~newBtnState)) // ignore longpress status
    	{
    		// a button has been released
    		if (!btnEvent)
    		{
    			btnEvent = (btnLastReg & BTN_EVT_LONGPRESS) | (btnLastReg & (~newBtnState)) | BTN_EVT_RELEASE;
    			btnLastReg = newBtnState;
    			delay_cnt = 0;
				repeat_cnt = 0;
				debounce_cnt = 0;
    		}
    	}
    	else if (delay_cnt == 0)
    	{
    		// button pressed, send first event
    		// wait for btnEvent == 0
			if (!btnEvent)
			{
				btnEvent = newBtnState;
				btnLastReg = newBtnState;
				btnDownCounter++;
				debounce_cnt = 0;
				delay_cnt++;
			}
    	}
    	else if (delay_cnt >= BTN_DELAY_REPEATFAST)
    	{
    		// fast button repeat enabled

			if (repeat_cnt < BTN_REPEATFAST)
			{
				repeat_cnt++;
				debounce_cnt = 0;
			}
			else
			{
				// wait for btnEvent == 0
				if (!btnEvent)
				{
					// key repeat
					repeat_cnt = 0;
					btnEvent = newBtnState | BTN_EVT_REPEAT;
					btnLastReg = (btnLastReg & BTN_EVT_LONGPRESS) | newBtnState;
					btnDownCounter++;
					debounce_cnt = 0;
				}
			}
    	}
    	else if (delay_cnt == BTN_DELAY_LONGPRESS)
    	{
    		// wait for btnEvent == 0
			if (!btnEvent)
			{
				// long press
				btnEvent = newBtnState | BTN_EVT_LONGPRESS;
				btnLastReg = newBtnState | BTN_EVT_LONGPRESS;
				debounce_cnt = 0;
				delay_cnt++;
			}
    	}
    	else if (delay_cnt >= BTN_DELAY_REPEAT)
    	{
    		// button repeat enabled
    		if (repeat_cnt < BTN_REPEAT)
    		{
    			repeat_cnt++;
    			debounce_cnt = 0;
    			delay_cnt++;
    		}
    		else
    		{
    			// wait for btnEvent == 0
				if (!btnEvent)
				{
					// key repeat
					repeat_cnt = 0;
					btnEvent = newBtnState | BTN_EVT_REPEAT;
					btnLastReg = (btnLastReg & BTN_EVT_LONGPRESS) | newBtnState;
					btnDownCounter++;
					debounce_cnt = 0;
					delay_cnt++;
				}
    		}
    	}
    	else
    	{
    		debounce_cnt = 0;
    		delay_cnt++;
    	}


    }


    lastBtnState = newBtnState;


}

uint8_t getButtonState(void)
{
	uint8_t retVal = btnEvent;
	btnEvent = 0;	// clear button state
	return retVal;
}

uint8_t getButtonCnt(void)
{
	return btnDownCounter;
}


uint32_t SysTick_GetTicks(void)
{
	uint32_t curTicks;
	IE_ET2 = 0;	// disable Timer2 IRQ
	curTicks = systickTicks;
	IE_ET2 = 1;	// enable Timer2 IRQ
	return curTicks;
}

// blocking delay
void SysTick_Delay (uint32_t delayTicks)
{
  uint32_t curTicks;
  curTicks = SysTick_GetTicks();

  // Make sure delay is at least 1 tick in case of division, etc.
  if (delayTicks == 0) delayTicks = 1;

  if (curTicks > 0xFFFFFFFF - delayTicks)
  {
    // Rollover will occur during delay
    while (SysTick_GetTicks() >= curTicks)
    {
      while (SysTick_GetTicks() < (delayTicks - (0xFFFFFFFF - curTicks)));
    }
  }
  else
  {
    while ((SysTick_GetTicks() - curTicks) < delayTicks);
  }
}

uint8_t SysTick_GetRollovers(void)
{
  return systickRollovers;
}

bool SysTick_CheckElapsed(uint32_t initialTicks, uint32_t delayTicks)
{
  if (SysTick_GetTicks() - initialTicks >= delayTicks) return true; // Delay elapsed = true
  return false; // Otherwise delay elapsed = false
}


//-----------------------------------------------------------------------------
// SysTick_Delay_us
//-----------------------------------------------------------------------------
//
// Return Value : None
// Parameters   :
//   1) uint8_t us - number of microseconds of delay
//                        range: 0 to 255
//
//-----------------------------------------------------------------------------

void delay_us(uint8_t us)
{

	uint8_t cnt;

	do
	{
		cnt = 11;
		do
		{
			NOP();
		} while (--cnt);


	} while(--us);
}


//	  uint8_t curTicks;
//	  curTicks = TMR2L;
//
//	  // Make sure delay is at least 1 tick in case of division, etc.
//	  if (us == 0) us = 1;
//
//	  if (curTicks > 0xFF - us)
//	  {
//	    // Rollover will occur during delay
//	    while (TMR2L >= curTicks)
//	    {
//	      while (TMR2L < (us - (0xFF - curTicks)));
//	    }
//	  }
//	  else
//	  {
//	    while ((TMR2L - curTicks) < us);
//	  }
//
//




	//us = 65535 - (us-1);

	//TH1 = us >> 8;
	//TL1 = us;

	//TCON_TF1 = 0;	// Clear overflow
	//TCON_TR1 = 1;	// Start T1
	//while (TCON_TF1 == 0);           // Wait for overflow
	//TCON_TR1 = 0;	// Stop T1
//}

