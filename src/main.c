#include <stdio.h>
#include <string.h>
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "main";

// #define GATTS_SERVICE_UUID 0x00FF
// #define GATTS_CHAR_UUID 0xFF01
// #define GATTS_NUM_HANDLE 4
// #define MAX_BLE_PACKET_SIZE 20 // Adjust based on your BLE stack's capabilities
#define MAX_MESSAGE_SIZE 1024
#define adv_config_flag (1 << 0)
#define scan_rsp_config_flag (1 << 1)
#define PROFILE_COUNT 1
#define LOCK_PROFILE_APP_ID 0
#define LOCK_PROFILE_HANDLE_COUNT 1
#define DEVICE_NAME "Hackable Lock"

// 31337000-feed-face-100c-deadcafefeed
static uint8_t lock_service_uuid128[16] = {0xED, 0xFE, 0xFE, 0xCA, 0xAD, 0xDE, 0x0C, 0x10, 0xCE, 0xFA, 0xED, 0xFE, 0x00, 0x70, 0x33, 0x31};
// 31337000-feed-face-100c-00c0ffeeface
static uint8_t lock_characteristic_uuid128[16] = {0xCE, 0xFA, 0xEE, 0xFF, 0xC0, 0x00, 0x0C, 0x10, 0xCE, 0xFA, 0xED, 0xFE, 0x00, 0x70, 0x33, 0x31};

esp_ble_adv_data_t adv_data = {
    .set_scan_rsp = false,
    .include_name = false,
    .include_txpower = false,
    .min_interval = 0x20,
    .max_interval = 0x40,
    // Reference: https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Assigned_Numbers/out/en/Assigned_Numbers.pdf?v=1710832648803
    // .appearance = 0x0708, // Access Control - Door Lock
    .manufacturer_len = 0,
    .p_manufacturer_data = NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(lock_service_uuid128),
    .p_service_uuid = lock_service_uuid128,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};
