#include "stdio.h"
#include "string.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_ota_ops.h"
#include "zh_network.h"
#include "zh_config.h"

#define ZH_FIRST_WHITE_CHANNEL LEDC_CHANNEL_0
#define ZH_SECOND_WHITE_CHANNEL LEDC_CHANNEL_1
#define ZH_RED_CHANNEL LEDC_CHANNEL_2
#define ZH_GREEN_CHANNEL LEDC_CHANNEL_3
#define ZH_BLUE_CHANNEL LEDC_CHANNEL_4

#define ZH_MESSAGE_TASK_PRIORITY 2
#define ZH_MESSAGE_STACK_SIZE 2048

static uint8_t s_led_type = HALT_NONE;
static uint8_t s_first_white_pin = NOT_USED;
static uint8_t s_second_white_pin = NOT_USED;
static uint8_t s_red_pin = NOT_USED;
static uint8_t s_green_pin = NOT_USED;
static uint8_t s_blue_pin = NOT_USED;

static uint8_t s_led_status = OFF;
static uint8_t s_brightness_status = 0;
static uint16_t s_temperature_status = 255;
static uint8_t s_red_status = 0;
static uint8_t s_green_status = 0;
static uint8_t s_blue_status = 0;

static uint8_t s_gateway_mac[6] = {0};
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

static void s_zh_network_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void s_zh_set_gateway_offline_status(void);

void app_main(void)
{
#if CONFIG_LED_TYPE_W
    s_led_type = HALT_W;
#elif CONFIG_LED_TYPE_WW
    s_led_type = HALT_WW;
#elif CONFIG_LED_TYPE_RGB
    s_led_type = HALT_RGB;
#elif CONFIG_LED_TYPE_RGBW
    s_led_type = HALT_RGBW;
#elif CONFIG_LED_TYPE_RGBWW
    s_led_type = HALT_RGBWW;
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
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state = {0};
    esp_ota_get_state_partition(running, &ota_state);
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();
    s_zh_load_config();
    s_zh_load_status();
    s_zh_gpio_init();
    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_init_config);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR);
    esp_wifi_start();
    zh_network_init_config_t zh_network_init_config = ZH_NETWORK_INIT_CONFIG_DEFAULT();
    zh_network_init(&zh_network_init_config);
    esp_event_handler_instance_register(ZH_NETWORK, ESP_EVENT_ANY_ID, &s_zh_network_event_handler, NULL, NULL);
    if (ota_state == ESP_OTA_IMG_PENDING_VERIFY)
    {
        vTaskDelay(60000 / portTICK_PERIOD_MS);
        esp_ota_mark_app_valid_cancel_rollback();
    }
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
    nvs_get_u8(nvs_handle, "sec_white_pin", &s_second_white_pin);
    nvs_get_u8(nvs_handle, "red_pin", &s_red_pin);
    nvs_get_u8(nvs_handle, "green_pin", &s_green_pin);
    nvs_get_u8(nvs_handle, "blue_pin", &s_blue_pin);
    nvs_close(nvs_handle);
}

static void s_zh_save_config(void)
{
    nvs_handle_t nvs_handle = 0;
    nvs_open("config", NVS_READWRITE, &nvs_handle);
    nvs_set_u8(nvs_handle, "led_type", s_led_type);
    nvs_set_u8(nvs_handle, "frs_white_pin", s_first_white_pin);
    nvs_set_u8(nvs_handle, "sec_white_pin", s_second_white_pin);
    nvs_set_u8(nvs_handle, "red_pin", s_red_pin);
    nvs_set_u8(nvs_handle, "green_pin", s_green_pin);
    nvs_set_u8(nvs_handle, "blue_pin", s_blue_pin);
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
    ledc_timer_config_t timer_config = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .freq_hz = 1000,
        .clk_cfg = LEDC_AUTO_CLK};
    ledc_timer_config(&timer_config);
    ledc_channel_config_t channel_config = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel = LEDC_TIMER_0,
        .intr_type = LEDC_INTR_DISABLE,
        .duty = 0,
        .hpoint = 0};
    if (s_first_white_pin != NOT_USED)
    {
        channel_config.channel = ZH_FIRST_WHITE_CHANNEL;
        channel_config.gpio_num = s_first_white_pin;
        ledc_channel_config(&channel_config);
    }
    if (s_second_white_pin != NOT_USED)
    {
        channel_config.channel = ZH_SECOND_WHITE_CHANNEL;
        channel_config.gpio_num = s_second_white_pin;
        ledc_channel_config(&channel_config);
    }
    if (s_red_pin != NOT_USED)
    {
        channel_config.channel = ZH_RED_CHANNEL;
        channel_config.gpio_num = s_red_pin;
        ledc_channel_config(&channel_config);
    }
    if (s_green_pin != NOT_USED)
    {
        channel_config.channel = ZH_GREEN_CHANNEL;
        channel_config.gpio_num = s_green_pin;
        ledc_channel_config(&channel_config);
    }
    if (s_blue_pin != NOT_USED)
    {
        channel_config.channel = ZH_BLUE_CHANNEL;
        channel_config.gpio_num = s_blue_pin;
        ledc_channel_config(&channel_config);
    }
    s_zh_gpio_set_level();
}

