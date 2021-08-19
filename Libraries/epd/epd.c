/**
 *  @filename   :   epd.c (Modified for ESL Periodic)
 *  @brief      :   Implements for Dual-color e-paper library
 *  @author     :   Yehui from Waveshare
 *
 *  Copyright (C) Waveshare     July 13 2017
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documnetation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to  whom the Software is
 * furished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdlib.h>
#include "epd.h"
#include "epdif.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "nrf_log.h"
#include "epdif.h"
#include "app_error.h"
#include "nrf_lcd.h"
#include "nrf_drv_spi.h"
#include "epdif.h"
#include "epd.h"
#include "GUI_Paint.h"
#include "fonts.h"
#include "bitmaps.h"
#include <stdio.h>
#include <string.h>

/******************************************************************************
 * Private defines
 ******************************************************************************/
#define DEVICE_NAME_MAX_LENGTH 40
#define DATA_TYPE_MAX_LENGTH 40
#define N_DATA_TYPES 6

#define PRINT_ALARM_DATA_TIMEOUT 4 // Should be smaller than accelerometer timeout

/******************************************************************************
 * Private function prototypes
 ******************************************************************************/
//static void epd_print(bool is_alarm);
//static void display_alarm_data(void);
//static void draw_no_alarm(void);
//static void draw_alarm_data(const uint8_t *data);
/******************************************************************************
 * Private variables
 ******************************************************************************/
static EPD epd;
static unsigned char BW_Image[EPD_SCREEN_HEIGHT * EPD_WIDTH / 8]; /* Define Black canvas buffer */
static unsigned char *R_Image;                                    /* Define Red canvas buffer */

static const unsigned char table[250][40] = {"empty string", "Berkay Arslan", "Ata Korkusuz"};                                                          

static bool is_print_data = false;

/**
 *  @brief: turn on epaper
 */
void paper_turn_on(void)
{
  // Turn on EPD MOSFET
  nrf_gpio_cfg_output(EPD_EP_EN_PIN);
  nrf_gpio_pin_write(EPD_EP_EN_PIN, 1); // 1 => ON, 0 => OFF
}

int EPD_Init(EPD *epd)
{
  epd->reset_pin = RST_PIN;
  epd->dc_pin = DC_PIN;
  epd->cs_pin = CS_PIN;
  epd->busy_pin = BUSY_PIN;
  epd->width = EPD_WIDTH;
  epd->height = EPD_HEIGHT;

  /* this calls the peripheral hardware interface, see epdif */
  if (EpdInitCallback() != 0)
  {
    return -1;
  }

  /* EPD hardware init start */
  EPD_Reset(epd);
  EPD_WaitUntilIdle(epd);

  EPD_SendCommand(epd, 0x12);
  EPD_WaitUntilIdle(epd);

  EPD_SendCommand(epd, 0x01);
  EPD_SendData(epd, (EPD_HEIGHT - 1) & 0xFF);
  EPD_SendData(epd, ((EPD_HEIGHT - 1) >> 8) & 0xFF);
  EPD_SendData(epd, 0x00);

  EPD_SendCommand(epd, 0x11);
  EPD_SendData(epd, 0x01);

  EPD_SendCommand(epd, 0x44); //set Ram-X address start/end position
  EPD_SendData(epd, 0x00);
  EPD_SendData(epd, 0x0F); //0x0F-->(15+1)*8=128

  EPD_SendCommand(epd, 0x45); //set Ram-Y address start/end position
  EPD_SendData(epd, (EPD_HEIGHT - 1) & 0xFF);
  EPD_SendData(epd, ((EPD_HEIGHT - 1) >> 8) & 0xFF);
  EPD_SendData(epd, 0x00);
  EPD_SendData(epd, 0x00);

  EPD_SendCommand(epd, 0x3C); //BorderWavefrom
  EPD_SendData(epd, 0x05);

  EPD_SendCommand(epd, 0x18); //Read built-in temperature sensor
  EPD_SendData(epd, 0x80);

  EPD_SendCommand(epd, 0x21); //  Display update control
  EPD_SendData(epd, 0x00);
  EPD_SendData(epd, 0x80);

  EPD_SendCommand(epd, 0x4E); // set RAM x address count to 0;
  EPD_SendData(epd, 0x00);
  EPD_SendCommand(epd, 0x4F); // set RAM y address count to 0X199;
  EPD_SendData(epd, (EPD_HEIGHT - 1) & 0xFF);
  EPD_SendData(epd, ((EPD_HEIGHT - 1) >> 8) & 0xFF);
  EPD_WaitUntilIdle(epd);

  /* EPD hardware init end */

  return 0;
}

/**
 *  @brief: this calls the corresponding function from epdif.h
 *          usually there is no need to change this function
 */
