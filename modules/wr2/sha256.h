#ifndef WR2_SHA256_H
#define WR2_SHA256_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t state[8];
    uint64_t bit_count;
    uint8_t block[64];
    size_t used;
} wr2_sha256_t;

void wr2_sha256_init(wr2_sha256_t *ctx);
int wr2_sha256_update(wr2_sha256_t *ctx, const void *data, size_t length);
void wr2_sha256_final(wr2_sha256_t *ctx, uint8_t digest[32]);

#endif
