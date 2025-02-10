#include "network.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_eth.h"
#include "esp_eth_mac.h"
#include "config.h" // Include if you need any config values in this file


static const char *TAG = "NETWORK";

static esp_netif_t *global_netif = NULL;
static bool ethernet_active = false;


esp_netif_t *network_init() {
    esp_eth_handle_t eth_handle = NULL;
    esp_eth_mac_t *mac = NULL;
    esp_eth_phy_t *phy = NULL;

    // --- Ethernet Initialization ---
    ESP_LOGI(TAG, "Initializing Ethernet MAC for WirelessTag WT32-ETH01...");

    eth_esp32_emac_config_t esp32_emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    esp32_emac_config.clock_config.rmii.clock_mode = EMAC_CLK_EXT_IN;
    esp32_emac_config.clock_config.rmii.clock_gpio = EMAC_CLK_IN_GPIO;
    esp32_emac_config.smi_mdc_gpio_num = 23;
    esp32_emac_config.smi_mdio_gpio_num = 18;


    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();

    mac = esp_eth_mac_new_esp32(&esp32_emac_config, &mac_config);


    ESP_LOGI(TAG, "Initializing Ethernet PHY (LAN8720A) for WT32-ETH01...");
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = 1;
    phy_config.reset_gpio_num = -1;
    phy = esp_eth_phy_new_lan87xx(&phy_config);

    // Enable external oscillator (pulled down at boot to allow IO0 strapping)
    ESP_ERROR_CHECK(gpio_set_direction(GPIO_NUM_16, GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_16, 1));
    ESP_LOGI(TAG, "Starting Ethernet interface...");

    // Install and start Ethernet driver
    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);

    ESP_ERROR_CHECK(esp_eth_driver_install(&eth_config, &eth_handle));

    esp_netif_config_t netif_config = ESP_NETIF_DEFAULT_ETH();
    global_netif = esp_netif_new(&netif_config);
    esp_netif_attach(global_netif, esp_eth_new_netif_glue(eth_handle));

    esp_err_t start_result = esp_eth_start(eth_handle);


    if (start_result == ESP_OK) {
        ethernet_active = true;
        ESP_LOGI(TAG, "Ethernet started successfully.");
        return global_netif;
    } else {
        ESP_LOGE(TAG, "Ethernet start failed: %s", esp_err_to_name(start_result));
        esp_netif_destroy(global_netif); // Clean up if ethernet fails
        return NULL;
    }
}

bool network_is_ethernet(){
    return ethernet_active;
}