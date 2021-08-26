/* General Header Files */
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "boards.h"
#include "app_error.h"
#include "app_timer.h"
#include "nrf_pwr_mgmt.h"

/* Header Files for BLE */
#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "nrf_sdh_soc.h"
#include "ble.h"
#include "ble_hci.h"
#include "ble_srv_common.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_conn_params.h"
#include "nrf_sdh.h"
#include "nrf_sdh_soc.h"
#include "nrf_sdh_ble.h"
#include "app_timer.h"
#include "fds.h"
#include "peer_manager.h"
#include "ble_conn_state.h"
#include "nrf_ble_gatt.h"
#include "nrf_ble_qwr.h"
#include "nrf_pwr_mgmt.h"


/* Header Files for Logging */
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

/* Header Files for e-paper display */
#include "epd.h"
#include "epdif.h"
#include "eink_service.h"

#define APP_BLE_CONN_CFG_TAG 1
#define APP_BLE_OBSERVER_PRIO 3

#define DEVICE_NAME "ESL"

#define MIN_CONN_INTERVAL MSEC_TO_UNITS(100, UNIT_1_25_MS);
#define MAX_CONN_INTERVAL MSEC_TO_UNITS(200, UNIT_1_25_MS);
#define SLAVE_LATENCY 0
#define CONN_SUP_TIMEOUT MSEC_TO_UNITS(2000, UNIT_10_MS)

#define APP_ADV_INTERVAL 300
#define APP_ADV_DURATION 0

#define FIRST_CONN_PARAMS_UPDATE_DELAY APP_TIMER_TICKS(5000)
#define NEXT_CONN_PARAMS_UPDATE_DELAY APP_TIMER_TICKS(30000)
#define MAX_CONN_PARAMS_UPDATE_COUNT 3 

#define SEC_PARAM_BOND                  1                                       /**< Perform bonding. */
#define SEC_PARAM_MITM                  0                                       /**< Man In The Middle protection not required. */
#define SEC_PARAM_LESC                  0                                       /**< LE Secure Connections not enabled. */
#define SEC_PARAM_KEYPRESS              0                                       /**< Keypress notifications not enabled. */
#define SEC_PARAM_IO_CAPABILITIES       BLE_GAP_IO_CAPS_NONE                    /**< No I/O capabilities. */
#define SEC_PARAM_OOB                   0                                       /**< Out Of Band data not available. */
#define SEC_PARAM_MIN_KEY_SIZE          7                                       /**< Minimum encryption key size. */
#define SEC_PARAM_MAX_KEY_SIZE          16                                      /**< Maximum encryption key size. */

#define DEAD_BEEF                       0xDEADBEEF                              /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */

#define BATTERY_LEVEL_MEAS_INTERVAL     APP_TIMER_TICKS(30000)                 /**< Battery level measurement interval (ticks). */

static uint16_t m_conn_handle = BLE_CONN_HANDLE_INVALID;                        /**< Handle of the current connection. */

static uint8_t m_adv_handle = BLE_GAP_ADV_SET_HANDLE_NOT_SET;                   /**< Advertising handle used to identify an advertising set. */
static uint8_t m_enc_advdata[BLE_GAP_ADV_SET_DATA_SIZE_MAX];                    /**< Buffer for storing an encoded advertising set. */
static uint8_t m_enc_scan_response_data[BLE_GAP_ADV_SET_DATA_SIZE_MAX];         /**< Buffer for storing an encoded scan data. */

/**@brief Struct that contains pointers to the encoded advertising data. */
static ble_gap_adv_data_t m_adv_data =
{
    .adv_data =
    {
        .p_data = m_enc_advdata,
        .len    = BLE_GAP_ADV_SET_DATA_SIZE_MAX
    },
    .scan_rsp_data =
    {
        .p_data = m_enc_scan_response_data,
        .len    = BLE_GAP_ADV_SET_DATA_SIZE_MAX
    }
};

APP_TIMER_DEF(m_battery_timer_id);
NRF_BLE_QWR_DEF(m_qwr);
NRF_BLE_GATT_DEF(m_gatt);
BLE_ADVERTISING_DEF(m_advertising);
BLE_EINK_SERVICE_DEF(m_eink_service);

static void eink_write_handler(uint16_t conn_handle, ble_eink_service_t * p_eink_service, uint8_t * data, uint16_t data_len);


static void advertising_start();

