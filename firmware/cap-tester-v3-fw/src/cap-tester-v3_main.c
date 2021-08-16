//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include <SI_EFM8UB3_Register_Enums.h>                // SFR declarations
#include <string.h>
#include <stdio.h>
#include <systick.h>
#include "efm8_device.h"
#include "boot.h"
#include "USB_main.h"
#include "spi.h"
#include "pga.h"
#include "globals.h"
#include "ssd1306/ssd1306_tests.h"
#include "ssd1306/ssd1306.h"
#include "meas.h"
#include "delay.h"


enum main_menu_id
{
	MNU_ID_NONE,
	MNU_ID_SETTINGS,
	MNU_ID_CALIBRATE,
	MNU_ID_SELFTEST,
	MNU_ID_ABOUT
};

enum setting_type
{
	SETTING_TYPE_NONE,
	SETTING_TYPE_U8,
	SETTING_TYPE_I8,
	SETTING_TYPE_I16
};


struct menu_top {
	char code *title;
	uint8_t num_entries;
	struct menu_top code *parent;
	struct menu_view code *menuList;
};

struct menu_setting {
	char code *title;
	enum setting_type type;
	int16_t min_value;
	int16_t max_value;
	struct menu_top code *parent;
	char code **mapStrings;
};


struct menu_top code mnuMain;
struct menu_top code mnuSettings;

struct menu_view {
    /* Menu navigation */
//	const struct menu_view code  *back;
//	const struct menu_view code  *enter;
//	const struct menu_view code  *prev;
//	const struct menu_view code  *next;
	struct menu_top code *child;
	//struct menu_setting code *child_setting;
	//uint8_t code *prev;
	//uint8_t code *next;

    /* Name or title of this menu entry */
    char code *title;

    /* RAM data for this menu entry, if any -- can be any type */
    //void  *data;

    /* Function to draw the rest of the display (except title); last parameter is dwell time in milliseconds */
    void (*update)(const struct menu_view code *, void *, unsigned int);

    /* Events; last parameter is the time since last event in microseconds */
    void (*increment)(const struct menu_view code *, void *, unsigned int);
    void (*decrement)(const struct menu_view code *, void *, unsigned int);
    void (*press)(const struct menu_view code *, void *, unsigned int);
    void (*release)(const struct menu_view code *, void *, unsigned int);
};




struct menu_view code mnuMainList[] = {
		{&mnuSettings, "Settings", NULL, NULL, NULL, NULL, NULL},
		{NULL, "Calibrate", NULL, NULL, NULL, NULL, NULL},
		{NULL, "Self test", NULL, NULL, NULL, NULL, NULL},
		{NULL, "About", NULL, NULL, NULL, NULL, NULL}
};

struct menu_top code mnuMain =
{
		"Main menu",
		4,
		NULL,
		&mnuMainList
};


struct menu_view code mnuSettingsList[] = {
		{NULL, "Brightness", NULL, NULL, NULL, NULL, NULL},
		{NULL, "Screensaver", NULL, NULL, NULL, NULL, NULL},
		{NULL, "Powerbank mode", NULL, NULL, NULL, NULL, NULL},
		{NULL, "Aut. r. speed", NULL, NULL, NULL, NULL, NULL},
		{NULL, "Save settings", NULL, NULL, NULL, NULL, NULL}
};

struct menu_top code mnuSettings =
{
		"Settings",
		5,
		&mnuMain,
		&mnuSettingsList
};


char code *mapDisEn[] = {
		"Disabled",
		"Enabled"
};

struct menu_setting code settingScrnSaver = {
		"Screensaver",
		SETTING_TYPE_U8,
		0,
		1,
		&mnuSettingsList,
		mapDisEn
};

struct menu_setting code settingScrnBri = {
		"Brightness",
		SETTING_TYPE_U8,
		0,
		255,
		&mnuSettingsList,
		NULL
};


//struct menu_view code mnuMain1, mnuMain2, mnuMain3, mnuMain4;
//
//
//struct menu_view code mnuMain1 = {NULL, NULL, NULL, &mnuMain2, "Settings", NULL, NULL, NULL, NULL, NULL};
//struct menu_view code mnuMain2 = {NULL, NULL, &mnuMain1, &mnuMain3, "Calibrate", NULL, NULL, NULL, NULL, NULL};
//struct menu_view code mnuMain3 = {NULL, NULL, &mnuMain2, &mnuMain4, "Self test", NULL, NULL, NULL, NULL, NULL};
//struct menu_view code mnuMain4 = {NULL, NULL, &mnuMain3, NULL, "About", NULL, NULL, NULL, NULL, NULL};


struct menu_top *curr_mnu = NULL;
uint8_t curr_mnu_item = 0;

uint8_t curr_mnu_offset = 0;


/* USB packet data structure
 * Addr		| Description
 * -------------------------
 * 0		| "Magic byte" (0xA6)
 * 1		| Sequence number (0-255)
 * 2		| Command (0-63)
 * 3-63		| Command-specific data
 */






#define SW_VER_MAJOR	0
#define SW_VER_MINOR	0
#define SW_VER_BUILD	1

//#define ESR_CHG_TIME	9	// Charge pulse length = 9 us
//#define ESR_DIS_TIME	27	// Discharge release length = 27 us
//#define ESR_ADC_TIME	27	// ADC clock gate length = 27 us



// Converts command byte into zero based opcode (makes code smaller)
//#define OPCODE(cmd) ((cmd) - BOOT_CMD_IDENT)

