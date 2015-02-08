#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/avl.h>
#include <sys/debug.h>
#include <sys/sysmacros.h>
#include <inttypes.h>
#include <netinet/in.h>

#include "proto.h"
#include "lz4.h"
#include "atag.h"

#define	KERNEL_ADDR	0x8000ul
#define RANDOM_ADDR	~0ul

#define TGT_ADDR	(200 * 1024 * 1024)
#define TGT_LEN		(16 * 1024)

uint32_t raw_bytes;
uint32_t compressed_bytes;
uint32_t skipped_bytes;

/*
 * Keeps track of free ranges of RAM
 */
static avl_tree_t rr;

struct ramrange {
	avl_node_t tree;
	uint32_t addr;
	uint32_t len;
};

static int rrcmp(const void *va, const void *vb)
{
	const struct ramrange *a = va;
	const struct ramrange *b = vb;
	const uint32_t astart = a->addr;
	const uint32_t aend   = a->addr + a->len;
	const uint32_t bstart = b->addr;
	const uint32_t bend   = b->addr + b->len;

	if (aend <= bstart)
		return -1;
	if (bend <= astart)
		return 1;
	return 0;
}

static uint32_t select_random_addr(uint32_t len)
{
	struct ramrange *r;
	uint32_t addr;

	ASSERT((len & (ARM_PAGESIZE - 1)) == 0);

	for (r = avl_first(&rr); r; r = AVL_NEXT(&rr, r)) {
		if (r->len < len)
			continue;

		addr = r->addr;

		if (r->len == len) {
			avl_remove(&rr, r);
			free(r);
		} else {
			r->addr += len;
			r->len  -= len;
		}

		return addr;
	}

	ASSERT(0);
	return ~0ul;
}

static uint32_t alloc_addr(uint32_t addr, uint32_t len)
{
	struct ramrange key;
	struct ramrange *r;
	uint32_t rstart, rend;
	uint32_t start, end;

	len = P2ROUNDUP(len, ARM_PAGESIZE);

	if (addr == RANDOM_ADDR)
		return select_random_addr(len);

	ASSERT((addr & (ARM_PAGESIZE - 1)) == 0);

	key.addr = addr;
	key.len  = len;

	r = avl_find(&rr, &key, NULL);
	if (!r)
		goto overlap;

	/*
	 * ok, we have an overlap, but that doesn't mean that we're safe,
	 * the returned interval could me smaller than we want or not cover
	 * the whole range
	 */
	rstart = r->addr;
	rend   = r->addr + r->len;
	start  = addr;
	end    = addr + len;

	/* check for not entirely covered case */
	if ((rstart > start) || (rend < end))
		goto overlap;

	/*
	 * now, depending on the range we got, we need to treat it a bit
	 * differently
	 */
	if ((r->addr == addr) && (r->len == len)) {
		/* exact match */
		avl_remove(&rr, r);
		free(r);
	} else if (r->addr == addr) {
		/* exact start */
		r->addr += len;
		r->len  -= len;
	} else if ((r->addr + r->len) == (addr + len)) {
		/* exact end */
		r->len -= len;
	} else {
		/* split */
		struct ramrange *r2;

		r2 = malloc(sizeof(struct ramrange));
		ASSERT(r2);

		/*
		 *       addr
		 *        v
		 * +------+---------+--------+
		 * |  r   | alloc'd |   r2   |
		 * +------+---------+--------+
		 *
		 */

		r2->addr = addr + len;
		r2->len  = r->len - (r2->addr - r->addr);

		r->len   = addr - r->addr;

		avl_add(&rr, r2);
	}

	return addr;

overlap:
	fprintf(stderr, "Overlap detected when allocating %#010x.%x\n",
		addr, len);
	ASSERT(0);
	return ~0ul;
}

static void setup_ramranges()
{
	struct ramrange *r;

	avl_create(&rr, rrcmp, sizeof(struct ramrange),
		   offsetof(struct ramrange, tree));

	r = malloc(sizeof(struct ramrange));
	ASSERT(r);

	r->addr = 0ul;
	r->len  = ~0ul;
	avl_add(&rr, r);

	/*
	 * "allocate" what our target binary uses so we don't accidentally
	 * overwrite ourselves
	 */
	alloc_addr(TGT_ADDR, TGT_LEN);

	/*
	 * "allocate" the first 32kB because that's where we end up with
	 * ATAGs
	 */
	alloc_addr(0, 0x8000);
}

