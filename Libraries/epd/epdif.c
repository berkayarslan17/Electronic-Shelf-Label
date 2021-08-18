/** @file epdif.c
 *
 * @brief Implements EPD interface functions
 *
 * @par
*/

#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "nrf_log.h"
#include "epdif.h"
#include "app_error.h"
#include "nrf_lcd.h"
#include "nrf_drv_spi.h"

#include "epdif.h"

EPD_Pin epd_cs_pin = {
    EPD_SPI_CS_PIN,
};

EPD_Pin epd_rst_pin = {
    EPD_RST_PIN,
};

EPD_Pin epd_dc_pin = {
    EPD_DC_PIN,
};

EPD_Pin epd_busy_pin = {
    EPD_BUSY_PIN,
};

EPD_Pin pins[4];

static const nrf_drv_spi_t spi = NRF_DRV_SPI_INSTANCE(WSEPD_SPI_INSTANCE);

ret_code_t epd_spi_init(void)
{
  ret_code_t err_code;

  nrf_drv_spi_config_t spi_config = NRF_DRV_SPI_DEFAULT_CONFIG;

  spi_config.sck_pin  = EPD_SPI_SCK_PIN;
  spi_config.mosi_pin = EPD_SPI_MOSI_PIN;
  spi_config.ss_pin = EPD_SPI_CS_PIN;
  spi_config.frequency = NRF_DRV_SPI_FREQ_1M;

  err_code = nrf_drv_spi_init(&spi, &spi_config, NULL, NULL);
  return err_code;
}

void EPD_GPIO_Init(void)
{
  nrf_gpio_pin_dir_set(EPD_DC_PIN, NRF_GPIO_PIN_DIR_OUTPUT);
  nrf_gpio_pin_dir_set(EPD_RST_PIN, NRF_GPIO_PIN_DIR_OUTPUT);
  //nrf_gpio_pin_dir_set(EPD_SPI_CS_PIN, NRF_GPIO_PIN_DIR_OUTPUT);
  nrf_gpio_cfg_input(SPI_BUSY_PIN, NRF_GPIO_PIN_PULLUP);

  nrf_gpio_pin_write(EPD_DC_PIN, 0);
  nrf_gpio_pin_write(EPD_RST_PIN, 0);
  //nrf_gpio_pin_write(EPD_SPI_CS_PIN, 0);
  //gpio_pin_set(epaper_gpio, EPD_BUSY_PIN, 0);
  int value = nrf_gpio_pin_read(EPD_BUSY_PIN);
  //printk("Busy pin: %d\n", value);
}

void EpdDigitalWriteCallback(int pin_num, int value)
{
  if (value == HIGH)
  {
    nrf_gpio_pin_write(pins[pin_num].pin, 1);
  }
  else
  {
    nrf_gpio_pin_write(pins[pin_num].pin, 0);
  }
}

int EpdDigitalReadCallback(int pin_num)
{
  uint32_t value;
  value = nrf_gpio_pin_read(pins[pin_num].pin);
  //printk("value = %d\n", value);

  //if(read_status)
  //{
  //  printk("Failed to read gpio %d", pins[pin_num].pin);
  //  return -1;
  //}

  if (value)
  { //nrf_gpio_pin_read(pins[pin_num].pin)) {
    return HIGH;
  }
  else
  {
    return LOW;
  }
}

void EpdDelayMsCallback(unsigned int delaytime)
{
  nrf_delay_ms(delaytime);
}

void EpdSpiTransferCallback(const void *data, size_t size)
{
  nrf_gpio_pin_write(pins[CS_PIN].pin, 0);
  
  APP_ERROR_CHECK(nrf_drv_spi_transfer(&spi, data, size, NULL, 0));

  nrf_gpio_pin_write(pins[CS_PIN].pin, 1);
}

int EpdInitCallback(void)
{
  pins[CS_PIN] = epd_cs_pin;
  pins[RST_PIN] = epd_rst_pin;
  pins[DC_PIN] = epd_dc_pin;
  pins[BUSY_PIN] = epd_busy_pin;

  return 0;
}