// Holds the current command opcode
//static uint8_t opcode;

// Holds reply to the current command
static uint8_t reply;

uint8_t  usb_seq_last;
bool usb_seq_init = false;

//#define BL_DERIVATIVE_ID  (0x3200 | DEVICE_DERIVID)


uint8_t rf_ver;

char xdata str[20];




void usb_pkt_process();




//uint8_t sfr_save;
//uint32_t tmp32;
//uint8_t result;

  //uint32_t mV;                         // Measured voltage in mV
  uint32_t xdata result2;

  uint32_t xdata vdd;
  uint32_t xdata ldo;
  uint32_t xdata in;
  uint32_t xdata fet;
  float xdata dcr;
  float xdata zero = 0;

  uint8_t acc_cnt;


  uint8_t btn;


//-----------------------------------------------------------------------------
// SiLabs_Startup() Routine
// ----------------------------------------------------------------------------
// This function is called immediately after reset, before the initialization
// code is run in SILABS_STARTUP.A51 (which runs before main() ). This is a
// useful place to disable the watchdog timer, which is enable by default
// and may trigger before main() in some instances.
//-----------------------------------------------------------------------------
void SiLabs_Startup (void)
{
	//Disable Watchdog with key sequence
	WDTCN = 0xDE;
	WDTCN = 0xAD;

	// Disable 3.3 V regulator
	SET_SFRPAGE(PG3_PAGE);
	REG1CN = REG1CN_REG1ENB__DISABLED | REG1CN_BIASENB__DISABLED;

	// PFE0CN - Prefetch Engine Control
	// PFEN (Prefetch Enable) = ENABLED (Enable the prefetch engine (SYSCLK > 25 MHz).)
	// FLRT (Flash Read Timing) = SYSCLK_BELOW_50_MHZ (SYSCLK < 50 MHz.)
	SET_SFRPAGE(PG2_PAGE);
	//PFE0CN |= PFE0CN_FLRT__SYSCLK_BELOW_50_MHZ;

	/***********************************************************************
	 - Ensure SYSCLK is > 24 MHz before switching to HFOSC1
	 - Clock derived from the Internal High Frequency Oscillator 0
	 - SYSCLK is equal to selected clock source divided by 1
	***********************************************************************/
	CLKSEL = CLKSEL_CLKSL__HFOSC0 | CLKSEL_CLKDIV__SYSCLK_DIV_1;
	while ((CLKSEL & CLKSEL_DIVRDY__BMASK) == CLKSEL_DIVRDY__NOT_READY);
	//CLKSEL = CLKSEL_CLKSL__HFOSC1 | CLKSEL_CLKDIV__SYSCLK_DIV_1;
	//while ((CLKSEL & CLKSEL_DIVRDY__BMASK) == CLKSEL_DIVRDY__NOT_READY);

	// Source the USB clock from HFOSC1 (48 MHz)
	SET_SFRPAGE(USB0_PAGE);
	USB0CF = USB0CF_USBCLK__HFOSC1;

	// restore default page
	SET_SFRPAGE(0);


}





















