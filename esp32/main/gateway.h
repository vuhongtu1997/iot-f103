#ifndef IOT_GATEWAY_H
#define IOT_GATEWAY_H
#include <stdint.h>
#include "protocol.h"
#include "cJSON.h"
typedef struct {uint8_t uid[12],address,mode;uint32_t version;} node_info;
extern node_info nodes[MAX_NODES];
extern unsigned node_count;
void bus_init(void);
int bus_exchange(uint8_t addr,uint8_t fc,const uint8_t *payload,size_t plen,uint8_t *out,size_t cap,int timeout_ms,int *activity);
int discover_nodes(void);
int assign_address(uint8_t address,node_info *assigned);
int persist_registry(void);
int read_node(const node_info *,uint16_t regs[12]);
int node_ota_op(const node_info *,uint8_t op,const uint8_t *,size_t,uint32_t *received);
int run_ota(const cJSON *);
void publish_status(const char *job,const char *state,const char *detail);
void uid_hex(const uint8_t *,char out[25]);
int parse_hex(const char *,uint8_t *,size_t);
#endif
