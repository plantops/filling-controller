idf_component_register(
    SRCS "g1_diag.cpp"
    INCLUDE_DIRS "."
    REQUIRES driver esp_timer esp_system esp_hw_support spi_flash
             esp_eth esp_netif esp_event lwip
)
