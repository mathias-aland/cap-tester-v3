/*
 * meas.c
 *
 *  Created on: 14 feb. 2021
 *      Author: Mathias
 */

#include <SI_EFM8UB3_Register_Enums.h>                // SFR declarations
#include <string.h>
#include <stdio.h>
#include <systick.h>
#include "efm8_device.h"
#include "boot.h"
#include "spi.h"
#include "pga.h"
#include "globals.h"
#include "ssd1306/ssd1306_tests.h"
#include "ssd1306/ssd1306.h"
#include "delay.h"
#include "meas.h"
#include <math.h>




// PCA0CP = (4095 - x) / 2
// Times need to be in odd number of us
#define ESR_CHG_TIME	11	// Charge pulse length = 11 us
#define ESR_DIS_TIME	33	// Discharge release length = 33 us
#define ESR_ADC_TIME	35	// ADC clock gate length = 35 us












volatile measurement_state_t measurement_curState = MEAS_STATE_IDLE;
volatile measurement_state_t measurement_reqState = MEAS_STATE_NONE;


uint32_t xdata meas_wait_initial = 0;
uint32_t xdata meas_wait = 0;

uint32_t xdata meas_timeout_initial = 0;
uint32_t xdata meas_timeout = 0;

uint8_t meas_curCurrent = 0;
uint8_t meas_sel_curr = CURR_OFF;

uint8_t meas_sel_curr_c = CURR_OFF;

meas_dcr_result_t xdata meas_dcr_result;
meas_c_result_t xdata meas_c_result;


//uint16_t t3_tmp, t3_tmp2;
//uint16_t t4_tmp, t4_tmp2;

uint32_t start_time, end_time;


uint8_t esr_adc_step = 0;
uint8_t esr_adc_samples = 0;

volatile bit esr_adc_ready = 0;

uint32_t xdata esr[3] = {0,0,0};


extern char xdata str[20];

//SI_INTERRUPT(TIMER4_ISR, TIMER4_IRQn)
//{
//	TMR4CN0_TF4H = 0;	// Clear Timer4 IRQ
//}


//SI_INTERRUPT(PCA0_ISR, PCA0_IRQn)
//{
//	// timeout
//	PCA0CN0_CCF2 = 0;	// Clear PCA IRQ
//}
//
//SI_INTERRUPT(CMP0_ISR, CMP0_IRQn)
//{
//	// at start trigger level
//	CMP0CN0 &= ~(CMP0CN0_CPRIF__BMASK | CMP0CN0_CPFIF__BMASK);	// Clear CMP0 IRQs
//}
//
//SI_INTERRUPT(CMP1_ISR, CMP1_IRQn)
//{
//	// measurement complete
//	CMP1CN0 &= ~(CMP1CN0_CPRIF__BMASK | CMP1CN0_CPFIF__BMASK);	// Clear CMP1 IRQs
//}


void set_isrc(uint8_t current);




bool select_isrc(uint8_t current)
{


	switch (current)
	{
		case CURR_02:
			P1SKIP = 0x79;	// select 0.2 mA
			meas_sel_curr = CURR_02;
			break;
		case CURR_2:
			P1SKIP = 0x75;	// select 2 mA
			meas_sel_curr = CURR_2;
			break;
		case CURR_20:
			P1SKIP = 0x5D;	// select 20 mA
			meas_sel_curr = CURR_20;
			break;
		case CURR_200:
			P1SKIP = 0x3D;	// select 200 mA
			meas_sel_curr = CURR_200;
			break;
		default:
			return false;

	}

	P1MDIN &= ~P1MDIN_B4__DIGITAL;	// disable digital mode (PD)
	XBR1 = XBR1_PCA0ME__CEX0_CEX1;

	return true;

}



// Reset PWM outputs by forcing overflow. Might not work on 16-bit center aligned mode.
void PCA_pwmReset(void) reentrant
{
	uint8_t tmpCent, tmpMode, tmpCR, tmpIE;

	tmpIE = IE_EA;
	IE_EA = 0;

	tmpCent = PCA0CENT;
	tmpMode = PCA0MD;
	tmpCR = PCA0CN0_CR;

	// force overflow (switch off PWM)
	PCA0CN0_CR = 0;
	PCA0MD = PCA0MD_CPS__SYSCLK_DIV_12;
	PCA0CN0_CF = 0;	// clear overflow flag
	PCA0H = 0xFF;
	PCA0L = 0xFE;
	PCA0CENT = 0;	// edge aligned mode
	PCA0CN0_CR = 1;

	while (!PCA0CN0_CF);
	PCA0CN0 = 0;	// stop PCA0, clear overflow and CCF flags

	PCA0CENT = tmpCent;
	PCA0MD = tmpMode;
	PCA0CN0_CR = tmpCR;
	IE_EA = tmpIE;

}


