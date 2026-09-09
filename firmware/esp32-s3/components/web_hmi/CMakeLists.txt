idf_component_register(
    SRCS "web_hmi.cpp"
    INCLUDE_DIRS "include"
    REQUIRES controller tlb485 esp_http_server esp_wifi esp_netif esp_event nvs_flash esp_eth driver
)
