idf_component_register(
    SRCS "tlb485.cpp"
    INCLUDE_DIRS "include"
    REQUIRES controller esp_driver_uart
)
