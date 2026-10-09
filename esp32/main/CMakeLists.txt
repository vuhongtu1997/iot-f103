idf_component_register(SRCS "main.c" "bus.c" "ota.c" "../../common/protocol.c" "../../common/sha256.c"
 INCLUDE_DIRS "." "../../common"
 REQUIRES driver esp_wifi esp_event esp_netif nvs_flash mqtt json esp_http_client app_update spiffs mbedtls)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-misleading-indentation)
