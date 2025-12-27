idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES hap_wrapper yahap-pal esp_pm esp_adc esp_driver_gpio esp_event
)
