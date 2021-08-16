/*
 * globals.h
 *
 *  Created on: 14 juli 2020
 *      Author: Mathias
 */

#ifndef INC_GLOBALS_H_
#define INC_GLOBALS_H_

#include <si_toolchain.h>

extern bool rf_tx_done_pend;
extern bool rf_rx_done_pend;
extern uint8_t last_tx_seq;
extern SI_SEGMENT_VARIABLE(rx_buffer[32], uint8_t, SI_SEG_XDATA);
extern uint8_t rx_pkt_len;

#endif /* INC_GLOBALS_H_ */
