idf_component_register(
    SRCS "perch_main.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi nvs_flash esp_event esp_timer mbedtls
)
