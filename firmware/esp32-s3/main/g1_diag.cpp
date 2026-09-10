#include <cstdio>
#include <cstring>

#include "esp_event.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {
constexpr char kSsid[] = "SP01-G1-ALIVE";

void serial_heartbeat(void*) {
    unsigned long n = 0;
    for (;;) {
        std::printf("SP01 G1 ALIVE HB %lu\r\n", ++n);
        std::fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

esp_err_t start_alive_ap() {
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    init.nvs_enable = false;
    if ((err = esp_wifi_init(&init)) != ESP_OK) return err;
    if ((err = esp_wifi_set_storage(WIFI_STORAGE_RAM)) != ESP_OK) return err;
    if ((err = esp_wifi_set_mode(WIFI_MODE_AP)) != ESP_OK) return err;

    wifi_config_t ap{};
    std::memcpy(ap.ap.ssid, kSsid, sizeof(kSsid) - 1);
    ap.ap.ssid_len = sizeof(kSsid) - 1;
    ap.ap.channel = 6;
    ap.ap.authmode = WIFI_AUTH_OPEN;
    ap.ap.max_connection = 1;

    if ((err = esp_wifi_set_config(WIFI_IF_AP, &ap)) != ESP_OK) return err;
    return esp_wifi_start();
}
}  // namespace

extern "C" void app_main(void) {
    std::printf("\r\nSP01 G1 MINIMAL ALIVE BOOT\r\n");
    std::fflush(stdout);

    (void)xTaskCreate(serial_heartbeat, "g1_hb", 2048, nullptr, 1, nullptr);

    const esp_err_t err = start_alive_ap();
    std::printf("SP01 G1 WIFI AP result=%s SSID=%s\r\n", esp_err_to_name(err), kSsid);
    std::fflush(stdout);

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
