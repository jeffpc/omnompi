#ifndef __LZ4_H
#define __LZ4_H

extern uint32_t lz4_compress(void *s_start, void *d_start, size_t s_len, size_t d_len, int n);
extern int lz4_decompress(void *s_start, void *d_start, size_t s_len, size_t d_len, int n);

#endif
