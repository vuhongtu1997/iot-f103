#include "Uart.h"

#include <stdio.h>
#include <string>
#include <string.h>
#include "Log.h"
#include "Led.h"
#include "esp_log.h"

using namespace std;

#define DEBUG_ENABLE 1

#define PATTERN_CHR_NUM (3)

#define BUF_SIZE (512)
#define RD_BUF_SIZE (BUF_SIZE)
#define RD_BLE_MESSAGE_MAX_SIZE 50

Uart::Uart(uart_port_t num, int txPin, int rxPin, int baudrate)
    : num(num), txPin(txPin), rxPin(rxPin), baudrate(baudrate), queue(NULL), initialized(false)
{
}

static void uart_event_task(void *pvParameters)
{
	if (pvParameters == NULL)
		return;
	Uart *uart = (Uart *)pvParameters;
	uart_event_t event;
	size_t buffered_size;
	uint8_t *rx_buf = (uint8_t *)heap_caps_malloc_prefer(RD_BUF_SIZE * 2, 2, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM, MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL);
	int rx_count = 0, rx_remain = 0;
	int length = 0;
	int pos;
	uint32_t current = esp_log_timestamp();
	for (;;)
	{
		if (xQueueReceive(uart->GetQueue(), (void *)&event, (TickType_t)5))
		{
			switch (event.type)
			{
			// Event of UART receving data
			/*We'd better handler data event fast, there would be much more data events than
			other types of events. If we take too much time on data event, the queue might
			be full.*/
			case UART_DATA:
				if (rx_count > 0 && esp_log_timestamp() - current > 100)
				{
					rx_count = 0;
				}
				current = esp_log_timestamp();
				ESP_ERROR_CHECK(uart_get_buffered_data_len(uart->GetNum(), (size_t *)&length));
				length = uart_read_bytes(uart->GetNum(), rx_buf + rx_count, length, portMAX_DELAY);
				rx_count += length;
#if DEBUG_ENABLE
				if (rx_count)
				{
					string txStr = "";
					char temp[10];
					for (int i = 0; i < rx_count; i++)
					{
						sprintf(temp, "%02X ", rx_buf[i]);
						txStr += temp;
					}
					LOGI("rx: %s", txStr.c_str());
				}
#endif
				rx_remain = uart->OnMessage(rx_buf, rx_count);
				if (rx_remain)
					LOGD("rx_remain: %d", rx_remain);
				if (rx_remain && rx_remain < RD_BLE_MESSAGE_MAX_SIZE)
				{
					if (rx_remain != rx_count)
					{
						for (int i = 0; i < rx_remain; i++)
						{
							rx_buf[i] = rx_buf[i + rx_count - rx_remain];
						}
						rx_count = rx_remain;
					}
				}
				else
				{
					rx_count = 0;
				}
				vTaskDelay(10 / portTICK_PERIOD_MS);
				break;
			// Event of HW FIFO overflow detected
			case UART_FIFO_OVF:
				LOGI("hw fifo overflow");
				// If fifo overflow happened, you should consider adding flow control for your application.
				// The ISR has already reset the rx FIFO,
				// As an example, we directly flush the rx buffer here in order to read more data.
				uart_flush_input(uart->GetNum());
				xQueueReset(uart->GetQueue());
				break;
			// Event of UART ring buffer full
			case UART_BUFFER_FULL:
				LOGI("ring buffer full");
				// If buffer full happened, you should consider encreasing your buffer size
				// As an example, we directly flush the rx buffer here in order to read more data.
				uart_flush_input(uart->GetNum());
				xQueueReset(uart->GetQueue());
				break;
			// Event of UART RX break detected
			case UART_BREAK:
				LOGI("uart rx break");
				break;
			// Event of UART parity check error
			case UART_PARITY_ERR:
				LOGI("uart parity error");
				break;
			// Event of UART frame error
			case UART_FRAME_ERR:
				LOGI("uart frame error");
				break;
			// UART_PATTERN_DET
			case UART_PATTERN_DET:
				uart_get_buffered_data_len(uart->GetNum(), &buffered_size);
				pos = uart_pattern_pop_pos(uart->GetNum());
				LOGI("[UART PATTERN DETECTED] pos: %d, buffered size: %d", pos, buffered_size);
				if (pos == -1)
				{
					// There used to be a UART_PATTERN_DET event, but the pattern position queue is full so that it can not
					// record the position. We should set a larger queue size.
					// As an example, we directly flush the rx buffer here.
					uart_flush_input(uart->GetNum());
				}
				else
				{
					// uart_read_bytes(uart->GetNum(), dtmp, pos, 100 / portTICK_PERIOD_MS);
					// uint8_t pat[PATTERN_CHR_NUM + 1];
					// memset(pat, 0, sizeof(pat));
					// uart_read_bytes(uart->GetNum(), pat, PATTERN_CHR_NUM, 100 / portTICK_PERIOD_MS);
					// LOGI("read data: %s", dtmp);
					// LOGI("read pat : %s", pat);
				}
				break;
			// Others
			default:
				LOGI("uart event type: %d", event.type);
				break;
			}
			vTaskDelay(10 / portTICK_PERIOD_MS);
		}
		vTaskDelay(10 / portTICK_PERIOD_MS);
	}
	vTaskDelete(NULL);
}

