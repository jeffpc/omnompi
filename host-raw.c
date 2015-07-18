#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>

#include "host-xfer.h"

void usage(const char *prog)
{
	fprintf(stderr, "%s <dev> <stage2> [<kernel> <initrd>...]\n", prog);
	exit(1);
}

static void dump(int fd, int triplebang)
{
	int bangs = 0;

	for (;;) {
		uint8_t byte;
		int ret;

		ret = read(fd, &byte, 1);
		if (ret < 0)
			break;

		write(STDOUT_FILENO, &byte, 1);

		if (byte == '!')
			bangs++;
		else
			bangs = 0;

		if (bangs == 3 && triplebang)
			break;
	}
}

int main(int argc, char **argv)
{
	struct termios termios;
	int dev;

	if (argc < 3)
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

	dump(dev, 1);

	fprintf(stderr, "Commencing stage2 transfer...\n");

	host_xfer_stage2(dev, dev, argc, argv, 2);

	dump(dev, 1);

	fprintf(stderr, "Commencing transfer...\n");

	host_xfer(dev, dev, argc, argv, 2);

	fprintf(stderr, "!\n");
	fprintf(stderr, "dumping everything from device:\n\n");

	dump(dev, 0);

	close(dev);

	return 0;
}
