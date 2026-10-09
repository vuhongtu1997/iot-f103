#ifndef IOT_SECRETS_H
#define IOT_SECRETS_H
#define WIFI_SSID "CHANGE_ME"
#define WIFI_PASSWORD "CHANGE_ME"
#define MQTT_URI "mqtts://mqtt.example.com:8883"
#define MQTT_USERNAME "CHANGE_ME"
#define MQTT_PASSWORD "CHANGE_ME"
#define GATEWAY_ID "gw-001"
/* Only this HTTPS origin is accepted, including explicit port if used.
 * No redirects; URL must be HOST + slash + path. */
#define OTA_ORIGIN "https://firmware.example.com"
#define POLL_INTERVAL_MS 5000
#define RS485_TX_PIN 17
#define RS485_RX_PIN 16
#define RS485_DE_PIN 4
/* Empty -> ESP-IDF public certificate bundle; otherwise use your private CA PEM.
 * The same CA is used for MQTT and firmware server. Split if providers differ. */
#define CLOUD_CA_PEM ""
#endif
