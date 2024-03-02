#include "stdio.h"
#include "string.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_timer.h"
#include "esp_ota_ops.h"
#include "zh_espnow.h"
#include "zh_network.h"
#include "zh_config.h"

#if CONFIG_NETWORK_TYPE_DIRECT
#define zh_send_message(a, b, c) zh_espnow_send(a, b, c)
#endif
#if CONFIG_NETWORK_TYPE_MESH
#define zh_send_message(a, b, c) zh_network_send(a, b, c)
#endif

#define ZH_MESSAGE_TASK_PRIORITY 2
#define ZH_MESSAGE_STACK_SIZE 2048

static uint8_t s_led_type = HALT_NONE;
static uint8_t s_first_white_pin = ZH_NOT_USED;
static uint8_t s_first_white_channel = 0;
static uint8_t s_second_white_pin = ZH_NOT_USED;
static uint8_t s_second_white_channel = 0;
static uint8_t s_red_pin = ZH_NOT_USED;
static uint8_t s_red_channel = 0;
static uint8_t s_green_pin = ZH_NOT_USED;
static uint8_t s_green_channel = 0;
static uint8_t s_blue_pin = ZH_NOT_USED;
static uint8_t s_blue_channel = 0;

static uint8_t s_led_status = ZH_OFF;
static uint8_t s_brightness_status = 0;
static uint16_t s_temperature_status = 255;
static uint8_t s_red_status = 0;
static uint8_t s_green_status = 0;
static uint8_t s_blue_status = 0;

static uint8_t s_gateway_mac[ESP_NOW_ETH_ALEN] = {0};
static bool s_gateway_is_available = false;

static TaskHandle_t s_led_attributes_message_task = {0};
static TaskHandle_t s_led_keep_alive_message_task = {0};

static const esp_partition_t *s_update_partition = NULL;
static esp_ota_handle_t s_update_handle = 0;
static uint16_t s_ota_message_part_number = 0;

static void s_zh_load_config(void);
static void s_zh_save_config(void);
static void s_zh_load_status(void);
static void s_zh_save_status(void);

static void s_zh_gpio_init(void);
static void s_zh_gpio_set_level(void);
static int32_t s_zh_map(int32_t value, int32_t in_min, int32_t in_max, int32_t out_min, int32_t out_max);

static void s_zh_send_led_attributes_message_task(void *pvParameter);
static void s_zh_send_led_config_message(void);
static void s_zh_send_led_keep_alive_message_task(void *pvParameter);
static void s_zh_send_led_status_message(void);

#if CONFIG_NETWORK_TYPE_DIRECT
static void s_zh_espnow_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
#endif
#if CONFIG_NETWORK_TYPE_MESH
static void s_zh_network_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
#endif
static void s_zh_set_gateway_offline_status(void);

void app_main(void)
{
#if CONFIG_LED_TYPE_W
    s_led_type = HALT_W;
    s_first_white_channel = 0;
#elif CONFIG_LED_TYPE_WW
    s_led_type = HALT_WW;
    s_first_white_channel = 0;
    s_second_white_channel = 1;
#elif CONFIG_LED_TYPE_RGB
    s_led_type = HALT_RGB;
    s_red_channel = 0;
    s_green_channel = 1;
    s_blue_channel = 2;
#elif CONFIG_LED_TYPE_RGBW
    s_led_type = HALT_RGBW;
    s_first_white_channel = 0;
    s_red_channel = 1;
    s_green_channel = 2;
    s_blue_channel = 3;
#elif CONFIG_LED_TYPE_RGBWW
    s_led_type = HALT_RGBWW;
    s_first_white_channel = 0;
    s_second_white_channel = 1;
    s_red_channel = 2;
    s_green_channel = 3;
    s_blue_channel = 4;
#endif
#if CONFIG_FIRST_WHITE_PIN
    s_first_white_pin = CONFIG_FIRST_WHITE_PIN;
#endif
#if CONFIG_SECOND_WHITE_PIN
    s_second_white_pin = CONFIG_SECOND_WHITE_PIN;
#endif
#if CONFIG_RED_PIN
    s_red_pin = CONFIG_RED_PIN;
#endif
#if CONFIG_GREEN_PIN
    s_green_pin = CONFIG_GREEN_PIN;
#endif
#if CONFIG_BLUE_PIN
    s_blue_pin = CONFIG_BLUE_PIN;
#endif
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();
    s_zh_load_config();
    s_zh_load_status();
    s_zh_gpio_init();
    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_init_config);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B);
    esp_wifi_start();