esp_ble_adv_data_t scan_rsp_data = {
    .set_scan_rsp = true,
    .include_name = true,
    .include_txpower = true,
    // .min_interval = 0x20,
    // .max_interval = 0x40,
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

struct gatts_profile
{
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_handle;
    esp_gatt_srvc_id_t service_id;
    uint16_t char_handle;
    esp_bt_uuid_t char_uuid;
    esp_gatt_perm_t perm;
    esp_gatt_char_prop_t property;
    uint16_t descr_handle;
    esp_bt_uuid_t descr_uuid;
};

static void gatts_lock_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
static struct gatts_profile gatts_profile_table[PROFILE_COUNT] = {
    [LOCK_PROFILE_APP_ID] = {
        .gatts_cb = gatts_lock_profile_event_handler,
        .gatts_if = ESP_GATT_IF_NONE,
    },
};

// static uint16_t characteristic_handle;
// static uint8_t service_uuid[16] = {
//     /* UUID for the service */
// };

// static uint8_t raw_data[MAX_MESSAGE_SIZE];
// static int raw_data_index = 0;
static uint8_t adv_config_done = 0;

// static esp_gatt_char_prop_t a_property = 0;

// static esp_attr_value_t gatts_demo_char1_val = {
//     .attr_max_len = MAX_MESSAGE_SIZE,
//     .attr_len     = sizeof(raw_data),
//     .attr_value   = raw_data,
// };

// static void example_write_event_env(esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
// {
//     esp_gatt_status_t status = ESP_GATT_WRITE_NOT_PERMIT;

//     if (param->write.handle == characteristic_handle)
//     {
//         if (param->write.is_prep)
//         {
//             // Handle prepare write if you need long writes
//         }
//         else
//         {
//             size_t len = param->write.len;
//             uint8_t *value = param->write.value;

//             // Example of handling data in chunks, simple protocol assumed
//             if (len > 0)
//             {
//                 if (value[0] == 0x01)
//                 { // Assuming 0x01 indicates start of new message
//                     raw_data_index = 0;
//                 }

//                 if (raw_data_index + len - 1 < MAX_MESSAGE_SIZE)
//                 {
//                     memcpy(&raw_data[raw_data_index], &value[1], len - 1); // Skip the first byte (protocol byte)
//                     raw_data_index += (len - 1);

//                     if (value[0] == 0x03)
//                     { // Assuming 0x03 indicates end of message
//                         // Here, you have a full message in raw_data
//                         // Process the message as needed
//                         status = ESP_GATT_OK;
//                     }
//                 }
//             }
//         }
//     }

//     // Send a response to the write request
//     esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, status, NULL);
// }

static void gatts_lock_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    switch (event)
    {
    case ESP_GATTS_REG_EVT:
        ESP_LOGI(TAG, "Lock: GATTS Register app event, status %d, app_id %d", param->reg.status, param->reg.app_id);
        gatts_profile_table[LOCK_PROFILE_APP_ID].service_id.is_primary = true;
        gatts_profile_table[LOCK_PROFILE_APP_ID].service_id.id.inst_id = 0x00;
        gatts_profile_table[LOCK_PROFILE_APP_ID].service_id.id.uuid.len = ESP_UUID_LEN_128;
        memcpy(gatts_profile_table[LOCK_PROFILE_APP_ID].service_id.id.uuid.uuid.uuid128, lock_service_uuid128, ESP_UUID_LEN_128);
        // Set the device name
        esp_err_t set_device_name_ret = esp_ble_gap_set_device_name(DEVICE_NAME);
        if (set_device_name_ret)
        {
            ESP_LOGE(TAG, "esp_ble_gap_set_device_name() failed, error code = %x", set_device_name_ret);
        }
        // Configure advertisements
        esp_err_t config_adv_data_ret;
        config_adv_data_ret = esp_ble_gap_config_adv_data(&adv_data);
        if (config_adv_data_ret)
        {
            ESP_LOGE(TAG, "esp_ble_gap_config_adv_data() with advertising data failed, error code = %x", config_adv_data_ret);
        }
        adv_config_done |= adv_config_flag;
        // Configure Scan Response data
        config_adv_data_ret = esp_ble_gap_config_adv_data(&scan_rsp_data);
        if (config_adv_data_ret)
        {
            ESP_LOGE(TAG, "esp_ble_gap_config_adv_data() with scan response data failed, error code = %x", config_adv_data_ret);
        }
        adv_config_done |= scan_rsp_config_flag;

        esp_ble_gatts_create_service(gatts_if, &gatts_profile_table[LOCK_PROFILE_APP_ID].service_id, LOCK_PROFILE_HANDLE_COUNT);
        break;
    case ESP_GATTS_READ_EVT:
        ESP_LOGI(TAG, "GATTS: Client requested a read");
        // TODO
        break;
    case ESP_GATTS_WRITE_EVT:
        ESP_LOGI(TAG, "GATTS: Client requested an write");
        // TODO
        break;
    case ESP_GATTS_EXEC_WRITE_EVT:
        ESP_LOGI(TAG, "GATTS: Client requested an execute write");
        // TODO
        break;
    case ESP_GATTS_MTU_EVT:
        ESP_LOGI(TAG, "GATTS: Set mtu complete");
        // TODO
        break;
    case ESP_GATTS_UNREG_EVT:
        ESP_LOGI(TAG, "GATTS: Application ID unregistered");
        // TODO
        break;
    case ESP_GATTS_CREATE_EVT:
        ESP_LOGI(TAG, "GATTS: Create service complete");
        // TODO
        break;
    case ESP_GATTS_ADD_INCL_SRVC_EVT:
        ESP_LOGI(TAG, "GATTS: Add included service");
        // TODO
        break;
    case ESP_GATTS_ADD_CHAR_EVT:
        ESP_LOGI(TAG, "GATTS: Add Characteristic complete, status %d, attr_handle %d, service handle %d",
            param->add_char.status,
            param->add_char.attr_handle,
            param->add_char.service_handle);
        gatts_profile_table[LOCK_PROFILE_APP_ID].char_handle = param->add_char.attr_handle;
        //
        // gatts_profile_table[LOCK_PROFILE_APP_ID].descr_uuid.len = ESP_UUID_LEN_16;
        // gatts_profile_table[LOCK_PROFILE_APP_ID].descr_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
        //
        gatts_profile_table[LOCK_PROFILE_APP_ID].descr_uuid.len = ESP_UUID_LEN_128;
        memcpy(gatts_profile_table[LOCK_PROFILE_APP_ID].descr_uuid.uuid.uuid128, lock_characteristic_uuid128, ESP_UUID_LEN_128);

        const uint8_t *prf_char;
        uint16_t length = 0;
        esp_err_t get_attr_ret = esp_ble_gatts_get_attr_value(param->add_char.attr_handle, &length, &prf_char);
        if (get_attr_ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Illegal handle");
        }
        ESP_LOGI(TAG, "The gatts demo char length = %x", length);
        for (int i = 0; i < length; i++) {
            ESP_LOGI(TAG, "prf_char[%x] = %x", i, prf_char[i]);
        }
        esp_err_t add_descr_ret = esp_ble_gatts_add_char_descr(
            gatts_profile_table[LOCK_PROFILE_APP_ID].service_handle,
            &gatts_profile_table[LOCK_PROFILE_APP_ID].descr_uuid,
            ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            NULL,
            NULL);
        if (add_descr_ret) {
            ESP_LOGE(TAG, "add char descr failed, error code = %x", add_descr_ret);
        }
        //
        //
        // uint16_t length = 0;
        // const uint8_t *prf_char;
        // ESP_LOGI(GATTS_TAG, "ADD_CHAR_EVT, status %d,  attr_handle %d, service_handle %d\n",
        //         param->add_char.status, param->add_char.attr_handle, param->add_char.service_handle);
        // gl_profile_tab[PROFILE_A_APP_ID].char_handle = param->add_char.attr_handle;
        // gl_profile_tab[PROFILE_A_APP_ID].descr_uuid.len = ESP_UUID_LEN_16;
        // gl_profile_tab[PROFILE_A_APP_ID].descr_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;

        // esp_err_t get_attr_ret = esp_ble_gatts_get_attr_value(param->add_char.attr_handle,  &length, &prf_char);
        // if (get_attr_ret == ESP_FAIL){
        //     ESP_LOGE(GATTS_TAG, "ILLEGAL HANDLE");
        // }

        // ESP_LOGI(GATTS_TAG, "the gatts demo char length = %x\n", length);
        // for(int i = 0; i < length; i++){
        //     ESP_LOGI(GATTS_TAG, "prf_char[%x] =%x\n",i,prf_char[i]);
        // }
        // esp_err_t add_descr_ret = esp_ble_gatts_add_char_descr(gl_profile_tab[PROFILE_A_APP_ID].service_handle, &gl_profile_tab[PROFILE_A_APP_ID].descr_uuid,
        //                                                         ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, NULL, NULL);
        // if (add_descr_ret){
        //     ESP_LOGE(GATTS_TAG, "add char descr failed, error code =%x", add_descr_ret);
        // }

        break;
    case ESP_GATTS_ADD_CHAR_DESCR_EVT:
        ESP_LOGI(TAG, "GATTS: Add Characteristic Descriptor complete");
        // TODO
        break;
    case ESP_GATTS_DELETE_EVT:
        ESP_LOGI(TAG, "GATTS: Delete");
        // TODO
        break;
    case ESP_GATTS_START_EVT:
        ESP_LOGI(TAG, "GATTS: Start: Status %d, Service Handle: %d",
            param->start.status,
            param->start.service_handle);
        break;
    case ESP_GATTS_STOP_EVT:
        ESP_LOGI(TAG, "GATTS: Stop");
        // TODO
        break;
    case ESP_GATTS_CONNECT_EVT: ;
        esp_ble_conn_update_params_t conn_params = {0};
        memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
        // https://developer.apple.com/accessories/Accessory-Design-Guidelines.pdf
        //   Section 49.6
        conn_params.latency = 0;
        conn_params.max_int = 0x20;    // max_int = 0x20*1.25ms = 40ms
        conn_params.min_int = 0x10;    // min_int = 0x10*1.25ms = 20ms
        conn_params.timeout = 400;    // timeout = 400*10ms = 4000ms
        ESP_LOGI(TAG, "GATTS: Connected, conn_id %d, remote %02x:%02x:%02x:%02x:%02x:%02x:",
                 param->connect.conn_id,
                 param->connect.remote_bda[0], param->connect.remote_bda[1], param->connect.remote_bda[2],
                 param->connect.remote_bda[3], param->connect.remote_bda[4], param->connect.remote_bda[5]);
        gatts_profile_table[LOCK_PROFILE_APP_ID].conn_id = param->connect.conn_id;
        esp_ble_gap_update_conn_params(&conn_params);
        break;
    case ESP_GATTS_DISCONNECT_EVT:
        ESP_LOGI(TAG, "GATTS: Disconnected. Remote BDA: %02x:%02x:%02x:%02x:%02x:%02x, Reason: 0x%x",
            param->disconnect.remote_bda[0],
            param->disconnect.remote_bda[1],
            param->disconnect.remote_bda[2],
            param->disconnect.remote_bda[3],
            param->disconnect.remote_bda[4],
            param->disconnect.remote_bda[5],
            param->disconnect.reason);
        esp_ble_gap_start_advertising(&adv_params);
        break;
    case ESP_GATTS_CONF_EVT:
        // TODO
        break;
    case ESP_GATTS_OPEN_EVT:
    case ESP_GATTS_CANCEL_OPEN_EVT:
    case ESP_GATTS_CLOSE_EVT:
    case ESP_GATTS_LISTEN_EVT:
    case ESP_GATTS_CONGEST_EVT:
    default:
        ESP_LOGE(TAG, "Unhandled GATTS Event. Event code = %x", event);
    }
}

