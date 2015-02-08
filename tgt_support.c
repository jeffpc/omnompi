#include <stdint.h>

#include "tgt_support.h"

void memcpy(void *dst, void *src, uint32_t len)
{
	uint8_t *s = src;
	uint8_t *d = dst;

	while (len) {
		*d = *s;

		d++;
		s++;
		len--;
	}
}

void memset(void *dst, int val, uint32_t len)
{
	uint8_t *ptr = dst;

	while (len) {
		*ptr = val;

		ptr++;
		len--;
	}
}