static void s_zh_gpio_set_level(void)
{
    if (s_led_status == ON)
    {
        if (s_red_status == 255 && s_green_status == 255 && s_blue_status == 255)
        {
            if (s_led_type == HALT_W || s_led_type == HALT_RGBW)
            {
                ledc_set_duty(LEDC_LOW_SPEED_MODE, ZH_FIRST_WHITE_CHANNEL, s_brightness_status);
                ledc_update_duty(LEDC_LOW_SPEED_MODE, ZH_FIRST_WHITE_CHANNEL);
            }
            if (s_led_type == HALT_WW || s_led_type == HALT_RGBWW)
            {
                ledc_set_duty(LEDC_LOW_SPEED_MODE, ZH_FIRST_WHITE_CHANNEL, s_zh_map(s_brightness_status, 0, 255, 0, s_zh_map(s_temperature_status, 500, 153, 0, 255)));
                ledc_update_duty(LEDC_LOW_SPEED_MODE, ZH_FIRST_WHITE_CHANNEL);
                ledc_set_duty(LEDC_LOW_SPEED_MODE, ZH_SECOND_WHITE_CHANNEL, s_zh_map(s_brightness_status, 0, 255, 0, s_zh_map(s_temperature_status, 153, 500, 0, 255)));
                ledc_update_duty(LEDC_LOW_SPEED_MODE, ZH_SECOND_WHITE_CHANNEL);
            }
            if (s_led_type == HALT_RGB)
            {
                ledc_set_duty(LEDC_LOW_SPEED_MODE, ZH_RED_CHANNEL, s_zh_map(s_red_status, 0, 255, 0, s_brightness_status));
                ledc_update_duty(LEDC_LOW_SPEED_MODE, ZH_RED_CHANNEL);
                ledc_set_duty(LEDC_LOW_SPEED_MODE, ZH_GREEN_CHANNEL, s_zh_map(s_green_status, 0, 255, 0, s_brightness_status));
                ledc_update_duty(LEDC_LOW_SPEED_MODE, ZH_GREEN_CHANNEL);
                ledc_set_duty(LEDC_LOW_SPEED_MODE, ZH_BLUE_CHANNEL, s_zh_map(s_blue_status, 0, 255, 0, s_brightness_status));
                ledc_update_duty(LEDC_LOW_SPEED_MODE, ZH_BLUE_CHANNEL);
            }
            if (s_led_type == HALT_RGBW || s_led_type == HALT_RGBWW)
            {
                ledc_set_duty(LEDC_LOW_SPEED_MODE, ZH_RED_CHANNEL, 0);
                ledc_update_duty(LEDC_LOW_SPEED_MODE, ZH_RED_CHANNEL);
                ledc_set_duty(LEDC_LOW_SPEED_MODE, ZH_GREEN_CHANNEL, 0);
                ledc_update_duty(LEDC_LOW_SPEED_MODE, ZH_GREEN_CHANNEL);
                ledc_set_duty(LEDC_LOW_SPEED_MODE, ZH_BLUE_CHANNEL, 0);
                ledc_update_duty(LEDC_LOW_SPEED_MODE, ZH_BLUE_CHANNEL);
            }
        }
        else
        {
            if (s_led_type == HALT_W)
            {
                ledc_set_duty(LEDC_LOW_SPEED_MODE, ZH_FIRST_WHITE_CHANNEL, s_brightness_status);
                ledc_update_duty(LEDC_LOW_SPEED_MODE, ZH_FIRST_WHITE_CHANNEL);
            }
            if (s_led_type == HALT_WW)
            {
                ledc_set_duty(LEDC_LOW_SPEED_MODE, ZH_FIRST_WHITE_CHANNEL, s_zh_map(s_brightness_status, 0, 255, 0, s_zh_map(s_temperature_status, 500, 153, 0, 255)));
                ledc_update_duty(LEDC_LOW_SPEED_MODE, ZH_FIRST_WHITE_CHANNEL);
                ledc_set_duty(LEDC_LOW_SPEED_MODE, ZH_SECOND_WHITE_CHANNEL, s_zh_map(s_brightness_status, 0, 255, 0, s_zh_map(s_temperature_status, 153, 500, 0, 255)));
                ledc_update_duty(LEDC_LOW_SPEED_MODE, ZH_SECOND_WHITE_CHANNEL);
            }
            if (s_led_type == HALT_RGBW || s_led_type == HALT_RGBWW)
            {
                ledc_set_duty(LEDC_LOW_SPEED_MODE, ZH_FIRST_WHITE_CHANNEL, 0);
                ledc_update_duty(LEDC_LOW_SPEED_MODE, ZH_FIRST_WHITE_CHANNEL);
            }
            if (s_led_type == HALT_RGBWW)
            {
                ledc_set_duty(LEDC_LOW_SPEED_MODE, ZH_SECOND_WHITE_CHANNEL, 0);
                ledc_update_duty(LEDC_LOW_SPEED_MODE, ZH_SECOND_WHITE_CHANNEL);
            }
            if (s_led_type == HALT_RGB || s_led_type == HALT_RGBW || s_led_type == HALT_RGBWW)
            {
                ledc_set_duty(LEDC_LOW_SPEED_MODE, ZH_RED_CHANNEL, s_zh_map(s_red_status, 0, 255, 0, s_brightness_status));
                ledc_update_duty(LEDC_LOW_SPEED_MODE, ZH_RED_CHANNEL);
                ledc_set_duty(LEDC_LOW_SPEED_MODE, ZH_GREEN_CHANNEL, s_zh_map(s_green_status, 0, 255, 0, s_brightness_status));
                ledc_update_duty(LEDC_LOW_SPEED_MODE, ZH_GREEN_CHANNEL);
                ledc_set_duty(LEDC_LOW_SPEED_MODE, ZH_BLUE_CHANNEL, s_zh_map(s_blue_status, 0, 255, 0, s_brightness_status));
                ledc_update_duty(LEDC_LOW_SPEED_MODE, ZH_BLUE_CHANNEL);
            }
        }
    }
    else
    {
        if (s_first_white_pin != NOT_USED)
        {
            ledc_set_duty(LEDC_LOW_SPEED_MODE, ZH_FIRST_WHITE_CHANNEL, 0);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, ZH_FIRST_WHITE_CHANNEL);
        }
        if (s_second_white_pin != NOT_USED)
        {
            ledc_set_duty(LEDC_LOW_SPEED_MODE, ZH_SECOND_WHITE_CHANNEL, 0);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, ZH_SECOND_WHITE_CHANNEL);
        }
        if (s_red_pin != NOT_USED)
        {
            ledc_set_duty(LEDC_LOW_SPEED_MODE, ZH_RED_CHANNEL, 0);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, ZH_RED_CHANNEL);
        }
        if (s_green_pin != NOT_USED)
        {
            ledc_set_duty(LEDC_LOW_SPEED_MODE, ZH_GREEN_CHANNEL, 0);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, ZH_GREEN_CHANNEL);
        }
        if (s_blue_pin != NOT_USED)
        {
            ledc_set_duty(LEDC_LOW_SPEED_MODE, ZH_BLUE_CHANNEL, 0);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, ZH_BLUE_CHANNEL);
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
    const esp_app_desc_t *app_info = esp_app_get_description();
    zh_attributes_message_t attributes_message = {0};
    attributes_message.chip_type = HACHT_ESP32;
    strcpy(attributes_message.flash_size, CONFIG_ESPTOOLPY_FLASHSIZE);
    attributes_message.cpu_frequency = CONFIG_ESP32_DEFAULT_CPU_FREQ_MHZ;
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
        zh_network_send(s_gateway_mac, (uint8_t *)&data, sizeof(zh_espnow_data_t));
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
    zh_network_send(s_gateway_mac, (uint8_t *)&data, sizeof(zh_espnow_data_t));
}

