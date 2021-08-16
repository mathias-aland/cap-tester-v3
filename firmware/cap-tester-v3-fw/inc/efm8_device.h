/******************************************************************************
 * Copyright (c) 2015 by Silicon Laboratories Inc. All rights reserved.
 *
 * http://developer.silabs.com/legal/version/v11/Silicon_Labs_Software_License_Agreement.txt
 *****************************************************************************/

#ifndef __EFM8_DEVICE_H__
#define __EFM8_DEVICE_H__

#ifdef __C51__
#include "SI_EFM8UB3_Register_Enums.h"
#else
#include "SI_EFM8UB3_Defs.inc"
#endif

// Select the STK device if one has not been specified by the project
#ifndef EFM8UB3_DEVICE
#define EFM8UB3_DEVICE EFM8UB31F40G_QSOP24
#endif
#include "SI_EFM8UB3_Devices.h"


// USB Vendor and Product ID's
// WARNING: pid.codes test PID !!!
#define BL_USB_VID 0x1209
#define BL_USB_PID 0x0002

// Defines for managing SFR pages (used for porting between devices)
#define SET_SFRPAGE(p)  SFRPAGE = (p)


#define CURR_OFF     0x00
#define CURR_02      0x01
#define CURR_2       0x02
#define CURR_20      0x04
#define CURR_200     0x08
#define CURR_DISC    0x10
#define CURR_PD		 0x20

#define ISRC_02_PIN     P1_B2
#define ISRC_2_PIN      P1_B3
#define ISRC_20_PIN     P1_B5
#define ISRC_200_PIN    P1_B6
#define ISRC_DISC_PIN   P1_B1
#define ISRC_PD_PIN   	P1_B4



#define ADC_IN_A		ADC0MX_ADC0MX__ADC0P7
#define ADC_IN_B		ADC0MX_ADC0MX__ADC0P8
#define ADC_IN_CAL		ADC0MX_ADC0MX__ADC0P12


/*
   * Divide positive or negative dividend by positive divisor and round
   * to closest integer. Result is undefined for negative divisors and
   * for negative dividends if the divisor variable type is unsigned.
   */
  #define DIV_ROUND_CLOSEST(x, divisor)(                  \
  {                                                       \
          typeof(x) __x = x;                              \
          typeof(divisor) __d = divisor;                  \
         (((typeof(x))-1) > 0 ||                         \
          ((typeof(divisor))-1) > 0 || (__x) > 0) ?      \
                 (((__x) + ((__d) / 2)) / (__d)) :       \
                 (((__x) - ((__d) / 2)) / (__d));        \
 }                                                       \
 )




#endif // __EFM8_DEVICE_H__
