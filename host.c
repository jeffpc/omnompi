#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "host-xfer.h"

void usage(const char *prog)
{
	fprintf(stderr, "%s <stage2> [<kernel> <initrd>...]\n", prog);
	exit(1);
}

int main(int argc, char **argv)
{
	host_xfer_stage2(STDIN_FILENO, STDOUT_FILENO, argc, argv, 1);
	host_xfer(STDIN_FILENO, STDOUT_FILENO, argc, argv, 1);

	return 0;
}
