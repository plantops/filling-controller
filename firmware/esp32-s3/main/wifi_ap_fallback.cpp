#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include <cstdio>

namespace {

constexpr char kTag[] = "sp01_wifi";
constexpr char kFieldApSsid[] = "SP01-HMI";
constexpr char kFieldApPassword[] = "sp01filling";

bool wifi_ap_already_running() noexcept {
    wifi_mode_t mode = WIFI_MODE_NULL;
    const esp_err_t err = esp_wifi_get_mode(&mode);
    if (err != ESP_OK) return false;
    return mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA;
}

void ensure_net_stack() noexcept {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        if (nvs_flash_erase() == ESP_OK) err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "fallback AP: NVS init failed: %s", esp_err_to_name(err));
    }

    err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "fallback AP: netif init failed: %s", esp_err_to_name(err));
    }

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "fallback AP: event loop init failed: %s", esp_err_to_name(err));
    }
}

void start_fallback_ap() noexcept {
    if (CONFIG_SP01_WIFI_SSID[0] != '\0') {
        ESP_LOGI(kTag, "fallback AP skipped: configured Wi-Fi STA is enabled");
        return;
    }

    if (wifi_ap_already_running()) {
        ESP_LOGI(kTag, "fallback AP already running");
        return;
    }

    ensure_net_stack();

    if (wifi_ap_already_running()) return;

    esp_netif_t* ap_netif = esp_netif_create_default_wifi_ap();
    if (ap_netif == nullptr) {
        ESP_LOGW(kTag, "fallback AP unavailable: AP netif create failed");
        return;
    }
    (void)esp_netif_set_hostname(ap_netif, "sp01-ap");

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t err = esp_wifi_init(&init);
    if (err != ESP_OK && err != ESP_ERR_WIFI_INIT_STATE) {
        ESP_LOGW(kTag, "fallback AP unavailable: Wi-Fi init failed: %s", esp_err_to_name(err));
        return;
    }

    if (wifi_ap_already_running()) return;

    wifi_config_t ap{};
    std::snprintf(reinterpret_cast<char*>(ap.ap.ssid), sizeof(ap.ap.ssid), "%s", kFieldApSsid);
    std::snprintf(reinterpret_cast<char*>(ap.ap.password), sizeof(ap.ap.password), "%s", kFieldApPassword);
    ap.ap.channel = 1;
    ap.ap.max_connection = 4;
    ap.ap.authmode = WIFI_AUTH_WPA2_PSK;

    err = esp_wifi_set_mode(WIFI_MODE_AP);
    if (err == ESP_OK) err = esp_wifi_set_config(WIFI_IF_AP, &ap);
    if (err == ESP_OK) err = esp_wifi_start();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "fallback AP unavailable: %s", esp_err_to_name(err));
        return;
    }

    esp_netif_ip_info_t ip{};
    if (esp_netif_get_ip_info(ap_netif, &ip) == ESP_OK) {
        ESP_LOGI(kTag, "FALLBACK HOTSPOT READY: SSID=%s HMI=http://" IPSTR,
                 kFieldApSsid, IP2STR(&ip.ip));
    } else {
        ESP_LOGI(kTag, "FALLBACK HOTSPOT READY: SSID=%s HMI=http://192.168.4.1",
                 kFieldApSsid);
    }
}

void fallback_task(void*) {
    // Let normal app/HMI startup win. If it does not bring AP up, recover here.
    vTaskDelay(pdMS_TO_TICKS(2000));
    start_fallback_ap();
    vTaskDelete(nullptr);
}

struct FallbackBootstrap {
    FallbackBootstrap() noexcept {
        const BaseType_t ok = xTaskCreate(fallback_task, "sp01_wifi_fallback", 4096,
                                          nullptr, 5, nullptr);
        if (ok != pdPASS) {
            ESP_LOGW(kTag, "fallback AP task create failed");
        }
    }
};

FallbackBootstrap g_fallback_bootstrap;

}  // namespace
