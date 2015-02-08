#ifndef __PROTO_H
#define __PROTO_H

#include <stdint.h>

#define ARM_PAGESIZE	4096

#define XFER_SIZE	4096

enum cmd {
	CMD_MEM_CLEAR = 1,
	CMD_MEM_WRITE,
#if 0
	CMD_MEM_READ,
#endif
	CMD_REG_WRITE,
	CMD_DONE,
};

struct xferclear {
	uint32_t addr;
	uint32_t len;
};

struct xferbuf {
	uint32_t addr;
	uint8_t buf[XFER_SIZE];
};

struct xferreg {
	uint32_t reg;
	uint32_t val;
};

#endif
