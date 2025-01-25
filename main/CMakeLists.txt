idf_component_register(SRCS "main.c"
		"config.c"
		"core_dump.c"
		"log.c"
		"interface/ntrip_util.c"
		"retry.c"
		"status_led.c"
		"stream_stats.c"
		"uart.c"
		"util.c"
		"web_server.c"
		"wifi.c"
		"../button/button.c"
		"interface/ntrip_server.c"
		"interface/ntrip_server_2.c"
		"protocol/nmea.c"
        INCLUDE_DIRS "include" "../button"
		REQUIRES button esp_eth esp_timer app_update  esp_bootloader_format bootloader bootloader_support soc esp_netif driver nvs_flash esp_partition mbedtls tcp_transport lwip esp_partition esp_http_server espcoredump esp_https_ota esp_wifi mbedtls vfs json tcp_transport esp_timer spiffs

		)

spiffs_create_partition_image(www ../www FLASH_IN_PROJECT)