#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/types.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/debug.h>

#define BSWAP16(x)	(((x & 0xff) << 8) | ((x & 0xff00) >> 8));

void usage(const char *prog)
{
	fprintf(stderr, "%s <dev>\n", prog);
	exit(1);
}

static void dump(int fd)
{
	int lt = 0;

	for (;;) {
		uint8_t byte;
		int ret;

		ret = read(fd, &byte, 1);
		if (ret < 0)
			break;

		write(STDOUT_FILENO, &byte, 1);

		if (byte == '<')
			lt++;
		else if (byte == '>' && lt)
			break;
		else
			lt = 0;
	}
}

static void fullread(int fd, void *buf, uint32_t len)
{
	uint8_t *ptr = buf;
	ssize_t ret;
	size_t x;

	x = 0;
	while (x != len) {
		ret = read(fd, &ptr[x], len - x);
		if (!ret)
			ASSERT(0);
		if (ret == -1)
			ASSERT(0);

		x += ret;
	}
}

static void fullwrite(int fd, void *buf, uint32_t len)
{
	uint8_t *ptr = buf;
	ssize_t ret;
	size_t x;

	x = 0;
	while (x != len) {
		ret = write(fd, &ptr[x], len - x);
		if (!ret)
			ASSERT(0);
		if (ret == -1)
			ASSERT(0);

		x += ret;
	}
}

static void
hex(uint16_t v, char *buf)
{
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
}

int process(int fd, uint16_t sent)
{
	char recv_char[5];
	char sent_char[5];
	uint16_t recv_int;
	uint16_t swapped;

	memset(recv_char, 0, sizeof(recv_char));
	memset(sent_char, 0, sizeof(sent_char));

	swapped = BSWAP16(sent);
	fullwrite(fd, &swapped, 2);

	fullread(fd, recv_char, 4);

	fullread(fd, &recv_int, 2);
	swapped = BSWAP16(recv_int);

	if (swapped != sent) {
		fprintf(stderr, "Sent '%04x' received '%04x' (int)\n", sent,
			swapped);
		exit(1);
	}

	hex(sent, sent_char);
	if (strcmp(sent_char, recv_char)) {
		fprintf(stderr, "Sent '%s' received '%s' (str)\n", sent_char,
		       recv_char);
		exit(1);
	}

	return 0;
}

int main(int argc, char **argv)
{
	struct termios termios;
	uint16_t i;
	int dev;

	if (argc < 2)
		usage(argv[0]);

	dev = open(argv[1], O_RDWR);
	if (dev < 0)
		usage(argv[0]);

	if (tcgetattr(dev, &termios) == -1)
		usage(argv[0]);

	termios.c_cc[VTIME] = 0;
	termios.c_cc[VMIN] = 1;
	termios.c_iflag = 0;
	termios.c_oflag = 0;
	termios.c_cflag = CS8 | CREAD | CLOCAL;
	termios.c_lflag = 0;

	if ((cfsetispeed(&termios, B115200) < 0) ||
	    (cfsetospeed(&termios, B115200) < 0))
		usage(argv[0]);

	if (tcsetattr(dev, TCSAFLUSH, &termios) == -1)
		usage(argv[0]);

	dump(dev);

	for (i = 0; i <= 0xffff; i++)
		process(dev, i);

	return 0;
}

