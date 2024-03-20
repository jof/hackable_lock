#include <stdio.h>
#include <string.h>
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

static const char* TAG = "main";

#define GATTS_SERVICE_UUID   0x00FF
#define GATTS_CHAR_UUID      0xFF01
#define GATTS_NUM_HANDLE     4
#define MAX_BLE_PACKET_SIZE  20  // Adjust based on your BLE stack's capabilities
#define MAX_MESSAGE_SIZE     1024
static uint16_t characteristic_handle;
// static uint8_t service_uuid[16] = {
//     /* UUID for the service */
// };

static uint8_t raw_data[MAX_MESSAGE_SIZE];
static int raw_data_index = 0;

// static esp_gatt_char_prop_t a_property = 0;

// static esp_attr_value_t gatts_demo_char1_val = {
//     .attr_max_len = MAX_MESSAGE_SIZE,
//     .attr_len     = sizeof(raw_data),
//     .attr_value   = raw_data,
// };

static void example_write_event_env(esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    esp_gatt_status_t status = ESP_GATT_WRITE_NOT_PERMIT;

    if (param->write.handle == characteristic_handle) {
        if (param->write.is_prep) {
            // Handle prepare write if you need long writes
        } else {
            size_t len = param->write.len;
            uint8_t *value = param->write.value;

            // Example of handling data in chunks, simple protocol assumed
            if (len > 0) {
                if (value[0] == 0x01) { // Assuming 0x01 indicates start of new message
                    raw_data_index = 0;
                }

                if (raw_data_index + len - 1 < MAX_MESSAGE_SIZE) {
                    memcpy(&raw_data[raw_data_index], &value[1], len - 1); // Skip the first byte (protocol byte)
                    raw_data_index += (len - 1);

                    if (value[0] == 0x03) { // Assuming 0x03 indicates end of message
                        // Here, you have a full message in raw_data
                        // Process the message as needed
                        status = ESP_GATT_OK;
                    }
                }
            }
        }
    }

    // Send a response to the write request
    esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, status, NULL);
}

static void gatts_profile_event_handler(esp_gatts_cb_event_t event, 
                                         esp_gatt_if_t gatts_if, 
                                         esp_ble_gatts_cb_param_t *param) {
    switch (event) {
        case ESP_GATTS_WRITE_EVT:
            example_write_event_env(gatts_if, param);
            break;
        // Handle other events as needed
        default:
            break;
    }
}

esp_ble_adv_data_t adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = true,
    .min_interval = 0x20,
    .max_interval = 0x40,
    // Reference: https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Assigned_Numbers/out/en/Assigned_Numbers.pdf?v=1710832648803
    .appearance = 0x0708, // Access Control - Door Lock
    .manufacturer_len = 0,
    .p_manufacturer_data = NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = 0,
    .p_service_uuid = NULL,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};
esp_ble_adv_params_t adv_params = {
    .adv_int_min = 0x20,
    .adv_int_max = 0x40,
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,

};

static uint8_t adv_config_done = 0;
#define adv_config_flag (1 << 0)
#define scan_rsp_config_flag (1 << 1)

static void esp_gap_callback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            adv_config_done &= (~adv_config_flag);
            if (adv_config_done == 0) {
                esp_ble_gap_start_advertising(&adv_params);
            }
            break;
        case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
            adv_config_done &= (~scan_rsp_config_flag);
            if (adv_config_done == 0) {
                esp_ble_gap_start_advertising(&adv_params);
            }
            break;
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
                ESP_LOGI(TAG, "BLE: GAP: Advertising started");
            }
            else {
                ESP_LOGE(TAG, "Unable to start BLE advertisement, error code %d", param->adv_start_cmpl.status);
            }
            break;
        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
            if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(TAG, "Advertising stop failed");
            }
            break;
        case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
            ESP_LOGI(TAG, "update connection params status = %d, min_int = %d, max_int = %d, conn_int = %d, latency = %d, timeout = %d",
                param->update_conn_params.status,
                param->update_conn_params.min_int,
                param->update_conn_params.max_int,
                param->update_conn_params.conn_int,
                param->update_conn_params.latency,
                param->update_conn_params.timeout);
            break;
        default:
            ESP_LOGI(TAG, "Event %d unhandled", event);
            break;
    }
}

void app_main(void) {
    esp_log_level_set("*", ESP_LOG_DEBUG);
    esp_err_t ret;

    // Initialize NVS
    ESP_LOGI(TAG, "Initializing NVS");
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // BLE only; free up memory for Classic BT
    ESP_LOGI(TAG, "Freeing BT Classic memory");
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    // Initialize the Bluetooth stack
    ESP_LOGI(TAG, "Initializing BLE controller");
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    ESP_LOGI(TAG, "BLE: Configuring Generic Access Profile (GAP)");
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(esp_gap_callback));
    ESP_ERROR_CHECK(esp_ble_gap_set_device_name("hackable_lock"));

    ESP_LOGI(TAG, "BLE: Configuring Generic Attribute Profile (GATT)");
    ESP_ERROR_CHECK(esp_ble_gatts_register_callback(gatts_profile_event_handler));

    // ESP_ERROR_CHECK(esp_ble_gap_config_adv_data(&adv_data));
}
