#ifndef __TGT_SUPPORT_H
#define __TGT_SUPPORT_H

extern uint32_t arm_reg_read(uint32_t);
extern void arm_reg_write(uint32_t, uint32_t);
extern void execute(uint32_t *regs, uint32_t pc);

extern void memcpy(void *dst, const void *src, uint32_t len);
extern void memset(void *dst, int val, uint32_t len);

extern char * strstr(const char *as1, const char *as2);

#define	BSWAP_32(x)	(((uint32_t)(x) << 24) | \
			(((uint32_t)(x) << 8) & 0xff0000) | \
			(((uint32_t)(x) >> 8) & 0xff00) | \
			((uint32_t)(x)  >> 24))

#define BE_32(x)	BSWAP_32(x)

#define	BE_IN8(xa) \
	*((uint8_t *)(xa))

#define	BE_IN16(xa) \
	(((uint16_t)BE_IN8(xa) << 8) | BE_IN8((uint8_t *)(xa) + 1))

#define	BE_IN32(xa) \
	(((uint32_t)BE_IN16(xa) << 16) | BE_IN16((uint8_t *)(xa) + 2))

#define NULL	((void *)0)

#endif