/**@brief Callback function for asserts in the SoftDevice.
 *
 * @details This function will be called in case of an assert in the SoftDevice.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyze
 *          how your product is supposed to react in case of Assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in] line_num   Line number of the failing ASSERT call.
 * @param[in] file_name  File name of the failing ASSERT call.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
    app_error_handler(DEAD_BEEF, line_num, p_file_name);
}

static void timers_init()
{
  ret_code_t err_code = app_timer_init();
  APP_ERROR_CHECK(err_code);

}

static void gap_params_init()
{
  ret_code_t err_code;

  ble_gap_conn_params_t   gap_conn_params;
  ble_gap_conn_sec_mode_t sec_mode;

  BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

  err_code = sd_ble_gap_device_name_set(&sec_mode, (const uint8_t *) DEVICE_NAME, strlen(DEVICE_NAME));
  APP_ERROR_CHECK(err_code);

  memset(&gap_conn_params, 0, sizeof(gap_conn_params));

  gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
  gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
  gap_conn_params.slave_latency     = SLAVE_LATENCY;
  gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

  err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
  APP_ERROR_CHECK(err_code);
}

static void gatt_init()
{
  ret_code_t err_code = nrf_ble_gatt_init(&m_gatt, NULL);
  APP_ERROR_CHECK(err_code);
}

static void nrf_qwr_error_handler(uint32_t nrf_error)
{
  APP_ERROR_HANDLER(nrf_error);
}

static void services_init()
{
  ret_code_t err_code;
  ble_eink_service_init_t eink_init;

  nrf_ble_qwr_init_t qwr_init = {0};

  qwr_init.error_handler = nrf_qwr_error_handler;

  err_code = nrf_ble_qwr_init(&m_qwr, &qwr_init);
  APP_ERROR_CHECK(err_code);
  // 1. Initialize the eink service
  eink_init.eink_write_handler = eink_write_handler;

  err_code = ble_eink_service_init(&m_eink_service, &eink_init);
  APP_ERROR_CHECK(err_code);

}

static void on_conn_params_evt(ble_conn_params_evt_t * p_evt)
{
  ret_code_t err_code;

  if(p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED)
  {
    err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
    APP_ERROR_CHECK(err_code);
  }

  if(p_evt->evt_type == BLE_CONN_PARAMS_EVT_SUCCEEDED)
  {
    
  }
}

static void conn_params_error_handler(uint32_t nrf_error)
{
  APP_ERROR_HANDLER(nrf_error);
}

static void conn_params_init()
{
  ret_code_t err_code;

  ble_conn_params_init_t cp_init;
  
  memset(&cp_init, 0, sizeof(cp_init));

  cp_init.p_conn_params = NULL;
  cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
  cp_init.next_conn_params_update_delay = NEXT_CONN_PARAMS_UPDATE_DELAY;
  cp_init.max_conn_params_update_count = MAX_CONN_PARAMS_UPDATE_COUNT;
  cp_init.start_on_notify_cccd_handle = BLE_GATT_HANDLE_INVALID;
  cp_init.disconnect_on_fail = false;
  cp_init.error_handler = conn_params_error_handler;
  cp_init.evt_handler = on_conn_params_evt;

}

static void ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context)
{
  ret_code_t err_code = NRF_SUCCESS;

  switch(p_ble_evt->header.evt_id)
  {
    case BLE_GAP_EVT_DISCONNECTED:
      NRF_LOG_INFO("Device is disconnected.");
      all_leds_off();
      led_on(LED_RED_PIN);
      break;

    case BLE_GAP_EVT_CONNECTED:
      NRF_LOG_INFO("Device is connected.");
      all_leds_off(); 
      led_on(LED_BLUE_PIN);

      m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;

      err_code = nrf_ble_qwr_conn_handle_assign(&m_qwr, m_conn_handle);
      APP_ERROR_CHECK(err_code);
      break;

      case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
          // Pairing not supported
          err_code = sd_ble_gap_sec_params_reply(m_conn_handle,
                                                 BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP,
                                                 NULL,
                                                 NULL);
          APP_ERROR_CHECK(err_code);
          break;


      case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
      {
          NRF_LOG_DEBUG("PHY update request.");
          ble_gap_phys_t const phys =
          {
              .rx_phys = BLE_GAP_PHY_AUTO,
              .tx_phys = BLE_GAP_PHY_AUTO,
          };
          err_code = sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys);
          APP_ERROR_CHECK(err_code);
      } break;

      case BLE_GATTS_EVT_SYS_ATTR_MISSING:
          // No system attributes have been stored.
          err_code = sd_ble_gatts_sys_attr_set(m_conn_handle, NULL, 0, 0);
          APP_ERROR_CHECK(err_code);
          break;

      case BLE_GATTC_EVT_TIMEOUT:
          // Disconnect on GATT Client timeout event.
          NRF_LOG_DEBUG("GATT Client Timeout.");
          err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gattc_evt.conn_handle,
                                           BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
          APP_ERROR_CHECK(err_code);
          break;

      case BLE_GATTS_EVT_TIMEOUT:
          // Disconnect on GATT Server timeout event.
          NRF_LOG_DEBUG("GATT Server Timeout.");
          err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gatts_evt.conn_handle,
                                           BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
          APP_ERROR_CHECK(err_code);
          break;

      default:
        break;
  }
}

static void ble_stack_init()
{
  ret_code_t err_code;
  
  err_code = nrf_sdh_enable_request();
  APP_ERROR_CHECK(err_code);

  uint32_t ram_start = 0;

  err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
  APP_ERROR_CHECK(err_code);

  err_code = nrf_sdh_ble_enable(&ram_start);
  APP_ERROR_CHECK(err_code);

  NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, ble_evt_handler, NULL);
}

/**@brief Function for handling write events to the EINK characteristic.
 *
 * @param[in] p_eink_service  Instance of eink Service to which the write applies.
 * @param[in] eink_state      Written/desired state of the eink.
 */
