# ESP-NOW led

ESP-NOW based led controller/light for ESP32 ESP-IDF and ESP8266 RTOS SDK. Alternate firmware for Tuya/SmartLife/eWeLink WiFi led controllers/lights.

## Tested on

1. ESP8266 RTOS_SDK v3.4
2. ESP32 ESP-IDF v5.2

## Features

1. Saves the last state when the power is turned off.
2. Automatically adds led configuration to Home Assistan via MQTT discovery as a light.
3. Update firmware from HTTPS server via ESP-NOW.
4. Direct or mesh work mode.

## Notes

1. Work mode must be same with [gateway](https://github.com/aZholtikov/zh_gateway) work mode.
2. ESP-NOW mesh network based on the [zh_network](https://github.com/aZholtikov/zh_network).
3. For initial settings use "menuconfig -> ZH ESP-NOW Led Configuration". After first boot all settings will be stored in NVS memory for prevente change during OTA firmware update.
4. To restart the led, send the "restart" command to the root topic of the led (example - "homeassistant/espnow_led/24-62-AB-F9-1F-A8").
5. To update the led firmware, send the "update" command to the root topic of the led (example - "homeassistant/espnow_led/70-03-9F-44-BE-F7"). The update path should be like as "https://your_server/zh_espnow_led_esp32.bin" (for ESP32) or "https://your_server/zh_espnow_led_esp8266.app1.bin + https://your_server/zh_espnow_led_esp8266.app2.bin" (for ESP8266). The time and success of the update depends on the load on WiFi channel 1. Average update time is up to five minutes. The online status of the update will be displayed in the root led topic.
6. To change initial settings of the led (except work mode), send the X1,X2,X3,X4,X5,X6 command to the hardware topic of the led (example - "homeassistant/espnow_led/70-03-9F-44-BE-F7/hardware"). The configuration will only be accepted if it does not cause errors. The current configuration status is displayed in the configuration topic of the led (example - "homeassistant/espnow_led/70-03-9F-44-BE-F7/config").

MQTT configuration message should filled according to the template "X1,X2,X3,X4,X5,X6". Where:

1. X1 - Led type. 1 for W, 2 for WW, 3 for RGB, 4 for RGBW, 5 for RGBWW.
2. X2 - First white GPIO number. 0 - 48 (according to the module used), 255 if not used.
3. X3 - Second white GPIO number. 0 - 48 (according to the module used), 255 if not used.
4. X4 - Red GPIO number. 0 - 48 (according to the module used), 255 if not used.
5. X5 - Green GPIO number. 0 - 48 (according to the module used), 255 if not used.
6. X6 - Blue GPIO number. 0 - 48 (according to the module used), 255 if not used.

Legend:
W - Cold white or Warm white or one another color.
WW - Cold white + Warm white.
RGB - Red + Green + Blue colors.
RGBW - Red + Green + Blue + Cold white or Warm white colors.
RGBWW - Red + Green + Blue + Cold white + Warm white colors.

## Build and flash

Run the following command to firmware build and flash module:

```text
cd your_projects_folder
git clone --recurse-submodules https://github.com/aZholtikov/zh_espnow_led.git
cd zh_espnow_led
```

For ESP32 family:

```text
idf.py set-target (TARGET) // Optional
idf.py menuconfig
idf.py build
idf.py flash
```

For ESP8266 family:

```text
make menuconfig
make
make flash
```

## Tested on hardware

See [here](https://github.com/aZholtikov/zh_espnow_led/tree/main/hardware).

## Attention

1. A gateway is required. For details see [zh_gateway](https://github.com/aZholtikov/zh_gateway).
2. Use the "make ota" command instead of "make" to prepare 2 bin files for OTA update.

Any [feedback](mailto:github@azholtikov.ru) will be gladly accepted.