//-----------------------------------------------------------------------------
// main() Routine
// ----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int main (void)
{








	// Configure Timer0
	//TMOD = TMOD_T0M__MODE1;
	//CKCON0 |= CKCON0_SCA__SYSCLK_DIV_48;

	// Configure Port 0
	P0 = 0xE7;
	P0MDOUT = 0x38;
	P0MDIN = 0x7D;
	P0SKIP = 0xFA;

	// Configure Port 1
	P1 = 0xFD;
	P1MDOUT = 0x6E;
	P1MDIN = 0x6E;
	P1SKIP = 0x79;

	// Configure Port 2
	P2 = 0x03;
	P2MDOUT = 0x00;
	SFRPAGE = 0x20;
	P2MDIN = 0x03;
	SFRPAGE = 0x00;

	// Enable crossbar
	XBR2 = XBR2_WEAKPUD__PULL_UPS_ENABLED | XBR2_XBARE__ENABLED;


	// free i2c bus if needed
	delay_us(100);	// wait for I/O to stabilize

	while (!P0_B0)
	{
		// SDA stuck low, cannot send STOP
		P0_B2 = 0;	// SCL low
		delay_us(10);
		P0_B2 = 1;	// SCL high
		delay_us(10);
	}

	// send STOP condition
	P0_B0 = 0;	// SDA low
	delay_us(10);
	P0_B0 = 1;	// SDA high
	delay_us(10);

	// Configure Crossbar

	/***********************************************************************
	 - SMBus 0 I/O routed to Port pins
	 - CEX0, CEX1 routed to Port pins
	***********************************************************************/
	XBR0 = XBR0_SMB0E__ENABLED;
	XBR1 = XBR1_PCA0ME__CEX0_CEX1;




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
//	PCA0MD = PCA0MD_CIDL__NORMAL | PCA0MD_ECF__OVF_INT_DISABLED | PCA0MD_CPS__T0_OVERFLOW;
//	PCA0CENT = PCA0CENT_CEX0CEN__CENTER | PCA0CENT_CEX1CEN__CENTER | PCA0CENT_CEX2CEN__CENTER;
//	PCA0CLR = PCA0CLR_CPCE0__ENABLED | PCA0CLR_CPCE1__ENABLED | PCA0CLR_CPCE2__ENABLED;
//	PCA0PWM = PCA0PWM_CLSEL__11_BITS;
//	PCA0CPM0 = PCA0CPM0_ECOM__ENABLED | PCA0CPM0_PWM__ENABLED;
//	PCA0CPM1 = PCA0CPM1_ECOM__ENABLED | PCA0CPM1_PWM__ENABLED;
//	PCA0CPM2 = PCA0CPM2_ECOM__ENABLED | PCA0CPM2_PWM__ENABLED;
//	PCA0CPL0 = (4095 - ESR_DIS_TIME) / 2;
//	PCA0CPH0 = ((4095 - ESR_DIS_TIME) / 2) >> 8;
//	PCA0CPL1 = (4095 - ESR_CHG_TIME) / 2;
//	PCA0CPH1 = ((4095 - ESR_CHG_TIME) / 2) >> 8;
//	PCA0CPL2 = (4095 - ESR_ADC_TIME) / 2;
//	PCA0CPH2 = ((4095 - ESR_ADC_TIME) / 2) >> 8;
//	PCA0PWM |= PCA0PWM_ARSEL__AUTORELOAD;
//	PCA0CPL0 = (4095 - ESR_DIS_TIME) / 2;
//	PCA0CPH0 = ((4095 - ESR_DIS_TIME) / 2) >> 8;
//	PCA0CPL1 = (4095 - ESR_CHG_TIME) / 2;
//	PCA0CPH1 = ((4095 - ESR_CHG_TIME) / 2) >> 8;
//	PCA0CPL2 = (4095 - ESR_ADC_TIME) / 2;
//	PCA0CPH2 = ((4095 - ESR_ADC_TIME) / 2) >> 8;
//	PCA0PWM &= ~PCA0PWM_ARSEL__BMASK;
//	PCA0POL = PCA0POL_CEX0POL__INVERT | PCA0POL_CEX1POL__INVERT;


	//PCA0CN0_CR = PCA0CN0_CR__RUN;



	// Configure timer clocks
	/***********************************************************************
	 - System clock divided by 48
	 - Counter/Timer 0 uses the system clock
	 - Timer 2 high byte uses the system clock
	 - Timer 2 low byte uses the system clock
	 - Timer 3 high byte uses the clock defined by T3XCLK in TMR3CN0
	 - Timer 3 low byte uses the system clock
	 - Timer 1 uses the system clock
	***********************************************************************/
	CKCON0 = CKCON0_SCA__SYSCLK_DIV_48 | CKCON0_T0M__SYSCLK | CKCON0_T1M__SYSCLK | CKCON0_T2MH__SYSCLK | CKCON0_T2ML__SYSCLK | CKCON0_T3ML__SYSCLK;

	// Configure Timer0/1
	/***********************************************************************
	 - Timer 0: mode 2, 8-bit Timer with Auto-Reload
	 - Timer 1: mode 2, 8-bit Timer with Auto-Reload
	 - Timer 0 enabled when TR0 = 1 irrespective of INT0 logic level
	 - Timer 1 enabled when TR1 = 1 irrespective of INT1 logic level
	 - Timer 0 auto reload = 0xD0 (1 MHz overflow rate)
	 - Timer 1 auto reload = 0xD8 (1.2 MHz overflow rate)
	 - Start Timer 0
	***********************************************************************/
	TMOD = TMOD_T0M__MODE2 | TMOD_T1M__MODE2;
	//TH0  = 0xD0;
	//TL0  = 0xD0;
	//TH1  = 0xD8;	// 48 MHz SYSCLK
	//TL1  = 0xD8;	// 48 MHz SYSCLK
	TH1  = 0xEB;	// 24.5 MHz SYSCLK
	TL1  = 0xEB;	// 24.5 MHz SYSCLK

	TCON_TR1 = 1;	// start timer1
	//TCON_TR0 = 1;
	//TCON = TCON_TR0__RUN | TCON_TR1__RUN;


	// Configure Timer2
	/***********************************************************************
	 - Timer 2 operates as one 16-bit auto-reload timer
	 - period: 1 ms
	***********************************************************************/
	//TMR2RL = 0x4480;	// 48 MHz SYSCLK
	//TMR2   = 0x4480;	// 48 MHz SYSCLK
	TMR2RL = 0xA04C;	// 24.5 MHz SYSCLK
	TMR2   = 0xA04C;	// 24.5 MHz SYSCLK
	TMR2CN0 = TMR2CN0_T2SPLIT__16_BIT_RELOAD | TMR2CN0_TR2__RUN;

	//PCA0CN0_CR = 1;	// start PCA0
	//TMR2RLL = 0xC0;
	//TMR2RLL = 0xD8;


//	// Configure Timer3
//	/***********************************************************************
//	 - Capture high-to-low transitions on the configurable logic unit 3 synchronous output
//	 - Timer will reload on overflow events and CLU2 synchronous output high
//	 - Enable capture mode
//	***********************************************************************/
	SFRPAGE = 0x10;
//	TMR3CN1 = TMR3CN1_T3CSEL__CLU3_OUT | TMR3CN1_RLFSEL__CLU2_OUT;
//	TMR3CN0 = TMR3CN0_TF3CEN__ENABLED | TMR3CN0_TR3__RUN;
//
//	// Configure Timer4
//	/***********************************************************************
//	 - Capture high-to-low transitions on the configurable logic unit 3 synchronous output
//	 - Timer will reload on overflow events and CLU2 synchronous output high
//	 - Enable capture mode
//	 - Timer 4 is clocked by Timer 3 overflows
//	***********************************************************************/
//	TMR4CN1 = TMR4CN1_T4CSEL__CLU3_OUT | TMR4CN1_RLFSEL__CLU2_OUT;
//	TMR4CN0 = TMR4CN0_TF4CEN__ENABLED | TMR4CN0_T4XCLK__TIMER3 | TMR4CN0_TR4__RUN;

	// Configure Timer5
	/***********************************************************************
	 - Timer 5 uses the system clock
	 - Force reload on CLU3 high
	 - Start Timer 5
	***********************************************************************/
	//TMR5RL = 65008;
	//TMR5 = 65008;
	CKCON1 = CKCON1_T5ML__SYSCLK | CKCON1_T5MH__SYSCLK;
	//TMR5CN1 = TMR5CN1_RLFSEL__CLU3_OUT;
	//TMR5CN0 = TMR5CN0_TR5__RUN;

	SFRPAGE = 0x00;


	// Configure CLU2 and 3

	/***********************************************************************
	 - CLU2 Look-Up-Table function select = 0xCC
	 - Select CLU2A.2
	 - Select CLU2B.4
	 - Select CLU3A.3
	 - Select CLU3B.4
	 - Select LUT output
	 - CLU0 is disabled. The output of the block will be logic low
	 - CLU1 is disabled. The output of the block will be logic low
	 - CLU2 is enabled
	 - CLU3 is enabled
	***********************************************************************/
	SFRPAGE = 0x20;

	CLU0FN = 0xCC;	// OUT = ADBUSY
	CLU0MX = CLU0MX_MXA__CLU0A0 | CLU0MX_MXB__CLU0B4;	// MXB = ADBUSY

	CLU1FN = 0xCC;	// OUT = CLU0 OUT
	CLU1MX = CLU1MX_MXA__CLU1A1 | CLU1MX_MXB__CLU1B0;	// MXB = CLU0 OUT



	CLU2FN = 0xCC;
	CLU3FN = 0xCC;
	CLU2MX = CLU2MX_MXA__CLU2A2 | CLU2MX_MXB__CLU2B4;
	CLU3MX = CLU3MX_MXA__CLU3A3 | CLU3MX_MXB__CLU3B4;

	CLU0CF = CLU0CF_OUTSEL__LUT;
	CLU1CF = CLU1CF_OUTSEL__LUT;
	CLU2CF = CLU2CF_OUTSEL__LUT;
	CLU3CF = CLU3CF_OUTSEL__LUT;
	CLEN0 = CLEN0_C0EN__ENABLE | CLEN0_C1EN__ENABLE | CLEN0_C2EN__ENABLE | CLEN0_C3EN__ENABLE;
	SFRPAGE = 0x00;


	// Configure SMBus0
	/***********************************************************************
	 - SDA Setup and Hold time is 31 SYSCLKs
	 - SDA is mapped to the lower-numbered port pin, and SCL is mapped to the higher-numbered port pin
	 - No additional SDA falling edge recognition delay
	 - Timer 2 High Byte Overflow
	 - Slave states are inhibited
	 - Enable the SMBus module
	 - Enable SDA extended setup and hold times
	***********************************************************************/
	SMB0TC = SMB0TC_DLYEXT__LONG;
	SMB0CF = SMB0CF_SMBCS__TIMER1 | SMB0CF_INH__SLAVE_DISABLED | SMB0CF_ENSMB__ENABLED | SMB0CF_EXTHOLD__ENABLED;


	// Configure ADC0
	/***********************************************************************
	 - ADC0 conversion initiated on overflow of Timer 2 when CEX2 is logic high
	 - Enable the common mode buffer
	 - Select ADC0.8
	 - SAR Clock Divider = 0x01
	 - ADC0 operates in 10-bit or 12-bit mode
	 - The on-chip PGA gain is 1
	 - Normal Track Mode
	 - Right justified. No shifting applied
	 - Enable 12-bit mode
	 - ADC0H:ADC0L contain the result of the latest conversion when Burst Mode is disabled
	 - Perform and Accumulate 4 conversions
	 - The ADC will sample the input once at the beginning of each 12-bit conversion
	 - Burst Mode Tracking Time = 63
	 - Burst Mode Power Up Time = 0x0F
	 - Disable low power mode
	 - Low power mode disabled
	 - Select bias current mode 1
	 - Enable ADC0
	 - Enable ADC0 burst mode
	***********************************************************************/
	ADC0CN1 = ADC0CN1_ADCM__ADBUSY | ADC0CN1_ADCMBE__CM_BUFFER_ENABLED;
	ADC0MX = ADC0MX_ADC0MX__ADC0P8;
	ADC0CF = (0x01 << ADC0CF_ADSC__SHIFT) | ADC0CF_ADGN__GAIN_1;
	ADC0AC = ADC0AC_AD12BE__12_BIT_ENABLED | ADC0AC_ADRPT__ACC_4;
	ADC0TK = ADC0TK_AD12SM__SAMPLE_ONCE | (63 << ADC0TK_ADTK__SHIFT);
	ADC0PWR = (0x0F << ADC0PWR_ADPWR__SHIFT) | ADC0PWR_ADBIAS__MODE1;
	//ADC0CN0 |= ADC0CN0_ADEN__ENABLED | ADC0CN0_ADBMEN__BURST_ENABLED;


	// Configure CMP0/CMP1
	/***********************************************************************
	 - External pin CMP0P.7
	 - External pin CMP0N.15
	 - External pin CMP1P.0
	 - External pin CMP1N.15
	 - CMP0 Internal Comparator DAC Reference Level = 0x15
	 - CMP1 Internal Comparator DAC Reference Level = 0x2A
	 - The comparator output will always reflect the input conditions
	 - Mode 0
	 - Connect the CMP- input to the internal DAC output, and CMP+ is selected by CMXP
	 - Comparator enabled
	***********************************************************************/
//	SFRPAGE = 0x10;
//	CMP0MX = CMP0MX_CMXP__CMP0P7 | CMP0MX_CMXN__CMP0N15;
//	CMP1MX = CMP1MX_CMXP__CMP1P0 | CMP1MX_CMXN__CMP1N15;
//	CMP0CN1 = (0x15 << CMP0CN1_DACLVL__SHIFT);
//	CMP1CN1 = (0x2A << CMP1CN1_DACLVL__SHIFT);
//	CMP0MD = CMP0MD_INSL__CMXP_DAC;
//	CMP1MD = CMP1MD_INSL__CMXP_DAC;
//	CMP0CN0 = CMP0CN0_CPEN__ENABLED;
//	CMP1CN0 = CMP1CN0_CPEN__ENABLED;
//	SFRPAGE = 0x00;


	/***********************************************************************
	 - Enable the Temperature Sensor
	 - The ADC0 ground reference is the P0.1/AGND pin
	 - The internal reference operates at 2.4 V nominal
	 - The ADC0 voltage reference is the VDD pin
	***********************************************************************/
	REF0CN = REF0CN_TEMPE__TEMP_ENABLED | REF0CN_GNDSL__AGND_PIN | REF0CN_IREFLVL__2P4 | REF0CN_REFSL__VDD_PIN;



	//IE_EA = 1;  // Enable global interrupts

	// Initialize the USB driver
	//USB_initModule();

	IE_ET2 = 1;	// enable Timer2 IRQ
	IE_EA = 1;       // Enable global interrupts


	boot_bufRst();


	//ssd1306_TestAll();

	SysTick_Delay(100);


    ssd1306_Init();




    ssd1306_Fill(Black);
    ssd1306_SetCursor(0, 0);
    ssd1306_WriteString("ESRmeter v3", Font_11x18, White);

    sprintf(str,"SW: %bu.%bu.%bu", SW_VER_MAJOR, SW_VER_MINOR, SW_VER_BUILD);

    ssd1306_SetCursor(0, 24);
    ssd1306_WriteString(str, Font_7x10, White);

    ssd1306_UpdateScreen();


    SysTick_Delay(3000);

    //ssd1306_TestAll();



    pga_SetGain(PGA_GAIN_32);


    //if (mnuMain2.next == NULL)
    //	SysTick_Delay(3000);


   // measurement_capStart();

    meas_dcr_result.zero = 0;
    meas_dcr_result.dcr = 0;
    meas_c_result.capacitance = 0;

    measurement_reqState = MEAS_ESR_INIT;

    while (1)
    {
    	measurement_stateMachine();
    	//USB_pollModule();
    	//SysTick_Delay(200);


    	// menu

    	btn = getButtonState();

    	switch (btn)
    	{
			case 0x01:	// up
			case 0x01 | 0x20:	// up (repeat)
				if (curr_mnu != NULL)
				{
					if (curr_mnu_item > 0)
					{
						curr_mnu_item--;
						if (curr_mnu_item < curr_mnu_offset)
							curr_mnu_offset--;
					}
				}
				break;
			case 0x02 | 0x80:	// menu short press (release)
				if (curr_mnu == NULL)
				{
					curr_mnu_item = 0;
					curr_mnu_offset = 0;
					curr_mnu = &mnuMain;
				}
				else if (curr_mnu->menuList[curr_mnu_item].child != NULL)
				{
					curr_mnu_item = 0;
					curr_mnu_offset = 0;
					curr_mnu = curr_mnu->menuList[curr_mnu_item].child;
				}
				break;
			case 0x02 | 0x40:	// menu long press
				if (curr_mnu == NULL)
					break;
				if (curr_mnu->parent == NULL)
				{
					curr_mnu_item = 0;
					curr_mnu_offset = 0;
					curr_mnu = NULL;
				}
				else
				{
					curr_mnu_item = 0;
					curr_mnu_offset = 0;
					curr_mnu = curr_mnu->parent;
				}
				break;
			case 0x04:	// down
			case 0x04 | 0x20:	// down (repeat)
				if (curr_mnu != NULL)
				{
					if (curr_mnu_item < (curr_mnu->num_entries-1))
					{
						curr_mnu_item++;
						if (curr_mnu_item > curr_mnu_offset+3)
							curr_mnu_offset++;
					}
				}
				else
				{
					// no menu, zero dcr/esr reading
					meas_dcr_result.zero = meas_dcr_result.dcr;
				}
				break;
    	}


    	if (curr_mnu != NULL)
    	{



			ssd1306_Fill(Black);
			ssd1306_SetCursor(0, 0);
			ssd1306_WriteString(curr_mnu->title, Font_7x10, White);

			ssd1306_Line(0,11,127,11, White);

			//ssd1306_FillRectangle(0,24,2,26,White);

			ssd1306_SetCursor(0, 17);
			ssd1306_WriteString(curr_mnu->menuList[curr_mnu_offset].title, Font_7x10, curr_mnu_offset == curr_mnu_item ? Black : White);

			ssd1306_SetCursor(0, 29);
			ssd1306_WriteString(curr_mnu->menuList[curr_mnu_offset+1].title, Font_7x10, curr_mnu_offset+1 == curr_mnu_item ? Black : White);

			ssd1306_SetCursor(0, 41);
			ssd1306_WriteString(curr_mnu->menuList[curr_mnu_offset+2].title, Font_7x10, curr_mnu_offset+2 == curr_mnu_item ? Black : White);

			ssd1306_SetCursor(0, 53);
			ssd1306_WriteString(curr_mnu->menuList[curr_mnu_offset+3].title, Font_7x10, curr_mnu_offset+3 == curr_mnu_item ? Black : White);





			ssd1306_SetCursor(0, 12);
			//ssd1306_WriteString(str, Font_7x10, White);






			ssd1306_SetCursor(0, 36);

			ssd1306_SetCursor(0, 48);
			//ssd1306_WriteString(str, Font_7x10, White);

			ssd1306_UpdateScreen();




    	}
    	else
    	{
    		// no menu active




    		ssd1306_Fill(Black);
    		//ssd1306_SetCursor(0, 0);
    		//ssd1306_WriteString("ESR meter v3", Font_7x10, White);

    		sprintf(str,"uptime: %lu s", SysTick_GetTicks() / 1000);

    		ssd1306_SetCursor(0, 0);
    		ssd1306_WriteString(str, Font_7x10, White);

    		sprintf(str,"%4.4f uF", meas_c_result.capacitance);

    		ssd1306_SetCursor(0, 24);
    		ssd1306_WriteString(str, Font_7x10, White);


    		sprintf(str,"state: %bu", measurement_curState);

    		ssd1306_SetCursor(0, 12);
    		ssd1306_WriteString(str, Font_7x10, White);






    		ssd1306_SetCursor(0, 36);


    		if (meas_sel_curr_c == CURR_02)
    			ssd1306_WriteString("Curr: 0.2 mA", Font_7x10, White);
    		else if (meas_sel_curr_c == CURR_2)
    			ssd1306_WriteString("Curr: 2 mA", Font_7x10, White);
    		else if (meas_sel_curr_c == CURR_20)
    			ssd1306_WriteString("Curr: 20 mA", Font_7x10, White);
    		else if (meas_sel_curr_c == CURR_200)
    			ssd1306_WriteString("Curr: 200 mA", Font_7x10, White);
    		else
    		{
    			sprintf(str,"Curr: 0x%bx", meas_sel_curr_c);
    			ssd1306_WriteString(str, Font_7x10, White);
    		}



    		//sprintf(str,"EIE1: %02bx", EIE1);

    		sprintf(str,"ESR:  %4.2f m", meas_dcr_result.dcr - meas_dcr_result.zero);

    		ssd1306_SetCursor(0, 48);
    		ssd1306_WriteString(str, Font_7x10, White);

    		ssd1306_UpdateScreen();


    	}








    	//PCON0 |= 0x01;
    	//PCON0 = PCON0;
    }




//    for (;;)
//    {
//    	measurement_capStart();
//
//    	while (!esr_adc_ready);
//
//
//
//
//        ssd1306_Fill(Black);
//    	ssd1306_SetCursor(0, 0);
//    	ssd1306_WriteString("ESR meter v3", Font_7x10, White);
//
//
//    	//while (esr_adc_step != 1);
//    	sprintf(str,"ESR1: %ld", esr[0]);
//
//    	ssd1306_SetCursor(0, 12);
//    	ssd1306_WriteString(str, Font_7x10, White);
//
//    	//while (esr_adc_step != 2);
//    	sprintf(str,"ESR2: %ld", esr[1]);
//
//    	ssd1306_SetCursor(0, 24);
//    	ssd1306_WriteString(str, Font_7x10, White);
//
//    	//while (esr_adc_step != 0);
//    	sprintf(str,"ESR3: %ld", esr[1] - esr[0]);
//
//    	ssd1306_SetCursor(0, 36);
//    	ssd1306_WriteString(str, Font_7x10, White);
//
//
//    	ssd1306_UpdateScreen();
//
//
//
//
//
//    	esr_adc_ready = false;
//
//
//    	SysTick_Delay(100);
//
//
//
//
//
//
//
//    }


    //SFRPAGE = 0x20;

    //CLU1CF = CLU1CF_OUTSEL__LUT | CLU1CF_OEN__ENABLE;

    //SFRPAGE = 0x00;

//    ADC0CN0_ADINT = 0;
//    EIE1 |= EIE1_EADC0__ENABLED;
//    IE |= IE_EA__ENABLED;
//
//    esr_adc_step = 0;
//
//    measurement_esrStart();
//
//	for (;;)
//	{
//
//
//    // wait for PCA overflow (sync)
//    //PCA0CN0_CF = 0;
//    //while (!PCA0CN0_CF);
//
//    // 1st sample point
//    //ADC0CN0_ADINT = 0;
//    //while (!ADC0CN0_ADINT);
//    //esr1 = ADC0;
//
//    // 2nd sample point
//    //ADC0CN0_ADINT = 0;
//    //while (!ADC0CN0_ADINT);
//    //esr2 = ADC0;
//
//    // 3rd sample point
//    //ADC0CN0_ADINT = 0;
//    //while (!ADC0CN0_ADINT);
//    //esr3 = ADC0;
//
//		while (!esr_adc_ready);
//
//
//
//    ssd1306_Fill(Black);
//	ssd1306_SetCursor(0, 0);
//	ssd1306_WriteString("ESR meter v3", Font_7x10, White);
//
//
//	//while (esr_adc_step != 1);
//	sprintf(str,"ESR1: %ld", esr[0]);
//
//	ssd1306_SetCursor(0, 12);
//	ssd1306_WriteString(str, Font_7x10, White);
//
//	//while (esr_adc_step != 2);
//	sprintf(str,"ESR2: %ld", esr[1]);
//
//	ssd1306_SetCursor(0, 24);
//	ssd1306_WriteString(str, Font_7x10, White);
//
//	//while (esr_adc_step != 0);
//	sprintf(str,"ESR3: %ld", esr[2]);
//
//	ssd1306_SetCursor(0, 36);
//	ssd1306_WriteString(str, Font_7x10, White);
//
//
//
//
//	dcr = esr[1] - esr[0];
//
//	dcr *= (16500);
//	dcr /= (32.0*1023.0*16.0*4);
//	dcr -= 10.0;
//
//	if (P0_B6 == 0)
//		zero = dcr;
//
//	dcr -= zero;
//
//	//dcr /= 134086656.0;
//
//	//sprintf(str,"DCR:  %ld m", dcr);
//
//	sprintf(str,"ESR:  %4.2f m", dcr);
//
//	ssd1306_SetCursor(0, 48);
//	ssd1306_WriteString(str, Font_7x10, White);
//
//	ssd1306_UpdateScreen();
//
//
//
//
//
//	esr_adc_ready = false;
//
//
//		//while (!ADC0CN0_ADBUSY);
//		//P1_B5 = 0;
//		//ADC0CN0_ADINT = 0;
//		//while (!ADC0CN0_ADINT);
//		//P1_B5 = 1;
//
//
//
//	}









    ADC0CN1 = ADC0CN1_ADCM__ADBUSY | ADC0CN1_ADCMBE__CM_BUFFER_ENABLED;
    ADC0AC = ADC0AC_AD12BE__12_BIT_ENABLED | ADC0AC_ADRPT__ACC_32;


    P1_B1 = 0;
    P1_B6 = 1;

    pga_SetGain(PGA_GAIN_32);


    acc_cnt = 128;
    vdd = 0;
    ldo = 0;
    in = 0;
    fet = 0;

	  // Loop until a run application command is received
	  while (true)
	  {

		  // adc


		  P1_B6 = 0;
		  delay_us(100);


		  REF0CN = REF0CN_TEMPE__TEMP_ENABLED | REF0CN_GNDSL__AGND_PIN | REF0CN_IREFLVL__2P4 | REF0CN_REFSL__INTERNAL_VREF;
		  ADC0CF = (0x01 << ADC0CF_ADSC__SHIFT) | ADC0CF_ADGN__GAIN_1;
		  ADC0MX =  ADC0MX_ADC0MX__LDO_OUT;
		  delay_us(100);
		  ADC0CN0_ADINT = 0;
		  ADC0CN0_ADBUSY = 1;
		  while (!ADC0CN0_ADINT);
		  result2 = ADC0;
		  ldo +=  (result2 * 2388) / 32736;

		  ADC0CF = (0x01 << ADC0CF_ADSC__SHIFT) | ADC0CF_ADGN__GAIN_0P5;
		  ADC0MX =  ADC0MX_ADC0MX__VDD;
		  delay_us(100);
		  ADC0CN0_ADINT = 0;
		  ADC0CN0_ADBUSY = 1;
		  while (!ADC0CN0_ADINT);
		  result2 = ADC0;
		  vdd +=  (result2 * 4776) / 32736;

		  ADC0MX =  ADC0MX_ADC0MX__ADC0P12;
		  delay_us(100);
		  ADC0CN0_ADINT = 0;
		  ADC0CN0_ADBUSY = 1;
		  while (!ADC0CN0_ADINT);
		  result2 = ADC0;
		  fet +=  (result2 * 4776) / 32736;


		  ADC0CF = (0x01 << ADC0CF_ADSC__SHIFT) | ADC0CF_ADGN__GAIN_1;
		  REF0CN = REF0CN_TEMPE__TEMP_ENABLED | REF0CN_GNDSL__AGND_PIN | REF0CN_IREFLVL__2P4 | REF0CN_REFSL__VDD_PIN;
		  ADC0MX =  ADC0MX_ADC0MX__ADC0P7;
		  delay_us(100);
		  ADC0CN0_ADINT = 0;
		  ADC0CN0_ADBUSY = 1;
		  while (!ADC0CN0_ADINT);
		  result2 = ADC0;
		  in = in + result2;
		  //in +=  (result2 * 102609) / 32736;



		  P1_B6 = 1;


		  acc_cnt--;

		  if (acc_cnt == 0)
		  {

			  ssd1306_Fill(Black);
			  //ssd1306_SetCursor(0, 0);
			  //ssd1306_WriteString("ESR meter v3", Font_7x10, White);

			  vdd /= 128;

			  sprintf(str,"VDD: %ld mV", vdd);


			  ssd1306_SetCursor(0, 0);
			  ssd1306_WriteString(str, Font_7x10, White);

			  ldo /= 128;
			  sprintf(str,"LDO: %ld mV", ldo);


			  ssd1306_SetCursor(0, 12);
			  ssd1306_WriteString(str, Font_7x10, White);


			  // dcr = (Rsource*ADCval) / (gain*ADCmax)
			  // ADCval is 16-bit
			 // dcr = (uint32_t)( 16500 * (in / 32)  ) / (uint32_t)( 32 * (1023*32) );
			  //in /= 32;
			  //dcr = (16500 * in) / 4190208;
			  dcr = in;

			  dcr *= (33000 / 2);
			  dcr /= (32.0*1023.0*32.0*128.0);
			  dcr -= 10.0;
			  //dcr /= 134086656.0;

			  //sprintf(str,"DCR:  %ld m", dcr);

			  sprintf(str,"DCR:  %4.2f m", dcr);


			  //in /= 128;
			  //sprintf(str,"IN:  %ld uV", in);






			  ssd1306_SetCursor(0, 24);
			  ssd1306_WriteString(str, Font_7x10, White);

			  fet /= 128;
			  sprintf(str,"FET:  %ld mV", fet);


			  ssd1306_SetCursor(0, 36);
			  ssd1306_WriteString(str, Font_7x10, White);


			  ssd1306_SetCursor(0, 48);
			  if (P2_B1 == 0)
			  {
				  ssd1306_WriteString("SW1", Font_7x10, White);
			  }

				ssd1306_SetCursor(40, 48);
				if (P2_B0 == 0)
				{
					ssd1306_WriteString("SW2", Font_7x10, White);
				}

				ssd1306_SetCursor(80, 48);
				if (P0_B6 == 0)
				{
					ssd1306_WriteString("SW3", Font_7x10, White);
				}



			  ssd1306_UpdateScreen();


			  acc_cnt = 128;
			  vdd = 0;
			  ldo = 0;
			  in = 0;
			  fet = 0;
		  }



		  //Timer1_Delay(65535);







//		  USB_pollModule();
//
//		  // USB cmd received?
//
//		  if ((usb_rxHead > 0) && (usb_txCount == 0))
//		  {
//			  usb_pkt_process();
//			  usb_txCount = USB_HID_IN_SIZE;
//			  boot_bufRst();
//
//			  // Reply with the results of the command
//			  //boot_sendReply(reply);
//
//
//		  }
//
//		  if (rf_tx_done_pend && (usb_txCount == 0))
//		  {
//			  // send TX DONE reply
//			  usb_txBuf[0] = USB_MAGIC;
//			  usb_txBuf[1] = last_tx_seq;
//			  usb_txBuf[2] = USB_CMD_SEND_PKT;
//			  usb_txBuf[3] = BOOT_TXDONE;
//			  usb_txBuf[4] = 0;
//			  usb_txCount = USB_HID_IN_SIZE;
//			  rf_tx_done_pend = false;
//		  }
//
//		  if (rf_rx_done_pend && (usb_txCount == 0))
//		  {
//			  // send TX DONE reply
//			  usb_txBuf[0] = USB_MAGIC;
//			  usb_txBuf[1] = ++usb_seq_last;
//			  usb_txBuf[2] = USB_CMD_PKT_RECV;
//			  usb_txBuf[3] = ~USB_CMD_PKT_RECV;
//			  usb_txBuf[4] = rx_pkt_len;
//			  usb_txBuf[5] = 0;
//			  usb_txBuf[6] = 0;
//			  memcpy(&usb_txBuf[7],rx_buffer,rx_pkt_len);
//			  usb_txCount = USB_HID_IN_SIZE;
//			  rf_rx_done_pend = false;
//		  }
//
//
//
//		  if (usb_txCount)
//		  {
//			  // send pending packet
//			  USB_sendReport();
//		  }



		  //else
		  //{
			  // Throw packet away
		//	  boot_bufRst();
		  //}




	  }







}



