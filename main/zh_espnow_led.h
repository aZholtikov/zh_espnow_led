#pragma once

#include "stdio.h"
#include "string.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_timer.h"
#include "esp_ota_ops.h"
#include "zh_config.h"

#ifdef CONFIG_NETWORK_TYPE_DIRECT
#include "zh_espnow.h"
#define zh_send_message(a, b, c) zh_espnow_send(a, b, c)
#define ZH_EVENT ZH_ESPNOW
#else
#include "zh_network.h"
#define zh_send_message(a, b, c) zh_network_send(a, b, c)
#define ZH_EVENT ZH_NETWORK
#endif

#ifdef CONFIG_IDF_TARGET_ESP8266
#define ZH_CHIP_TYPE HACHT_ESP8266
#elif CONFIG_IDF_TARGET_ESP32
#define ZH_CHIP_TYPE HACHT_ESP32
#elif CONFIG_IDF_TARGET_ESP32S2
#define ZH_CHIP_TYPE HACHT_ESP32S2
#elif CONFIG_IDF_TARGET_ESP32S3
#define ZH_CHIP_TYPE HACHT_ESP32S3
#elif CONFIG_IDF_TARGET_ESP32C2
#define ZH_CHIP_TYPE HACHT_ESP32C2
#elif CONFIG_IDF_TARGET_ESP32C3
#define ZH_CHIP_TYPE HACHT_ESP32C3
#elif CONFIG_IDF_TARGET_ESP32C6
#define ZH_CHIP_TYPE HACHT_ESP32C6
#endif

#ifdef CONFIG_IDF_TARGET_ESP8266
#define ZH_CPU_FREQUENCY CONFIG_ESP8266_DEFAULT_CPU_FREQ_MHZ;
#define get_app_description() esp_ota_get_app_description()
#else
#define ZH_CPU_FREQUENCY CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ;
#define get_app_description() esp_app_get_description()
#endif

#define ZH_LED_KEEP_ALIVE_MESSAGE_FREQUENCY 10 // Frequency of sending a led keep alive message to the gateway (in seconds).
#define ZH_LED_ATTRIBUTES_MESSAGE_FREQUENCY 60 // Frequency of sending a led attributes message to the gateway (in seconds).

#define ZH_MESSAGE_TASK_PRIORITY 2 // Prioritize the task of sending messages to the gateway.
#define ZH_MESSAGE_STACK_SIZE 2048 // The stack size of the task of sending messages to the gateway.

typedef struct // Structure of data exchange between tasks, functions and event handlers.
{
    struct // Storage structure of led hardware configuration data.
    {
        ha_led_type_t led_type;   // Led type.
        uint8_t first_white_pin;  // First white GPIO number.
        uint8_t second_white_pin; // Second white GPIO number (if present).
        uint8_t red_pin;          // Red GPIO number (if present).
        uint8_t green_pin;        // Green GPIO number (if present).
        uint8_t blue_pin;         // Blue GPIO number (if present).
    } hardware_config;
    struct // Storage structure of led status data.
    {
        ha_on_off_type_t status;     // Status of the zh_espnow_led. @note Example - ON / OFF. @attention Must be same with set on led_config_message structure.
        uint8_t brightness;          // Brightness value.
        uint16_t temperature;        // White color temperature value (if present).
        uint8_t red;                 // Red color value (if present).
        uint8_t green;               // Green color value (if present).
        uint8_t blue;                // Blue color value (if present).
        ha_led_effect_type_t effect; // Reserved for future development.
    } status;
    struct // Structure of led channels data.
    {
        uint8_t first_white;  // First white channel.
        uint8_t second_white; // Second white channel.
        uint8_t red;          // Red color channel.
        uint8_t green;        // Green color channel.
        uint8_t blue;         // Blue color channel.
    } channel;
    volatile bool gateway_is_available;      // Gateway availability status flag. @note Used to control the tasks when the gateway connection is established / lost.
    volatile bool is_first_connection;       // First connection status flag. @note Used to control the tasks when the gateway connection is established / lost.
    uint8_t gateway_mac[6];                  // Gateway MAC address.
    TaskHandle_t attributes_message_task;    // Unique task handle for zh_send_led_attributes_message_task().
    TaskHandle_t keep_alive_message_task;    // Unique task handle for zh_send_led_keep_alive_message_task().
    const esp_partition_t *update_partition; // Next update partition.
    esp_ota_handle_t update_handle;          // Unique OTA handle.
    uint16_t ota_message_part_number;        // The sequence number of the firmware upgrade part. @note Used to verify that all parts have been received.
} led_config_t;