static void eink_write_handler(uint16_t conn_handle, ble_eink_service_t * p_eink_service, uint8_t * data, uint16_t data_len)
{
  NRF_LOG_INFO("Displaying Reserved Screen...");
  unsigned char* str_data = (unsigned char*) calloc(data_len, sizeof(unsigned char));
  memcpy(str_data, data, data_len);
  str_data[data_len] = 0;
  if(str_data[0] == '0')
  {
    display_available();
  }
  else
    display_reserved(str_data);
  free(str_data);
}

static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
  ret_code_t err_code;

  switch(ble_adv_evt)
  {
    case BLE_ADV_EVT_FAST:
      NRF_LOG_INFO("Fast Advertising...");
      led_on(LED_RED_PIN);
      break;
    
    case BLE_ADV_EVT_IDLE:
      NRF_LOG_INFO("BLE IDLE...");
      all_leds_off(); 
      led_on(LED_GREEN_PIN);
      led_on(LED_RED_PIN);
      led_on(LED_BLUE_PIN);
      break;

    default:
      break;
      
  }
}

static void advertising_init()
{
  ret_code_t err_code;

  ble_advertising_init_t init;

  memset(&init, 0, sizeof(init));
  
  // Add service UUID
  ble_uuid128_t base_uuid = {BLE_UUID_EINK_SERVICE_BASE_UUID};
  uint8_t MY_UUID_TYPE;

  err_code = sd_ble_uuid_vs_add(&base_uuid, &MY_UUID_TYPE);
  APP_ERROR_CHECK(err_code);

  ble_uuid_t m_adv_uuids[] =                                               /**< Universally unique service identifiers. */
  {
    {BLE_UUID_EINK_DEVICE_UUID, MY_UUID_TYPE}
  };

  init.advdata.name_type = BLE_ADVDATA_FULL_NAME;
  init.advdata.include_appearance = true;
  init.advdata.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
  init.advdata.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
  init.advdata.uuids_complete.p_uuids  = m_adv_uuids;

  init.config.ble_adv_fast_enabled = true;
  init.config.ble_adv_fast_interval = APP_ADV_INTERVAL;
  init.config.ble_adv_fast_timeout = APP_ADV_DURATION;

  init.evt_handler = on_adv_evt;

  err_code = ble_advertising_init(&m_advertising, &init);
  APP_ERROR_CHECK(err_code);

  ble_advertising_conn_cfg_tag_set(&m_advertising, APP_BLE_CONN_CFG_TAG);
}

static void power_management_init()
{
  APP_ERROR_CHECK(nrf_pwr_mgmt_init());
}


static void log_init()
{
  APP_ERROR_CHECK(NRF_LOG_INIT(NULL));
  NRF_LOG_DEFAULT_BACKENDS_INIT();
}

static void idle_state_handle()
{
  if(NRF_LOG_PROCESS() == false)
  {
    nrf_pwr_mgmt_run();
  }
}

static void spi_init()
{
  APP_ERROR_CHECK(epd_spi_init());
}

static void advertising_start()
{
  ret_code_t err_code = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);
  APP_ERROR_CHECK(err_code);
}

int main(void) 
{
  log_init();
  spi_init();
  screen_init();
  timers_init();
  ble_stack_init();
  gap_params_init();
  gatt_init();
  advertising_init();
  services_init();
  conn_params_init();
  
  NRF_LOG_INFO("Electronic Shelf Label started...\r\n");

  advertising_start();

  while(1)
  {
    idle_state_handle();
  }
}