void usb_pkt_process()
{
	uint16_t usb_crc;
	uint8_t  usb_seq;
	uint8_t  usb_cmd;
	uint16_t usb_crc16;
	uint32_t tmp32;
	uint8_t retval;

	usb_seq = usb_rxBuf[1];
	usb_cmd = usb_rxBuf[2];


	usb_txBuf[0] = USB_MAGIC;
	usb_txBuf[1] = usb_seq;
	usb_txBuf[2] = usb_cmd;


	if (USB_MAGIC != usb_rxBuf[0])
	{
		usb_txBuf[3] = BOOT_ERR_MAGIC;
		return;
	}



	  // check sequence number
	  if (!usb_seq_init)
	  {
		  // first packet
		  usb_seq_init = true;
		  usb_seq_last = usb_seq;
	  }
	  else if (usb_seq == usb_seq_last)
	  {
		  usb_txBuf[3] = BOOT_ERR_DUP;
		  return;
	  }


	  if (~usb_cmd != usb_rxBuf[3])
	  {
		  // Wrong complement
		  usb_txBuf[3] = BOOT_ERR_COMPL;
		  return;
	  }



	    // Interpret the command opcode
	    switch (usb_cmd)
	    {
			case USB_CMD_IDENT:
				// Return an error if bootloader derivative ID does not match

				//if (BL_DERIVATIVE_ID != boot_getWord())
				//{
				usb_txBuf[3] = BOOT_ERR_BADID;
				return;
				//}
			break;

			//case USB_CMD_SEND_PKT:

				//retval = lora_send_pkt(&usb_rxBuf[7], usb_rxBuf[4]);

				//if (retval != 0)
				//{
				//	usb_txBuf[3] = BOOT_ERR_BUSY;
				//	return;
				//}

				//last_tx_seq = usb_seq;
				//break;



	      //case OPCODE(BOOT_CMD_RUNAPP):
	        // Acknowledge the command, then reset to start the user application
	       // boot_sendReply(BOOT_ACK_REPLY);
	       // boot_runApp();
	        //break;

	      default:
	        // Return FW revision for any unrecognized command
	    	 usb_txBuf[3] = BOOT_ERR_CMD;
	    	 return;
	    }

	    usb_txBuf[3] = BOOT_ACK_REPLY;




}
