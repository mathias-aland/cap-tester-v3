/******************************************************************************
 * Copyright (c) 2015 by Silicon Laboratories Inc. All rights reserved.
 *
 * http://developer.silabs.com/legal/version/v11/Silicon_Labs_Software_License_Agreement.txt
 *****************************************************************************/

#ifndef __BOOT_H__
#define __BOOT_H__


#define USB_MAGIC    0xA6

/// command byte definitions
#define USB_CMD_IDENT       0
#define USB_CMD_GET_FREQ    1
#define USB_CMD_SET_FREQ    2
#define USB_CMD_SEND_PKT    3
#define USB_CMD_PKT_RECV    90

/// Bootloader response byte definitions
#define BOOT_ACK_REPLY    '@'
#define BOOT_TXDONE       'Y'

#define BOOT_ERR_RANGE    'A'
#define BOOT_ERR_BADID    'B'
#define BOOT_ERR_CRC      'C'
#define BOOT_ERR_DUP      'D'
#define BOOT_ERR_MAGIC    'E'
#define BOOT_ERR_COMPL    'F'
#define BOOT_ERR_CMD      'G'
#define BOOT_ERR_BUSY     'H'

/// This array provides access to the bootloader signature and lock byte.
///   boot_otp[0] = bootloader signature byte
///   boot_otp[1] = flash lock byte
extern uint8_t SI_SEG_CODE boot_otp[];

// Used to implement boot_hasRemaining() macro below
extern uint8_t boot_rxSize;

/**************************************************************************//**
 * Initialize device hardware and start the communication channel.
 *****************************************************************************/
extern void boot_initDevice(void);


bool boot_dataAvail(void);
void boot_bufRst(void);

/**************************************************************************//**
 * Wait for the next boot record to arrive.
 *****************************************************************************/
extern void boot_nextRecord(void);

/**************************************************************************//**
 * Get the next byte in the boot record.
 *
 * @return The next byte from the boot record
 *****************************************************************************/
extern uint8_t boot_getByte(void);

/**************************************************************************//**
 * Get the next word in the boot record.
 *
 * @return The next 16-bit word from the boot record
 *****************************************************************************/
extern uint16_t boot_getWord(void);

/**************************************************************************//**
 * Returns the number of bytes remaining in the boot record.
 *
 * @return The number of bytes remaining in the current boot record
 *****************************************************************************/
#if defined(IS_DOXYGEN)
extern uint8_t boot_hasRemaining(void);
#else
#define boot_hasRemaining(i) (boot_rxSize)
#endif

/**************************************************************************//**
 * Send a one byte reply to the host.
 *
 * @param reply The byte to send.
 *****************************************************************************/
extern void boot_sendReply(uint8_t reply);

/**************************************************************************//**
 * Exit bootloader and start the user application.
 *****************************************************************************/
extern void boot_runApp(void);

#endif // __BOOT_H__