void EPD_DigitalWrite(EPD *epd, int pin, int value)
{
  EpdDigitalWriteCallback(pin, value);
}

/**
 *  @brief: this calls the corresponding function from epdif.h
 *          usually there is no need to change this function
 */
int EPD_DigitalRead(EPD *epd, int pin)
{
  return EpdDigitalReadCallback(pin);
}

/**
 *  @brief: this calls the corresponding function from epdif.h
 *          usually there is no need to change this function
 */
void EPD_DelayMs(EPD *epd, unsigned int delaytime)
{ // 1ms
  EpdDelayMsCallback(delaytime);
}

/**
 *  @brief: basic function for sending commands
 */
void EPD_SendCommand(EPD *epd, unsigned char command)
{
  EPD_DigitalWrite(epd, epd->dc_pin, LOW);
  EpdSpiTransferCallback(&command, sizeof(command));
}

/**
 *  @brief: basic function for sending data
 */
void EPD_SendData(EPD *epd, unsigned char data)
{
  EPD_DigitalWrite(epd, epd->dc_pin, HIGH);
  EpdSpiTransferCallback(&data, sizeof(data));
}

/**
 *  @brief: Wait until the busy_pin goes HIGH
 */
void EPD_WaitUntilIdle(EPD *epd)
{
  while (EPD_DigitalRead(epd, epd->busy_pin) == 1)
  { //1: busy, 0: idle
    //EPD_DelayMs(epd, 500);
  }
}

/**
 *  @brief: module reset.
 *          often used to awaken the module in deep sleep,
 *          see EPD::Sleep();
 */
void EPD_Reset(EPD *epd)
{
  EPD_DigitalWrite(epd, epd->reset_pin, LOW); //module reset
  EPD_DelayMs(epd, 200);
  EPD_DigitalWrite(epd, epd->reset_pin, HIGH);
  EPD_DelayMs(epd, 200);
}

/**
 *  @brief: set the look-up tables
 */
void EPD_SetLutBw(EPD *epd)
{
  unsigned int count;
  EPD_SendCommand(epd, 0x20); //g vcom
  for (count = 0; count < 15; count++)
  {
    EPD_SendData(epd, lut_vcom0[count]);
  }
  EPD_SendCommand(epd, 0x21); //g ww --
  for (count = 0; count < 15; count++)
  {
    EPD_SendData(epd, lut_w[count]);
  }
  EPD_SendCommand(epd, 0x22); //g bw r
  for (count = 0; count < 15; count++)
  {
    EPD_SendData(epd, lut_b[count]);
  }
  EPD_SendCommand(epd, 0x23); //g wb w
  for (count = 0; count < 15; count++)
  {
    EPD_SendData(epd, lut_g1[count]);
  }
  EPD_SendCommand(epd, 0x24); //g bb b
  for (count = 0; count < 15; count++)
  {
    EPD_SendData(epd, lut_g2[count]);
  }
}

void EPD_SetLutRed(EPD *epd)
{
  unsigned int count;
  EPD_SendCommand(epd, 0x25);
  for (count = 0; count < 15; count++)
  {
    EPD_SendData(epd, lut_vcom1[count]);
  }
  EPD_SendCommand(epd, 0x26);
  for (count = 0; count < 15; count++)
  {
    EPD_SendData(epd, lut_red0[count]);
  }
  EPD_SendCommand(epd, 0x27);
  for (count = 0; count < 15; count++)
  {
    EPD_SendData(epd, lut_red1[count]);
  }
}


void EPD_DisplayFrame(EPD *epd, const unsigned char *BW_Image, const unsigned char *R_Image, uint8_t update, bool is_image, bool is_alarm)
{
  unsigned int Width, Height, i, j;
  Width = (EPD_WIDTH % 8 == 0) ? (EPD_WIDTH / 8) : (EPD_WIDTH / 8 + 1);
  Height = EPD_HEIGHT;

  if (update == 0 || update == 2)
  {
    EPD_SendCommand(epd, 0x26);
    for (j = 0; j < Height; j++)
    {
      for (i = 0; i < Width; i++)
      {
        if(EPD_WHITE_SPACE && !is_image)
        {
          if(j < EPD_WHITE_SPACE/2 || j > EPD_SCREEN_HEIGHT+EPD_WHITE_SPACE/2-1)
          { // Side lines are painted red if alarm
            if(is_alarm)
            {
                EPD_SendData(epd,0xFF); // Alarm frame all red!
            }
            else
            {
                EPD_SendData(epd,0x00); // No alarm
            } 
          }
          else
          { // No paint
            EPD_SendData(epd,0x00);
            //EPD_SendData(epd, ~BW_Image[i + (j-EPD_WHITE_SPACE/2) * Width]);
          }
        }
        else
        {
          EPD_SendData(epd, ~BW_Image[i + j * Width]);
        }
      }
    }
  }

  if (update == 0 || update == 1)
  {
    EPD_SendCommand(epd, 0x24);
    for (j = 0; j < Height; j++)
    {
      for (i = 0; i < Width; i++)
      {
        if(EPD_WHITE_SPACE && !is_image)
        {
          if(j < EPD_WHITE_SPACE/2 || j > EPD_SCREEN_HEIGHT+EPD_WHITE_SPACE/2-1)
          {
            EPD_SendData(epd,0xff);
          }
          else
          {
            EPD_SendData(epd, BW_Image[i + (j-EPD_WHITE_SPACE/2) * Width]);
          }
        }
        else
        {
          EPD_SendData(epd, BW_Image[i + j * Width]);
        }
      }
    }
  }
  
  EPD_SendCommand(epd, 0x22); //Display Update Control
  EPD_SendData(epd, 0xF7);
  EPD_SendCommand(epd, 0x20); //Activate Display Update Sequence
  EPD_WaitUntilIdle(epd);
}

