/*
 * globals.c
 *
 *  Created on: 14 juli 2020
 *      Author: Mathias
 */
#include <si_toolchain.h>

bool rf_tx_done_pend = false;
bool rf_rx_done_pend = false;
uint8_t last_tx_seq = 0;
SI_SEGMENT_VARIABLE(rx_buffer[32], uint8_t, SI_SEG_XDATA);
uint8_t rx_pkt_len = 0;
