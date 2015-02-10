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
#include <sys/mman.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <openssl/sha.h>
#include <pthread.h>

#include "proto.h"
#include "lz4.h"
#include "atag.h"

#define	KERNEL_ADDR	0x00008000ul
#define INITRD_ADDR	0x00800000ul
#define RANDOM_ADDR	~0ul

#define MXINIT(x)	ASSERT(pthread_mutex_init((x), NULL) == 0)
#define MXDESTROY(x)	ASSERT(pthread_mutex_destroy(x) == 0)
#define MXLOCK(x)	ASSERT(pthread_mutex_lock(x) == 0)
#define MXUNLOCK(x)	ASSERT(pthread_mutex_unlock(x) == 0)

static void get_tgt_addr(uint32_t *addr, uint32_t *len);

uint32_t raw_bytes;
uint32_t compressed_bytes;
uint32_t zeroed_bytes;
uint32_t copied_bytes;

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

	fprintf(stderr, "Allocating %#010x.%x\n", addr, len);

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
	uint32_t tgt_addr, tgt_len;
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
	get_tgt_addr(&tgt_addr, &tgt_len);
	alloc_addr(tgt_addr, P2ROUNDUP(tgt_len, ARM_PAGESIZE));

	/*
	 * "allocate" the first 32kB because that's where we end up with
	 * ATAGs
	 */
	alloc_addr(0, 0x8000);
}

/*
 * Keeps track of duplicate blocks
 */
static avl_tree_t ddt;
static pthread_mutex_t ddtlock;

struct deduprec {
	avl_node_t tree;
	uint32_t addr;
	uint32_t len;
	uint8_t cksum[SHA_DIGEST_LENGTH];
};

static int dedupcmp(const void *va, const void *vb)
{
	const struct deduprec *a = va;
	const struct deduprec *b = vb;
	int ret;

	ret = memcmp(a->cksum, b->cksum, sizeof(a->cksum));

	if (ret < 0)
		return -1;
	if (ret > 0)
		return 1;
	return 0;
}

static void dedupinit()
{
	MXINIT(&ddtlock);
	avl_create(&ddt, dedupcmp, sizeof(struct deduprec),
		   offsetof(struct deduprec, tree));
}

static void dedupclear()
{
	void *cookie;
	struct deduprec *node;

	fprintf(stderr, "about to remove %lu ddt entries\n",
		avl_numnodes(&ddt));

	MXLOCK(&ddtlock);

	cookie = NULL;
	while ((node = avl_destroy_nodes(&ddt, &cookie)))
		free(node);
	avl_destroy(&ddt);

	MXUNLOCK(&ddtlock);
	MXDESTROY(&ddtlock);

	dedupinit();
}

static struct deduprec *dedupfind(uint8_t *buf, uint32_t len)
{
	struct deduprec key;
	struct deduprec *ret;

	SHA1(buf, len, key.cksum);

	MXLOCK(&ddtlock);
	ret = avl_find(&ddt, &key, NULL);
	MXUNLOCK(&ddtlock);

	return ret;
}

static void dedupadd(uint8_t *buf, uint32_t addr, uint32_t len)
{
	struct deduprec *v;
	avl_index_t where;

	v = malloc(sizeof(struct deduprec));
	ASSERT(v);

	v->addr = addr;
	v->len  = len;
	SHA1(buf, len, v->cksum);

	MXLOCK(&ddtlock);
	if (avl_find(&ddt, v, &where))
		free(v);
	else
		avl_insert(&ddt, v, where);
	MXUNLOCK(&ddtlock);
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

	zeroed_bytes += len;
}

static void __mem_copy(uint32_t dst, uint32_t src, uint32_t len)
{
	struct xfermemcpy xfer;
	enum cmd cmd;

	/* cool, we can just send a mem-copying command */
	fprintf(stderr, "  copy   of %u bytes at %#010x from %#010x...",
		len, dst, src);

	cmd = htonl(CMD_MEM_COPY);
	xfer.dst = htonl(dst);
	xfer.src = htonl(src);
	xfer.len = htonl(len);

	fullwrite(1, &cmd, sizeof(cmd));
	fullwrite(1, &xfer, sizeof(xfer));

	checkstatus();

	copied_bytes += len;
}

