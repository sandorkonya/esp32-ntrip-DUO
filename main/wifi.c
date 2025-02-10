#include <string.h>
#include <esp_wifi.h>
#include <esp_log.h>
#include <nvs.h>
#include <nvs_flash.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_netif.h"
#include "esp_netif_types.h"
#include "wifi.h"
#include "config.h"  // Added to include config parameters
#include "network.h" // Include network.h to check the status of the network interface

static const char *TAG = "WIFI";

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define WIFI_GOT_IP_BIT    BIT2
#define WIFI_DISCONNECTED_BIT BIT3


#define NVS_NAMESPACE "wifi_config"
#define NVS_KEY_SSID "ssid"
#define NVS_KEY_PASSWORD "password"

static EventGroupHandle_t s_wifi_event_group = NULL;

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

static void wifi_disconnect_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

static void ip_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

static bool load_wifi_config(char *ssid, char *password);

static void save_wifi_config(const char *ssid, const char *password);


void wifi_init() {
    // Initialize the event group
   if (s_wifi_event_group == NULL){
        s_wifi_event_group = xEventGroupCreate();
    }

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL);
      esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &wifi_disconnect_handler, NULL);

    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_init_config);

    char ssid[33] = "";
    char password[65] = "";
    
    wifi_config_t wifi_config = {0};

    // Load saved wifi if available
     if (load_wifi_config(ssid, password)){
          ESP_LOGI(TAG, "Loaded saved WiFi config. connecting...");
       strcpy((char*)wifi_config.sta.ssid, ssid);
       strcpy((char*)wifi_config.sta.password, password);
       wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
       wifi_config.sta.pmf_cfg.capable = true;
       wifi_config.sta.pmf_cfg.required = false;

        esp_wifi_set_mode(WIFI_MODE_STA);
        esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
        esp_wifi_start();
        esp_wifi_connect();


    } else {
        // Start in hotspot mode if no saved config
        ESP_LOGI(TAG, "Starting in Access Point mode");
        strcpy((char*)wifi_config.ap.ssid, "ESP32-NTRIP-AP");
        strcpy((char*)wifi_config.ap.password, "password123");
        wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
        wifi_config.ap.max_connection = 4;

        esp_wifi_set_mode(WIFI_MODE_AP);
        esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
        esp_wifi_start();
        }


    ESP_LOGI(TAG, "WiFi initialization complete");
}

void wifi_set_ap_config(const char *ssid, const char *password){
  wifi_config_t wifi_config;
  esp_wifi_get_config(WIFI_IF_AP, &wifi_config);
  strncpy((char*)wifi_config.ap.ssid, ssid, sizeof(wifi_config.ap.ssid) - 1);
  strncpy((char*)wifi_config.ap.password, password, sizeof(wifi_config.ap.password) -1);
  wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
  esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
}

void wifi_set_sta_config(const char *ssid, const char *password)
{

    wifi_config_t wifi_config;
    esp_wifi_get_config(WIFI_IF_STA, &wifi_config);
   
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) -1 );
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

     esp_wifi_set_mode(WIFI_MODE_STA);
      esp_wifi_set_config(WIFI_IF_STA, &wifi_config);

    esp_wifi_connect();

    save_wifi_config(ssid, password);

}


wifi_ap_record_t * wifi_scan(uint16_t *number) {
    ESP_LOGI(TAG, "Starting WiFi scan");

     wifi_scan_config_t scan_config = {
            .ssid = NULL,
            .bssid = NULL,
            .channel = 0,
            .show_hidden = true,
            .scan_type = WIFI_SCAN_TYPE_ACTIVE,
            .scan_time.active.min = 10,
            .scan_time.active.max = 50,
        };

    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));

    uint16_t ap_count;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));

    if (ap_count == 0) {
        *number = 0;
        ESP_LOGW(TAG, "WiFi scan complete, no AP found");
        return NULL;
    }
    
    wifi_ap_record_t *ap_records = (wifi_ap_record_t *) malloc(sizeof(wifi_ap_record_t) * ap_count);
    if(ap_records == NULL){
       *number = 0;
        ESP_LOGE(TAG, "Memory allocation failed.");
        return NULL;
    }

    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_records));
    
    *number = ap_count;

    ESP_LOGI(TAG, "WiFi scan complete, %d AP(s) found", ap_count);
    return ap_records;
}


