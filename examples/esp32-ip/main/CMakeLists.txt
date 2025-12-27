idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES hap_wrapper yahap-pal nvs_flash esp_wifi mdns esp_event mbedtls esp_timer esp_pm
)
