/*
 * spi.h
 *
 *  Created on: 16 feb. 2018
 *      Author: Mathias
 */

#ifndef INC_SPI_H_
#define INC_SPI_H_

#define RF_REG_FIFO                 0x00
#define RF_REG_OP_MODE              0x01
#define RF_REG_FRF_MSB              0x06
#define RF_REG_FRF_MID              0x07
#define RF_REG_FRF_LSB              0x08
#define RF_REG_PA_CONFIG            0x09
#define RF_REG_LNA                  0x0c
#define RF_REG_FIFO_ADDR_PTR        0x0d
#define RF_REG_FIFO_TX_BASE_ADDR    0x0e
#define RF_REG_FIFO_RX_BASE_ADDR    0x0f
#define RF_REG_FIFO_RX_CURRENT_ADDR 0x10
#define RF_REG_IRQ_FLAGS            0x12
#define RF_REG_RX_NB_BYTES          0x13
#define RF_REG_PKT_SNR_VALUE        0x19
#define RF_REG_PKT_RSSI_VALUE       0x1a
#define RF_REG_MODEM_CONFIG_1       0x1d
#define RF_REG_MODEM_CONFIG_2       0x1e
#define RF_REG_PREAMBLE_MSB         0x20
#define RF_REG_PREAMBLE_LSB         0x21
#define RF_REG_PAYLOAD_LENGTH       0x22
#define RF_REG_MODEM_CONFIG_3       0x26
#define RF_REG_RSSI_WIDEBAND        0x2c
#define RF_REG_DETECTION_OPTIMIZE   0x31
#define RF_REG_DETECTION_THRESHOLD  0x37
#define RF_REG_SYNC_WORD            0x39
#define RF_REG_DIO_MAPPING_1        0x40
#define RF_REG_VERSION              0x42

// modes
#define MODE_LONG_RANGE_MODE     0x80
#define MODE_SLEEP               0x00
#define MODE_STDBY               0x01
#define MODE_TX                  0x03
#define MODE_RX_CONTINUOUS       0x05
#define MODE_RX_SINGLE           0x06


// PA config
#define PA_BOOST                 0x80

// IRQ masks
#define IRQ_TX_DONE_MASK           0x08
#define IRQ_PAYLOAD_CRC_ERROR_MASK 0x20
#define IRQ_RX_DONE_MASK           0x40

#define MAX_PKT_LENGTH           255

void RF_write_reg(uint8_t addr, uint8_t dat);
void RF_write_reg_burst(uint8_t addr, uint8_t *dat, uint8_t len);
uint8_t RF_read_reg(uint8_t addr);
void RF_read_reg_burst(uint8_t addr, uint8_t *dat, uint8_t len);
uint32_t RF_getFreqKhz();
bool RF_setFreqKhz(uint32_t freq);
void RF_init();
bool RF_setPower(uint8_t txPower);
uint8_t lora_send_pkt(uint8_t *pktbuf, uint8_t pktlen);


#endif /* INC_SPI_H_ */