/**
 * @brief Function for loading the led hardware configuration from NVS memory.
 *
 * @param[out] led_config Pointer to structure of data exchange between tasks, functions and event handlers.
 */
void zh_load_config(led_config_t *led_config);

/**
 * @brief Function for saving the led hardware configuration to NVS memory.
 *
 * @param[in] led_config Pointer to structure of data exchange between tasks, functions and event handlers.
 */
void zh_save_config(const led_config_t *led_config);

/**
 * @brief Function for loading the led status from NVS memory.
 *
 * @param[out] led_config Pointer to structure of data exchange between tasks, functions and event handlers.
 */
void zh_load_status(led_config_t *led_config);

/**
 * @brief Function for saving the led status to NVS memory.
 *
 * @param[out] led_config Pointer to structure of data exchange between tasks, functions and event handlers.
 */
void zh_save_status(const led_config_t *led_config);

/**
 * @brief Function for GPIO initialization.
 *
 * @param[in,out] led_config Pointer to structure of data exchange between tasks, functions and event handlers.
 */
void zh_gpio_init(led_config_t *led_config);

/**
 * @brief Function for changing the led status.
 *
 * @param[in,out] led_config Pointer to structure of data exchange between tasks, functions and event handlers.
 */
void zh_gpio_set_level(const led_config_t *led_config);

/**
 * @brief Function for converts the value of a variable from one range to another.
 *
 * @param value Value to convert.
 * @param in_min Lower limit of the current range.
 * @param in_max Upper limit of the current range.
 * @param out_min Lower limit of the new range.
 * @param out_max Upper limit of the new range.
 *
 * @return Converted value
 */
int32_t zh_map(int32_t value, int32_t in_min, int32_t in_max, int32_t out_min, int32_t out_max);

/**
 * @brief Task for prepare the led attributes message and sending it to the gateway.
 *
 * @param[in,out] pvParameter Pointer to structure of data exchange between tasks, functions and event handlers.
 */
void zh_send_led_attributes_message_task(void *pvParameter);

/**
 * @brief Function for prepare the led configuration message and sending it to the gateway.
 *
 * @param[in] led_config Pointer to the structure of data exchange between tasks, functions and event handlers.
 */
void zh_send_led_config_message(const led_config_t *led_config);

/**
 * @brief Function for prepare the led hardware configuration message and sending it to the gateway.
 *
 * @param[in] led_config Pointer to the structure of data exchange between tasks, functions and event handlers.
 */
void zh_send_led_hardware_config_message(const led_config_t *led_config);

/**
 * @brief Task for prepare the led keep alive message and sending it to the gateway.
 *
 * @param[in] pvParameter Pointer to the structure of data exchange between tasks, functions and event handlers.
 */
void zh_send_led_keep_alive_message_task(void *pvParameter);

/**
 * @brief Function for prepare the led status message and sending it to the gateway.
 *
 * @param[in] led_config Pointer to the structure of data exchange between tasks, functions and event handlers.
 */
void zh_send_led_status_message(const led_config_t *led_config);

/**
 * @brief Function for ESP-NOW event processing.
 *
 * @param[in,out] arg Pointer to the structure of data exchange between tasks, functions and event handlers.
 */
void zh_espnow_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);