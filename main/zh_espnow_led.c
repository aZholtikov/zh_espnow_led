#include "zh_espnow_led.h"

led_config_t led_main_config = {0};

void app_main(void)
{
    led_config_t *led_config = &led_main_config;
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();
    zh_load_config(led_config);
    zh_load_status(led_config);
    zh_gpio_init(led_config);
    zh_gpio_set_level(led_config);
    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_init_config);
    esp_wifi_set_mode(WIFI_MODE_STA);
#ifdef CONFIG_IDF_TARGET_ESP8266
    esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
#else
    esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR);
#endif
    esp_wifi_start();
#ifdef CONFIG_NETWORK_TYPE_DIRECT
    zh_espnow_init_config_t espnow_init_config = ZH_ESPNOW_INIT_CONFIG_DEFAULT();
    zh_espnow_init(&espnow_init_config);
#else
    zh_network_init_config_t network_init_config = ZH_NETWORK_INIT_CONFIG_DEFAULT();
    zh_network_init(&network_init_config);
#endif
#ifdef CONFIG_IDF_TARGET_ESP8266
    esp_event_handler_register(ZH_EVENT, ESP_EVENT_ANY_ID, &zh_espnow_event_handler, led_config);
#else
    esp_event_handler_instance_register(ZH_EVENT, ESP_EVENT_ANY_ID, &zh_espnow_event_handler, led_config, NULL);
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state = {0};
    esp_ota_get_state_partition(running, &ota_state);
    if (ota_state == ESP_OTA_IMG_PENDING_VERIFY)
    {
        vTaskDelay(60000 / portTICK_PERIOD_MS);
        esp_ota_mark_app_valid_cancel_rollback();
    }
#endif
}

void zh_load_config(led_config_t *led_config)
{
    nvs_handle_t nvs_handle = 0;
    nvs_open("config", NVS_READWRITE, &nvs_handle);
    uint8_t config_is_present = 0;
    if (nvs_get_u8(nvs_handle, "present", &config_is_present) == ESP_ERR_NVS_NOT_FOUND)
    {
        nvs_set_u8(nvs_handle, "present", 0xFE);
        nvs_close(nvs_handle);
    SETUP_INITIAL_SETTINGS:
#ifdef CONFIG_LED_TYPE_W
        led_config->hardware_config.led_type = HALT_W;
#elif CONFIG_LED_TYPE_WW
        led_config->hardware_config.led_type = HALT_WW;
#elif CONFIG_LED_TYPE_RGB
        led_config->hardware_config.led_type = HALT_RGB;
#elif CONFIG_LED_TYPE_RGBW
        led_config->hardware_config.led_type = HALT_RGBW;
#elif CONFIG_LED_TYPE_RGBWW
        led_config->hardware_config.led_type = HALT_RGBWW;
#else
        led_config->hardware_config.led_type = HALT_NONE;
#endif
#ifdef CONFIG_FIRST_WHITE_PIN
        led_config->hardware_config.first_white_pin = CONFIG_FIRST_WHITE_PIN;
#else
        led_config->hardware_config.first_white_pin = ZH_NOT_USED;
#endif
#ifdef CONFIG_SECOND_WHITE_PIN
        led_config->hardware_config.second_white_pin = CONFIG_SECOND_WHITE_PIN;
#else
        led_config->hardware_config.second_white_pin = ZH_NOT_USED;
#endif
#ifdef CONFIG_RED_PIN
        led_config->hardware_config.red_pin = CONFIG_RED_PIN;
#else
        led_config->hardware_config.red_pin = ZH_NOT_USED;
#endif
#ifdef CONFIG_GREEN_PIN
        led_config->hardware_config.green_pin = CONFIG_GREEN_PIN;
#else
        led_config->hardware_config.green_pin = ZH_NOT_USED;
#endif
#ifdef CONFIG_BLUE_PIN
        led_config->hardware_config.blue_pin = CONFIG_BLUE_PIN;
#else
        led_config->hardware_config.blue_pin = ZH_NOT_USED;
#endif
        zh_save_config(led_config);
        return;
    }
    esp_err_t err = ESP_OK;
    err += nvs_get_u8(nvs_handle, "led_type", (uint8_t *)&led_config->hardware_config.led_type);
    err += nvs_get_u8(nvs_handle, "frs_white_pin", &led_config->hardware_config.first_white_pin);
    err += nvs_get_u8(nvs_handle, "sec_white_pin", &led_config->hardware_config.second_white_pin);
    err += nvs_get_u8(nvs_handle, "red_pin", &led_config->hardware_config.red_pin);
    err += nvs_get_u8(nvs_handle, "green_pin", &led_config->hardware_config.green_pin);
    err += nvs_get_u8(nvs_handle, "blue_pin", &led_config->hardware_config.blue_pin);
    nvs_close(nvs_handle);
    if (err != ESP_OK)
    {
        goto SETUP_INITIAL_SETTINGS;
    }
}

