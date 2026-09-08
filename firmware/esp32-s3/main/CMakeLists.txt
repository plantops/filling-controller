idf_component_register(
    SRCS "app_main.cpp"
    INCLUDE_DIRS "."
    REQUIRES controller board_io tlb485 web_hmi esp_timer esp_task_wdt
)
