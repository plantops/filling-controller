idf_component_register(
    SRCS "web_hmi.cpp"
    INCLUDE_DIRS "include"
    REQUIRES controller timekeep esp_http_server
    EMBED_TXTFILES "www/op.html"
)