wifi_sta_list_t *wifi_ap_sta_list() {
     wifi_sta_list_t *sta_list = (wifi_sta_list_t *) malloc(sizeof(wifi_sta_list_t));
    if (sta_list == NULL){
       ESP_LOGE(TAG, "Memory allocation failed.");
        return NULL;
    }

    esp_wifi_ap_get_sta_list(sta_list);

    return sta_list;
}


void wifi_ap_status(wifi_ap_status_t *status) {
    wifi_config_t wifi_config;
    esp_wifi_get_config(WIFI_IF_AP, &wifi_config);

    status->active = false;
    wifi_mode_t mode;
    esp_wifi_get_mode(&mode);
    if (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA) {
         status->active = true;
    }

    strncpy(status->ssid, (const char*)wifi_config.ap.ssid, sizeof(status->ssid) - 1);
    status->ssid[sizeof(status->ssid) - 1] = '\0';
    status->authmode = wifi_config.ap.authmode;

    wifi_sta_list_t *sta_list = wifi_ap_sta_list();
    if (sta_list == NULL){
        status->devices = 0;
    }else{
         status->devices = sta_list->num;
          free(sta_list);
    }



    esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (ap_netif != NULL) {
        esp_netif_ip_info_t ip_info;
        if(esp_netif_get_ip_info(ap_netif, &ip_info) == ESP_OK) {
            status->ip4_addr = ip_info.ip;
        }
    }

    esp_ip6_addr_t ip6_addr;
     if(esp_netif_get_ip6_global(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip6_addr) == ESP_OK) {
          status->ip6_addr = ip6_addr;
    }
}


void wifi_sta_status(wifi_sta_status_t *status) {
    status->active = false;
    status->connected = false;
     wifi_mode_t mode;
     esp_wifi_get_mode(&mode);
    if (mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA) {
         status->active = true;
    }
    wifi_ap_record_t ap_info;
    if(esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK){
    status->connected = true;
    strncpy(status->ssid, (const char*)ap_info.ssid, sizeof(status->ssid) - 1);
    status->ssid[sizeof(status->ssid) - 1] = '\0';
    status->authmode = ap_info.authmode;
    status->rssi = ap_info.rssi;
  }

    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if(sta_netif != NULL) {
        esp_netif_ip_info_t ip_info;
       if(esp_netif_get_ip_info(sta_netif, &ip_info) == ESP_OK) {
           status->ip4_addr = ip_info.ip;
       }
    }

     esp_ip6_addr_t ip6_addr;
      if(esp_netif_get_ip6_global(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip6_addr) == ESP_OK) {
          status->ip6_addr = ip6_addr;
    }
}


void wait_for_ip() {
     if(network_is_ethernet()) {
        ESP_LOGI(TAG, "Waiting for Ethernet connection...");
         esp_netif_t *eth_netif = esp_netif_get_handle_from_ifkey("ETH_DEF");
         if(eth_netif == NULL){
          ESP_LOGE(TAG, "No Ethernet interface found.");
          return;
        }
       esp_netif_ip_info_t ip_info;
         if(esp_netif_get_ip_info(eth_netif, &ip_info) == ESP_OK){
             if(ip_info.ip.addr != 0){
              ESP_LOGI(TAG, "Ethernet got IP.");
            return;
            }
        }

    } else{
         ESP_LOGI(TAG, "Waiting for WiFi connection...");
          if (s_wifi_event_group == NULL){
                ESP_LOGE(TAG, "Event group not initialized.");
                 return;
           }

          EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_GOT_IP_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

        if (!(bits & WIFI_GOT_IP_BIT)) {
           ESP_LOGE(TAG, "Failed to get IP Address from WiFi.");
         }else{
               ESP_LOGI(TAG, "WiFi got IP.");
          }
     }

}


