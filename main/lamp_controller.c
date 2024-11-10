/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 * Zigbee HA_color_dimmable_switch Example
 *
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 *
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
 */

#include "lamp_controller.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ha/esp_zigbee_ha_standard.h"
#include "nvs_flash.h"
#include "string.h"

#define ESP_ZB_GATEWAY_ENDPOINT 1 /* Gateway endpoint identifier */
#define APP_PROD_CFG_CURRENT_VERSION                                           \
  0x0001 /* Production configuration version */

#if !defined CONFIG_ZB_ZCZR
#error Define ZB_ZCZR in idf.py menuconfig to compile light switch (Coordinator) source code.
#endif

typedef struct light_bulb_device_params_s {
  esp_zb_ieee_addr_t ieee_addr;
  uint8_t endpoint;
  uint16_t short_addr;
} light_bulb_device_params_t;

static switch_func_pair_t button_func_pair[] = {
    {GPIO_INPUT_IO_TOGGLE_SWITCH, SWITCH_ONOFF_TOGGLE_CONTROL}};

/* R, G, B of color x,y define table */
static uint16_t color_x_table[3] = {41942, 19660, 9830};
static uint16_t color_y_table[3] = {21626, 38321, 3932};

static const char *TAG = "ESP_LAMP_CONTROLLER";

/* Production configuration app data */
typedef struct app_production_config_s {
  uint16_t version;
  uint16_t manuf_code;
  char manuf_name[16];
} app_production_config_t;

static void zb_buttons_handler(switch_func_pair_t *button_func_pair) {
  uint8_t step = 10;
  static uint8_t level_value = 5;
  static uint8_t press_count = 0;
  if (button_func_pair->func == SWITCH_ONOFF_TOGGLE_CONTROL) {
    /* implemented light switch toggle functionality */
    uint16_t refer_x = color_x_table[press_count % 3];
    uint16_t refer_y = color_y_table[press_count % 3];
    if (press_count % 2 == 1) {
      esp_zb_zcl_color_move_to_color_cmd_t cmd_color;
      cmd_color.color_x = refer_x;
      cmd_color.color_y = refer_y;
      cmd_color.transition_time = 0;
      cmd_color.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
      cmd_color.zcl_basic_cmd.src_endpoint = HA_COLOR_DIMMABLE_SWITCH_ENDPOINT;
      esp_zb_lock_acquire(portMAX_DELAY);
      esp_zb_zcl_color_move_to_color_cmd_req(&cmd_color);
      esp_zb_lock_release();
      ESP_EARLY_LOGI(TAG, "Send command for moving light color to (0x%x, 0x%x)",
                     refer_x, refer_y);
    } else {
      esp_zb_zcl_move_to_level_cmd_t cmd_level;
      cmd_level.zcl_basic_cmd.src_endpoint = HA_COLOR_DIMMABLE_SWITCH_ENDPOINT;
      cmd_level.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
      cmd_level.level = level_value;
      cmd_level.transition_time = 0xffff;
      esp_zb_lock_acquire(portMAX_DELAY);
      esp_zb_zcl_level_move_to_level_with_onoff_cmd_req(&cmd_level);
      esp_zb_lock_release();
      ESP_EARLY_LOGI(TAG, "Send command for moving light to %d level",
                     level_value);
      level_value += step;
    }
    press_count++;
  }
}

static esp_err_t deferred_driver_init(void) {
  ESP_RETURN_ON_FALSE(switch_driver_init(button_func_pair,
                                         PAIR_SIZE(button_func_pair),
                                         zb_buttons_handler),
                      ESP_FAIL, TAG, "Failed to initialize switch driver");
  return ESP_OK;
}

static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask) {
  ESP_RETURN_ON_FALSE(esp_zb_bdb_start_top_level_commissioning(mode_mask) ==
                          ESP_OK,
                      , TAG, "Failed to start Zigbee bdb commissioning");
}

static void bind_cb(esp_zb_zdp_status_t zdo_status, void *user_ctx) {
  if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS) {
    ESP_LOGI(TAG, "Bound successfully!");
    if (user_ctx) {
      light_bulb_device_params_t *light =
          (light_bulb_device_params_t *)user_ctx;
      ESP_LOGI(TAG, "The light originating from address(0x%x) on endpoint(%d)",
               light->short_addr, light->endpoint);
      free(light);
    }
  }
}