void zh_save_config(const led_config_t *led_config)
{
    nvs_handle_t nvs_handle = 0;
    nvs_open("config", NVS_READWRITE, &nvs_handle);
    nvs_set_u8(nvs_handle, "led_type", led_config->hardware_config.led_type);
    nvs_set_u8(nvs_handle, "frs_white_pin", led_config->hardware_config.first_white_pin);
    nvs_set_u8(nvs_handle, "sec_white_pin", led_config->hardware_config.second_white_pin);
    nvs_set_u8(nvs_handle, "red_pin", led_config->hardware_config.red_pin);
    nvs_set_u8(nvs_handle, "green_pin", led_config->hardware_config.green_pin);
    nvs_set_u8(nvs_handle, "blue_pin", led_config->hardware_config.blue_pin);
    nvs_close(nvs_handle);
}

void zh_load_status(led_config_t *led_config)
{
    led_config->status.status = HAONOFT_OFF;
    led_config->status.temperature = 255;
    nvs_handle_t nvs_handle = 0;
    nvs_open("status", NVS_READWRITE, &nvs_handle);
    uint8_t status_is_present = 0;
    if (nvs_get_u8(nvs_handle, "present", &status_is_present) == ESP_ERR_NVS_NOT_FOUND)
    {
        nvs_set_u8(nvs_handle, "present", 0xFE);
        nvs_close(nvs_handle);
        zh_save_status(led_config);
        return;
    }
    nvs_get_u8(nvs_handle, "led_state", (uint8_t *)&led_config->status.status);
    nvs_get_u8(nvs_handle, "bright_state", &led_config->status.brightness);
    nvs_get_u16(nvs_handle, "temp_state", &led_config->status.temperature);
    nvs_get_u8(nvs_handle, "red_state", &led_config->status.red);
    nvs_get_u8(nvs_handle, "green_state", &led_config->status.green);
    nvs_get_u8(nvs_handle, "blue_state", &led_config->status.blue);
    nvs_get_u8(nvs_handle, "effect", (uint8_t *)&led_config->status.effect);
    nvs_close(nvs_handle);
}

void zh_save_status(const led_config_t *led_config)
{
    nvs_handle_t nvs_handle = 0;
    nvs_open("status", NVS_READWRITE, &nvs_handle);
    nvs_set_u8(nvs_handle, "led_state", led_config->status.status);
    nvs_set_u8(nvs_handle, "bright_state", led_config->status.brightness);
    nvs_set_u16(nvs_handle, "temp_state", led_config->status.temperature);
    nvs_set_u8(nvs_handle, "red_state", led_config->status.red);
    nvs_set_u8(nvs_handle, "green_state", led_config->status.green);
    nvs_set_u8(nvs_handle, "blue_state", led_config->status.blue);
    nvs_set_u8(nvs_handle, "effect", led_config->status.effect);
    nvs_close(nvs_handle);
}

void zh_gpio_init(led_config_t *led_config)
{
    uint8_t channel = 0;
#ifdef CONFIG_IDF_TARGET_ESP8266
    ledc_timer_config_t timer_config = {0};
    timer_config.freq_hz = 1000;
    ledc_timer_config(&timer_config);
    ledc_channel_config_t channel_config = {0};
#else
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
#endif
    if (led_config->hardware_config.led_type != HALT_NONE)
    {
        if (led_config->hardware_config.first_white_pin != ZH_NOT_USED)
        {
            led_config->channel.first_white = channel++;
            channel_config.channel = led_config->channel.first_white;
            channel_config.gpio_num = led_config->hardware_config.first_white_pin;
            if (ledc_channel_config(&channel_config) != ESP_OK)
            {
                led_config->hardware_config.first_white_pin = ZH_NOT_USED;
                --channel;
            }
        }
        if (led_config->hardware_config.second_white_pin != ZH_NOT_USED)
        {
            led_config->channel.second_white = channel++;
            channel_config.channel = led_config->channel.second_white;
            channel_config.gpio_num = led_config->hardware_config.second_white_pin;
            if (ledc_channel_config(&channel_config) != ESP_OK)
            {
                led_config->hardware_config.second_white_pin = ZH_NOT_USED;
                --channel;
            }
        }
        if (led_config->hardware_config.red_pin != ZH_NOT_USED)
        {
            led_config->channel.red = channel++;
            channel_config.channel = led_config->channel.red;
            channel_config.gpio_num = led_config->hardware_config.red_pin;
            if (ledc_channel_config(&channel_config) != ESP_OK)
            {
                led_config->hardware_config.red_pin = ZH_NOT_USED;
                --channel;
            }
        }
        if (led_config->hardware_config.green_pin != ZH_NOT_USED)
        {
            led_config->channel.green = channel++;
            channel_config.channel = led_config->channel.green;
            channel_config.gpio_num = led_config->hardware_config.green_pin;
            if (ledc_channel_config(&channel_config) != ESP_OK)
            {
                led_config->hardware_config.green_pin = ZH_NOT_USED;
                --channel;
            }
        }
        if (led_config->hardware_config.blue_pin != ZH_NOT_USED)
        {
            led_config->channel.blue = channel++;
            channel_config.channel = led_config->channel.blue;
            channel_config.gpio_num = led_config->hardware_config.blue_pin;
            if (ledc_channel_config(&channel_config) != ESP_OK)
            {
                led_config->hardware_config.blue_pin = ZH_NOT_USED;
            }
        }
#ifdef CONFIG_IDF_TARGET_ESP8266
        ledc_fade_func_install(0);
#endif
    }
}