SI_INTERRUPT(ADC0EOC_ISR, ADC0EOC_IRQn)
{
	uint16_t adcVal = ADC0;
	ADC0CN0_ADINT = 0;


	switch (esr_adc_step)
	{
	case 0:
		if (esr_adc_samples < 16)
			esr[0] += adcVal;
		break;
	case 1:
		if (esr_adc_samples < 16)
			esr[1] += adcVal;
		break;
	case 2:
		if (esr_adc_samples < 16)
			esr[2] += adcVal;
		break;
	}

	esr_adc_step++;
	if (esr_adc_step == 3)
	{
		if (esr_adc_samples < 16)
		{
			esr_adc_samples++;
			if (esr_adc_samples == 16)
				measurement_curState = MEAS_ESR_READY;	// signal data ready
				//esr_adc_ready = true;	// signal data ready
		}
		else
		{
			if (measurement_curState != MEAS_ESR_READY)
			{
				esr_adc_samples = 0;	// reset counter if status bit is cleared
				esr[0] = 0;
				esr[1] = 0;
				esr[2] = 0;
			}
		}

		esr_adc_step = 0;
	}



}








SI_INTERRUPT(PCA0_ISR, PCA0_IRQn)
{
	// timeout
	SFRPAGE = 0x00;

	// TODO: move range switching to state machine

	// force overflow (switch off PWM)
	TCON_TR0 = 0;	// stop Timer0
	PCA_pwmReset();
//	PCA0CN0_CF = 0;
//	TL0  = 0x00;
//	PCA0H = 0xFF;
//	PCA0L = 0xFE;
//	TCON_TR0 = 1;	// start Timer0
//
//	while (!PCA0CN0_CF);
//	TCON_TR0 = 0;	// stop Timer0

	//set_isrc(CURR_DISC);


	CMP1CN0 &= ~(CMP1CN0_CPRIF__BMASK | CMP1CN0_CPFIF__BMASK);	// Clear CMP1 IRQs
	CMP0CN0 &= ~(CMP0CN0_CPRIF__BMASK | CMP0CN0_CPFIF__BMASK);	// Clear CMP0 IRQs
	PCA0CN0_CCF2 = 0;	// Clear PCA IRQ


	if ((meas_sel_curr_c == CURR_200) || (measurement_curState == MEAS_C_CHG_3))
	{
		// timeout


		//CMP1CN0 &= ~(CMP1CN0_CPRIF__BMASK | CMP1CN0_CPFIF__BMASK);	// Clear CMP1 IRQs
		//CMP0CN0 &= ~(CMP0CN0_CPRIF__BMASK | CMP0CN0_CPFIF__BMASK);	// Clear CMP0 IRQs

		// disable CMP1 IRQ
		EIE1 &= ~(EIE1_ECP1__BMASK | EIE1_ECP0__BMASK | EIE1_EPCA0__BMASK);

		measurement_curState = MEAS_STATE_IDLE;
		measurement_reqState = MEAS_ESR_INIT;
		//measurement_curState = MEAS_STATE_C_START;
	}
	else
	{
		if (meas_sel_curr_c == CURR_20)
		{
			meas_sel_curr_c = CURR_200;
			select_isrc(CURR_200);
			PCA0CPH2 = 5120 / 64;	// timeout after 5120 ms
		}
		else if (meas_sel_curr_c == CURR_2)
		{
			meas_sel_curr_c = CURR_20;
			select_isrc(CURR_20);
		}
		else if (meas_sel_curr_c == CURR_02)
		{
			meas_sel_curr_c = CURR_2;
			select_isrc(CURR_2);
		}

		// start PWM
		//PCA0CPM0 = PCA0CPM0_ECOM__ENABLED | PCA0CPM0_PWM__ENABLED | PCA0CPM0_PWM16__16_BIT;
		//PCA0CPM1 = PCA0CPM1_ECOM__ENABLED | PCA0CPM1_PWM__ENABLED | PCA0CPM0_PWM16__16_BIT;
		//PCA0CN0_CR = 1;
		TCON_TR0 = 1;	// start Timer0


	}




}

SI_INTERRUPT(CMP0_ISR, CMP0_IRQn)
{
	// at start trigger level
	//uint8_t sfr_save = SFRPAGE;
	//uint16_t t3, t4;

	SFRPAGE = 0x10;

	// Big-endian
	*((unsigned char *) (&start_time)) =   TMR4RLH;
	*((unsigned char *) (&start_time)+1) = TMR4RLL;
	*((unsigned char *) (&start_time)+2) = TMR3RLH;
	*((unsigned char *) (&start_time)+3) = TMR3RLL;

	//start_time = (uint32_t)(TMR4RL << 16) | TMR3RL;

	//t3_tmp = TMR3RL;
	//t4_tmp = TMR4RL;

	TMR4CN1 = TMR4CN1_T4CSEL__CLU3_OUT;
	TMR3CN1 = TMR3CN1_T3CSEL__CLU3_OUT;

	CMP0CN0 &= ~(CMP0CN0_CPRIF__BMASK | CMP0CN0_CPFIF__BMASK);	// Clear CMP0 IRQs

	// enable CMP1 IRQ, disable CMP0
	EIE1 &= ~EIE1_ECP0__BMASK;
	EIE1 |= EIE1_ECP1__ENABLED;

	//SFRPAGE = sfr_save;


	//esr[0] = t3;
	//esr[0] |= (uint32_t)t4 << 16;

	measurement_curState = MEAS_C_CHG_3;

	PCA0CPH2 = 10240 / 64;	// timeout after 10240 ms
	PCA0CN0_CCF2 = 0;	// Clear PCA IRQ

}

