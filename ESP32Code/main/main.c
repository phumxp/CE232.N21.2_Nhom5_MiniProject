#include <stdio.h>
#include "dht11.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <mqtt_client.h>
#include "driver/i2c.h"
#include "ssd1306.h"
#include "font8x8_basic.h"

#define DHT_PIN GPIO_NUM_26

#define ESP_WIFI_SSID "MXPi"
#define ESP_WIFI_PASS "zzzzzzzz"
#define ESP_MAXIMUM_RETRY 25

static EventGroupHandle_t wifi_event_group;
static EventGroupHandle_t mqtt_event_group;
static int s_retry_num = 0;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define MQTT_CONNECTED_BIT BIT0

#define CONFIG_BROKER_HOST "mqtt.flespi.io"
#define CONFIG_BROKER_PORT 1883
#define CONFIG_BROKER_USERNAME "FlespiToken xJgIo4dRvVNp6oHvLRMT8zDvZJ0U7tsu1y1fs4ULPv1JHy2hfE0b1suP1kxWBPfs"
#define CONFIG_BROKER_PASSWORD ""
#define CONFIG_BROKER_CLIENT_ID "MiniProjectClient"
static esp_mqtt_client_handle_t MQTT_CLIENT = NULL;

#define MQTT_SEND_DELAY 3000

#define SSD1306_UPDATE_DELAY 500

#define I2C_MASTER_SDA_IO 5
#define I2C_MASTER_SCL_IO 4
#define I2C_MASTER_FREQ_HZ 100000
#define I2C_MASTER_RX_BUF_DISABLE 0
#define I2C_MASTER_TX_BUF_DISABLE 0

static const char *WIFI_TAG = "WIFI_TAG";
static const char *MQTT_TAG = "MQTT_TAG";

static esp_err_t wifi_event_handler(void *arg, esp_event_base_t event_base,
                                    int32_t event_id, void *event_data);

static esp_err_t wifi_init_sta(void);

static void log_error_if_nonzero(const char *message, int error_code);
static esp_err_t mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

static esp_err_t mqtt_init();

void ssd1306_init();
void ssd1306_display_clear();
void ssd1306_display_clear_line(uint8_t line_number);
void ssd1306_display_text(const void *arg_text, uint8_t row);

static void temperature_send_mqtt_task(void *pvParameter);
static void humidity_send_mqtt_task(void *pvParameter);
static void ssd1306_task(void *pvParameter);

void app_main(void)
{
    DHT11_init(DHT_PIN);
    xTaskCreate(&ssd1306_task, "ssd1306_task", 2048, NULL, 5, NULL);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(wifi_init_sta());

    ESP_ERROR_CHECK(mqtt_init());

    int msg_id = esp_mqtt_client_subscribe(MQTT_CLIENT, "TEMPERATURE_TOPIC", 0);
    ESP_LOGI(MQTT_TAG, "Sent subscribe successful, msg_id = %d", msg_id);
    msg_id = esp_mqtt_client_subscribe(MQTT_CLIENT, "HUMIDITY_TOPIC", 0);
    ESP_LOGI(MQTT_TAG, "Sent subscribe successful, msg_id = %d", msg_id);

    xTaskCreate(&temperature_send_mqtt_task, "temperature_send_mqtt_task", 2048, NULL, 5, NULL);
    xTaskCreate(&humidity_send_mqtt_task, "humidity_send_mqtt_task", 2048, NULL, 5, NULL);
}

static esp_err_t wifi_event_handler(void *arg, esp_event_base_t event_base,
                                    int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }

    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (s_retry_num < ESP_MAXIMUM_RETRY)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(WIFI_TAG, "retry to connect to the AP");
        }
        else
        {
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(WIFI_TAG, "connect to the AP fail");
    }

    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(WIFI_TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
    return ESP_OK;
}