uart_port_t Uart::GetNum()
{
	return num;
}

QueueHandle_t Uart::GetQueue()
{
	return queue;
}

void Uart::Init()
{
	uart_config_t uart_config = {
		.baud_rate = baudrate,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
		.source_clk = UART_SCLK_APB,
	};
	// Install UART driver, and get the queue.
	esp_err_t err = uart_driver_install(num, BUF_SIZE * 2, BUF_SIZE * 2, 20, &queue, 0);
	if (err != ESP_OK)
	{
		LOGE("uart_driver_install failed: %d", err);
		queue = NULL;
		initialized = false;
		return;
	}

	err = uart_param_config(num, &uart_config);
	if (err != ESP_OK)
	{
		LOGE("uart_param_config failed: %d", err);
		initialized = false;
		return;
	}

	// Set UART pins (using UART0 default pins ie no changes.)
	err = uart_set_pin(num, txPin, rxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
	if (err != ESP_OK)
	{
		LOGE("uart_set_pin failed: %d", err);
		initialized = false;
		return;
	}

	initialized = true;

	// Create a task to handler UART event from ISR
	LOGI("Free memory: %d bytes, internal: %d bytes", esp_get_free_heap_size(), esp_get_free_internal_heap_size());
	if (xTaskCreate(uart_event_task, "uart_event_task", 4096, this, 10, NULL) != pdPASS)
	{
		LOGE("Failed to create task");
		SetLedService(false);
	}
}

int Uart::Open(int baudrate)
{
	return 0;
}

int Uart::Close()
{
	return 0;
}

int Uart::ChangeBaudrate(int baudrate)
{
	return 0;
}

ssize_t Uart::Write(const void *buf, size_t count)
{
	if (!initialized)
	{
		LOGE("uart write failed: driver not initialized");
		return -1;
	}
	if (buf == NULL || count == 0)
	{
		return 0;
	}

#if DEBUG_ENABLE
	const unsigned char *bufChar = reinterpret_cast<const unsigned char *>(buf);
	string txStr = "";
	char temp[10];
	for (size_t i = 0; i < count; i++)
	{
		sprintf(temp, "%02X ", bufChar[i]);
		txStr += temp;
	}
	LOGI("tx: %s", txStr.c_str());
#endif

	int written = uart_write_bytes(num, buf, count);
	if (written < 0)
	{
		LOGE("uart_write_bytes failed: %d", written);
		return written;
	}

	return written;
}

int Uart::OnMessage(unsigned char *data, int len)
{
	LOGI("OnMessage len: %d", len);
	return 0;
}
