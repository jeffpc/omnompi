#ifndef __PROTO_H
#define __PROTO_H

#include <stdint.h>

#define ARM_PAGESIZE	4096

#define XFER_SIZE	(1024 * 1024)

enum cmd {
	CMD_MEM_CLEAR = 1,
	CMD_MEM_WRITE,
	CMD_MEM_READ,
	CMD_REG_WRITE,
	CMD_DONE,
};

struct xfermem {
	uint32_t addr;
	uint32_t len;
	uint32_t comp;
};

struct xferreg {
	uint32_t reg;
	uint32_t val;
};

#endif
