# ESP-NOW led

ESP-NOW based led controller/light for ESP8266 RTOS SDK. Alternate firmware for Tuya/SmartLife/eWeLink WiFi led controllers/lights.

There are two branches - for ESP8266 family and for ESP32 family. Please use the appropriate one.

## Features

1. Saves the last state when the power is turned off.
2. Automatically adds led configuration to Home Assistan via MQTT discovery as a light.
3. Update firmware from HTTPS server via ESP-NOW.

## Notes

1. For initial settings use "menuconfig -> ZH ESP-NOW Led Configuration". After first boot all settings will be stored in NVS memory for prevente change during OTA firmware update.
2. To restart the led, send the "restart" command to the root topic of the led (example - "homeassistant/espnow_led/24-62-AB-F9-1F-A8").
3. To update the led firmware, send the "update" command to the root topic of the led (example - "homeassistant/espnow_led/70-03-9F-44-BE-F7"). The update paths should be like as "https://your_server/zh_espnow_led_esp8266.app1.bin + https://your_server/zh_espnow_led_esp8266.app2.bin". The time and success of the update depends on the load on WiFi channel 1. Average update time is up to five minutes. The online status of the update is displayed in the root led topic.

## Build and flash

Run the following command to firmware build and flash module:

```text
cd your_projects_folder
bash <(curl -Ls http://git.zh.com.ru/alexey.zholtikov/zh_espnow_led/raw/branch/esp8266/install.sh)
cd zh_espnow_led
make menuconfig
make
make flash
```

## Tested on

See [here](http://git.zh.com.ru/alexey.zholtikov/zh_espnow_led/src/branch/esp8266/hardware).

## Attention

1. A gateway is required. For details see [zh_gateway](http://git.zh.com.ru/alexey.zholtikov/zh_gateway).
2. Use the "make ota" command instead of "make" to prepare 2 bin files for OTA update.

Any [feedback](mailto:github@azholtikov.ru) will be gladly accepted.