void zh_gpio_set_level(const led_config_t *led_config)
{
    if (led_config->hardware_config.led_type != HALT_NONE)
    {
        if (led_config->status.status == HAONOFT_ON)
        {
            if (led_config->status.red == 255 && led_config->status.green == 255 && led_config->status.blue == 255)
            {
                if (led_config->hardware_config.led_type == HALT_W || led_config->hardware_config.led_type == HALT_RGBW)
                {
                    if (led_config->hardware_config.first_white_pin != ZH_NOT_USED)
                    {
#ifdef CONFIG_IDF_TARGET_ESP8266
                        ledc_set_duty(LEDC_LOW_SPEED_MODE, led_config->channel.first_white, zh_map(led_config->status.brightness, 0, 255, 0, 8196));
#else
                        ledc_set_duty(LEDC_LOW_SPEED_MODE, led_config->channel.first_white, led_config->status.brightness);
#endif
                        ledc_update_duty(LEDC_LOW_SPEED_MODE, led_config->channel.first_white);
                    }
                }
                if (led_config->hardware_config.led_type == HALT_WW || led_config->hardware_config.led_type == HALT_RGBWW)
                {
                    if (led_config->hardware_config.first_white_pin != ZH_NOT_USED && led_config->hardware_config.second_white_pin != ZH_NOT_USED)
                    {
#ifdef CONFIG_IDF_TARGET_ESP8266
                        ledc_set_duty(LEDC_LOW_SPEED_MODE, led_config->channel.first_white, zh_map(zh_map(led_config->status.brightness, 0, 255, 0, zh_map(led_config->status.temperature, 500, 153, 0, 255)), 0, 255, 0, 8196));
                        ledc_set_duty(LEDC_LOW_SPEED_MODE, led_config->channel.second_white, zh_map(zh_map(led_config->status.brightness, 0, 255, 0, zh_map(led_config->status.temperature, 153, 500, 0, 255)), 0, 255, 0, 8196));
#else
                        ledc_set_duty(LEDC_LOW_SPEED_MODE, led_config->channel.first_white, zh_map(led_config->status.brightness, 0, 255, 0, zh_map(led_config->status.temperature, 500, 153, 0, 255)));
                        ledc_set_duty(LEDC_LOW_SPEED_MODE, led_config->channel.second_white, zh_map(led_config->status.brightness, 0, 255, 0, zh_map(led_config->status.temperature, 153, 500, 0, 255)));
#endif
                        ledc_update_duty(LEDC_LOW_SPEED_MODE, led_config->channel.first_white);
                        ledc_update_duty(LEDC_LOW_SPEED_MODE, led_config->channel.second_white);
                    }
                }
                if (led_config->hardware_config.led_type == HALT_RGB)
                {
                    if (led_config->hardware_config.red_pin != ZH_NOT_USED && led_config->hardware_config.green_pin != ZH_NOT_USED && led_config->hardware_config.blue_pin != ZH_NOT_USED)
                    {
#ifdef CONFIG_IDF_TARGET_ESP8266
                        ledc_set_duty(LEDC_LOW_SPEED_MODE, led_config->channel.red, zh_map(led_config->status.red, 0, 255, 0, zh_map(led_config->status.brightness, 0, 255, 0, 8196)));
                        ledc_set_duty(LEDC_LOW_SPEED_MODE, led_config->channel.green, zh_map(led_config->status.green, 0, 255, 0, zh_map(led_config->status.brightness, 0, 255, 0, 8196)));
                        ledc_set_duty(LEDC_LOW_SPEED_MODE, led_config->channel.blue, zh_map(led_config->status.blue, 0, 255, 0, zh_map(led_config->status.brightness, 0, 255, 0, 8196)));
#else
                        ledc_set_duty(LEDC_LOW_SPEED_MODE, led_config->channel.red, zh_map(led_config->status.red, 0, 255, 0, led_config->status.brightness));
                        ledc_set_duty(LEDC_LOW_SPEED_MODE, led_config->channel.green, zh_map(led_config->status.green, 0, 255, 0, led_config->status.brightness));
                        ledc_set_duty(LEDC_LOW_SPEED_MODE, led_config->channel.blue, zh_map(led_config->status.blue, 0, 255, 0, led_config->status.brightness));
#endif
                        ledc_update_duty(LEDC_LOW_SPEED_MODE, led_config->channel.red);
                        ledc_update_duty(LEDC_LOW_SPEED_MODE, led_config->channel.green);
                        ledc_update_duty(LEDC_LOW_SPEED_MODE, led_config->channel.blue);
                    }
                }
                if (led_config->hardware_config.led_type == HALT_RGBW || led_config->hardware_config.led_type == HALT_RGBWW)
                {
                    if (led_config->hardware_config.red_pin != ZH_NOT_USED && led_config->hardware_config.green_pin != ZH_NOT_USED && led_config->hardware_config.blue_pin != ZH_NOT_USED)
                    {
                        ledc_set_duty(LEDC_LOW_SPEED_MODE, led_config->channel.red, 0);
                        ledc_set_duty(LEDC_LOW_SPEED_MODE, led_config->channel.green, 0);
                        ledc_set_duty(LEDC_LOW_SPEED_MODE, led_config->channel.blue, 0);
                        ledc_update_duty(LEDC_LOW_SPEED_MODE, led_config->channel.red);
                        ledc_update_duty(LEDC_LOW_SPEED_MODE, led_config->channel.green);
                        ledc_update_duty(LEDC_LOW_SPEED_MODE, led_config->channel.blue);
                    }
                }
            }
            else
            {
                if (led_config->hardware_config.led_type == HALT_W)
                {
                    if (led_config->hardware_config.first_white_pin != ZH_NOT_USED)
                    {
#ifdef CONFIG_IDF_TARGET_ESP8266
                        ledc_set_duty(LEDC_LOW_SPEED_MODE, led_config->channel.first_white, zh_map(led_config->status.brightness, 0, 255, 0, 8196));
#else
                        ledc_set_duty(LEDC_LOW_SPEED_MODE, led_config->channel.first_white, led_config->status.brightness);
#endif
                        ledc_update_duty(LEDC_LOW_SPEED_MODE, led_config->channel.first_white);
                    }
                }
                if (led_config->hardware_config.led_type == HALT_WW)
                {
                    if (led_config->hardware_config.first_white_pin != ZH_NOT_USED && led_config->hardware_config.second_white_pin != ZH_NOT_USED)
                    {
#ifdef CONFIG_IDF_TARGET_ESP8266
                        ledc_set_duty(LEDC_LOW_SPEED_MODE, led_config->channel.first_white, zh_map(zh_map(led_config->status.brightness, 0, 255, 0, zh_map(led_config->status.temperature, 500, 153, 0, 255)), 0, 255, 0, 8196));
                        ledc_set_duty(LEDC_LOW_SPEED_MODE, led_config->channel.second_white, zh_map(zh_map(led_config->status.brightness, 0, 255, 0, zh_map(led_config->status.temperature, 153, 500, 0, 255)), 0, 255, 0, 8196));
#else
                        ledc_set_duty(LEDC_LOW_SPEED_MODE, led_config->channel.first_white, zh_map(led_config->status.brightness, 0, 255, 0, zh_map(led_config->status.temperature, 500, 153, 0, 255)));
                        ledc_set_duty(LEDC_LOW_SPEED_MODE, led_config->channel.second_white, zh_map(led_config->status.brightness, 0, 255, 0, zh_map(led_config->status.temperature, 153, 500, 0, 255)));
#endif
                        ledc_update_duty(LEDC_LOW_SPEED_MODE, led_config->channel.first_white);
                        ledc_update_duty(LEDC_LOW_SPEED_MODE, led_config->channel.second_white);
                    }
                }
                if (led_config->hardware_config.led_type == HALT_RGBW || led_config->hardware_config.led_type == HALT_RGBWW)
                {
                    if (led_config->hardware_config.first_white_pin != ZH_NOT_USED)
                    {
                        ledc_set_duty(LEDC_LOW_SPEED_MODE, led_config->channel.first_white, 0);
                        ledc_update_duty(LEDC_LOW_SPEED_MODE, led_config->channel.first_white);
                    }
                }
                if (led_config->hardware_config.led_type == HALT_RGBWW)
                {
                    if (led_config->hardware_config.second_white_pin != ZH_NOT_USED)
                    {
                        ledc_set_duty(LEDC_LOW_SPEED_MODE, led_config->channel.second_white, 0);
                        ledc_update_duty(LEDC_LOW_SPEED_MODE, led_config->channel.second_white);
                    }
                }
                if (led_config->hardware_config.led_type == HALT_RGB || led_config->hardware_config.led_type == HALT_RGBW || led_config->hardware_config.led_type == HALT_RGBWW)
                {
                    if (led_config->hardware_config.red_pin != ZH_NOT_USED && led_config->hardware_config.green_pin != ZH_NOT_USED && led_config->hardware_config.blue_pin != ZH_NOT_USED)
                    {
#ifdef CONFIG_IDF_TARGET_ESP8266
                        ledc_set_duty(LEDC_LOW_SPEED_MODE, led_config->channel.red, zh_map(led_config->status.red, 0, 255, 0, zh_map(led_config->status.brightness, 0, 255, 0, 8196)));
                        ledc_set_duty(LEDC_LOW_SPEED_MODE, led_config->channel.green, zh_map(led_config->status.green, 0, 255, 0, zh_map(led_config->status.brightness, 0, 255, 0, 8196)));
                        ledc_set_duty(LEDC_LOW_SPEED_MODE, led_config->channel.blue, zh_map(led_config->status.blue, 0, 255, 0, zh_map(led_config->status.brightness, 0, 255, 0, 8196)));
#else
                        ledc_set_duty(LEDC_LOW_SPEED_MODE, led_config->channel.red, zh_map(led_config->status.red, 0, 255, 0, led_config->status.brightness));
                        ledc_set_duty(LEDC_LOW_SPEED_MODE, led_config->channel.green, zh_map(led_config->status.green, 0, 255, 0, led_config->status.brightness));
                        ledc_set_duty(LEDC_LOW_SPEED_MODE, led_config->channel.blue, zh_map(led_config->status.blue, 0, 255, 0, led_config->status.brightness));
#endif
                        ledc_update_duty(LEDC_LOW_SPEED_MODE, led_config->channel.red);
                        ledc_update_duty(LEDC_LOW_SPEED_MODE, led_config->channel.green);
                        ledc_update_duty(LEDC_LOW_SPEED_MODE, led_config->channel.blue);
                    }
                }
            }
        }
        else
        {
            if (led_config->hardware_config.first_white_pin != ZH_NOT_USED)
            {
                ledc_set_duty(LEDC_LOW_SPEED_MODE, led_config->channel.first_white, 0);
                ledc_update_duty(LEDC_LOW_SPEED_MODE, led_config->channel.first_white);
            }
            if (led_config->hardware_config.second_white_pin != ZH_NOT_USED)
            {
                ledc_set_duty(LEDC_LOW_SPEED_MODE, led_config->channel.second_white, 0);
                ledc_update_duty(LEDC_LOW_SPEED_MODE, led_config->channel.second_white);
            }
            if (led_config->hardware_config.red_pin != ZH_NOT_USED)
            {
                ledc_set_duty(LEDC_LOW_SPEED_MODE, led_config->channel.red, 0);
                ledc_update_duty(LEDC_LOW_SPEED_MODE, led_config->channel.red);
            }
            if (led_config->hardware_config.green_pin != ZH_NOT_USED)
            {
                ledc_set_duty(LEDC_LOW_SPEED_MODE, led_config->channel.green, 0);
                ledc_update_duty(LEDC_LOW_SPEED_MODE, led_config->channel.green);
            }
            if (led_config->hardware_config.blue_pin != ZH_NOT_USED)
            {
                ledc_set_duty(LEDC_LOW_SPEED_MODE, led_config->channel.blue, 0);
                ledc_update_duty(LEDC_LOW_SPEED_MODE, led_config->channel.blue);
            }
        }
    }
}

