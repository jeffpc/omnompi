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

	val = ((raw & 0xff) << 8) | ((raw & 0xff00) >> 8);

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
