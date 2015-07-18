#include <stdint.h>

#include "tgt_support.h"
#include "proto.h"
#include "lz4.h"
#include "atag.h"
#include "libuart/uart.h"

/* we only actually care about the first 14 regs */
#define NUMREGS		14
uint32_t regs[NUMREGS];

static char *
get_atag_cmdline(atag_header_t *chain)
{
	static char buf[512];
	atag_cmdline_t *cmd;
	uint32_t *tmp;
	uint32_t len;

	/* double check that we have a ATAG (and not a device tree) */
	tmp = (uint32_t *)chain;
	if (tmp[1] != 0x54410001)
		return NULL;

	cmd = (atag_cmdline_t *)atag_find(chain, ATAG_CMDLINE);
	if (!cmd)
		return NULL;

	len = cmd->al_header.ah_size * sizeof(uint32_t);
	if (len >= sizeof(buf))
		return NULL; /* sadly, we can't print anything at this point */

	memcpy(buf, cmd->al_cmdline, len);
	buf[len] = '\0';

	return buf;
}

static uint32_t
uart_get_clk(const char *cmdline)
{
	uint32_t clk;
	char *tmp;

	if (!cmdline)
		return 0;

	tmp = strstr(cmdline, uart_clock_string);
	if (!tmp)
		return 0;

	tmp += strlen(uart_clock_string);

	clk = 0;
	while ((*tmp >= '0') && (*tmp <= '9')) {
		clk = (clk * 10) + (*tmp - '0');
		tmp++;
	}

	return clk;
}

static uint32_t getprog(void)
{
	uint32_t addr;
	uint32_t len;

	read(&addr, sizeof(addr));
	read(&len, sizeof(len));

	puts("Reading @ ");
	puthex(addr);
	puts(".");
	puthex(len);
	puts(" BE\n");

	addr = BSWAP_32(addr);
	len  = BSWAP_32(len);

	puts("Reading @ ");
	puthex(addr);
	puts(".");
	puthex(len);
	puts("\n");

	read((void *)addr, len);

	return addr;
}

void main(uint32_t r0, uint32_t r1, uint32_t r2)
{
	char *cmdline;
	extern uint8_t _edata;
	extern uint8_t _end;

	memset(&_edata, 0, &_end - &_edata);

	regs[0] = r0;
	regs[1] = r1;
	regs[2] = r2;

	cmdline = get_atag_cmdline((atag_header_t *)r2);
	uart_init(uart_get_clk(cmdline));

	puts("\n\nWelcome to OmNom ");
	puts(platform_name);
	puts(" stage 1 loader " VERSION "...\n");
	puts("(using libuart version ");
	puts(libuart_version);
	puts(")\n\n");

	if (cmdline) {
		puts("\nATAG_CMDLINE:\n");
		puts(cmdline);
		puts("\n\n");
	}

	puts("Ready to receive...\n");

	synchronize();

	execute(regs, getprog());

	puts("Uh oh... failed to leave stage 1.\nSpinning forever!\n");
	for (;;)
		;
}