static void user_find_cb(esp_zb_zdp_status_t zdo_status, uint16_t addr,
                         uint8_t endpoint, void *user_ctx) {
  if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS) {
    ESP_LOGI(TAG, "Found dimmable light");
    esp_zb_zdo_bind_req_param_t bind_req;
    light_bulb_device_params_t *light = (light_bulb_device_params_t *)malloc(
        sizeof(light_bulb_device_params_t));
    light->endpoint = endpoint;
    light->short_addr = addr;
    esp_zb_ieee_address_by_short(light->short_addr, light->ieee_addr);
    esp_zb_get_long_address(bind_req.src_address);
    bind_req.src_endp = HA_COLOR_DIMMABLE_SWITCH_ENDPOINT;
    bind_req.cluster_id = ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL;
    bind_req.dst_addr_mode = ESP_ZB_ZDO_BIND_DST_ADDR_MODE_64_BIT_EXTENDED;
    memcpy(bind_req.dst_address_u.addr_long, light->ieee_addr,
           sizeof(esp_zb_ieee_addr_t));
    bind_req.dst_endp = endpoint;
    bind_req.req_dst_addr =
        esp_zb_get_short_address(); /* TODO: Send bind request to self */
    ESP_LOGI(TAG, "Try to bind color control");
    esp_zb_zdo_device_bind_req(&bind_req, bind_cb, NULL);
    bind_req.cluster_id = ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL;
    ESP_LOGI(TAG, "Try to bind level control");
    esp_zb_zdo_device_bind_req(&bind_req, bind_cb, (void *)light);
  }
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct) {
  uint32_t *p_sg_p = signal_struct->p_app_signal;
  esp_err_t err_status = signal_struct->esp_err_status;
  esp_zb_app_signal_type_t sig_type = *p_sg_p;
  esp_zb_zdo_signal_device_annce_params_t *dev_annce_params = NULL;

  switch (sig_type) {
  case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
#if CONFIG_EXAMPLE_CONNECT_WIFI
    ESP_RETURN_ON_FALSE(example_connect() == ESP_OK, , TAG,
                        "Failed to connect to Wi-Fi");
#if CONFIG_ESP_COEX_SW_COEXIST_ENABLE
    ESP_RETURN_ON_FALSE(esp_wifi_set_ps(WIFI_PS_MIN_MODEM) == ESP_OK, , TAG,
                        "Failed to set Wi-Fi minimum modem power save type");
    esp_coex_wifi_i154_enable();
#else
    ESP_RETURN_ON_FALSE(esp_wifi_set_ps(WIFI_PS_NONE) == ESP_OK, , TAG,
                        "Failed to set Wi-Fi no power save type");
#endif
#endif
    ESP_LOGI(TAG, "Initialize Zigbee stack");
    esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
    break;
  case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
  case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
    if (err_status == ESP_OK) {
      ESP_LOGI(TAG, "Deferred driver initialization %s",
               deferred_driver_init() ? "failed" : "successful");
      ESP_LOGI(TAG, "Device started up in %s factory-reset mode",
               esp_zb_bdb_is_factory_new() ? "" : "non");

      if (esp_zb_bdb_is_factory_new()) {
        ESP_LOGI(TAG, "Start network formation");
        esp_zb_bdb_start_top_level_commissioning(
            ESP_ZB_BDB_MODE_NETWORK_FORMATION);
      } else {
        esp_zb_bdb_open_network(180);
        ESP_LOGI(TAG, "Device rebooted");
      }
    } else {
      ESP_LOGE(TAG, "Failed to initialize Zigbee stack (status: %s)",
               esp_err_to_name(err_status));
    }
    break;
  case ESP_ZB_BDB_SIGNAL_FORMATION:
    if (err_status == ESP_OK) {
      esp_zb_ieee_addr_t ieee_address;
      esp_zb_get_long_address(ieee_address);
      ESP_LOGI(TAG,
               "Formed network successfully (Extended PAN ID: "
               "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, "
               "Channel:%d, Short Address: 0x%04hx)",
               ieee_address[7], ieee_address[6], ieee_address[5],
               ieee_address[4], ieee_address[3], ieee_address[2],
               ieee_address[1], ieee_address[0], esp_zb_get_pan_id(),
               esp_zb_get_current_channel(), esp_zb_get_short_address());
      esp_zb_bdb_start_top_level_commissioning(
          ESP_ZB_BDB_MODE_NETWORK_STEERING);
    } else {
      ESP_LOGI(TAG, "Restart network formation (status: %s)",
               esp_err_to_name(err_status));
      esp_zb_scheduler_alarm(
          (esp_zb_callback_t)bdb_start_top_level_commissioning_cb,
          ESP_ZB_BDB_MODE_NETWORK_FORMATION, 1000);
    }
    break;
  case ESP_ZB_BDB_SIGNAL_STEERING:
    if (err_status == ESP_OK) {
      ESP_LOGI(TAG, "Network steering started");
    }
    break;
  case ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE:
    dev_annce_params =
        (esp_zb_zdo_signal_device_annce_params_t *)esp_zb_app_signal_get_params(
            p_sg_p);
    ESP_LOGI(TAG, "New device commissioned or rejoined (short: 0x%04hx)",
             dev_annce_params->device_short_addr);
    /* find color dimmable light once device joining the network */
    esp_zb_zdo_match_desc_req_param_t  cmd_req;
    cmd_req.dst_nwk_addr = dev_annce_params->device_short_addr;
    cmd_req.addr_of_interest = dev_annce_params->device_short_addr;
    esp_zb_zdo_find_color_dimmable_light(&cmd_req, user_find_cb, NULL);
    break;
  case ESP_ZB_NWK_SIGNAL_PERMIT_JOIN_STATUS:
    if (err_status == ESP_OK) {
      if (*(uint8_t *)esp_zb_app_signal_get_params(p_sg_p)) {
        ESP_LOGI(TAG, "Network(0x%04hx) is open for %d seconds",
                 esp_zb_get_pan_id(),
                 *(uint8_t *)esp_zb_app_signal_get_params(p_sg_p));
      } else {
        ESP_LOGW(TAG, "Network(0x%04hx) closed, devices joining not allowed.",
                 esp_zb_get_pan_id());
      }
    }
    break;
  case ESP_ZB_ZDO_SIGNAL_PRODUCTION_CONFIG_READY:
    ESP_LOGI(TAG, "Production configuration is ready");
    if (err_status == ESP_OK) {
      app_production_config_t *prod_cfg =
          (app_production_config_t *)esp_zb_app_signal_get_params(p_sg_p);
      if (prod_cfg->version == APP_PROD_CFG_CURRENT_VERSION) {
        ESP_LOGI(TAG, "Manufacturer_code: 0x%x, manufacturer_name:%s",
                 prod_cfg->manuf_code, prod_cfg->manuf_name);
        esp_zb_set_node_descriptor_manufacturer_code(prod_cfg->manuf_code);
      }
    } else {
      ESP_LOGW(TAG, "Production configuration is not present");
    }
    break;
  default:
    ESP_LOGI(TAG, "ZDO signal: %s (0x%x), status: %s",
             esp_zb_zdo_signal_to_string(sig_type), sig_type,
             esp_err_to_name(err_status));
    break;
  }
}