static void gatts_event_handler(esp_gatts_cb_event_t event,
                                esp_gatt_if_t gatts_if,
                                esp_ble_gatts_cb_param_t *param)
{
    if (event == ESP_GATTS_REG_EVT)
    {
        if (param->reg.status == ESP_GATT_OK)
        {
            gatts_profile_table[param->reg.app_id].gatts_if = gatts_if;
        }
        else
        {
            ESP_LOGI(TAG, "GATTS app registration failed, app_id %04x, status %d",
                     param->reg.app_id,
                     param->reg.status);
            return;
        }
    }

    do
    {
        int idx;
        for (idx = 0; idx < PROFILE_COUNT; idx++)
        {
            if (gatts_if == ESP_GATT_IF_NONE || gatts_if == gatts_profile_table[idx].gatts_if)
            {
                if (gatts_profile_table[idx].gatts_cb)
                {
                    gatts_profile_table[idx].gatts_cb(event, gatts_if, param);
                }
            }
        }
    } while (0);
}

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event)
    {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        adv_config_done &= (~adv_config_flag);
        if (adv_config_done == 0)
        {
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;
    case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
        adv_config_done &= (~scan_rsp_config_flag);
        if (adv_config_done == 0)
        {
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;
    case ESP_GAP_BLE_EXT_ADV_START_COMPLETE_EVT:
        if (param->ext_adv_start.status == ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGI(TAG, "BLE: GAP: Ext Advertising started");
        }
        else
        {
            ESP_LOGE(TAG, "Unable to start BLE ext advertisement, error code %d", param->ext_adv_start.status);
        }
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGI(TAG, "BLE: GAP: Advertising started");
        }
        else
        {
            ESP_LOGE(TAG, "Unable to start BLE advertisement, error code %d", param->adv_start_cmpl.status);
        }
        break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS)
        {
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
    case ESP_GAP_BLE_PHY_UPDATE_COMPLETE_EVT:
        ESP_LOGI(TAG, "BLE: GAP: phy update. status = %d, bda = %02x:%02x:%02x:%02x:%02x:%02x, tx_phy = %d, rx_phy = %d",
            param->phy_update.status,
            param->phy_update.bda[0],
            param->phy_update.bda[1],
            param->phy_update.bda[2],
            param->phy_update.bda[3],
            param->phy_update.bda[4],
            param->phy_update.bda[5],
            param->phy_update.tx_phy,
            param->phy_update.rx_phy);
        break;
    default:
        ESP_LOGE(TAG, "Unhandled GAP Event. Event code %d", event);
        break;
    }
}

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_DEBUG);
    esp_err_t ret;

    // Initialize NVS
    ESP_LOGI(TAG, "Initializing NVS");
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
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
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_event_handler));
    // ESP_ERROR_CHECK(esp_ble_gap_set_device_name("hackable_lock"));

    ESP_LOGI(TAG, "BLE: Configuring Generic Attribute Profile (GATT)");
    ESP_ERROR_CHECK(esp_ble_gatts_register_callback(gatts_event_handler));

    ESP_ERROR_CHECK(esp_ble_gatts_app_register(LOCK_PROFILE_APP_ID));

    // ESP_ERROR_CHECK(esp_ble_gap_config_adv_data(&adv_data));
}