#if CONFIG_NETWORK_TYPE_DIRECT
    zh_espnow_init_config_t zh_espnow_init_config = ZH_ESPNOW_INIT_CONFIG_DEFAULT();
    zh_espnow_init(&zh_espnow_init_config);
    esp_event_handler_register(ZH_ESPNOW, ESP_EVENT_ANY_ID, &s_zh_espnow_event_handler, NULL);
#endif
#if CONFIG_NETWORK_TYPE_MESH
    zh_network_init_config_t zh_network_init_config = ZH_NETWORK_INIT_CONFIG_DEFAULT();
    zh_network_init(&zh_network_init_config);
    esp_event_handler_register(ZH_NETWORK, ESP_EVENT_ANY_ID, &s_zh_network_event_handler, NULL);
#endif
}

static void s_zh_load_config(void)
{
    nvs_handle_t nvs_handle = 0;
    nvs_open("config", NVS_READWRITE, &nvs_handle);
    uint8_t config_is_present = 0;
    if (nvs_get_u8(nvs_handle, "present", &config_is_present) == ESP_ERR_NVS_NOT_FOUND)
    {
        nvs_set_u8(nvs_handle, "present", 0xFE);
        nvs_close(nvs_handle);
        s_zh_save_config();
        return;
    }
    nvs_get_u8(nvs_handle, "led_type", &s_led_type);
    nvs_get_u8(nvs_handle, "frs_white_pin", &s_first_white_pin);
    nvs_get_u8(nvs_handle, "frs_white_ch", &s_first_white_channel);
    nvs_get_u8(nvs_handle, "sec_white_pin", &s_second_white_pin);
    nvs_get_u8(nvs_handle, "sec_white_ch", &s_second_white_channel);
    nvs_get_u8(nvs_handle, "red_pin", &s_red_pin);
    nvs_get_u8(nvs_handle, "red_ch", &s_red_channel);
    nvs_get_u8(nvs_handle, "green_pin", &s_green_pin);
    nvs_get_u8(nvs_handle, "green_ch", &s_green_channel);
    nvs_get_u8(nvs_handle, "blue_pin", &s_blue_pin);
    nvs_get_u8(nvs_handle, "blue_ch", &s_blue_channel);
    nvs_close(nvs_handle);
}

static void s_zh_save_config(void)
{
    nvs_handle_t nvs_handle = 0;
    nvs_open("config", NVS_READWRITE, &nvs_handle);
    nvs_set_u8(nvs_handle, "led_type", s_led_type);
    nvs_set_u8(nvs_handle, "frs_white_pin", s_first_white_pin);
    nvs_set_u8(nvs_handle, "frs_white_ch", s_first_white_channel);
    nvs_set_u8(nvs_handle, "sec_white_pin", s_second_white_pin);
    nvs_set_u8(nvs_handle, "sec_white_ch", s_second_white_channel);
    nvs_set_u8(nvs_handle, "red_pin", s_red_pin);
    nvs_set_u8(nvs_handle, "red_ch", s_red_channel);
    nvs_set_u8(nvs_handle, "green_pin", s_green_pin);
    nvs_set_u8(nvs_handle, "green_ch", s_green_channel);
    nvs_set_u8(nvs_handle, "blue_pin", s_blue_pin);
    nvs_set_u8(nvs_handle, "blue_ch", s_blue_channel);
    nvs_close(nvs_handle);
}