SI_INTERRUPT(CMP1_ISR, CMP1_IRQn)
{
	uint8_t pca0ticks;
	// measurement complete
	//uint8_t sfr_save = SFRPAGE;
	//uint16_t t3, t4;
	TCON_TR0 = 0;	// stop Timer0

	SFRPAGE = 0x10;

	//t3_tmp2 = TMR3RL;
	//t4_tmp2 = TMR4RL;

	// Big-endian
	*((unsigned char *) (&end_time)) =   TMR4RLH;
	*((unsigned char *) (&end_time)+1) = TMR4RLL;
	*((unsigned char *) (&end_time)+2) = TMR3RLH;
	*((unsigned char *) (&end_time)+3) = TMR3RLL;

	//end_time = (uint32_t)(TMR4RL << 16) | TMR3RL;

	pca0ticks = PCA0L;	// need to read PCA0L before PCA0H
	pca0ticks = PCA0H;


	// force overflow (switch off PWM)
	TCON_TR0 = 0;	// stop Timer0
	PCA_pwmReset();




	TMR4CN1 = TMR4CN1_T4CSEL__CLU2_OUT;
	TMR3CN1 = TMR3CN1_T3CSEL__CLU2_OUT;

	// disable CMP1 IRQ
	EIE1 &= ~(EIE1_ECP1__BMASK | EIE1_ECP0__BMASK | EIE1_EPCA0__BMASK);


	CMP1CN0 &= ~(CMP1CN0_CPRIF__BMASK | CMP1CN0_CPFIF__BMASK);	// Clear CMP1 IRQs
	CMP0CN0 &= ~(CMP0CN0_CPRIF__BMASK | CMP0CN0_CPFIF__BMASK);	// Clear CMP0 IRQs

	PCA0CN0_CCF2 = 0;	// Clear PCA IRQ


	// range switch checking (capacitance)

	if (pca0ticks < (64 / 64)) // < 64 ms
	{
		if (meas_sel_curr_c == CURR_200)
		{
			meas_sel_curr_c = CURR_20;
		}
		else if (meas_sel_curr_c == CURR_20)
		{
			meas_sel_curr_c = CURR_2;
		}
		else if (meas_sel_curr_c == CURR_2)
		{
			meas_sel_curr_c = CURR_02;
		}
	}




	//SFRPAGE = sfr_save;

	if (measurement_curState == MEAS_C_CHG_3)
	{
		//esr_adc_ready = 1;
		measurement_curState = MEAS_C_CHG_4;
		// enable CMP0 IRQ
		//EIE1 |= EIE1_ECP0__ENABLED;

	}
	else
	{
		measurement_curState = MEAS_STATE_IDLE;
		measurement_reqState = MEAS_ESR_INIT;

		//measurement_curState = MEAS_STATE_C_START;
	}


}









void measurement_capStart()
{

	measurement_curState = MEAS_STATE_C_INIT;
	return;




}


void set_isrc(uint8_t current)
{

	XBR1 = XBR1_PCA0ME__DISABLED;
	P1SKIP = 0x7F;	// GPIO control

    // 0.2 mA (P-MOS)
    if (current & CURR_02)
        ISRC_02_PIN = 0;   // ISRC_02_PIN -> low (0.2 mA ON)
    else
    	ISRC_02_PIN = 1;    // ISRC_02_PIN -> high (0.2 mA OFF)

    // 2 mA (P-MOS)
    if (current & CURR_2)
        ISRC_2_PIN = 0;   // ISRC_2_PIN -> low (2 mA ON)
    else
    	ISRC_2_PIN = 1;    // ISRC_2_PIN -> high (2 mA OFF)

    // 20 mA (P-MOS)
    if (current & CURR_20)
        ISRC_20_PIN = 0;   // ISRC_20_PIN -> low (20 mA ON)
    else
    	ISRC_20_PIN = 1;    // ISRC_20_PIN -> high (20 mA OFF)

	// 200 mA (P-MOS)
	if (current & CURR_200)
		ISRC_200_PIN = 0;   // ISRC_200_PIN -> low (200 mA ON)
	else
		ISRC_200_PIN = 1;    // ISRC_200_PIN -> high (200 mA OFF)

    // Discharge (N-MOS + 3R3)
    if (current & CURR_DISC)
    	ISRC_DISC_PIN = 1;   // ISRC_DISC_PIN -> high (DISCHG ON)
    else
    	ISRC_DISC_PIN = 0;    // ISRC_DISC_PIN -> low (DISCHG OFF)

    // Weak pull-down (1k)
    if (current & CURR_PD)
    {
    	P1MDIN |= P1MDIN_B4__DIGITAL;	// enable digital mode
    	ISRC_PD_PIN = 0;   // ISRC_PD_PIN -> low (PD ON)
    }
    else
    {
    	ISRC_PD_PIN = 1;    // ISRC_PD_PIN -> high (PD OFF)
    	P1MDIN &= ~P1MDIN_B4__DIGITAL;	// disable digital mode
    }

    //meas_sel_curr = current;
}








void meas_wait_start(uint32_t delayMs)
{
    meas_wait_initial = SysTick_GetTicks();
    meas_wait = delayMs;
}



