#include "bcm2835_uart.h"
#include "tgt_support.h"
#include "proto.h"
#include "lz4.h"

/* we only actually care about the first 14 regs */
#define NUMREGS		14
uint32_t regs[NUMREGS];

void puts(const char *str)
{
	const char *c = str;

	while (*c != '\0')
		bcm2835_uart_putc(*c++);
}

static void
puthex(uint32_t v)
{
	char buf[2 + 8 + 1];
	char c;
	int i;

	buf[0] = '0';
	buf[1] = 'x';
	buf[10] = '\0';

	for (i = 7; i >= 0; i--) {
		c = v & 0xf;
		v >>= 4;

		if (c > 9)
			c += 'a' - 10;
		else
			c += '0';

		buf[2 + i] = c;
	}

	puts(buf);
}

void read(void *buf, uint32_t len)
{
	uint8_t *ptr = buf;

	while (len) {
		*ptr = bcm2835_uart_getbyte();

		ptr++;
		len--;
	}
}

void write(void *buf, uint32_t len)
{
	uint8_t *ptr = buf;

	while (len) {
		bcm2835_uart_putbyte(*ptr);

		ptr++;
		len--;
	}
}

void send_ack()
{
	write("+", 1);
}

void send_nack()
{
	write("-", 1);
}

enum cmd read_cmd()
{
	enum cmd cmd;

	/* get the command */
	read(&cmd, sizeof(cmd));

	return BSWAP_32(cmd);
}

static void handle_mem_clear()
{
	struct xfermem xfer;

	read(&xfer, sizeof(xfer));

	xfer.addr = BSWAP_32(xfer.addr);
	xfer.len  = BSWAP_32(xfer.len);

	memset((void *)xfer.addr, 0, xfer.len);
}

static void handle_mem_copy()
{
	struct xfermemcpy xfer;

	read(&xfer, sizeof(xfer));

	xfer.dst = BSWAP_32(xfer.dst);
	xfer.src = BSWAP_32(xfer.src);
	xfer.len = BSWAP_32(xfer.len);

	memcpy((void *)xfer.dst, (void *)xfer.src, xfer.len);
}

static void handle_mem_write()
{
	static struct xfermem xfer;

	read(&xfer, sizeof(xfer));

	xfer.addr = BSWAP_32(xfer.addr);
	xfer.len  = BSWAP_32(xfer.len);
	xfer.comp = BSWAP_32(xfer.comp);

	if (!xfer.comp) {
		read((void *)xfer.addr, xfer.len);
	} else {
		static uint8_t tmp[XFER_SIZE];

		read(tmp, xfer.len);

		lz4_decompress(tmp, (void *)xfer.addr, xfer.len, XFER_SIZE, 0);
	}
}

static void handle_mem_read()
{
	static struct xfermem xfer;
	uint32_t addr, len;

	read(&xfer, sizeof(xfer));

	addr = BSWAP_32(xfer.addr);
	len  = BSWAP_32(xfer.len);

	send_ack();

	xfer.addr = BSWAP_32(addr);
	xfer.len  = BSWAP_32(len);
	xfer.comp = 0;

	write(&xfer, sizeof(xfer));
	write((void *)addr, len);
}

static void handle_reg_write()
{
	static struct xferreg buf;

	read(&buf, sizeof(buf));

	buf.reg = BSWAP_32(buf.reg);
	buf.val = BSWAP_32(buf.val);

	if (buf.reg < NUMREGS)
		regs[buf.reg] = buf.val;
}

static void print_regs(uint32_t pc)
{
	puts("r0  = ");
	puthex(regs[0]);
	puts("    r1  = ");
	puthex(regs[1]);
	puts("    r2  = ");
	puthex(regs[2]);
	puts("    r3  = ");
	puthex(regs[3]);
	puts("\n");

	puts("r4  = ");
	puthex(regs[4]);
	puts("    r5  = ");
	puthex(regs[5]);
	puts("    r6  = ");
	puthex(regs[6]);
	puts("    r7  = ");
	puthex(regs[7]);
	puts("\n");

	puts("r8  = ");
	puthex(regs[8]);
	puts("    r9  = ");
	puthex(regs[9]);
	puts("    r10 = ");
	puthex(regs[10]);
	puts("    r11 = ");
	puthex(regs[11]);
	puts("\n");

	puts("r12 = ");
	puthex(regs[12]);
	puts("    r13 = ");
	puthex(regs[13]);
	puts("    r14 = ----------    r15 = ");
	puthex(pc);
	puts("\n");
}

static void handle_done()
{
	uint32_t pc;

	read(&pc, sizeof(pc));

	pc = BSWAP_32(pc);

	send_ack();

	puts("About to execute: ");
	puthex(pc);
	puts("\n");

	print_regs(pc);

	puts("\nPress any key to boot\n");
	bcm2835_uart_getc();
	puts("Bye!\n=================\n");

	execute(regs, pc);
}

void main(uint32_t r0, uint32_t r1, uint32_t r2)
{
	extern uint8_t _edata;
	extern uint8_t _end;

	memset(&_edata, 0, &_end - &_edata);

	regs[0] = r0;
	regs[1] = r1;
	regs[2] = r2;

	bcm2835_uart_init();

	puts("Welcome to OmNom Raspberry Pi loader...\n");
	puts("Ready to receive...\n");

	for (;;) {
		enum cmd cmd;

		switch ((cmd = read_cmd())) {
			case CMD_MEM_CLEAR:
				handle_mem_clear();
				break;
			case CMD_MEM_WRITE:
				handle_mem_write();
				break;
			case CMD_MEM_READ:
				handle_mem_read();
				break;
			case CMD_MEM_COPY:
				handle_mem_copy();
				break;
			case CMD_REG_WRITE:
				handle_reg_write();
				break;
			case CMD_DONE:
				handle_done();
				puts("Uh oh, hand off failed\n");
				goto brick;
			default:
				send_nack();
				puts("Unknown command\n");
				goto brick;
		}

		send_ack();
	}

brick:
	puts("Spinning forever!\n");
	for (;;)
		;
}
