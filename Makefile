CC=gcc
ARMCC=/opt/armtc/usr/bin/arm-pc-solaris2.11-gcc
ARMAS=/opt/armtc/usr/gnu/bin/gas
ARMLD=/opt/armtc/usr/bin/ld
ARMOBJCOPY=/opt/armtc/usr/gnu/bin/gobjcopy

CFLAGS=-Wall -O2 -g -DDEBUG
ARMCFLAGS=-ffreestanding -DTGT -march=armv7-a
CFLAGSV6=
CFLAGSV7=-march=armv7-a
VERDEF=-DVERSION='"$(shell git describe --tags --dirty)"'

LIBSV6=libuart/libuart-bcm2835.a
LIBSV7=libuart/libuart-bcm2836.a

all: host host-raw
	VER=6 gmake tgt
	VER=7 gmake tgt

clean:
	rm -f host host-raw
	rm -f tgt-stage?-? tgt-stage?-?.elf *.o
	cd libuart; gmake clean

tgt: $(LIBSV$(VER)) tgt-stage1-$(VER) tgt-stage2-$(VER)
	@echo > /dev/null

libuart/libuart-%.a:
	cd libuart; CC=$(ARMCC) gmake

host: host.c host-xfer.c lz4.c atag.c
	$(CC) $(CFLAGS) $(VERDEF) -lumem -lcrypto -lpthread -o $@ $^ /usr/lib/libavl.so.1

host-raw: host-raw.c host-xfer.c lz4.c atag.c
	$(CC) $(CFLAGS) $(VERDEF) -lumem -lcrypto -lpthread -o $@ $^ /usr/lib/libavl.so.1

%-$(VER).o: %.s
	$(ARMCC) $(CFLAGS) $(ARMCFLAGS) $(CFLAGSV$(VER)) \
		-x assembler-with-cpp -c -o $@ $<

%-$(VER).o: %.c
	$(ARMCC) $(CFLAGS) $(ARMCFLAGS) $(CFLAGSV$(VER)) \
		$(VERDEF) -c -o $@ $<

tgt-stage1-%.elf: stage1-%.o tgt_start-%.o tgt_support-%.o atag-%.o $(LIBSV$(VER))
	$(ARMLD) -dy -b -znointerp -o $@ -e _start -M mapfile-stage1 $^

tgt-stage2-%.elf: stage2-%.o tgt_start-%.o tgt_support-%.o atag-%.o lz4-%.o $(LIBSV$(VER))
	$(ARMLD) -dy -b -znointerp -o $@ -e _start -M mapfile-stage2 $^

tgt-%: tgt-%.elf
	$(ARMOBJCOPY) $< -O binary $@

.KEEP: \
	tgt-stage1-6.elf tgt-stage1-7.elf \
	tgt-stage2-6.elf tgt-stage2-7.elf