static void __mem_write(uint8_t *buf, uint32_t addr, uint32_t len)
{
	uint8_t tmp[XFER_SIZE];
	struct xfermem xfer;
	bool compress;
	uint32_t clen;
	enum cmd cmd;

	clen = lz4_compress(buf, tmp, len, XFER_SIZE, 0);

	compress = (clen >= len) ? false : true;

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
	struct deduprec *ddr;
	int i;

	for (i = 0; i < len; i++) {
		if (buf[i]) {
			ddr = dedupfind(buf, len);
			if (!ddr || (ddr->addr > (addr - XFER_SIZE))) {
				__mem_write(buf, addr, len);
			} else {
				ASSERT(ddr->len == len);
				__mem_copy(addr, ddr->addr, len);
			}
			return;
		}
	}

	__mem_clear(addr, len);
}

static void get_tgt_addr(uint32_t *addr, uint32_t *len)
{
	struct xfermem xfer;
	enum cmd cmd;

	fprintf(stderr, "Requesting target address map...");

	cmd = htonl(CMD_TGT_ADDR);

	fullwrite(1, &cmd, sizeof(cmd));

	checkstatus();

	fprintf(stderr, "Reading header...");

	fullread(0, &xfer, sizeof(xfer));

	checkstatus();

	ASSERT(ntohl(xfer.comp) == 0);

	*addr = ntohl(xfer.addr);
	*len  = ntohl(xfer.len);
}

#define NHELPERS	7

struct helperargs {
	uint8_t *buf;
	uint32_t len;
	uint32_t addr;
	uint32_t offset;
};

static void *populateddt(void *p)
{
	struct helperargs *args = p;
	uint32_t off;

	for (off = args->offset; off + XFER_SIZE < args->len; off += NHELPERS)
		dedupadd(args->buf + off, args->addr + off, XFER_SIZE);

	free(args);

	return NULL;
}

static void upload(const char *fname, uint32_t addr, uint32_t *raddr, uint32_t *rlen)
{
	pthread_t helpers[NHELPERS];
	uint8_t *mappedfile;
	struct stat statinfo;
	uint32_t len;
	uint32_t off;
	int ret;
	int fd;
	int i;

	ASSERT(avl_numnodes(&ddt) == 0);

	fd = open(fname, O_RDONLY);
	ASSERT(fd >= 0);

	ret = fstat(fd, &statinfo);
	ASSERT(ret == 0);

	mappedfile = (void *)mmap(NULL, statinfo.st_size, PROT_READ, MAP_SHARED, fd, 0);
	ASSERT(mappedfile != MAP_FAILED);

	len = statinfo.st_size;

	addr = alloc_addr(addr, len);

	if (raddr)
		*raddr = addr;
	if (rlen)
		*rlen = len;

	fprintf(stderr, "%s @ %#010x is %u bytes\n", fname, addr, len);

	/*
	 * prepare dedup table
	 */
	for (i = 0; i < NHELPERS; i++) {
		struct helperargs *args;

		args = malloc(sizeof(struct helperargs));
		ASSERT(args);

		args->buf = mappedfile;
		args->len = len;
		args->addr = addr;
		args->offset = i;

		ret = pthread_create(&helpers[i], NULL, populateddt, args);
		ASSERT(ret == 0);
	}

	/*
	 * upload the file
	 */
	for (off = 0; off < len; off += XFER_SIZE)
		mem_write(mappedfile + off, addr + off,
			  MIN(XFER_SIZE, len - off));

	/*
	 * wait for helpers to terminate
	 */
	for (i = 0; i < NHELPERS; i++) {
		ret = pthread_join(helpers[i], NULL);
		ASSERT(ret == 0);
	}

	munmap((void *)mappedfile, statinfo.st_size);
	close(fd);

	dedupclear();
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
	uint32_t skipped_bytes;
	uint32_t total_bytes;
	uint32_t initrd_addr, initrd_len;

	if (argc != 3)
		usage(argv[0]);

	dedupinit();
	setup_ramranges();

	upload(argv[1], KERNEL_ADDR, NULL, NULL);
	upload(argv[2], INITRD_ADDR, &initrd_addr, &initrd_len);

	tweak_atags(initrd_addr, initrd_len);

	done(KERNEL_ADDR);

	skipped_bytes = zeroed_bytes + copied_bytes;
	total_bytes = skipped_bytes + raw_bytes;

#define P(x)	((x) / (float) total_bytes)

	fprintf(stderr, "total bytes      %8u\n", total_bytes);
	fprintf(stderr, "zeroed bytes     %8u %f\n", zeroed_bytes, P(zeroed_bytes));
	fprintf(stderr, "copied bytes     %8u %f\n", copied_bytes, P(copied_bytes));
	fprintf(stderr, "raw bytes        %8u %f\n", raw_bytes, P(raw_bytes));
	fprintf(stderr, "compressed bytes %8u %f\n", compressed_bytes, P(compressed_bytes));

	return 0;
}
