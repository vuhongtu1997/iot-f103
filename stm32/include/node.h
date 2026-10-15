#ifndef IOT_NODE_H
#define IOT_NODE_H
#include "protocol.h"
uint8_t config_address(void);
int config_set_address(uint8_t);
int config_factory_reset(void);
int stage_begin(const uint8_t descriptor[16]);
int stage_data(uint32_t, const uint8_t *, size_t);
uint32_t stage_received(void);
int stage_commit(void);
int stage_install(void);
void node_run(int boot_mode);
#endif