int32_t zh_map(int32_t value, int32_t in_min, int32_t in_max, int32_t out_min, int32_t out_max)
{
    return ((value - in_min) * (out_max - out_min) + ((in_max - in_min) / 2)) / (in_max - in_min) + out_min;
}

void zh_send_led_attributes_message_task(void *pvParameter)
{
    led_config_t *led_config = pvParameter;
    const esp_app_desc_t *app_info = get_app_description();
    zh_espnow_data_t data = {0};
    data.device_type = ZHDT_LED;
    data.payload_type = ZHPT_ATTRIBUTES;
    data.payload_data.attributes_message.chip_type = ZH_CHIP_TYPE;
    strcpy(data.payload_data.attributes_message.flash_size, CONFIG_ESPTOOLPY_FLASHSIZE);
    data.payload_data.attributes_message.cpu_frequency = ZH_CPU_FREQUENCY;
    data.payload_data.attributes_message.reset_reason = (uint8_t)esp_reset_reason();
    strcpy(data.payload_data.attributes_message.app_name, app_info->project_name);
    strcpy(data.payload_data.attributes_message.app_version, app_info->version);
    for (;;)
    {
        data.payload_data.attributes_message.heap_size = esp_get_free_heap_size();
        data.payload_data.attributes_message.min_heap_size = esp_get_minimum_free_heap_size();
        data.payload_data.attributes_message.uptime = esp_timer_get_time() / 1000000;
        zh_send_message(led_config->gateway_mac, (uint8_t *)&data, sizeof(zh_espnow_data_t));
        vTaskDelay(ZH_LED_ATTRIBUTES_MESSAGE_FREQUENCY * 1000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

void zh_send_led_config_message(const led_config_t *led_config)
{
    zh_espnow_data_t data = {0};
    data.device_type = ZHDT_LED;
    data.payload_type = ZHPT_CONFIG;
    data.payload_data.config_message.led_config_message.unique_id = 1;
    data.payload_data.config_message.led_config_message.led_type = led_config->hardware_config.led_type;
    data.payload_data.config_message.led_config_message.payload_on = HAONOFT_ON;
    data.payload_data.config_message.led_config_message.payload_off = HAONOFT_OFF;
    data.payload_data.config_message.led_config_message.enabled_by_default = true;
    data.payload_data.config_message.led_config_message.optimistic = false;
    data.payload_data.config_message.led_config_message.qos = 2;
    data.payload_data.config_message.led_config_message.retain = true;
    zh_send_message(led_config->gateway_mac, (uint8_t *)&data, sizeof(zh_espnow_data_t));
}

void zh_send_led_hardware_config_message(const led_config_t *led_config)
{
    zh_espnow_data_t data = {0};
    data.device_type = ZHDT_LED;
    data.payload_type = ZHPT_HARDWARE;
    data.payload_data.config_message.led_hardware_config_message.led_type = led_config->hardware_config.led_type;
    data.payload_data.config_message.led_hardware_config_message.first_white_pin = led_config->hardware_config.first_white_pin;
    data.payload_data.config_message.led_hardware_config_message.second_white_pin = led_config->hardware_config.second_white_pin;
    data.payload_data.config_message.led_hardware_config_message.red_pin = led_config->hardware_config.red_pin;
    data.payload_data.config_message.led_hardware_config_message.green_pin = led_config->hardware_config.green_pin;
    data.payload_data.config_message.led_hardware_config_message.blue_pin = led_config->hardware_config.blue_pin;
    zh_send_message(led_config->gateway_mac, (uint8_t *)&data, sizeof(zh_espnow_data_t));
}

void zh_send_led_keep_alive_message_task(void *pvParameter)
{
    led_config_t *led_config = pvParameter;
    zh_espnow_data_t data = {0};
    data.device_type = ZHDT_LED;
    data.payload_type = ZHPT_KEEP_ALIVE;
    data.payload_data.keep_alive_message.online_status = ZH_ONLINE;
    data.payload_data.keep_alive_message.message_frequency = ZH_LED_KEEP_ALIVE_MESSAGE_FREQUENCY;
    for (;;)
    {
        zh_send_message(led_config->gateway_mac, (uint8_t *)&data, sizeof(zh_espnow_data_t));
        vTaskDelay(ZH_LED_KEEP_ALIVE_MESSAGE_FREQUENCY * 1000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

void zh_send_led_status_message(const led_config_t *led_config)
{
    zh_espnow_data_t data = {0};
    data.device_type = ZHDT_LED;
    data.payload_type = ZHPT_STATE;
    data.payload_data.status_message.led_status_message.status = led_config->status.status;
    data.payload_data.status_message.led_status_message.brightness = led_config->status.brightness;
    data.payload_data.status_message.led_status_message.temperature = led_config->status.temperature;
    data.payload_data.status_message.led_status_message.red = led_config->status.red;
    data.payload_data.status_message.led_status_message.green = led_config->status.green;
    data.payload_data.status_message.led_status_message.blue = led_config->status.blue;
    data.payload_data.status_message.led_status_message.effect = led_config->status.effect;
    zh_send_message(led_config->gateway_mac, (uint8_t *)&data, sizeof(zh_espnow_data_t));
}

void zh_espnow_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    led_config_t *led_config = arg;
    switch (event_id)
    {
#ifdef CONFIG_NETWORK_TYPE_DIRECT
    case ZH_ESPNOW_ON_RECV_EVENT:;
        zh_espnow_event_on_recv_t *recv_data = event_data;
        if (recv_data->data_len != sizeof(zh_espnow_data_t))
        {
            goto ZH_ESPNOW_EVENT_HANDLER_EXIT;
        }
#else
    case ZH_NETWORK_ON_RECV_EVENT:;
        zh_network_event_on_recv_t *recv_data = event_data;
        if (recv_data->data_len != sizeof(zh_espnow_data_t))
        {
            goto ZH_NETWORK_EVENT_HANDLER_EXIT;
        }
#endif
        zh_espnow_data_t *data = (zh_espnow_data_t *)recv_data->data;
        switch (data->device_type)
        {
        case ZHDT_GATEWAY:
            switch (data->payload_type)
            {
            case ZHPT_KEEP_ALIVE:
                if (data->payload_data.keep_alive_message.online_status == ZH_ONLINE)
                {
                    if (led_config->gateway_is_available == false)
                    {
                        led_config->gateway_is_available = true;
                        memcpy(led_config->gateway_mac, recv_data->mac_addr, 6);
                        zh_send_led_hardware_config_message(led_config);
                        if (led_config->hardware_config.led_type != HALT_NONE)
                        {
                            zh_send_led_config_message(led_config);
                            zh_send_led_status_message(led_config);
                            if (led_config->is_first_connection == false)
                            {
                                xTaskCreatePinnedToCore(&zh_send_led_attributes_message_task, "NULL", ZH_MESSAGE_STACK_SIZE, led_config, ZH_MESSAGE_TASK_PRIORITY, (TaskHandle_t *)&led_config->attributes_message_task, tskNO_AFFINITY);
                                xTaskCreatePinnedToCore(&zh_send_led_keep_alive_message_task, "NULL", ZH_MESSAGE_STACK_SIZE, led_config, ZH_MESSAGE_TASK_PRIORITY, (TaskHandle_t *)&led_config->keep_alive_message_task, tskNO_AFFINITY);
                                led_config->is_first_connection = true;
                            }
                            else
                            {
                                vTaskResume(led_config->attributes_message_task);
                                vTaskResume(led_config->keep_alive_message_task);
                            }
                        }
                    }
                }
                else
                {
                    if (led_config->gateway_is_available == true)
                    {
                        led_config->gateway_is_available = false;
                        if (led_config->hardware_config.led_type != HALT_NONE)
                        {
                            vTaskSuspend(led_config->attributes_message_task);
                            vTaskSuspend(led_config->keep_alive_message_task);
                        }
                    }
                }
                break;
            case ZHPT_SET:
                led_config->status.status = data->payload_data.status_message.led_status_message.status;
                zh_gpio_set_level(led_config);
                zh_save_status(led_config);
                zh_send_led_status_message(led_config);
                break;
            case ZHPT_BRIGHTNESS:
                led_config->status.brightness = data->payload_data.status_message.led_status_message.brightness;
                zh_gpio_set_level(led_config);
                zh_save_status(led_config);
                zh_send_led_status_message(led_config);
                break;
            case ZHPT_TEMPERATURE:
                led_config->status.temperature = data->payload_data.status_message.led_status_message.temperature;
                zh_gpio_set_level(led_config);
                zh_save_status(led_config);
                zh_send_led_status_message(led_config);
                break;
            case ZHPT_RGB:
                led_config->status.red = data->payload_data.status_message.led_status_message.red;
                led_config->status.green = data->payload_data.status_message.led_status_message.green;
                led_config->status.blue = data->payload_data.status_message.led_status_message.blue;
                zh_gpio_set_level(led_config);
                zh_save_status(led_config);
                zh_send_led_status_message(led_config);
                break;
            case ZHPT_HARDWARE:
                led_config->hardware_config.led_type = data->payload_data.config_message.led_hardware_config_message.led_type;
                led_config->hardware_config.first_white_pin = data->payload_data.config_message.led_hardware_config_message.first_white_pin;
                led_config->hardware_config.second_white_pin = data->payload_data.config_message.led_hardware_config_message.second_white_pin;
                led_config->hardware_config.red_pin = data->payload_data.config_message.led_hardware_config_message.red_pin;
                led_config->hardware_config.green_pin = data->payload_data.config_message.led_hardware_config_message.green_pin;
                led_config->hardware_config.blue_pin = data->payload_data.config_message.led_hardware_config_message.blue_pin;
                zh_save_config(led_config);
                if (led_config->hardware_config.led_type != HALT_NONE)
                {
                    vTaskDelete(led_config->attributes_message_task);
                    vTaskDelete(led_config->keep_alive_message_task);
                }
                data->device_type = ZHDT_LED;
                data->payload_type = ZHPT_KEEP_ALIVE;
                data->payload_data.keep_alive_message.online_status = ZH_OFFLINE;
                data->payload_data.keep_alive_message.message_frequency = ZH_LED_KEEP_ALIVE_MESSAGE_FREQUENCY;
                zh_send_message(led_config->gateway_mac, (uint8_t *)data, sizeof(zh_espnow_data_t));
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                esp_restart();
                break;
            case ZHPT_UPDATE:;
                if (led_config->hardware_config.led_type != HALT_NONE)
                {
                    vTaskSuspend(led_config->attributes_message_task);
                    vTaskSuspend(led_config->keep_alive_message_task);
                }
                data->device_type = ZHDT_LED;
                data->payload_type = ZHPT_KEEP_ALIVE;
                data->payload_data.keep_alive_message.online_status = ZH_OFFLINE;
                data->payload_data.keep_alive_message.message_frequency = ZH_LED_KEEP_ALIVE_MESSAGE_FREQUENCY;
                zh_send_message(led_config->gateway_mac, (uint8_t *)data, sizeof(zh_espnow_data_t));
                const esp_app_desc_t *app_info = get_app_description();
                led_config->update_partition = esp_ota_get_next_update_partition(NULL);
                strcpy(data->payload_data.ota_message.espnow_ota_data.app_version, app_info->version);
#ifdef CONFIG_IDF_TARGET_ESP8266
                char *app_name = (char *)heap_caps_malloc(strlen(app_info->project_name) + 6, MALLOC_CAP_8BIT);
                memset(app_name, 0, strlen(app_info->project_name) + 6);
                sprintf(app_name, "%s.app%d", app_info->project_name, led_config->update_partition->subtype - ESP_PARTITION_SUBTYPE_APP_OTA_0 + 1);
                strcpy(data->payload_data.ota_message.espnow_ota_data.app_name, app_name);
                heap_caps_free(app_name);
#else
                strcpy(data->payload_data.ota_message.espnow_ota_data.app_name, app_info->project_name);
#endif
                data->payload_type = ZHPT_UPDATE;
                zh_send_message(led_config->gateway_mac, (uint8_t *)data, sizeof(zh_espnow_data_t));
                break;
            case ZHPT_UPDATE_BEGIN:
#ifdef CONFIG_IDF_TARGET_ESP8266
                esp_ota_begin(led_config->update_partition, OTA_SIZE_UNKNOWN, &led_config->update_handle);
#else
                esp_ota_begin(led_config->update_partition, OTA_SIZE_UNKNOWN, (esp_ota_handle_t *)&led_config->update_handle);
#endif
                led_config->ota_message_part_number = 1;
                data->device_type = ZHDT_LED;
                data->payload_type = ZHPT_UPDATE_PROGRESS;
                zh_send_message(led_config->gateway_mac, (uint8_t *)data, sizeof(zh_espnow_data_t));
                break;
            case ZHPT_UPDATE_PROGRESS:
                if (led_config->ota_message_part_number == data->payload_data.ota_message.espnow_ota_message.part)
                {
                    ++led_config->ota_message_part_number;
                    esp_ota_write(led_config->update_handle, (const void *)data->payload_data.ota_message.espnow_ota_message.data, data->payload_data.ota_message.espnow_ota_message.data_len);
                }
                data->device_type = ZHDT_LED;
                data->payload_type = ZHPT_UPDATE_PROGRESS;
                zh_send_message(led_config->gateway_mac, (uint8_t *)data, sizeof(zh_espnow_data_t));
                break;
            case ZHPT_UPDATE_ERROR:
                esp_ota_end(led_config->update_handle);
                if (led_config->hardware_config.led_type != HALT_NONE)
                {
                    vTaskResume(led_config->attributes_message_task);
                    vTaskResume(led_config->keep_alive_message_task);
                }
                break;
            case ZHPT_UPDATE_END:
                if (esp_ota_end(led_config->update_handle) != ESP_OK)
                {
                    data->device_type = ZHDT_LED;
                    data->payload_type = ZHPT_UPDATE_FAIL;
                    zh_send_message(led_config->gateway_mac, (uint8_t *)data, sizeof(zh_espnow_data_t));
                    if (led_config->hardware_config.led_type != HALT_NONE)
                    {
                        vTaskResume(led_config->attributes_message_task);
                        vTaskResume(led_config->keep_alive_message_task);
                    }
                    break;
                }
                esp_ota_set_boot_partition(led_config->update_partition);
                data->device_type = ZHDT_LED;
                data->payload_type = ZHPT_UPDATE_SUCCESS;
                zh_send_message(led_config->gateway_mac, (uint8_t *)data, sizeof(zh_espnow_data_t));
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                esp_restart();
                break;
            case ZHPT_RESTART:
                if (led_config->hardware_config.led_type != HALT_NONE)
                {
                    vTaskDelete(led_config->attributes_message_task);
                    vTaskDelete(led_config->keep_alive_message_task);
                }
                data->device_type = ZHDT_LED;
                data->payload_type = ZHPT_KEEP_ALIVE;
                data->payload_data.keep_alive_message.online_status = ZH_OFFLINE;
                data->payload_data.keep_alive_message.message_frequency = ZH_LED_KEEP_ALIVE_MESSAGE_FREQUENCY;
                zh_send_message(led_config->gateway_mac, (uint8_t *)data, sizeof(zh_espnow_data_t));
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                esp_restart();
                break;
            default:
                break;
            }
            break;
        default:
            break;
        }
#ifdef CONFIG_NETWORK_TYPE_DIRECT
    ZH_ESPNOW_EVENT_HANDLER_EXIT:
        heap_caps_free(recv_data->data);
        break;
    case ZH_ESPNOW_ON_SEND_EVENT:;
        zh_espnow_event_on_send_t *send_data = event_data;
        if (send_data->status == ZH_ESPNOW_SEND_FAIL && led_config->gateway_is_available == true)
        {
            led_config->gateway_is_available = false;
            if (led_config->hardware_config.led_type != HALT_NONE)
            {
                vTaskSuspend(led_config->attributes_message_task);
                vTaskSuspend(led_config->keep_alive_message_task);
            }
        }
        break;
#else
    ZH_NETWORK_EVENT_HANDLER_EXIT:
        heap_caps_free(recv_data->data);
        break;
    case ZH_NETWORK_ON_SEND_EVENT:;
        zh_network_event_on_send_t *send_data = event_data;
        if (send_data->status == ZH_NETWORK_SEND_FAIL && led_config->gateway_is_available == true)
        {
            led_config->gateway_is_available = false;
            if (led_config->hardware_config.led_type != HALT_NONE)
            {
                vTaskSuspend(led_config->attributes_message_task);
                vTaskSuspend(led_config->keep_alive_message_task);
            }
        }
        break;
#endif
    default:
        break;
    }
}