#include <stdint.h>

#include "libuart/uart.h"
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

size_t
strlen(const char *s)
{
	const char *s0 = s + 1;

	while (*s++ != '\0')
		;
	return (s - s0);
}

void
puts(const char *str)
{
	const char *c = str;

	while (*c != '\0')
		uart_putc(*c++);
}

void
puthex(uint32_t v)
{
	char buf[2 + 8 + 1];
	char c;
	int i;

	buf[0] = '0';
	buf[1] = 'x';
	buf[10] = '\0';

	for (i = 7; i >= 0; i--) {
		c = v & 0xf;
		v >>= 4;

		if (c > 9)
			c += 'a' - 10;
		else
			c += '0';

		buf[2 + i] = c;
	}

	puts(buf);
}

void
read(void *buf, uint32_t len)
{
	uint8_t *ptr = buf;

	while (len) {
		*ptr = uart_getbyte();

		ptr++;
		len--;
	}
}

void
synchronize(void)
{
	/*
	 * synchronize input and output by sending "!!!" and looking for
	 * "###"
	 */
	puts("!!!");

	for (;;) {
		if (uart_getbyte() != '#')
			continue;

		if (uart_getbyte() != '#')
			continue;

		if (uart_getbyte() != '#')
			continue;

		break;
	}
}
