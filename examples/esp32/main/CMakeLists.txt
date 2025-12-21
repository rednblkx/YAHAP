idf_component_register(
    SRCS "main.cpp" "Esp32Ble.cpp" "Esp32Platform.cpp" "Esp32Crypto.cpp"
    INCLUDE_DIRS "."
    REQUIRES hap_wrapper nvs_flash bt libsodium mbedtls esp_pm
)
