#ifndef __PROTO_H
#define __PROTO_H

#include <stdint.h>

#define ARM_PAGESIZE	4096

#define XFER_SIZE	(16 * 1024)

enum cmd {
	CMD_MEM_CLEAR = 1,
	CMD_MEM_WRITE,
	CMD_MEM_READ,
	CMD_MEM_COPY,
	CMD_TGT_ADDR,
	CMD_REG_WRITE,
	CMD_DONE,
};

struct xfermem {
	uint32_t addr;
	uint32_t len;
	uint32_t comp;
};

struct xfermemcpy {
	uint32_t dst;
	uint32_t src;
	uint32_t len;
};

struct xferreg {
	uint32_t reg;
	uint32_t val;
};

#endif