static esp_err_t wifi_init_sta(void)
{
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = ESP_WIFI_SSID,
            .password = ESP_WIFI_PASS,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(WIFI_TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(WIFI_TAG, "connected to ap SSID:%s. Password:%s",
                 ESP_WIFI_SSID, ESP_WIFI_PASS);
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(WIFI_TAG, "Failed to connect to SSID:%s, password:%s",
                 ESP_WIFI_SSID, ESP_WIFI_PASS);
    }
    else
    {
        ESP_LOGE(WIFI_TAG, "UNEXPECTED EVENT");
    }

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(wifi_event_group);
    return ESP_OK;
}

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0)
    {
        ESP_LOGE(MQTT_TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static esp_err_t mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(MQTT_TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;

    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_CONNECTED");
        xEventGroupSetBits(mqtt_event_group, MQTT_CONNECTED_BIT);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_DATA");
        int temp_len = event->topic_len;
        char *temp_topic = (char *)malloc((temp_len + 1) * sizeof(char));
        strncpy(temp_topic, event->topic, event->topic_len);
        temp_topic[temp_len] = '\0';

        ESP_LOGI(MQTT_TAG, "TOPIC = %s", temp_topic);

        if (strcmp(temp_topic, "TEMPERATURE_TOPIC") == 0)
            ESP_LOGI(MQTT_TAG, "DATA = %0.1f", *((float *)(event->data)));
        else
            ESP_LOGI(MQTT_TAG, "DATA = %d", *((int *)(event->data)));
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
        {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno", event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(MQTT_TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;

    default:
        ESP_LOGI(MQTT_TAG, "Other event id:%d", event->event_id);
        break;
    }
    return ESP_OK;
}

static esp_err_t mqtt_init()
{
    mqtt_event_group = xEventGroupCreate();

    ESP_LOGI(MQTT_TAG, "[APP] Startup..");
    ESP_LOGI(MQTT_TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(MQTT_TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_BASE", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

    esp_mqtt_client_config_t mqtt_cfg = {
        .host = CONFIG_BROKER_HOST,
        .port = CONFIG_BROKER_PORT,
        .username = CONFIG_BROKER_USERNAME,
        // .password = CONFIG_BROKER_PASSWORD,
        .client_id = CONFIG_BROKER_CLIENT_ID,
    };

    MQTT_CLIENT = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(MQTT_CLIENT, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(MQTT_CLIENT);

    ESP_LOGI(MQTT_TAG, "Waiting for connection to the broker... ");
    xEventGroupWaitBits(mqtt_event_group, MQTT_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    ESP_LOGI(MQTT_TAG, "Broker connected!!!");

    return ESP_OK;
}

void ssd1306_init()
{
    esp_err_t espRc;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_STREAM, true);
    i2c_master_write_byte(cmd, OLED_CMD_SET_CHARGE_PUMP, true);
    i2c_master_write_byte(cmd, 0x14, true);
    i2c_master_write_byte(cmd, OLED_CMD_SET_SEGMENT_REMAP, true);
    i2c_master_write_byte(cmd, OLED_CMD_SET_COM_SCAN_MODE, true);
    i2c_master_write_byte(cmd, OLED_CMD_DISPLAY_NORMAL, true);
    i2c_master_write_byte(cmd, OLED_CMD_DISPLAY_OFF, true);
    i2c_master_write_byte(cmd, OLED_CMD_DISPLAY_ON, true);
    i2c_master_stop(cmd);
    espRc = i2c_master_cmd_begin(I2C_NUM_0, cmd, 10 / portTICK_PERIOD_MS);
    if (espRc == ESP_OK)
    {
        printf("OLED configured successfully\n");
    }
    else
    {
        printf("OLED configuration failed. code: 0x%.2X\n", espRc);
    }
    i2c_cmd_link_delete(cmd);
}

void ssd1306_display_clear()
{
    i2c_cmd_handle_t cmd;
    uint8_t clear[128];
    for (uint8_t i = 0; i < 128; i++)
    {
        clear[i] = 0;
    }
    for (uint8_t i = 0; i < 8; i++)
    {
        cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
        i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_SINGLE, true);
        i2c_master_write_byte(cmd, 0xB0 | i, true);

        i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_DATA_STREAM, true);
        i2c_master_write(cmd, clear, 128, true);
        i2c_master_stop(cmd);
        i2c_master_cmd_begin(I2C_NUM_0, cmd, 10 / portTICK_PERIOD_MS);
        i2c_cmd_link_delete(cmd);
    }
}

void ssd1306_display_clear_line(uint8_t line_number)
{
    i2c_cmd_handle_t cmd;
    uint8_t clear[128];
    for (uint8_t i = 0; i < 128; i++)
    {
        clear[i] = 0;
    }
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_SINGLE, true);
    i2c_master_write_byte(cmd, 0xB0 | line_number, true);

    i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_DATA_STREAM, true);
    i2c_master_write(cmd, clear, 128, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_NUM_0, cmd, 10 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
}

void ssd1306_display_text(const void *arg_text, uint8_t row)
{
    char *text = (char *)arg_text;
    uint8_t text_len = strlen(text);
    i2c_cmd_handle_t cmd;

    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_STREAM, true);
    i2c_master_write_byte(cmd, 0x00, true);
    i2c_master_write_byte(cmd, 0x10, true);
    i2c_master_write_byte(cmd, 0xB0 | row, true);

    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_NUM_0, cmd, 10 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    for (uint8_t i = 0; i < text_len; i++)
    {
        cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
        if (text[i] == '\n')
        {
            i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_STREAM, true);
            i2c_master_write_byte(cmd, 0x00, true);
            i2c_master_write_byte(cmd, 0x10, true);
            i2c_master_write_byte(cmd, 0xB0 | ++row, true);
        }
        else
        {
            i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_DATA_STREAM, true);
            if (text[i] == ('°' & 0x00FF))
            {
                i2c_master_write(cmd, font8x8_degrees, 8, true);
            }
            else
            {
                i2c_master_write(cmd, font8x8_basic_tr[(uint8_t)text[i]], 8, true);
            }
        }
        i2c_master_stop(cmd);
        i2c_master_cmd_begin(I2C_NUM_0, cmd, 10 / portTICK_PERIOD_MS);
        i2c_cmd_link_delete(cmd);
    }
}