/* After this command is transmitted, the chip would enter the deep-sleep mode to save power. 
   The deep sleep mode would return to standby by hardware reset. The only one parameter is a 
   check code, the command would be executed if check code = 0xA5. 
   You can use EPD_Reset() to awaken and EPD_Init() to initialize */
void EPD_Sleep(EPD *epd)
{
  EPD_SendCommand(epd, 0x10); //Display Update Control
  EPD_SendData(epd, 0x01);
  nrf_delay_ms(100);
}

const unsigned char lut_vcom0[] =
    {
        0x0E, 0x14, 0x01, 0x0A, 0x06, 0x04, 0x0A, 0x0A,
        0x0F, 0x03, 0x03, 0x0C, 0x06, 0x0A, 0x00};

const unsigned char lut_w[] =
    {
        0x0E, 0x14, 0x01, 0x0A, 0x46, 0x04, 0x8A, 0x4A,
        0x0F, 0x83, 0x43, 0x0C, 0x86, 0x0A, 0x04};

const unsigned char lut_b[] =
    {
        0x0E, 0x14, 0x01, 0x8A, 0x06, 0x04, 0x8A, 0x4A,
        0x0F, 0x83, 0x43, 0x0C, 0x06, 0x4A, 0x04};

const unsigned char lut_g1[] =
    {
        0x8E, 0x94, 0x01, 0x8A, 0x06, 0x04, 0x8A, 0x4A,
        0x0F, 0x83, 0x43, 0x0C, 0x06, 0x0A, 0x04};

const unsigned char lut_g2[] =
    {
        0x8E, 0x94, 0x01, 0x8A, 0x06, 0x04, 0x8A, 0x4A,
        0x0F, 0x83, 0x43, 0x0C, 0x06, 0x0A, 0x04};

