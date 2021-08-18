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

static char device_name[DEVICE_NAME_MAX_LENGTH];

static const unsigned char adv_data_types[N_DATA_TYPES][DATA_TYPE_MAX_LENGTH] = {"Temperature",
                                                                                 "Humidity",
                                                                                 "Battery Level",
                                                                                 "Device Type"
                                                                                 "Missing Inventory",
                                                                                 "Custom"};

const char inventory_item_type_names[6][20] = {"ERROR",
                                               "DEFAULT", 
                                               "STETHOSCOPE", 
                                               "DEFIBRILATOR", 
                                               "VENTILATOR",
                                               "ECG"};

static const uint8_t mock_alarm_data[4] = {0x66, 0x05, 0x02, 0x34};


//static uint8_t missing_inventory_data[MON_DATA_SIZE+1] = {0x05, 0x66, 0x01, 0x02, 0x34,
//                                                              0x66, 0x02, 0x02, 0x34,
//                                                              0x66, 0x03, 0x02, 0x34,
//                                                              0x66, 0x04, 0x02, 0x34,
//                                                              0x66, 0x05, 0x02, 0x34};

static uint8_t env_data[N_DATA_TYPES];
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
#if 0
static void display_alarm_data(void)
{
    int n_missing_items = missing_inventory_data[0];
    int64_t alarm_start_time = k_uptime_get();
    int64_t volatile time_delta = PRINT_ALARM_DATA_TIMEOUT*60*MSEC_PER_SEC; // in msecs


    int64_t current_time = k_uptime_get();
    while(current_time < alarm_start_time + time_delta)
    {
        if(!n_missing_items)
        {
            draw_no_alarm();
            epd_print(false);
            k_sleep(K_MSEC(time_delta+1000));
        }
        //draw_no_alarm();
        //epd_print(false);
        //k_sleep(K_SECONDS(10));
        for(int i = 0; i < n_missing_items; i++)
        {
            draw_alarm_data(&missing_inventory_data[4*i+1]);
            epd_print(true);
            k_sleep(K_SECONDS(10));
        }
        current_time = k_uptime_get();
    }
}

static void draw_no_alarm(void)
{
    unsigned char str_data[40];
    Paint_Clear(WHITE);

    Paint_DrawBitMap(tick_bitmap_BW, 96, 96, 2, 8);

    sprintf(str_data, "ALL");
    Paint_DrawString_EN(120, 40, str_data, &Font20, WHITE, BLACK);

    sprintf(str_data, "SYSTEMS");
    Paint_DrawString_EN(120, 60, str_data, &Font20, WHITE, BLACK);

    sprintf(str_data, "CLEAR!");
    Paint_DrawString_EN(120, 80, str_data, &Font20, WHITE, BLACK);

    Paint_DrawBitMap(b_logo_BW, 24, 48, 110, 190);
}

static void draw_alarm_data(const uint8_t *data)
{
    unsigned char str_data[40];
    Paint_Clear(WHITE);

    Paint_DrawBitMap(&item_bitmaps_BW[data[1]][0], 96, 96, 2, 8);

    sprintf(str_data, "MISSING");
    Paint_DrawString_EN(110, 10, str_data, &Font20, WHITE, BLACK);
    sprintf(str_data, "ITEM");
    Paint_DrawString_EN(110, 25, str_data, &Font20, WHITE, BLACK);

    sprintf(str_data, "TYPE:");
    Paint_DrawString_EN(110, 50, str_data, &Font16, WHITE, BLACK);
    sprintf(str_data, "%s", inventory_item_type_names[data[1]]);
    Paint_DrawString_EN(110, 65, str_data, &Font16, WHITE, BLACK);
    sprintf(str_data, "BATTERY:");
    Paint_DrawString_EN(110, 85, str_data, &Font16, WHITE, BLACK);
    sprintf(str_data, "%u%%", data[3]);
    Paint_DrawString_EN(110, 100, str_data, &Font16, WHITE, BLACK);
     
    Paint_DrawBitMap(b_logo_BW, 24, 48, 110, 190);
}

static int display_settings_init()
{
    int err = -1;
    strcpy(device_name, CONFIG_BT_DEVICE_NAME);
    err = 0;
    return err;
}

/******************************************************************************
 * Public function definitions
 ******************************************************************************/
int get_missing_inventory_data(uint8_t *data)
{
    int err;
    
    err = mon_data_get(data, missing_inventory_data, MON_DATA_MISSING_INVENTORY);
    if (err)
    {
        printk("Error parsing missing inventory data! (err: %d)", err);
    }
    else
    {
        k_sem_give(&draw_alarm_data_sem);
    }

    return err;
}

void draw_env_data(const uint8_t *data)
{
    unsigned char str_data[40];
    int err;
    int print_height = 40;

    Paint_Clear(WHITE);

    sprintf(str_data, device_name);
    Paint_DrawString_EN(0, 10, str_data, &Font16, WHITE, BLACK);

    for (int i = 0; i < N_DATA_TYPES; i++)
    {
        err = mon_data_get(data, &env_data[i], i);
        if (!err)
        {
            sprintf(str_data, (i == MON_DATA_HUMIDITY || i == MON_DATA_BATTERY_LEVEL) ? "%s : %d%%" : "%s : %d", adv_data_types[i], env_data[i]);
            Paint_DrawString_EN(0, print_height, str_data, &Font16, WHITE, BLACK);
            print_height += 20;
        }
    }
}

int enable_print(void)
{
    int err = -1;
    is_print_data = true;
    err = 0;
    return err;
}

int disable_print(void)
{
    int err = -1;
    is_print_data = false;
    err = 0;
    return err;
}

void display_task(void)
{
    k_sem_take(&draw_alarm_data_sem, K_FOREVER);
    //k_sleep(K_SECONDS(10));
    display_alarm_data(); // After PRINT_ALARM_DATA_TIMEOUT minutes, system is rebooted.
    sys_reboot(0);
}
#endif
void screen_init(void)
{
  unsigned char str_data[40];

  paper_turn_on();
  LED_Init();

  //display_settings_init();

  /* Init e-paper display */
  EPD_GPIO_Init();
  EPD_Init(&epd);
  
  //EPD_WhiteScreen_ALL_Clean(&epd); /* Display Image */
  Paint_NewImage(BW_Image, EPD_WIDTH, EPD_SCREEN_HEIGHT, 270, WHITE);      /* Set screen size and display orientation */
  Paint_NewImage(R_Image, EPD_WIDTH, EPD_SCREEN_HEIGHT, 270, WHITE); /* Set screen size and display orientation */
  Paint_SetMirroring(MIRROR_VERTICAL);
  Paint_SelectImage(BW_Image); /* Set the virtual canvas data storage location */
  //Paint_Clear(WHITE);
  //EPD_DisplayFrame(&epd, gImage_BW, gImage_R, 0, true, false);
  //nrf_delay_ms(300);
  
  Paint_Clear(WHITE);

  Paint_DrawBitMap(gImage_request_logo, 128, 128, 0, 120);

  Paint_DrawLine(0, 35, 130, 35, BLACK, LINE_STYLE_SOLID, DOT_PIXEL_2X2);
  sprintf(str_data, "RESERVED");
  Paint_DrawString_EN(0, 10, str_data, &Font24, WHITE, BLACK);

  sprintf(str_data, "Berkay");
  Paint_DrawString_EN(0, 45, str_data, &Font16, WHITE, BLACK);

  sprintf(str_data, "Arslan");
  Paint_DrawString_EN(0, 65, str_data, &Font16, WHITE, BLACK);
  epd_print(false);
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