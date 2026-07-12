idf_component_register(SRCS "main.cpp" "pwm.cpp" INCLUDE_DIRS "." PRIV_REQUIRES spi_flash esp_partition esp_timer esp_driver_ledc esp_driver_gpio)
target_compile_options(${COMPONENT_LIB} PRIVATE "-include" "driver/gpio.h")

idf_component_get_property(pwm_lib espressif__pwm_audio COMPONENT_LIB)
target_link_libraries(${pwm_lib} PRIVATE idf::esp_driver_ledc idf::esp_driver_gptimer idf::esp_driver_gpio)
target_compile_options(${pwm_lib} PRIVATE "-include" "driver/gpio.h")