void measurement_stateMachine()
{

	measurement_state_t tmpState = measurement_curState;
	bit ie_tmp;
	uint8_t tmp8;

	 float res_val;
	//float esr = 0.1f;






	if (measurement_reqState != MEAS_STATE_NONE)
	{
		// stop everything
		// disable interrupts first
		IE_EA = 0;	// make sure no IRQ can modify EIE1
		EIE1 &= ~(EIE1_ECP0__ENABLED | EIE1_ECP1__ENABLED | EIE1_EPCA0__ENABLED | EIE1_EADC0__ENABLED);	// Disable PCA, Comparator, ADC IRQs
		IE_EA = 1;

		set_isrc(CURR_DISC);	// discharge ON

		TCON_TR0 = 0;	// stop Timer0
		PCA0CN0_CR = 0;	// stop PCA0
		ADC0CN0_ADBMEN = 0;	// stop ADC
		ADC0CN0_ADEN = 0;	// stop ADC
		TMR3CN0 &= ~TMR3CN0_TR3__BMASK;	// stop Timer3
		SFRPAGE = 0x10;
		TMR4CN0 &= ~TMR4CN0_TR4__BMASK; // stop Timer4
		SFRPAGE = 0x00;
		PCA_pwmReset();	// reset PWM

		//meas_wait_start(100);

		measurement_curState = measurement_reqState;
		measurement_reqState = MEAS_STATE_NONE;
	}


	if (tmpState == MEAS_STATE_IDLE)
	{
		return;   // If IDLE, don't do anything
	}


	if (meas_wait > 0)
	    {
	        // Wait enabled, check if elapsed
	        if (!SysTick_CheckElapsed(meas_wait_initial, meas_wait)) return;    // Not elapsed

	        // Elapsed, reset vars
	        meas_wait = 0;
	    }





	switch (tmpState)
	{
		case MEAS_STATE_C_INIT:

			// ADC config
			//EIE1 &= ~EIE1_EADC0__ENABLED;
			ADC0CN1 = ADC0CN1_ADCM__ADBUSY | ADC0CN1_ADCMBE__CM_BUFFER_ENABLED;
			ADC0MX = ADC_IN_A;
			ADC0CF = (0x01 << ADC0CF_ADSC__SHIFT) | ADC0CF_ADGN__GAIN_1;
			ADC0AC = ADC0AC_AD12BE__12_BIT_ENABLED | ADC0AC_ADRPT__ACC_64;
			ADC0TK = ADC0TK_AD12SM__SAMPLE_ONCE | (63 << ADC0TK_ADTK__SHIFT);
			ADC0PWR = (0x0F << ADC0PWR_ADPWR__SHIFT) | ADC0PWR_ADBIAS__MODE1;
			ADC0CN0 |= ADC0CN0_ADEN__ENABLED | ADC0CN0_ADBMEN__BURST_ENABLED;


			pga_SetGain(PGA_GAIN_16);

			// Configure CLU2 and 3

						/***********************************************************************
						 - CLU2 Look-Up-Table function select = 0xCC
						 - Select CLU2A.5 (CEX1)
						 - Select CLU2B.4 (CMP0)
						 - Select CLU3A.3 (C3OUT)
						 - Select CLU3B.4 (CMP1)
						 - Select LUT output
						 - CLU2 is enabled
						 - CLU3 is enabled
						***********************************************************************/
						SFRPAGE = 0x20;
						//CLU2FN = 0xCF;	// !CEX1 + CMP0
						CLU2FN = 0xCC;	// CMP0

						//CLU0FN = 0xFF;	// test
						//CLU1FN = 0xFF;	// test
						//CLU2FN = 0xFF;	// test


						CLU3FN = 0xCC;
						CLU2MX = CLU2MX_MXA__CLU2A5 | CLU2MX_MXB__CLU2B4;
						CLU3MX = CLU3MX_MXA__CLU3A3 | CLU3MX_MXB__CLU3B4;
						CLU2CF = CLU2CF_OUTSEL__LUT;
						CLU3CF = CLU3CF_OUTSEL__LUT;
						CLEN0 = CLEN0_C2EN__ENABLE | CLEN0_C3EN__ENABLE;
						SFRPAGE = 0x00;


						// configure comparators
						/***********************************************************************
						 - External pin CMP0N.7
						 - External pin CMP0P.15
						 - External pin CMP1N.0
						 - External pin CMP1P.15
						 - CMP0 Internal Comparator DAC Reference Level = 0x15
						 - CMP1 Internal Comparator DAC Reference Level = 0x2A
						 - The comparator output will always reflect the input conditions
						 - Mode 0
						 - Connect the CMP+ input to the internal DAC output, and CMP- is selected by CMXN
						 - Comparator enabled
						***********************************************************************/
						SFRPAGE = 0x10;
						CMP0MX = CMP0MX_CMXP__CMP0P15 | CMP0MX_CMXN__CMP0N7;
						CMP1MX = CMP1MX_CMXP__CMP1P15 | CMP1MX_CMXN__CMP1N0;
						//CMP0CN1 = (21 << CMP0CN1_DACLVL__SHIFT);
						CMP0CN1 = (13 << CMP0CN1_DACLVL__SHIFT);
						CMP1CN1 = (58 << CMP1CN1_DACLVL__SHIFT);
						CMP0MD = CMP0MD_INSL__DAC_CMXN | CMP0MD_CPFIE__FALL_INT_ENABLED | CMP0MD_CPMD__MODE3;
						CMP1MD = CMP1MD_INSL__DAC_CMXN | CMP1MD_CPFIE__FALL_INT_ENABLED | CMP1MD_CPMD__MODE3;
						CMP0CN0 = CMP0CN0_CPEN__ENABLED | CMP0CN0_CPHYP__ENABLED_MODE2 | CMP0CN0_CPHYN__DISABLED;
						CMP1CN0 = CMP1CN0_CPEN__ENABLED | CMP1CN0_CPHYP__ENABLED_MODE2 | CMP0CN0_CPHYN__DISABLED;
						SFRPAGE = 0x00;

						// Configure timer clocks
						/***********************************************************************
						 - Counter/Timer 0 uses the SYSCLK/48
						***********************************************************************/
						CKCON0 &= ~CKCON0_T0M__BMASK;
						CKCON0 |= CKCON0_T0M__PRESCALE;

						// Configure Timer0
						/***********************************************************************
						 - Timer 0: Timer mode 2, 8-bit Counter/Timer with Auto-Reload
						 - Timer 0 enabled when TR0 = 1 irrespective of INT0 logic level
						 - Timer 0 auto reload = 6 (4 kHz overflow rate)
						***********************************************************************/
						TMOD &= ~TMOD_T0M__FMASK;
						TMOD |= TMOD_T0M__MODE2;
						//TH0  = 6;	// 48 MHz
						//TL0  = 6;	// 48 MHz
						TH0  = 128;	// 24.5 MHz
						TL0  = 128;	// 24.5 MHz
						//TCON_TR0 = 1;




						// Configure Timer4
						/***********************************************************************
						 - Capture high-to-low transitions on the configurable logic unit 3 synchronous output
						 - Timer will reload on overflow events and CLU2 synchronous output high
						 - Enable capture mode
						 - Timer 4 is clocked by Timer 3 overflows
						***********************************************************************/
						SFRPAGE = 0x10;
						TMR4CN0 = TMR4CN0_TF4CEN__ENABLED | TMR4CN0_T4XCLK__TIMER3;
						TMR4 = 0;
						TMR4CN1 = TMR4CN1_T4CSEL__CLU2_OUT;

						// Configure Timer3
						/***********************************************************************
						 - Capture high-to-low transitions on the configurable logic unit 3 synchronous output
						 - Timer will reload on overflow events and CLU2 synchronous output high
						 - Enable capture mode
						***********************************************************************/
						TMR3CN0 = TMR3CN0_TF3CEN__ENABLED;
						TMR3 = 0;
						TMR3CN1 = TMR3CN1_T3CSEL__CLU2_OUT;
						SFRPAGE = 0x00;




						// Configure PCA0
						/***********************************************************************
						 - PCA continues to function normally while the system controller is in Idle Mode
						 - Clock = Timer 0 overflow
						 - 16-bit PWM for CH0-1
						 - Use Comparator 1 for the comparator clear function
						 - Enable the comparator clear function on PCA channel 0-1
						 - Enable comparator function
						 - Enable PWM function
						***********************************************************************/
						PCA0MD = PCA0MD_CIDL__NORMAL | PCA0MD_ECF__OVF_INT_DISABLED | PCA0MD_CPS__T0_OVERFLOW;
						PCA0CENT = PCA0CENT_CEX0CEN__EDGE | PCA0CENT_CEX1CEN__EDGE | PCA0CENT_CEX2CEN__EDGE;
						PCA0CLR = PCA0CLR_CPSEL__CMP1 | PCA0CLR_CPCE0__ENABLED | PCA0CLR_CPCE1__ENABLED | PCA0CLR_CPCE2__DISABLED;
						//PCA0CLR = 0;
						PCA0PWM = PCA0PWM_CLSEL__11_BITS;
						PCA0CPM0 = PCA0CPM0_ECOM__ENABLED | PCA0CPM0_PWM__ENABLED | PCA0CPM0_PWM16__16_BIT;
						PCA0CPM1 = PCA0CPM1_ECOM__ENABLED | PCA0CPM1_PWM__ENABLED | PCA0CPM0_PWM16__16_BIT;
						PCA0CPM2 = PCA0CPM2_ECOM__ENABLED | PCA0CPM2_MAT__ENABLED | PCA0CPM2_ECCF__ENABLED;	// software timer, enable IRQ
						PCA0CPL0 = 0;	// (2) disable discharge after 500 us
						PCA0CPH0 = 64 / 64;	// disable discharge after 64 ms
						PCA0CPL1 = 1000 / 250;	// (1) enable charge after 250 us
						PCA0CPH1 = 64 / 64;	// enable charge after 65 ms
						PCA0CPL2 = 0;
						//PCA0CPH2 = 320 / 64;	// timeout after 320 ms
						PCA0CPH2 = 128 / 64;	// timeout after 128 ms
						PCA0POL = PCA0POL_CEX0POL__INVERT | PCA0POL_CEX1POL__INVERT;
						PCA0L = 0;
						PCA0H = 0;








			measurement_curState = MEAS_STATE_C_START;
			/* no break */
		case MEAS_STATE_C_START:    // fall through
			//meas_curCurrent = CURR_02;  // Current setting = 0.2 mA
			set_isrc(CURR_DISC);     // Start discharge

			CMP0CN1 = (8 << CMP0CN1_DACLVL__SHIFT);

			meas_wait_start(10); // Wait for discharge
			measurement_curState = MEAS_STATE_C_DIS_CHECK_0;    // Next step is to check DUT charge
			break;
		case MEAS_STATE_C_DIS_CHECK_0:
			set_isrc(CURR_PD);	// Leave weak pull-down enabled to avoid ghost voltages
			meas_wait_start(2); // Wait for voltage to stabilize
			//meas_wait_start(300); // Wait for voltage to stabilize
			measurement_curState = MEAS_STATE_C_DIS_CHECK_1;
			break;
		case MEAS_STATE_C_DIS_CHECK_1:
			//ADC0CN0_ADBUSY = 1;	// start ADC

			if (CMP0CN0 & CMP0CN0_CPOUT__BMASK)
			{
				// discharged below threshold, continue
				set_isrc(CURR_DISC);
				CMP0CN1 = (13 << CMP0CN1_DACLVL__SHIFT);	// set new comparator level
				measurement_curState = MEAS_C_CHG_1;  		// Next step is to start charging DUT
			}
			else
			{
				// need to keep discharging
				measurement_curState = MEAS_STATE_C_START; // Resume discharge
			}

			meas_wait_start(10);

			//measurement_curState = MEAS_STATE_C_DIS_CHECK_2;
			break;
		case MEAS_C_CHG_1:

			// Configure PCA0
			PCA0CPL0 = 0;	// (2) disable discharge after 500 us
			PCA0CPH0 = 64 / 64;	// disable discharge after 64 ms
			PCA0CPL1 = 1000 / 250;	// (1) enable charge after 250 us
			PCA0CPH1 = 64 / 64;	// enable charge after 65 ms
			PCA0CPL2 = 0;
			//PCA0CPH2 = 320 / 64;	// timeout after 320 ms
			PCA0CPH2 = 128 / 64;	// timeout after 128 ms
			PCA0L = 0;
			PCA0H = 0;
			PCA0CN0_CR = 1;	// start PCA (no clock enabled yet!)


			// force overflow (switch off PWM)
			TCON_TR0 = 0;	// stop Timer0
			PCA_pwmReset();

			set_isrc(CURR_OFF);


			// select current
			if (meas_sel_curr_c == CURR_OFF)
				meas_sel_curr_c = CURR_02;

			meas_curCurrent = meas_sel_curr_c;


			select_isrc(meas_sel_curr_c);

			// configure IRQs

			PCA0CN0_CCF2 = 0;	// Clear PCA IRQ
			CMP0CN0 &= ~(CMP0CN0_CPRIF__BMASK | CMP0CN0_CPFIF__BMASK);	// Clear CMP0 IRQs
			CMP1CN0 &= ~(CMP1CN0_CPRIF__BMASK | CMP1CN0_CPFIF__BMASK);	// Clear CMP1 IRQs
			//EIE1 |= EIE1_ECP0__ENABLED | EIE1_ECP1__ENABLED | EIE1_EPCA0__ENABLED;	// Enable PCA, Comparator IRQs

			//SFRPAGE = 0x10;
			//TMR4RL = 0xFFFF;
			//TMR4CN0_TF4H = 0;	// Clear Timer4 IRQs
			//TMR4CN0_TF4L = 0;
			//EIE2 |= EIE2_ET4__ENABLED;	// Enable Timer4 IRQ
			//SFRPAGE = 0x00;



			SFRPAGE = 0x10;
			IE_EA = 0;
			TMR4CN0 |= TMR4CN0_TR4__RUN;
			TMR3CN0 |= TMR3CN0_TR3__RUN;
			SFRPAGE = 0x00;

			TCON_TR0 = 1;	// start Timer 0 (provide clock to PCA)

			measurement_curState = MEAS_C_CHG_2;

			//EIE1 |= EIE1_ECP0__ENABLED | EIE1_ECP1__ENABLED | EIE1_EPCA0__ENABLED;	// Enable PCA, Comparator IRQs
			EIE1 |= EIE1_ECP0__ENABLED | EIE1_EPCA0__ENABLED;	// Enable PCA, Comparator IRQs
			IE_EA = 1;


			break;
		case MEAS_C_CHG_4:
			// meas complete

			if (meas_sel_curr & CURR_200)
				res_val = 33.0f/2.0f;
			else if (meas_sel_curr & CURR_20)
				res_val = 165.0f;
			else if (meas_sel_curr & CURR_2)
				res_val = 1650.0f;
			else
				res_val = 16500.0f;


			//meas_c_result.c_time = (uint32_t)((uint32_t)((uint32_t)t4_tmp2 << 16) | t3_tmp2) - (uint32_t)((uint32_t)((uint32_t)t4_tmp << 16) | t3_tmp);
			meas_c_result.c_time = end_time - start_time;

			//meas_c_result.capacitance = (-(meas_c_result.c_time / 48.0f)) / ( (res_val + esr) * log(   ((0.90625f / 16.0f) - 1)  / ((0.09375f / 16.0f) - 1)    ));
			//meas_c_result.capacitance = (-(meas_c_result.c_time / 48.0f)) / ( (res_val + esr) * log(   ((0.90625f / 16.0f) - 1)  / ((0.203125f / 16.0f) - 1)    ));


			//meas_dcr_result.dcr=0;
			meas_c_result.capacitance = (-(meas_c_result.c_time / 24.5f)) / ( (res_val) * log(   ((0.90625f / 16.0f) - 1)  / ((0.203125f / 16.0f) - 1)    ));








	    	//measurement_curState = MEAS_STATE_C_START;
			measurement_curState = MEAS_STATE_IDLE;
			measurement_reqState = MEAS_ESR_INIT;

	    	//meas_wait_start(500);





			break;

		case MEAS_ESR_INIT:
			// init ESR measurement

			pga_SetGain(PGA_GAIN_32);


			// setup ADC
			ADC0CN1 = ADC0CN1_ADCM__TIMER5 | ADC0CN1_ADCMBE__CM_BUFFER_ENABLED;
			ADC0MX = ADC_IN_B;
			ADC0CF = (0x01 << ADC0CF_ADSC__SHIFT) | ADC0CF_ADGN__GAIN_1;
			ADC0AC = ADC0AC_AD12BE__12_BIT_ENABLED | ADC0AC_ADRPT__ACC_4;
			ADC0TK = ADC0TK_AD12SM__SAMPLE_ONCE | (63 << ADC0TK_ADTK__SHIFT);
			ADC0PWR = (0x0F << ADC0PWR_ADPWR__SHIFT) | ADC0PWR_ADBIAS__MODE1;
			ADC0CN0 |= ADC0CN0_ADEN__ENABLED | ADC0CN0_ADBMEN__BURST_ENABLED;


			// Configure CLU3

			/***********************************************************************
			 - CLU3 Look-Up-Table function select = 0x0F
			 - Select CLU3A.4
			 - Select CLU3B.3
			***********************************************************************/
			SFRPAGE = 0x20;
			CLU3FN = 0x0F; // OUT = !CEX2 (MXA)
			CLU3MX = CLU3MX_MXA__CLU3A5 | CLU3MX_MXB__CLU3B3;	// MXA = CEX2
			CLU3CF = CLU3CF_OUTSEL__LUT;
			CLEN0 = CLEN0_C3EN__ENABLE;
			SFRPAGE = 0x00;



			// Configure Timer5
			/***********************************************************************
			 - Force reload on CLU3 high
			 - Start Timer 5
			***********************************************************************/
			SFRPAGE = 0x10;
			CKCON1 &= ~(CKCON1_T5ML__BMASK);
			CKCON1 |= (CKCON1_T5ML__SYSCLK);
			TMR5RL = (65536-264);	// SYSCLK / 264 (~11 us)
			TMR5   = (65536-264);   // SYSCLK / 264 (~11 us)
			TMR5CN1 = TMR5CN1_RLFSEL__CLU3_OUT;
			TMR5CN0_TR5 = 1;
			SFRPAGE = 0x00;



			/***********************************************************************
			 - Counter/Timer 0 uses the SYSCLK
			***********************************************************************/
			CKCON0 &= ~CKCON0_T0M__BMASK;
			CKCON0 |= CKCON0_T0M__SYSCLK;

			// Configure Timer0
			/***********************************************************************
			 - Timer 0: Timer mode 2, 8-bit Counter/Timer with Auto-Reload
			 - Timer 0 enabled when TR0 = 1 irrespective of INT0 logic level
			 - Timer 0 auto reload = 0xE8 (1 MHz overflow rate)
			***********************************************************************/
			TMOD &= ~TMOD_T0M__FMASK;
			TMOD |= TMOD_T0M__MODE2;
			TH0  = (256-24);	// SYSCLK / 24 (~1 us)
			TL0  = (256-24);	// SYSCLK / 24 (~1 us)
			//TCON_TR0 = 1;



			// Configure PCA0
			/***********************************************************************
			 - PCA continues to function normally while the system controller is in Idle Mode
			 - Clock = Timer 0 overflow
			 - Center-aligned PWM for CH0-2
			 - Use Comparator 0 for the comparator clear function
			 - Enable the comparator clear function on PCA channel 0-2
			 - PWM length = 11 bits
			 - Enable comparator function
			 - Enable PWM function
			 - PCA Channel 0 Capture Module Low Byte = 0xE9
			 - PCA Channel 0 Capture Module High Byte = 0x07
			 - PCA Channel 1 Capture Module Low Byte = 0xF8
			 - PCA Channel 1 Capture Module High Byte = 0x07
			 - PCA Channel 2 Capture Module Low Byte = 0xE9
			 - PCA Channel 2 Capture Module High Byte = 0x07
			***********************************************************************/
			PCA0MD = PCA0MD_CIDL__NORMAL | PCA0MD_ECF__OVF_INT_DISABLED | PCA0MD_CPS__T0_OVERFLOW;
			PCA0CENT = PCA0CENT_CEX0CEN__CENTER | PCA0CENT_CEX1CEN__CENTER | PCA0CENT_CEX2CEN__CENTER;
			PCA0CLR = PCA0CLR_CPCE0__ENABLED | PCA0CLR_CPCE1__ENABLED | PCA0CLR_CPCE2__ENABLED;
			PCA0PWM = PCA0PWM_CLSEL__11_BITS;
			PCA0CPM0 = PCA0CPM0_ECOM__ENABLED | PCA0CPM0_PWM__ENABLED;
			PCA0CPM1 = PCA0CPM1_ECOM__ENABLED | PCA0CPM1_PWM__ENABLED;
			PCA0CPM2 = PCA0CPM2_ECOM__ENABLED | PCA0CPM2_PWM__ENABLED;
			PCA0CPL0 = (4095 - ESR_DIS_TIME) / 2;
			PCA0CPH0 = ((4095 - ESR_DIS_TIME) / 2) >> 8;
			PCA0CPL1 = (4095 - ESR_CHG_TIME) / 2;
			PCA0CPH1 = ((4095 - ESR_CHG_TIME) / 2) >> 8;
			PCA0CPL2 = (4095 - ESR_ADC_TIME) / 2;
			PCA0CPH2 = ((4095 - ESR_ADC_TIME) / 2) >> 8;
			PCA0PWM |= PCA0PWM_ARSEL__AUTORELOAD;
			PCA0CPL0 = (4095 - ESR_DIS_TIME) / 2;
			PCA0CPH0 = ((4095 - ESR_DIS_TIME) / 2) >> 8;
			PCA0CPL1 = (4095 - ESR_CHG_TIME) / 2;
			PCA0CPH1 = ((4095 - ESR_CHG_TIME) / 2) >> 8;
			PCA0CPL2 = (4095 - ESR_ADC_TIME) / 2;
			PCA0CPH2 = ((4095 - ESR_ADC_TIME) / 2) >> 8;
			PCA0PWM &= ~PCA0PWM_ARSEL__BMASK;
			PCA0POL = PCA0POL_CEX0POL__INVERT | PCA0POL_CEX1POL__INVERT;







			//P1SKIP = 0x5D;	// select 20 mA
			//P1SKIP = 0x3D;	// select 200 mA
			//P1SKIP = 0x75;	// select 2 mA

			//XBR1 |= XBR1_T2E__ENABLED;
			//XBR1 = XBR1_PCA0ME__CEX0_CEX1_CEX2;


			PCA_pwmReset();

			set_isrc(CURR_OFF);
			select_isrc(CURR_20);	// select 20 mA


			//TMR2L = 0xFE;

			esr_adc_step = 0;

			measurement_curState = MEAS_ESR_MEAS;
			EIE1 |= EIE1_EADC0__ENABLED;	// Enable ADC IRQ

			// start PCA
			PCA0CLR = 0;
			PCA0CN0_CR = 1;
			TCON_TR0 = 1;


			break;
		case MEAS_ESR_READY:
			// esr ready

			if (meas_sel_curr & CURR_200)
				res_val = 33.0f/2.0f;
			else if (meas_sel_curr & CURR_20)
				res_val = 165.0f;
			else if (meas_sel_curr & CURR_2)
				res_val = 1650.0f;
			else
				res_val = 16500.0f;


			meas_dcr_result.dcr = (esr[1] - esr[0]);

			meas_dcr_result.dcr *= res_val*1000.0;
			meas_dcr_result.dcr /= (32.0*1023.0*16.0*4);
			meas_dcr_result.dcr -= 10.0;

			measurement_curState = MEAS_STATE_IDLE;
			//measurement_curState = MEAS_ESR_MEAS;
			measurement_reqState = MEAS_STATE_C_INIT;

			break;
	}


	//while (esr_adc_step != 1);



}


