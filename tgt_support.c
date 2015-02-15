#include <stdint.h>

#include "tgt_support.h"

void memcpy(void *dst, const void *src, uint32_t len)
{
	const uint8_t *s = src;
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

char *strstr(const char *as1, const char *as2)
{
	const char *s1, *s2;
	const char *tptr;
	char c;

	s1 = as1;
	s2 = as2;

	if (s2 == NULL || *s2 == '\0')
		return ((char *)s1);

	c = *s2;
	while (*s1 != '\0') {
		if (c == *s1++) {
			tptr = s1;
			while ((c = *++s2) == *s1++ && c != '\0')
				continue;
			if (c == '\0')
				return ((char *)tptr - 1);
			s1 = tptr;
			s2 = as2;
			c = *s2;
		}
	}

	return (NULL);
}
