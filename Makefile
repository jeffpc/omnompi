CC=gcc
ARMCC=/opt/armtc/usr/bin/arm-pc-solaris2.11-gcc
ARMAS=/opt/armtc/usr/gnu/bin/gas
ARMLD=/opt/armtc/usr/bin/ld
ARMOBJCOPY=/opt/armtc/usr/gnu/bin/gobjcopy

CFLAGS=-Wall -O2 -g -DDEBUG
ARMCFLAGS=-ffreestanding -DTGT -fno-strict-aliasing
CFLAGSV6=
CFLAGSV7=-march=armv7-a
VERDEF=-DVERSION='"$(shell git describe --tags --dirty)"'

LIBSV6=libuart/libuart-bcm2835.a
LIBSV7=libuart/libuart-bcm2836.a

all: host host-raw
	VER=6 gmake tgt
	VER=7 gmake tgt

clean:
	rm -f host host-raw tgt-6 tgt-7 *.o
	cd libuart; gmake clean

tgt: $(LIBSV$(VER)) tgt-$(VER)
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

tgt-%.elf: tgt-%.o tgt_start-%.o tgt_support-%.o atag-%.o lz4-%.o $(LIBSV$(VER))
	$(ARMLD) -dy -b -znointerp -o $@ -e _start -M mapfile $^

tgt-%: tgt-%.elf
	$(ARMOBJCOPY) $< -O binary $@

.KEEP: tgt-6.elf tgt-7.elf