static void esp_zb_task(void *pvParameters) {
  /* initialize Zigbee stack */
  esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZC_CONFIG();
  esp_zb_init(&zb_nwk_cfg);
  esp_zb_set_primary_network_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK);
  esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();
  esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();
  esp_zb_endpoint_config_t endpoint_config = {
      .endpoint = ESP_ZB_GATEWAY_ENDPOINT,
      .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
      .app_device_id = ESP_ZB_HA_REMOTE_CONTROL_DEVICE_ID,
      .app_device_version = 0,
  };

  esp_zb_attribute_list_t *basic_cluster = esp_zb_basic_cluster_create(NULL);
  esp_zb_basic_cluster_add_attr(basic_cluster,
                                ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID,
                                ESP_MANUFACTURER_NAME);
  esp_zb_basic_cluster_add_attr(basic_cluster,
                                ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID,
                                ESP_MODEL_IDENTIFIER);
  esp_zb_cluster_list_add_basic_cluster(cluster_list, basic_cluster,
                                        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_identify_cluster(cluster_list,
                                           esp_zb_identify_cluster_create(NULL),
                                           ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_ep_list_add_gateway_ep(ep_list, cluster_list, endpoint_config);
  esp_zb_device_register(ep_list);
  ESP_ERROR_CHECK(esp_zb_start(false));
  esp_zb_stack_main_loop();
  vTaskDelete(NULL);
}

void app_main(void) {
  esp_zb_platform_config_t config = {
      .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
      .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
  };
  ESP_ERROR_CHECK(nvs_flash_init());
  ESP_ERROR_CHECK(esp_zb_platform_config(&config));
  xTaskCreate(esp_zb_task, "Zigbee_main", 4096, NULL, 5, NULL);
}
