CC=gcc
ARMCC=/opt/armtc/usr/bin/arm-pc-solaris2.11-gcc
ARMAS=/opt/armtc/usr/gnu/bin/gas
ARMLD=/opt/armtc/usr/bin/ld
ARMOBJCOPY=/opt/armtc/usr/gnu/bin/gobjcopy

#MODEL=-DPI_1_B_PLUS
MODEL=-DPI_2_B

CFLAGS=-Wall -O2 -g -DDEBUG $(MODEL)
ARMCFLAGS=-ffreestanding -DTGT
VERDEF=-DVERSION='"$(shell git describe --tags --dirty)"'

all: host tgt

clean:
	rm -f host tgt

host: host.c lz4.c atag.c
	$(CC) $(CFLAGS) $(VERDEF) -lumem -lcrypto -lpthread -o $@ $^ /usr/lib/libavl.so.1

tgt: tgt.c tgt_start.s uart.c tgt_support.c
	$(ARMCC) -x assembler-with-cpp -c -o tgt_start.o tgt_start.s
	$(ARMCC) $(CFLAGS) $(ARMCFLAGS) $(VERDEF) -c -o tgt.o tgt.c
	$(ARMCC) $(CFLAGS) $(ARMCFLAGS) -c -o atag.o atag.c
	$(ARMCC) $(CFLAGS) $(ARMCFLAGS) -c -o uart.o uart.c
	$(ARMCC) $(CFLAGS) $(ARMCFLAGS) -c -o tgt_support.o tgt_support.c
	$(ARMCC) $(CFLAGS) $(ARMCFLAGS) -c -o lz4.o lz4.c
	$(ARMLD) -dy -b -znointerp -o tgt.elf -e _start -M mapfile \
		tgt_start.o \
		tgt.o \
		uart.o \
		tgt_support.o \
		lz4.o \
		atag.o
	$(ARMOBJCOPY) tgt.elf -O binary $@
