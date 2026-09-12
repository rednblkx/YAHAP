idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES yahap yahap-pal esp_pm nvs_flash esp_event
)