const unsigned char lut_vcom1[] =
    {
        0x03, 0x1D, 0x01, 0x01, 0x08, 0x23, 0x37, 0x37,
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

const unsigned char lut_red0[] =
    {
        0x83, 0x5D, 0x01, 0x81, 0x48, 0x23, 0x77, 0x77,
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

const unsigned char lut_red1[] =
    {
        0x03, 0x1D, 0x01, 0x01, 0x08, 0x23, 0x37, 0x37,
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

void EPD_WhiteScreen_ALL_Clean(EPD *epd)
{
  unsigned int i;
  EPD_SendCommand(epd, 0x24); //write to Black RAM for black(0)/white (1)
  for (i = 0; i < EPD_WIDTH * EPD_HEIGHT / 8; i++)
  {
    EPD_SendData(epd, 0xff);
  }
  EPD_SendCommand(epd, 0x26); //write to Red RAM for red(1)/white (0)
  for (i = 0; i < EPD_WIDTH * EPD_HEIGHT / 8; i++)
  {
    EPD_SendData(epd, 0x00);
  }
  EPD_SendCommand(epd, 0x22); //Display Update Control
  EPD_SendData(epd, 0xF7);
  EPD_SendCommand(epd, 0x20); //Activate Display Update Sequence
  EPD_WaitUntilIdle(epd);
}


/******************************************************************************
 * Private function definitions
 ******************************************************************************/

static void epd_print(bool is_alarm)
{
    EPD_Reset(&epd);
    EPD_DisplayFrame(&epd, BW_Image, R_Image, 0, false, is_alarm); /* Display image */
    EPD_Sleep(&epd);
}

void display_reserved(unsigned char * str_data)
{

  Paint_Clear(WHITE);

  Paint_DrawBitMap(gImage_icon_logo, 128, 128, 0, 120);

  Paint_DrawLine(0, 35, 130, 35, BLACK, LINE_STYLE_SOLID, DOT_PIXEL_2X2);

  Paint_DrawString_EN(0, 10, "RESERVED", &Font24, WHITE, BLACK);

  Paint_DrawString_EN(0, 45, str_data, &Font16, WHITE, BLACK);

  epd_print(false);
}

void display_available()
{
  unsigned char str_data[40];

  Paint_Clear(WHITE);

  sprintf(str_data, "AVAILABLE");
  Paint_DrawString_EN(148, 64, str_data, &Font24, WHITE, BLACK);

  epd_print(false);
}

void screen_init(void)
{
  paper_turn_on();
  LED_Init();

  /* Init e-paper display */
  EPD_GPIO_Init();
  EPD_Init(&epd);
  
  Paint_NewImage(BW_Image, EPD_WIDTH, EPD_SCREEN_HEIGHT, 270, WHITE);      /* Set screen size and display orientation */
  Paint_NewImage(R_Image, EPD_WIDTH, EPD_SCREEN_HEIGHT, 270, WHITE); /* Set screen size and display orientation */
  Paint_SetMirroring(MIRROR_VERTICAL);
  Paint_SelectImage(BW_Image); /* Set the virtual canvas data storage location */
}

void all_leds_off()
{
  nrf_gpio_pin_write(LED_RED_PIN, 1);
  nrf_gpio_pin_write(LED_GREEN_PIN, 1);
  nrf_gpio_pin_write(LED_BLUE_PIN, 1);
}

void led_on(uint8_t pin_number)
{
  nrf_gpio_pin_dir_set(pin_number, NRF_GPIO_PIN_DIR_OUTPUT);
  nrf_gpio_pin_write(pin_number, 0);
}

/**
 *  @brief: initialise leds 
 */
void LED_Init(void)
{
  /* Set LED pins as output */
  nrf_gpio_pin_dir_set(LED_RED_PIN, NRF_GPIO_PIN_DIR_OUTPUT);
  nrf_gpio_pin_dir_set(LED_GREEN_PIN, NRF_GPIO_PIN_DIR_OUTPUT);
  nrf_gpio_pin_dir_set(LED_BLUE_PIN, NRF_GPIO_PIN_DIR_OUTPUT);

  nrf_gpio_pin_write(LED_RED_PIN, 1);
  nrf_gpio_pin_write(LED_GREEN_PIN, 1);
  nrf_gpio_pin_write(LED_BLUE_PIN, 1);

  nrf_gpio_pin_write(LED_RED_PIN, 0);
  nrf_delay_ms(200);
  nrf_gpio_pin_write(LED_RED_PIN, 1);
  nrf_delay_ms(200);
  nrf_gpio_pin_write(LED_GREEN_PIN, 0);
  nrf_delay_ms(200);
  nrf_gpio_pin_write(LED_GREEN_PIN, 1);
  nrf_delay_ms(200);
  nrf_gpio_pin_write(LED_BLUE_PIN, 0);
  nrf_delay_ms(200);
  nrf_gpio_pin_write(LED_BLUE_PIN, 1);
  nrf_delay_ms(200);

  nrf_gpio_pin_write(LED_RED_PIN, 0);
  nrf_gpio_pin_write(LED_GREEN_PIN, 0);
  nrf_gpio_pin_write(LED_BLUE_PIN, 0);
  nrf_delay_ms(200);

  nrf_gpio_pin_write(LED_RED_PIN, 1);
  nrf_gpio_pin_write(LED_GREEN_PIN, 1);
  nrf_gpio_pin_write(LED_BLUE_PIN, 1);
}
#if 0
void LED_Run(void)
{

  gpio_pin_set(port0, LED_RED_PIN, 0);
  k_sleep(K_MSEC(200));
  gpio_pin_set(port0, LED_RED_PIN, 1);
  k_sleep(K_MSEC(200));
  gpio_pin_set(port0, LED_GREEN_PIN, 0);
  k_sleep(K_MSEC(200));
  gpio_pin_set(port0, LED_GREEN_PIN, 1);
  k_sleep(K_MSEC(200));
  gpio_pin_set(port0, LED_BLUE_PIN, 0);
  k_sleep(K_MSEC(200));
  gpio_pin_set(port0, LED_BLUE_PIN, 1);
  k_sleep(K_MSEC(200));

  gpio_pin_set(port0, LED_GREEN_PIN, 0);
  gpio_pin_set(port0, LED_BLUE_PIN, 0);
  gpio_pin_set(port0, LED_RED_PIN, 0);
  k_sleep(K_MSEC(200));
  gpio_pin_set(port0, LED_GREEN_PIN, 1);
  gpio_pin_set(port0, LED_BLUE_PIN, 1);
  gpio_pin_set(port0, LED_RED_PIN, 1);
}
/* END OF FILE */
#endif