static void s_zh_send_led_keep_alive_message_task(void *pvParameter)
{
    zh_keep_alive_message_t keep_alive_message = {0};
    keep_alive_message.online_status = ONLINE;
    zh_espnow_data_t data = {0};
    data.device_type = ZHDT_LED;
    data.payload_type = ZHPT_KEEP_ALIVE;
    data.payload_data = (zh_payload_data_t)keep_alive_message;
    for (;;)
    {
        zh_network_send(s_gateway_mac, (uint8_t *)&data, sizeof(zh_espnow_data_t));
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

static void s_zh_send_led_status_message(void)
{
    zh_led_status_message_t led_status_message = {0};
    led_status_message.status = (s_led_status == ON) ? HAONOFT_ON : HAONOFT_OFF;
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
    zh_network_send(s_gateway_mac, (uint8_t *)&data, sizeof(zh_espnow_data_t));
}

static void s_zh_network_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    const esp_app_desc_t *app_info = esp_app_get_description();
    zh_espnow_data_t data_in = {0};
    zh_espnow_data_t data_out = {0};
    zh_espnow_ota_message_t espnow_ota_message = {0};
    data_out.device_type = ZHDT_LED;
    espnow_ota_message.chip_type = HACHT_ESP32;
    data_out.payload_data = (zh_payload_data_t)espnow_ota_message;
    switch (event_id)
    {
    case ZH_NETWORK_ON_RECV_EVENT:;
        zh_network_event_on_recv_t *recv_data = event_data;
        if (recv_data->data_len != sizeof(zh_espnow_data_t))
        {
            goto ZH_NETWORK_EVENT_HANDLER_EXIT;
        }
        memcpy(&data_in, recv_data->data, recv_data->data_len);
        switch (data_in.device_type)
        {
        case ZHDT_GATEWAY:
            switch (data_in.payload_type)
            {
            case ZHPT_KEEP_ALIVE:
                if (data_in.payload_data.keep_alive_message.online_status == ONLINE)
                {
                    if (s_gateway_is_available == false)
                    {
                        s_gateway_is_available = true;
                        memcpy(s_gateway_mac, recv_data->mac_addr, 6);
                        if (s_led_type != HALT_NONE)
                        {
                            s_zh_send_led_config_message();
                            s_zh_send_led_status_message();
                            xTaskCreatePinnedToCore(&s_zh_send_led_attributes_message_task, "s_zh_send_led_attributes_message_task", ZH_MESSAGE_STACK_SIZE, NULL, ZH_MESSAGE_TASK_PRIORITY, &s_led_attributes_message_task, tskNO_AFFINITY);
                            xTaskCreatePinnedToCore(&s_zh_send_led_keep_alive_message_task, "s_zh_send_led_keep_alive_message_task", ZH_MESSAGE_STACK_SIZE, NULL, ZH_MESSAGE_TASK_PRIORITY, &s_led_keep_alive_message_task, tskNO_AFFINITY);
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
                s_led_status = (data_in.payload_data.status_message.led_status_message.status == HAONOFT_ON) ? ON : OFF;
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
                strcpy(espnow_ota_message.app_name, app_info->project_name);
                strcpy(espnow_ota_message.app_version, app_info->version);
                data_out.payload_type = ZHPT_UPDATE;
                data_out.payload_data = (zh_payload_data_t)espnow_ota_message;
                zh_network_send(s_gateway_mac, (uint8_t *)&data_out, sizeof(zh_espnow_data_t));
                break;
            case ZHPT_UPDATE_BEGIN:
                esp_ota_begin(s_update_partition, OTA_SIZE_UNKNOWN, &s_update_handle);
                s_ota_message_part_number = 1;
                data_out.payload_type = ZHPT_UPDATE_PROGRESS;
                zh_network_send(s_gateway_mac, (uint8_t *)&data_out, sizeof(zh_espnow_data_t));
                break;
            case ZHPT_UPDATE_PROGRESS:
                if (s_ota_message_part_number == data_in.payload_data.espnow_ota_message.part)
                {
                    ++s_ota_message_part_number;
                    esp_ota_write(s_update_handle, (const void *)data_in.payload_data.espnow_ota_message.data, data_in.payload_data.espnow_ota_message.data_len);
                }
                data_out.payload_type = ZHPT_UPDATE_PROGRESS;
                zh_network_send(s_gateway_mac, (uint8_t *)&data_out, sizeof(zh_espnow_data_t));
                break;
            case ZHPT_UPDATE_ERROR:
                esp_ota_end(s_update_handle);
                break;
            case ZHPT_UPDATE_END:
                if (esp_ota_end(s_update_handle) != ESP_OK)
                {
                    data_out.payload_type = ZHPT_UPDATE_FAIL;
                    zh_network_send(s_gateway_mac, (uint8_t *)&data_out, sizeof(zh_espnow_data_t));
                    break;
                }
                esp_ota_set_boot_partition(s_update_partition);
                data_out.payload_type = ZHPT_UPDATE_SUCCESS;
                zh_network_send(s_gateway_mac, (uint8_t *)&data_out, sizeof(zh_espnow_data_t));
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
    ZH_NETWORK_EVENT_HANDLER_EXIT:
        free(recv_data->data);
        break;
    case ZH_NETWORK_ON_SEND_EVENT:
        zh_network_event_on_send_t *send_data = event_data;
        if (send_data->status == ZH_NETWORK_SEND_FAIL && s_gateway_is_available == true)
        {
            s_zh_set_gateway_offline_status();
        }
        break;
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