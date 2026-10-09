#ifndef TEST_UART_H
#define TEST_UART_H
#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include <assert.h>
#define ESP_ERROR_CHECK(x) assert((x)==0)
#define UART_NUM_2 2
#define UART_DATA_8_BITS 8
#define UART_PARITY_DISABLE 0
#define UART_STOP_BITS_1 1
#define UART_HW_FLOWCTRL_DISABLE 0
#define UART_SCLK_DEFAULT 0
#define UART_PIN_NO_CHANGE -1
#define UART_MODE_RS485_HALF_DUPLEX 1
#define UART_FRAME_ERR 1
#define UART_PARITY_ERR 2
#define UART_FIFO_OVF 3
#define UART_BUFFER_FULL 4
typedef struct {int baud_rate,data_bits,parity,stop_bits,flow_ctrl,source_clk;} uart_config_t;
typedef struct {int type;} uart_event_t;
int uart_driver_install(int,int,int,int,QueueHandle_t *,int);
int uart_param_config(int,const uart_config_t *);
int uart_set_pin(int,int,int,int,int);
int uart_set_mode(int,int);
int uart_flush_input(int);
int uart_write_bytes(int,const void *,size_t);
int uart_wait_tx_done(int,unsigned);
int uart_read_bytes(int,void *,unsigned,unsigned);
#endif
