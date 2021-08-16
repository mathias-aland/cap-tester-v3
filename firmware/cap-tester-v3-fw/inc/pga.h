#define PGA_GAIN_1  0
#define PGA_GAIN_2  1
#define PGA_GAIN_4  2
#define PGA_GAIN_5  3
#define PGA_GAIN_8  4
#define PGA_GAIN_10 5
#define PGA_GAIN_16 6
#define PGA_GAIN_32 7


#define PGA_CMD_NOP   0x00
#define PGA_CMD_SHDN  0x20
#define PGA_CMD_GAIN  0x40
#define PGA_CMD_CH    0x41


void pga_SetGain(uint8_t gain);
