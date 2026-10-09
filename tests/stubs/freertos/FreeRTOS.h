#ifndef TEST_FREERTOS_H
#define TEST_FREERTOS_H
#include <stdint.h>
typedef void *QueueHandle_t;
#define pdMS_TO_TICKS(x) (x)
int xQueueReceive(QueueHandle_t,void *,unsigned);
void xQueueReset(QueueHandle_t);
void vTaskDelay(unsigned);
#endif