static void s_zh_load_status(void)
{
    nvs_handle_t nvs_handle = 0;
    nvs_open("status", NVS_READWRITE, &nvs_handle);
    uint8_t status_is_present = 0;
    if (nvs_get_u8(nvs_handle, "present", &status_is_present) == ESP_ERR_NVS_NOT_FOUND)
    {
        nvs_set_u8(nvs_handle, "present", 0xFE);
        nvs_close(nvs_handle);
        s_zh_save_status();
        return;
    }
    nvs_get_u8(nvs_handle, "led_state", &s_led_status);
    nvs_get_u8(nvs_handle, "bright_state", &s_brightness_status);
    nvs_get_u16(nvs_handle, "temp_state", &s_temperature_status);
    nvs_get_u8(nvs_handle, "red_state", &s_red_status);
    nvs_get_u8(nvs_handle, "green_state", &s_green_status);
    nvs_get_u8(nvs_handle, "blue_state", &s_blue_status);
    nvs_close(nvs_handle);
}

static void s_zh_save_status(void)
{
    nvs_handle_t nvs_handle = 0;
    nvs_open("status", NVS_READWRITE, &nvs_handle);
    nvs_set_u8(nvs_handle, "led_state", s_led_status);
    nvs_set_u8(nvs_handle, "bright_state", s_brightness_status);
    nvs_set_u16(nvs_handle, "temp_state", s_temperature_status);
    nvs_set_u8(nvs_handle, "red_state", s_red_status);
    nvs_set_u8(nvs_handle, "green_state", s_green_status);
    nvs_set_u8(nvs_handle, "blue_state", s_blue_status);
    nvs_close(nvs_handle);
}

static void s_zh_gpio_init(void)
{
    ledc_timer_config_t timer_config = {0};
    timer_config.freq_hz = 1000;
    ledc_timer_config(&timer_config);
    ledc_channel_config_t channel_config = {0};
    if (s_first_white_pin != ZH_NOT_USED)
    {
        channel_config.channel = s_first_white_channel;
        channel_config.gpio_num = s_first_white_pin;
        ledc_channel_config(&channel_config);
    }
    if (s_second_white_pin != ZH_NOT_USED)
    {
        channel_config.channel = s_second_white_channel;
        channel_config.gpio_num = s_second_white_pin;
        ledc_channel_config(&channel_config);
    }
    if (s_red_pin != ZH_NOT_USED)
    {
        channel_config.channel = s_red_channel;
        channel_config.gpio_num = s_red_pin;
        ledc_channel_config(&channel_config);
    }
    if (s_green_pin != ZH_NOT_USED)
    {
        channel_config.channel = s_green_channel;
        channel_config.gpio_num = s_green_pin;
        ledc_channel_config(&channel_config);
    }
    if (s_blue_pin != ZH_NOT_USED)
    {
        channel_config.channel = s_blue_channel;
        channel_config.gpio_num = s_blue_pin;
        ledc_channel_config(&channel_config);
    }
    ledc_fade_func_install(0);
    s_zh_gpio_set_level();
}