static void usage(const char *prog)
{
	fprintf(stderr, "%s <kernel> <initrd>\n", prog);
	exit(1);
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

static void checkstatus()
{
	char resp;

	read(0, &resp, 1);

	fprintf(stderr, "%s (%c)\n", (resp == '+') ? "ok" : "failed", resp);
	ASSERT(resp == '+');
}

static void __mem_clear(uint32_t addr, uint32_t len)
{
	struct xfermem xfer;
	enum cmd cmd;

	/* cool, we can just send a mem-zeroing command */
	fprintf(stderr, "  clear  of %u bytes at %#010x...", len, addr);

	cmd = htonl(CMD_MEM_CLEAR);
	xfer.addr = htonl(addr);
	xfer.len  = htonl(len);
	xfer.comp = 0;

	fullwrite(1, &cmd, sizeof(cmd));
	fullwrite(1, &xfer, sizeof(xfer));

	checkstatus();

	skipped_bytes += len;
}

static void __mem_write(uint8_t *buf, uint32_t addr, uint32_t len)
{
	uint8_t tmp[XFER_SIZE];
	struct xfermem xfer;
	bool compress;
	uint32_t clen;
	enum cmd cmd;

	clen = lz4_compress(buf, tmp, len, XFER_SIZE, 0);

	compress = (clen > len) ? false : true;

	fprintf(stderr, "  doxfer of %u bytes to %#010x as %u bytes (%c)...",
		len, addr, clen, compress ? 'C' : 'U');

	cmd = htonl(CMD_MEM_WRITE);
	xfer.addr = htonl(addr);
	xfer.len  = htonl(compress ? clen : len);
	xfer.comp = htonl(compress ? 1 : 0);

	fullwrite(1, &cmd, sizeof(cmd));
	fullwrite(1, &xfer, sizeof(xfer));
	fullwrite(1, compress ? tmp : buf, compress ? clen : len);

	checkstatus();

	compressed_bytes += compress ? clen : len;
	raw_bytes += len;
}

static void mem_write(uint8_t *buf, uint32_t addr, uint32_t len)
{
	int i;

	for (i = 0; i < len; i++) {
		if (buf[i]) {
			__mem_write(buf, addr, len);
			return;
		}
	}

	__mem_clear(addr, len);
}

static void xfer(int fd, uint32_t addr, uint32_t off, uint32_t len)
{
	uint8_t buf[XFER_SIZE];
	ssize_t ret;
	size_t x;

	memset(buf, 0, sizeof(buf));

	x = 0;
	while (x != len) {
		ret = pread(fd, &buf[x], len - x, off + x);
		if (!ret)
			ASSERT(0);
		if (ret == -1)
			ASSERT(0);

		x += ret;
	}

	mem_write(buf, addr, len);
}

static void upload(const char *fname, uint32_t addr, uint32_t *raddr, uint32_t *rlen)
{
	struct stat statinfo;
	uint32_t len;
	uint32_t off;
	int ret;
	int fd;

	fd = open(fname, O_RDONLY);
	ASSERT(fd >= 0);

	ret = fstat(fd, &statinfo);
	ASSERT(ret == 0);

	len = statinfo.st_size;

	addr = alloc_addr(addr, len);

	if (raddr)
		*raddr = addr;
	if (rlen)
		*rlen = len;

	fprintf(stderr, "%s @ %#010x is %u bytes\n", fname, addr, len);

	for (off = 0; off < len; addr += XFER_SIZE, off += XFER_SIZE)
		xfer(fd, addr, off,
		     ((len - off) >= XFER_SIZE) ? XFER_SIZE : (len - off));

	close(fd);
}

static uint32_t read_mem(uint8_t *buf, uint32_t addr, uint32_t len)
{
	struct xfermem xfer;
	enum cmd cmd;

	fprintf(stderr, "Requesting %#010x.%#010x...", addr, len);

	cmd = htonl(CMD_MEM_READ);
	xfer.addr = htonl(addr);
	xfer.len  = htonl(len);
	xfer.comp = 0;

	fullwrite(1, &cmd, sizeof(cmd));
	fullwrite(1, &xfer, sizeof(xfer));

	checkstatus();

	fprintf(stderr, "Reading header...\n");

	fullread(0, &xfer, sizeof(xfer));

	xfer.addr = ntohl(xfer.addr);
	xfer.len  = ntohl(xfer.len);
	xfer.comp = ntohl(xfer.comp);

	ASSERT(xfer.comp == 0);
	ASSERT(xfer.len <= len);

	fprintf(stderr, "Reading data for %#010x.%#010x...", xfer.addr,
		xfer.len);

	fullread(0, buf, xfer.len);

	checkstatus();

	return xfer.len;
}

static void tweak_atags(uint32_t initrd_addr, uint32_t initrd_len)
{
	atag_initrd_t atag;
	uint8_t buf[0x8000];
	uint32_t len;

	fprintf(stderr, "Setting ATAG_INITRD2...\n");

	len = read_mem(buf, 0, sizeof(buf));
	ASSERT(len == sizeof(buf));

	fprintf(stderr, "Updating ATAGs...\n");
	atag.ai_header.ah_tag = ATAG_INITRD2;
	atag.ai_header.ah_size = 0x4;
	atag.ai_start = initrd_addr;
	atag.ai_size  = initrd_len;

	atag_append((atag_header_t *)&buf[0x100], &atag.ai_header);

	fprintf(stderr, "Writing out updated first %dkB\n", sizeof(buf) / 1024);
	mem_write(buf, 0, sizeof(buf));
}

static void done(uint32_t pc)
{
	enum cmd cmd;

	fprintf(stderr, "Done.  Setting PC = %#010x...", pc);

	cmd = htonl(CMD_DONE);
	pc = htonl(pc);

	fullwrite(1, &cmd, sizeof(cmd));
	fullwrite(1, &pc, sizeof(pc));

	checkstatus();
}

int main(int argc, char **argv)
{
	uint32_t initrd_addr, initrd_len;

	if (argc != 3)
		usage(argv[0]);

	setup_ramranges();

	upload(argv[1], KERNEL_ADDR, NULL, NULL);
	upload(argv[2], RANDOM_ADDR, &initrd_addr, &initrd_len);

	tweak_atags(initrd_addr, initrd_len);

	done(KERNEL_ADDR);

	fprintf(stderr, "raw bytes        %u\n", raw_bytes);
	fprintf(stderr, "skipped bytes    %u\n", skipped_bytes);
	fprintf(stderr, "compressed bytes %u\n", compressed_bytes);
	fprintf(stderr, "ratio            %f\n", compressed_bytes / (float) raw_bytes);
	fprintf(stderr, "total ratio      %f\n", compressed_bytes / (float) (skipped_bytes + raw_bytes));

	return 0;
}
