idf_component_register(
    SRCS "g1_diag.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_eth esp_event esp_netif esp_system driver
)