static void s_zh_gpio_set_level(void)
{
    if (s_led_status == ZH_ON)
    {
        if (s_red_status == 255 && s_green_status == 255 && s_blue_status == 255)
        {
            if (s_led_type == HALT_W || s_led_type == HALT_RGBW)
            {
                ledc_set_duty(LEDC_LOW_SPEED_MODE, s_first_white_channel, s_zh_map(s_brightness_status, 0, 255, 0, 8196));
                ledc_update_duty(LEDC_LOW_SPEED_MODE, s_first_white_channel);
            }
            if (s_led_type == HALT_WW || s_led_type == HALT_RGBWW)
            {
                ledc_set_duty(LEDC_LOW_SPEED_MODE, s_first_white_channel, s_zh_map(s_zh_map(s_brightness_status, 0, 255, 0, s_zh_map(s_temperature_status, 500, 153, 0, 255)), 0, 255, 0, 8196));
                ledc_update_duty(LEDC_LOW_SPEED_MODE, s_first_white_channel);
                ledc_set_duty(LEDC_LOW_SPEED_MODE, s_second_white_channel, s_zh_map(s_zh_map(s_brightness_status, 0, 255, 0, s_zh_map(s_temperature_status, 153, 500, 0, 255)), 0, 255, 0, 8196));
                ledc_update_duty(LEDC_LOW_SPEED_MODE, s_second_white_channel);
            }
            if (s_led_type == HALT_RGB)
            {
                ledc_set_duty(LEDC_LOW_SPEED_MODE, s_red_channel, s_zh_map(s_red_status, 0, 255, 0, s_zh_map(s_brightness_status, 0, 255, 0, 8196)));
                ledc_update_duty(LEDC_LOW_SPEED_MODE, s_red_channel);
                ledc_set_duty(LEDC_LOW_SPEED_MODE, s_green_channel, s_zh_map(s_green_status, 0, 255, 0, s_zh_map(s_brightness_status, 0, 255, 0, 8196)));
                ledc_update_duty(LEDC_LOW_SPEED_MODE, s_green_channel);
                ledc_set_duty(LEDC_LOW_SPEED_MODE, s_blue_channel, s_zh_map(s_blue_status, 0, 255, 0, s_zh_map(s_brightness_status, 0, 255, 0, 8196)));
                ledc_update_duty(LEDC_LOW_SPEED_MODE, s_blue_channel);
            }
            if (s_led_type == HALT_RGBW || s_led_type == HALT_RGBWW)
            {
                ledc_set_duty(LEDC_LOW_SPEED_MODE, s_red_channel, 0);
                ledc_update_duty(LEDC_LOW_SPEED_MODE, s_red_channel);
                ledc_set_duty(LEDC_LOW_SPEED_MODE, s_green_channel, 0);
                ledc_update_duty(LEDC_LOW_SPEED_MODE, s_green_channel);
                ledc_set_duty(LEDC_LOW_SPEED_MODE, s_blue_channel, 0);
                ledc_update_duty(LEDC_LOW_SPEED_MODE, s_blue_channel);
            }
        }
        else
        {
            if (s_led_type == HALT_W)
            {
                ledc_set_duty(LEDC_LOW_SPEED_MODE, s_first_white_channel, s_zh_map(s_brightness_status, 0, 255, 0, 8196));
                ledc_update_duty(LEDC_LOW_SPEED_MODE, s_first_white_channel);
            }
            if (s_led_type == HALT_WW)
            {
                ledc_set_duty(LEDC_LOW_SPEED_MODE, s_first_white_channel, s_zh_map(s_zh_map(s_brightness_status, 0, 255, 0, s_zh_map(s_temperature_status, 500, 153, 0, 255)), 0, 255, 0, 8196));
                ledc_update_duty(LEDC_LOW_SPEED_MODE, s_first_white_channel);
                ledc_set_duty(LEDC_LOW_SPEED_MODE, s_second_white_channel, s_zh_map(s_zh_map(s_brightness_status, 0, 255, 0, s_zh_map(s_temperature_status, 153, 500, 0, 255)), 0, 255, 0, 8196));
                ledc_update_duty(LEDC_LOW_SPEED_MODE, s_second_white_channel);
            }
            if (s_led_type == HALT_RGBW || s_led_type == HALT_RGBWW)
            {
                ledc_set_duty(LEDC_LOW_SPEED_MODE, s_first_white_channel, 0);
                ledc_update_duty(LEDC_LOW_SPEED_MODE, s_first_white_channel);
            }
            if (s_led_type == HALT_RGBWW)
            {
                ledc_set_duty(LEDC_LOW_SPEED_MODE, s_second_white_channel, 0);
                ledc_update_duty(LEDC_LOW_SPEED_MODE, s_second_white_channel);
            }
            if (s_led_type == HALT_RGB || s_led_type == HALT_RGBW || s_led_type == HALT_RGBWW)
            {
                ledc_set_duty(LEDC_LOW_SPEED_MODE, s_red_channel, s_zh_map(s_red_status, 0, 255, 0, s_zh_map(s_brightness_status, 0, 255, 0, 8196)));
                ledc_update_duty(LEDC_LOW_SPEED_MODE, s_red_channel);
                ledc_set_duty(LEDC_LOW_SPEED_MODE, s_green_channel, s_zh_map(s_green_status, 0, 255, 0, s_zh_map(s_brightness_status, 0, 255, 0, 8196)));
                ledc_update_duty(LEDC_LOW_SPEED_MODE, s_green_channel);
                ledc_set_duty(LEDC_LOW_SPEED_MODE, s_blue_channel, s_zh_map(s_blue_status, 0, 255, 0, s_zh_map(s_brightness_status, 0, 255, 0, 8196)));
                ledc_update_duty(LEDC_LOW_SPEED_MODE, s_blue_channel);
            }
        }
    }
    else
    {
        if (s_first_white_pin != ZH_NOT_USED)
        {
            ledc_set_duty(LEDC_LOW_SPEED_MODE, s_first_white_channel, 0);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, s_first_white_channel);
        }
        if (s_second_white_pin != ZH_NOT_USED)
        {
            ledc_set_duty(LEDC_LOW_SPEED_MODE, s_second_white_channel, 0);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, s_second_white_channel);
        }
        if (s_red_pin != ZH_NOT_USED)
        {
            ledc_set_duty(LEDC_LOW_SPEED_MODE, s_red_channel, 0);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, s_red_channel);
        }
        if (s_green_pin != ZH_NOT_USED)
        {
            ledc_set_duty(LEDC_LOW_SPEED_MODE, s_green_channel, 0);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, s_green_channel);
        }
        if (s_blue_pin != ZH_NOT_USED)
        {
            ledc_set_duty(LEDC_LOW_SPEED_MODE, s_blue_channel, 0);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, s_blue_channel);
        }
    }
    s_zh_save_status();
    if (s_gateway_is_available == true)
    {
        s_zh_send_led_status_message();
    }
}