void wait_for_network() {
    if(network_is_ethernet()) {
        ESP_LOGI(TAG, "Waiting for Ethernet connection...");
        esp_netif_t *eth_netif = esp_netif_get_handle_from_ifkey("ETH_DEF");
         if (eth_netif == NULL){
          ESP_LOGE(TAG, "No Ethernet interface found.");
         return;
        }
          esp_netif_ip_info_t ip_info;
         while(ip_info.ip.addr == 0){
             esp_netif_get_ip_info(eth_netif, &ip_info);
              vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
          ESP_LOGI(TAG, "Ethernet got IP");

    } else {
         ESP_LOGI(TAG, "Waiting for WiFi connection...");
          if (s_wifi_event_group == NULL){
                 ESP_LOGE(TAG, "Event group not initialized.");
                  return;
           }
            EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
              WIFI_CONNECTED_BIT,
              pdFALSE,
              pdFALSE,
              portMAX_DELAY);

            if (!(bits & WIFI_CONNECTED_BIT)) {
                ESP_LOGE(TAG, "Failed to connect to WiFi.");
             }else{
               ESP_LOGI(TAG, "WiFi connected.");
            }
    }
}


const char *esp_netif_name(esp_netif_t *esp_netif) {
    if(esp_netif == NULL){
        return "NO_INTERFACE";
    }
  const char *name = esp_netif_get_desc(esp_netif);
  return name == NULL ? "UNKNOWN_INTERFACE" : name;
}

const char * wifi_auth_mode_name(wifi_auth_mode_t auth_mode) {
    switch (auth_mode) {
        case WIFI_AUTH_OPEN:
           return "OPEN";
        case WIFI_AUTH_WEP:
            return "WEP";
        case WIFI_AUTH_WPA_PSK:
             return "WPA_PSK";
        case WIFI_AUTH_WPA2_PSK:
            return "WPA2_PSK";
        case WIFI_AUTH_WPA_WPA2_PSK:
              return "WPA_WPA2_PSK";
        case WIFI_AUTH_WPA2_ENTERPRISE:
             return "WPA2_ENTERPRISE";
        case WIFI_AUTH_WPA3_PSK:
             return "WPA3_PSK";
         case WIFI_AUTH_WPA2_WPA3_PSK:
             return "WPA2_WPA3_PSK";
          case WIFI_AUTH_WAPI_PSK:
            return "WAPI_PSK";
        default:
             return "UNKNOWN";
    }
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_STA_START) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
         ESP_LOGI(TAG, "WiFi Station started");
    } else if (event_id == WIFI_EVENT_STA_CONNECTED) {
         ESP_LOGI(TAG, "WiFi connected to the AP");
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGE(TAG, "WiFi Station disconnected from AP");
        esp_wifi_connect();
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
       xEventGroupClearBits(s_wifi_event_group, WIFI_GOT_IP_BIT);
    }
}


static void wifi_disconnect_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data){
     if(event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "WiFi disconnected. Try to reconnect.");
       xEventGroupClearBits(s_wifi_event_group, WIFI_GOT_IP_BIT);
     }
}


static void ip_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
   ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;

    ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    xEventGroupSetBits(s_wifi_event_group, WIFI_GOT_IP_BIT);
}


static bool load_wifi_config(char *ssid, char *password) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS open failed: %s", esp_err_to_name(err));
        return false;
    }

    size_t ssid_size = 33;
     size_t password_size = 65;

    err = nvs_get_str(nvs_handle, NVS_KEY_SSID, ssid, &ssid_size);
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        ESP_LOGW(TAG, "NVS get SSID failed: %s", esp_err_to_name(err));
        return false;
    }
     err = nvs_get_str(nvs_handle, NVS_KEY_PASSWORD, password, &password_size);
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        ESP_LOGW(TAG, "NVS get Password failed: %s", esp_err_to_name(err));
       return false;
    }

    nvs_close(nvs_handle);
    return true;
}

static void save_wifi_config(const char *ssid, const char *password) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS open failed: %s", esp_err_to_name(err));
        return;
    }

     err = nvs_set_str(nvs_handle, NVS_KEY_SSID, ssid);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS set SSID failed: %s", esp_err_to_name(err));
           nvs_close(nvs_handle);
            return;
     }

    err = nvs_set_str(nvs_handle, NVS_KEY_PASSWORD, password);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS set Password failed: %s", esp_err_to_name(err));
       nvs_close(nvs_handle);
        return;
    }

   err = nvs_commit(nvs_handle);
   if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS commit failed: %s", esp_err_to_name(err));
    }

    nvs_close(nvs_handle);
      ESP_LOGI(TAG, "WiFi config saved.");
}