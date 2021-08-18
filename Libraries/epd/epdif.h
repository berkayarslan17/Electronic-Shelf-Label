/** @file epdif.h
 *
 * @brief Header file of epdif.c providing EPD interface functions
 *
 * @par
 * 
 * 
*/

#ifndef EPDIF_H
#define EPDIF_H

#include "app_error.h"

#define EPD_BUSY_PIN        4
#define EPD_RST_PIN         5
#define EPD_DC_PIN          6
#define EPD_SPI_CS_PIN      9
#define EPD_SPI_SCK_PIN     14
#define EPD_SPI_MOSI_PIN    12
#define EPD_EP_EN_PIN       10
#define LED_RED_PIN         25
#define LED_GREEN_PIN       28
#define LED_BLUE_PIN        30

// Pin definition
#define CS_PIN           0
#define RST_PIN          1
#define DC_PIN           2
#define BUSY_PIN         3

// Pin level definition
#define LOW             0
#define HIGH            1

#define WSEPD_SPI_INSTANCE  0

typedef struct
{
  int pin;
} EPD_Pin;

ret_code_t epd_spi_init(void);
void EPD_GPIO_Init(void);

int EpdInitCallback(void);
void EpdDigitalWriteCallback(int pin, int value);
int EpdDigitalReadCallback(int pin);
void EpdDelayMsCallback(unsigned int delaytime);
void EpdSpiTransferCallback(const void *, size_t);

#endif /* EPDIF_H */