static int32_t s_zh_map(int32_t value, int32_t in_min, int32_t in_max, int32_t out_min, int32_t out_max)
{
    return ((value - in_min) * (out_max - out_min) + ((in_max - in_min) / 2)) / (in_max - in_min) + out_min;
}

static void s_zh_send_led_attributes_message_task(void *pvParameter)
{
    const esp_app_desc_t *app_info = esp_ota_get_app_description();
    zh_attributes_message_t attributes_message = {0};
    attributes_message.chip_type = HACHT_ESP8266;
    strcpy(attributes_message.flash_size, CONFIG_ESPTOOLPY_FLASHSIZE);
    attributes_message.cpu_frequency = CONFIG_ESP8266_DEFAULT_CPU_FREQ_MHZ;
    attributes_message.reset_reason = (uint8_t)esp_reset_reason();
    strcpy(attributes_message.app_name, app_info->project_name);
    strcpy(attributes_message.app_version, app_info->version);
    zh_espnow_data_t data = {0};
    data.device_type = ZHDT_LED;
    data.payload_type = ZHPT_ATTRIBUTES;
    for (;;)
    {
        attributes_message.heap_size = esp_get_free_heap_size();
        attributes_message.min_heap_size = esp_get_minimum_free_heap_size();
        attributes_message.uptime = esp_timer_get_time() / 1000000;
        data.payload_data = (zh_payload_data_t)attributes_message;
        zh_send_message(s_gateway_mac, (uint8_t *)&data, sizeof(zh_espnow_data_t));
        vTaskDelay(60000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

static void s_zh_send_led_config_message(void)
{
    zh_led_config_message_t led_config_message = {0};
    led_config_message.unique_id = 1;
    led_config_message.led_type = s_led_type;
    led_config_message.payload_on = HAONOFT_ON;
    led_config_message.payload_off = HAONOFT_OFF;
    led_config_message.enabled_by_default = true;
    led_config_message.optimistic = false;
    led_config_message.qos = 2;
    led_config_message.retain = true;
    zh_config_message_t config_message = {0};
    config_message = (zh_config_message_t)led_config_message;
    zh_espnow_data_t data = {0};
    data.device_type = ZHDT_LED;
    data.payload_type = ZHPT_CONFIG;
    data.payload_data = (zh_payload_data_t)config_message;
    zh_send_message(s_gateway_mac, (uint8_t *)&data, sizeof(zh_espnow_data_t));
}

static void s_zh_send_led_keep_alive_message_task(void *pvParameter)
{
    zh_keep_alive_message_t keep_alive_message = {0};
    keep_alive_message.online_status = ZH_ONLINE;
    zh_espnow_data_t data = {0};
    data.device_type = ZHDT_LED;
    data.payload_type = ZHPT_KEEP_ALIVE;
    data.payload_data = (zh_payload_data_t)keep_alive_message;
    for (;;)
    {
        zh_send_message(s_gateway_mac, (uint8_t *)&data, sizeof(zh_espnow_data_t));
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

static void s_zh_send_led_status_message(void)
{
    zh_led_status_message_t led_status_message = {0};
    led_status_message.status = (s_led_status == ZH_ON) ? HAONOFT_ON : HAONOFT_OFF;
    led_status_message.brightness = s_brightness_status;
    led_status_message.temperature = s_temperature_status;
    led_status_message.red = s_red_status;
    led_status_message.green = s_green_status;
    led_status_message.blue = s_blue_status;
    zh_status_message_t status_message = {0};
    status_message = (zh_status_message_t)led_status_message;
    zh_espnow_data_t data = {0};
    data.device_type = ZHDT_LED;
    data.payload_type = ZHPT_STATE;
    data.payload_data = (zh_payload_data_t)status_message;
    zh_send_message(s_gateway_mac, (uint8_t *)&data, sizeof(zh_espnow_data_t));
}

#if CONFIG_NETWORK_TYPE_DIRECT
static void s_zh_espnow_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
#endif
#if CONFIG_NETWORK_TYPE_MESH
    static void s_zh_network_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
#endif
{
    const esp_app_desc_t *app_info = esp_ota_get_app_description();
    zh_espnow_data_t data_in = {0};
    zh_espnow_data_t data_out = {0};
    zh_espnow_ota_message_t espnow_ota_message = {0};
    data_out.device_type = ZHDT_LED;
    espnow_ota_message.chip_type = HACHT_ESP8266;
    data_out.payload_data = (zh_payload_data_t)espnow_ota_message;
    switch (event_id)
    {
#if CONFIG_NETWORK_TYPE_DIRECT
    case ZH_ESPNOW_ON_RECV_EVENT:;
        zh_espnow_event_on_recv_t *recv_data = event_data;
        if (recv_data->data_len != sizeof(zh_espnow_data_t))
        {
            goto ZH_ESPNOW_EVENT_HANDLER_EXIT;
        }
#endif
#if CONFIG_NETWORK_TYPE_MESH
    case ZH_NETWORK_ON_RECV_EVENT:;
        zh_network_event_on_recv_t *recv_data = event_data;
        if (recv_data->data_len != sizeof(zh_espnow_data_t))
        {
            goto ZH_NETWORK_EVENT_HANDLER_EXIT;
        }
#endif
        memcpy(&data_in, recv_data->data, recv_data->data_len);
        switch (data_in.device_type)
        {
        case ZHDT_GATEWAY:
            switch (data_in.payload_type)
            {
            case ZHPT_KEEP_ALIVE:
                if (data_in.payload_data.keep_alive_message.online_status == ZH_ONLINE)
                {
                    if (s_gateway_is_available == false)
                    {
                        s_gateway_is_available = true;
                        memcpy(s_gateway_mac, recv_data->mac_addr, 6);
                        if (s_led_type != HALT_NONE)
                        {
                            s_zh_send_led_config_message();
                            s_zh_send_led_status_message();
                            xTaskCreate(&s_zh_send_led_attributes_message_task, "s_zh_send_led_attributes_message_task", ZH_MESSAGE_STACK_SIZE, NULL, ZH_MESSAGE_TASK_PRIORITY, &s_led_attributes_message_task);
                            xTaskCreate(&s_zh_send_led_keep_alive_message_task, "s_zh_send_led_keep_alive_message_task", ZH_MESSAGE_STACK_SIZE, NULL, ZH_MESSAGE_TASK_PRIORITY, &s_led_keep_alive_message_task);
                        }
                    }
                }
                else
                {
                    if (s_gateway_is_available == true)
                    {
                        s_zh_set_gateway_offline_status();
                    }
                }
                break;
            case ZHPT_SET:
                s_led_status = (data_in.payload_data.status_message.led_status_message.status == HAONOFT_ON) ? ZH_ON : ZH_OFF;
                s_zh_gpio_set_level();
                break;
            case ZHPT_BRIGHTNESS:
                s_brightness_status = data_in.payload_data.status_message.led_status_message.brightness;
                s_zh_gpio_set_level();
                break;
            case ZHPT_TEMPERATURE:
                s_temperature_status = data_in.payload_data.status_message.led_status_message.temperature;
                s_zh_gpio_set_level();
                break;
            case ZHPT_RGB:
                s_red_status = data_in.payload_data.status_message.led_status_message.red;
                s_green_status = data_in.payload_data.status_message.led_status_message.green;
                s_blue_status = data_in.payload_data.status_message.led_status_message.blue;
                s_zh_gpio_set_level();
                break;
            case ZHPT_UPDATE:
                s_update_partition = esp_ota_get_next_update_partition(NULL);
                char *app_name = (char *)calloc(1, strlen(app_info->project_name) + 5 + 1);
                sprintf(app_name, "%s.app%d", app_info->project_name, s_update_partition->subtype - ESP_PARTITION_SUBTYPE_APP_OTA_0 + 1);
                strcpy(espnow_ota_message.app_name, app_name);
                free(app_name);
                strcpy(espnow_ota_message.app_version, app_info->version);
                data_out.payload_type = ZHPT_UPDATE;
                data_out.payload_data = (zh_payload_data_t)espnow_ota_message;
                zh_send_message(s_gateway_mac, (uint8_t *)&data_out, sizeof(zh_espnow_data_t));
                break;
            case ZHPT_UPDATE_BEGIN:
                esp_ota_begin(s_update_partition, OTA_SIZE_UNKNOWN, &s_update_handle);
                s_ota_message_part_number = 1;
                data_out.payload_type = ZHPT_UPDATE_PROGRESS;
                zh_send_message(s_gateway_mac, (uint8_t *)&data_out, sizeof(zh_espnow_data_t));
                break;
            case ZHPT_UPDATE_PROGRESS:
                if (s_ota_message_part_number == data_in.payload_data.espnow_ota_message.part)
                {
                    ++s_ota_message_part_number;
                    esp_ota_write(s_update_handle, (const void *)data_in.payload_data.espnow_ota_message.data, data_in.payload_data.espnow_ota_message.data_len);
                }
                data_out.payload_type = ZHPT_UPDATE_PROGRESS;
                zh_send_message(s_gateway_mac, (uint8_t *)&data_out, sizeof(zh_espnow_data_t));
                break;
            case ZHPT_UPDATE_ERROR:
                esp_ota_end(s_update_handle);
                break;
            case ZHPT_UPDATE_END:
                if (esp_ota_end(s_update_handle) != ESP_OK)
                {
                    data_out.payload_type = ZHPT_UPDATE_FAIL;
                    zh_send_message(s_gateway_mac, (uint8_t *)&data_out, sizeof(zh_espnow_data_t));
                    break;
                }
                esp_ota_set_boot_partition(s_update_partition);
                data_out.payload_type = ZHPT_UPDATE_SUCCESS;
                zh_send_message(s_gateway_mac, (uint8_t *)&data_out, sizeof(zh_espnow_data_t));
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                esp_restart();
                break;
            case ZHPT_RESTART:
                esp_restart();
                break;
            default:
                break;
            }
            break;
        default:
            break;
        }
#if CONFIG_NETWORK_TYPE_DIRECT
    ZH_ESPNOW_EVENT_HANDLER_EXIT:
        free(recv_data->data);
        break;
    case ZH_ESPNOW_ON_SEND_EVENT:;
        zh_espnow_event_on_send_t *send_data = event_data;
        if (send_data->status == ZH_ESPNOW_SEND_FAIL && s_gateway_is_available == true)
        {
            s_zh_set_gateway_offline_status();
        }
        break;
#endif
#if CONFIG_NETWORK_TYPE_MESH
    ZH_NETWORK_EVENT_HANDLER_EXIT:
        free(recv_data->data);
        break;
    case ZH_NETWORK_ON_SEND_EVENT:;
        zh_network_event_on_send_t *send_data = event_data;
        if (send_data->status == ZH_NETWORK_SEND_FAIL && s_gateway_is_available == true)
        {
            s_zh_set_gateway_offline_status();
        }
        break;
#endif
    default:
        break;
    }
}

static void s_zh_set_gateway_offline_status(void)
{
    s_gateway_is_available = false;
    if (s_led_type != HALT_NONE)
    {
        vTaskDelete(s_led_attributes_message_task);
        vTaskDelete(s_led_keep_alive_message_task);
    }
}