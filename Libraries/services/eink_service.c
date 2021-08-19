/** @file eink_service.c
 *
 * @brief Implementing eink_service in order to control the epaper from smartphone.
 *
 * @par
*/

#include <string.h>
#include <stdlib.h>

#include "nrf_log.h"
#include "eink_service.h"

#define ATTR_CHAR_VALUE_INIT_LEN 25
#define ATTR_CHAR_VALUE_MAX_LEN 25
static const uint8_t eink_name[] = "Name Surname";

/**@brief Function for handling the Connect event.
 *
 * @param[in]   p_eink_service  eink service structure.
 * @param[in]   p_ble_evt      Event received from the BLE stack.
 */
static void on_connect(ble_eink_service_t *p_eink_service, ble_evt_t const *p_ble_evt)
{
    p_eink_service->conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
}

/**@brief Function for handling the Disconnect event.
 *
 * @param[in]   p_bas       eink service structure.
 * @param[in]   p_ble_evt   Event received from the BLE stack.
 */
static void on_disconnect(ble_eink_service_t *p_eink_service, ble_evt_t const *p_ble_evt)
{
    UNUSED_PARAMETER(p_ble_evt);
    p_eink_service->conn_handle = BLE_CONN_HANDLE_INVALID;
}

/**@brief Function for handling the Write event.
 *
 * @param[in] p_eink_service   eink Service structure.
 * @param[in] p_ble_evt       Event received from the BLE stack.
 */
static void on_write(ble_eink_service_t *p_eink_service, ble_evt_t const *p_ble_evt)
{
    ble_gatts_evt_write_t const *p_evt_write = &p_ble_evt->evt.gatts_evt.params.write;

    NRF_LOG_INFO("Length = %d \r\n", p_evt_write->len);
    
    if ((p_evt_write->handle == p_eink_service->eink_2_char_handles.value_handle) && (p_eink_service->eink_write_handler != NULL))
    {
        p_eink_service->eink_write_handler(p_ble_evt->evt.gap_evt.conn_handle, p_eink_service, p_evt_write->data, p_evt_write->len);
    }
}

/**@brief Function for adding the eink 2 characteristic.
 *
 */
static uint32_t eink_2_char_add(ble_eink_service_t *p_eink_service)
{
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_t attr_char_value;
    ble_gatts_attr_md_t attr_md;
    ble_uuid_t ble_uuid;

    memset(&char_md, 0, sizeof(char_md));
    memset(&attr_md, 0, sizeof(attr_md));
    memset(&attr_char_value, 0, sizeof(attr_char_value));

    char_md.char_props.read = 1;
    char_md.char_props.write = 1;
    char_md.p_char_user_desc = eink_name;
    char_md.char_user_desc_size = sizeof(eink_name);
    char_md.char_user_desc_max_size = sizeof(eink_name);
    char_md.p_char_pf = NULL;
    char_md.p_user_desc_md = NULL;
    char_md.p_cccd_md = NULL;
    char_md.p_sccd_md = NULL;

    // Define the eink 2 Characteristic UUID
    ble_uuid.type = p_eink_service->uuid_type;
    ble_uuid.uuid = BLE_UUID_EINK_2_CHAR_UUID;

    // Set permissions on the Characteristic value
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);

    // Attribute Metadata settings
    attr_md.vloc = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth = 0;
    attr_md.wr_auth = 0;
    attr_md.vlen = 0;

    // Attribute Value settings
    attr_char_value.p_uuid = &ble_uuid;
    attr_char_value.p_attr_md = &attr_md;
    attr_char_value.init_len = ATTR_CHAR_VALUE_INIT_LEN;
    attr_char_value.init_offs = 0;
    attr_char_value.max_len = ATTR_CHAR_VALUE_MAX_LEN;
    attr_char_value.p_value = NULL;

    return sd_ble_gatts_characteristic_add(p_eink_service->service_handle, &char_md,
                                           &attr_char_value,
                                           &p_eink_service->eink_2_char_handles);
}

uint32_t ble_eink_service_init(ble_eink_service_t *p_eink_service, const ble_eink_service_init_t *p_eink_service_init)
{
    uint32_t err_code;
    ble_uuid_t ble_uuid;

    // Initialize service structure
    p_eink_service->conn_handle = BLE_CONN_HANDLE_INVALID;

    // Initialize service structure.
    p_eink_service->eink_write_handler = p_eink_service_init->eink_write_handler;

    // Add service UUID
    ble_uuid128_t base_uuid = {BLE_UUID_EINK_SERVICE_BASE_UUID};
    err_code = sd_ble_uuid_vs_add(&base_uuid, &p_eink_service->uuid_type);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    // Set up the UUID for the service (base + service-specific)
    ble_uuid.type = p_eink_service->uuid_type;
    ble_uuid.uuid = BLE_UUID_EINK_SERVICE_UUID;

    // Set up and add the service
    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &ble_uuid, &p_eink_service->service_handle);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    // Add the different characteristics in the service:
    //   Button press characteristic:   E54B0002-67F5-479E-8711-B3B99198CE6C
    err_code = eink_2_char_add(p_eink_service);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    return NRF_SUCCESS;
}

void ble_eink_service_on_ble_evt(ble_evt_t const *p_ble_evt, void *p_context)
{
    ble_eink_service_t *p_eink_service = (ble_eink_service_t *)p_context;

    switch (p_ble_evt->header.evt_id)
    {
    case BLE_GAP_EVT_CONNECTED:
        on_connect(p_eink_service, p_ble_evt);
        break;

    case BLE_GATTS_EVT_WRITE:
        on_write(p_eink_service, p_ble_evt);
        break;

    case BLE_GAP_EVT_DISCONNECTED:
        on_disconnect(p_eink_service, p_ble_evt);
        break;

    default:
        // No implementation needed.
        break;
    }
}
