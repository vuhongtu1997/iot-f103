#ifndef IOT_SHA256_H
#define IOT_SHA256_H
#include <stdint.h>
#include <stddef.h>
typedef struct {uint32_t h[8];uint64_t bytes;uint8_t block[64];size_t used;} sha256_ctx;
typedef struct {sha256_ctx inner;uint8_t opad[64];} hmac_ctx;
void sha256_init(sha256_ctx *);
void sha256_update(sha256_ctx *,const void *,size_t);
void sha256_final(sha256_ctx *,uint8_t out[32]);
void hmac_init(hmac_ctx *,const uint8_t *,size_t);
void hmac_update(hmac_ctx *,const void *,size_t);
void hmac_final(hmac_ctx *,uint8_t out[32]);
#endif
