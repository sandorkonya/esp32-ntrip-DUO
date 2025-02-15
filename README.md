# ESP32 NTRIP Duo WT32-ETH01 

This version had been updated from ESP-IDF 4.1 to v5.2.3 and adapted to the WT32-ETH01 ESP32 Module.


UPDATE 2025-02-15:
[Release v0.2](https://github.com/sandorkonya/esp32-ntrip-DUO/releases/tag/v0.3) has the:

- NTRIP Caster functionality back (not fully tested though, can be buggy)


UPDATE 2025-02-14:
[Release v0.2](https://github.com/sandorkonya/esp32-ntrip-DUO/releases/tag/v0.2) has the:

 - socket client & server
 - increased stability by removing the button component

[Release v0.1](https://github.com/sandorkonya/esp32-ntrip-DUO/releases/tag/v0.1) has the features:

- WiFi Station
- WiFi Hotspot
- Web Interface
- UART configuration
- Two NTRIP Servers

## Pinout
By default it is set for UART0 TX gpio1, RX gpio3 (also default for the WT32-ETH01 module)
