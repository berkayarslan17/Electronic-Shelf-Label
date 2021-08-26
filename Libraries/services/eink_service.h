/** @file eink_service.h
 *
 * @brief Header file of eink_service.c
 *
 * @par
 * 
 * 
*/

#ifndef BLE_H
#define BLE_H

#include <stdint.h>
#include "boards.h"
#include "ble.h"
#include "ble_srv_common.h"
#include "nrf_sdh_ble.h"

/**@brief   Macro for defining a ble_eink_service instance.
 *
 * @param   _name   Name of the instance.
 * @hideinitializer
 */

#define BLE_EINK_SERVICE_BLE_OBSERVER_PRIO 2

#define BLE_EINK_SERVICE_DEF(_name)                          \
    static ble_eink_service_t _name;                         \
    NRF_SDH_BLE_OBSERVER(_name##_obs,                        \
                         BLE_EINK_SERVICE_BLE_OBSERVER_PRIO, \
                         ble_eink_service_on_ble_evt, &_name)

// eink service:                     E54B0001-67F5-479E-8711-B3B99198CE6C
//   eink 2 characteristic:   E54B0002-67F5-479E-8711-B3B99198CE6C

// The bytes are stored in little-endian format, meaning the
// Least Significant Byte is stored first
// (reversed from the order they're displayed as)

// Base UUID: E54B0000-67F5-479E-8711-B3B99198CE6C
#define BLE_UUID_EINK_SERVICE_BASE_UUID                                                                \
    {                                                                                                  \
        0x6C, 0xCE, 0x98, 0x91, 0xB9, 0xB3, 0x11, 0x87, 0x9E, 0x47, 0xF5, 0x67, 0x00, 0x00, 0x4B, 0xE1 \
    }

// Service & characteristics UUIDs
#define BLE_UUID_EINK_DEVICE_UUID 0x0000
#define BLE_UUID_EINK_SERVICE_UUID 0x0001
#define BLE_UUID_EINK_CHAR_UUID 0x0002

// Forward declaration of the custom_service_t type.
typedef struct ble_eink_service_s ble_eink_service_t;

typedef void (*ble_eink_service_eink_write_handler_t)(uint16_t conn_handle, ble_eink_service_t *p_eink_service, uint8_t * str_data, uint16_t data_len);

/** @brief eink Service init structure. This structure contains all options and data needed for
 *        initialization of the service.*/
typedef struct
{
    ble_eink_service_eink_write_handler_t eink_write_handler; /**< Event handler to be caleink when the eink Characteristic is written. */
} ble_eink_service_init_t;

/**@brief eink Service structure.
 *        This contains various status information
 *        for the service.
 */
typedef struct ble_eink_service_s
{
    uint16_t conn_handle;
    uint16_t service_handle;
    uint8_t uuid_type;
    ble_gatts_char_handles_t eink_char_handles;
    ble_eink_service_eink_write_handler_t eink_write_handler;

} ble_eink_service_t;

// Function Declarations

/**@brief Function for initializing the eink Service.
 *
 * @param[out]  p_eink_service  eink Service structure. This structure will have to be supplied by
 *                                the application. It will be initialized by this function, and will later
 *                                be used to identify this particular service instance.
 *
 * @return      NRF_SUCCESS on successful initialization of service, otherwise an error code.
 */
uint32_t ble_eink_service_init(ble_eink_service_t *p_eink_service, const ble_eink_service_init_t *p_eink_service_init);

/**@brief Function for handling the application's BLE stack events.
 *
 * @details This function handles all events from the BLE stack that are of interest to the eink Service.
 *
 * @param[in] p_ble_evt  Event received from the BLE stack.
 * @param[in] p_context  eink Service structure.
 */
void ble_eink_service_on_ble_evt(ble_evt_t const *p_ble_evt, void *p_context);

#endif /* EPDIF_H */
