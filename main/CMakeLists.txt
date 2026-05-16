idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES
        lvgl
        nv3041a
        gt911
        ui
        driver
        esp_timer
        freertos
)
