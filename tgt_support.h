#ifndef __TGT_SUPPORT_H
#define __TGT_SUPPORT_H

extern uint32_t arm_reg_read(uint32_t);
extern void arm_reg_write(uint32_t, uint32_t);
extern void execute(uint32_t *regs, uint32_t pc);

extern void memcpy(void *dst, void *src, uint32_t len);
extern void memset(void *dst, int val, uint32_t len);

#endif
