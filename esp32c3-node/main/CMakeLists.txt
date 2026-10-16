idf_component_register(
    SRCS "main.c" "board.c" "config.c" "stage.c"
         "../../stm32/src/node.c" "../../common/protocol.c"
    INCLUDE_DIRS "." "../../stm32/include" "../../common"
    REQUIRES esp_driver_uart esp_driver_gpio esp_adc esp_timer esp_system
             esp_hw_support nvs_flash app_update)
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    NODE_SENSOR_TYPE=${CONFIG_NODE_SENSOR_TYPE} NODE_CALIBRATED_ADC=1)
