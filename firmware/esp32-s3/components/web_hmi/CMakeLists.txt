idf_component_register(
    SRCS "web_hmi.cpp"
    INCLUDE_DIRS "include"
    REQUIRES controller timekeep trace testsource
             esp_http_server esp_timer esp_event esp_netif esp_wifi nvs_flash
    EMBED_TXTFILES "www/op.html" "www/dev.html"
)
