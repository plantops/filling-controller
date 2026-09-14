# Keep fw290 network stack dependencies explicit: HTTP must not start before lwIP/netif/event loop.
idf_component_register(
    SRCS "web_hmi.cpp"
    INCLUDE_DIRS "include"
    REQUIRES controller timekeep esp_http_server esp_timer esp_netif esp_event nvs_flash
    EMBED_TXTFILES "www/op.html"
)
