idf_component_register(
    SRCS "g1_diag.cpp" "vio_web.cpp" "vio_control.cpp"
    INCLUDE_DIRS "." "include"
    REQUIRES driver esp_timer esp_system esp_hw_support spi_flash
             esp_eth esp_netif esp_event lwip esp_http_server controller
)
