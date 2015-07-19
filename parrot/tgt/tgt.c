#include <stdint.h>

#include "../../libuart/uart.h"

#define NULL	((void *)0)

typedef uint32_t size_t; /* HACK */

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

void puts(const char *str)
{
	const char *c = str;

	while (*c != '\0')
		uart_putc(*c++);
}

static void
hex(uint16_t v)
{
	char buf[4 + 1];
	char c;
	int i;

	buf[4] = '\0';

	for (i = 3; i >= 0; i--) {
		c = v & 0xf;
		v >>= 4;

		if (c > 9)
			c += 'a' - 10;
		else
			c += '0';

		buf[i] = c;
	}

	puts(buf);
}

void read(void *buf, uint32_t len)
{
	uint8_t *ptr = buf;

	while (len) {
		*ptr = uart_getbyte();

		ptr++;
		len--;
	}
}

void write(void *buf, uint32_t len)
{
	uint8_t *ptr = buf;

	while (len) {
		uart_putbyte(*ptr);

		ptr++;
		len--;
	}
}

static void process(void)
{
	uint16_t raw;
	uint16_t val;

	read(&raw, sizeof(raw));

	val = ((raw & 0xff) << 8) | ((raw && 0xff00) >> 8);

	hex(val);
	write(&raw, sizeof(raw));
}

void main(uint32_t r0, uint32_t r1, uint32_t r2)
{
	extern uint8_t _edata;
	extern uint8_t _end;

	memset(&_edata, 0, &_end - &_edata);

	uart_init(0);

	puts("<>");

	for (;;)
		process();
}