//void measurement_esrStart()
//{
//	// Configure CLU3
//
//	/***********************************************************************
//	 - CLU3 Look-Up-Table function select = 0x0F
//	 - Select CLU3A.4
//	 - Select CLU3B.3
//	***********************************************************************/
//	SFRPAGE = 0x20;
//	CLU3FN = 0x0F; // OUT = !CEX2 (MXA)
//	CLU3MX = CLU3MX_MXA__CLU3A5 | CLU3MX_MXB__CLU3B3;	// MXA = CEX2
//
//	SFRPAGE = 0x00;
//
//	// setup ADC
//	ADC0CN1 = ADC0CN1_ADCM__TIMER5 | ADC0CN1_ADCMBE__CM_BUFFER_ENABLED;
//	ADC0MX = ADC0MX_ADC0MX__ADC0P8;
//	ADC0CF = (0x01 << ADC0CF_ADSC__SHIFT) | ADC0CF_ADGN__GAIN_1;
//	ADC0AC = ADC0AC_AD12BE__12_BIT_ENABLED | ADC0AC_ADRPT__ACC_4;
//	REF0CN = REF0CN_TEMPE__TEMP_ENABLED | REF0CN_GNDSL__AGND_PIN | REF0CN_IREFLVL__2P4 | REF0CN_REFSL__VDD_PIN;
//	ADC0CN0 |= ADC0CN0_ADEN__ENABLED | ADC0CN0_ADBMEN__BURST_ENABLED;
//
//
//
//
//	// Configure Timer5
//	/***********************************************************************
//	 - Force reload on CLU3 high
//	 - Start Timer 5
//	***********************************************************************/
//	SFRPAGE = 0x10;
//	TMR5RL = 65008;
//	TMR5 = 65008;
//	TMR5CN1 = TMR5CN1_RLFSEL__CLU3_OUT;
//	TMR5CN0_TR5 = 1;
//	SFRPAGE = 0x00;
//
//
//
//	//P1SKIP = 0x5D;	// select 20 mA
//	P1SKIP = 0x3D;	// select 200 mA
//	//P1SKIP = 0x75;	// select 2 mA
//
//	//XBR1 |= XBR1_T2E__ENABLED;
//	//XBR1 = XBR1_PCA0ME__CEX0_CEX1_CEX2;
//
//
//
//	// start PCA
//	PCA0CLR = 0;
//	PCA0CN0_CR = 1;
//	//TMR2L = 0xFE;
//
//
//
//
//
//
//
//}













