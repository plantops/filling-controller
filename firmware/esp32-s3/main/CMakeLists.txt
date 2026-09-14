idf_component_register(
    SRCS "app_main.cpp" "wifi_ap_fallback.cpp"
    INCLUDE_DIRS "."
    REQUIRES controller position timekeep trace board_io tlb485 web_hmi
             esp_timer esp_system esp_driver_gpio esp_app_format
             esp_wifi esp_netif esp_event nvs_flash
)
