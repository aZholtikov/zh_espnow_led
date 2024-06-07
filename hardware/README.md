# Tested on

1. AP-FUTURE RGB led controller. Built on Tuya WiFi module WB3S (BK7231N chip). Replacement with ESP-12E. [Photo](http://git.zh.com.ru/alexey.zholtikov/zh_espnow_led/src/branch/esp8266/hardware/AP-FUTURE_RGB). A connect EN to VCC is required. A pull-down to GND of GPIO15 is required.

```text
    Using menuconfig:

    Led type            RGB
    Red GPIO number     04
    Green GPIO number   12
    Blue GPIO number    14

    Using MQTT:

    "3,255,255,4,12,14"
```

2. FLYIDEA RGBW light E27 15W (coming soon)
3. FLYIDEA RGBWW light E14 7W (coming soon)