static void temperature_send_mqtt_task(void *pvParameter)
{
    while (true)
    {
        float temperature = DHT11_read().temperature;
        ESP_LOGI(MQTT_TAG, "Temperature: %0.1f", temperature);
        if ((temperature >= 0) && (temperature <= 50))
        {
            int msg_id = esp_mqtt_client_publish(MQTT_CLIENT,
                                                 "TEMPERATURE_TOPIC",
                                                 &temperature,
                                                 sizeof(temperature),
                                                 0,
                                                 false);
            ESP_LOGI(MQTT_TAG, "Sent publish successful, msg_id = %d", msg_id);
            vTaskDelay(MQTT_SEND_DELAY / portTICK_RATE_MS);
        }
    }
}

static void humidity_send_mqtt_task(void *pvParameter)
{
    while (1)
    {
        int humidity = DHT11_read().humidity;
        ESP_LOGI(MQTT_TAG, "Humidity: %d", humidity);
        if ((humidity >= 20) && (humidity <= 90))
        {
            int msg_id = esp_mqtt_client_publish(MQTT_CLIENT,
                                                 "HUMIDITY_TOPIC",
                                                 &humidity,
                                                 sizeof(humidity),
                                                 0,
                                                 false);
            ESP_LOGI(MQTT_TAG, "Sent publish successful, msg_id = %d", msg_id);
            vTaskDelay(MQTT_SEND_DELAY / portTICK_RATE_MS);
        }
    }
}

static void ssd1306_task(void *pvParameter)
{
    int i2c_master_port = 0;
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_param_config(i2c_master_port, &conf);
    i2c_driver_install(i2c_master_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
    ssd1306_init();
    ssd1306_display_clear();

    float last_temperature = -1;
    int last_humidity = -1;

    while (1)
    {
        float temperature = DHT11_read().temperature;
        int humidity = DHT11_read().humidity;
        if ((temperature != last_temperature) && (temperature >= 0) && (temperature <= 50))
        {
            ssd1306_display_clear_line(3);
            char buffer[13];
            sprintf(buffer, "Temp: %.1f%cC", temperature, '°');
            ssd1306_display_text(buffer, 3);
            last_temperature = temperature;
        }
        if ((humidity != last_humidity) && (humidity >= 20) && (humidity <= 80))
        {
            ssd1306_display_clear_line(4);
            char buffer[11];
            sprintf(buffer, "Hum: %d%%", humidity);
            ssd1306_display_text(buffer, 4);
            last_humidity = humidity;
        }
        vTaskDelay(SSD1306_UPDATE_DELAY / portTICK_RATE_MS);
    }
}