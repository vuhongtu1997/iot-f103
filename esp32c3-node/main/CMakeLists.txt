idf_component_register(
    SRCS "main.c" "board.c" "config.c" "stage.c" "sensors.c" "soft_bus.c"
         "../../stm32/src/node.c" "../../common/protocol.c" "../../common/sensor_codec.c"
         "../../common/soft_uart.c"
    INCLUDE_DIRS "." "../../stm32/include" "../../common"
    REQUIRES esp_driver_uart esp_driver_gpio esp_driver_i2c esp_driver_rmt esp_timer esp_system esp_rom
             esp_hw_support nvs_flash app_update)
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    NODE_SENSOR_TYPE=${CONFIG_NODE_SENSOR_TYPE} NODE_DIGITAL_SENSORS=1)
