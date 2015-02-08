CC=gcc
ARMCC=/opt/armtc/usr/bin/arm-pc-solaris2.11-gcc
ARMAS=/opt/armtc/usr/gnu/bin/gas
ARMLD=/opt/armtc/usr/bin/ld
ARMOBJCOPY=/opt/armtc/usr/gnu/bin/gobjcopy

CFLAGS=-Wall -O2 -g -DDEBUG
ARMCFLAGS=-ffreestanding

all: host tgt

clean:
	rm -f host tgt

host: host.c
	$(CC) $(CFLAGS) -o $@ $< /usr/lib/libavl.so.1

tgt: tgt.c tgt_start.s bcm2835_uart.c tgt_support.c
	$(ARMCC) -x assembler-with-cpp -c -o tgt_start.o tgt_start.s
	$(ARMCC) $(CFLAGS) $(ARMCFLAGS) -c -o tgt.o tgt.c
	$(ARMCC) $(CFLAGS) $(ARMCFLAGS) -c -o bcm2835_uart.o bcm2835_uart.c
	$(ARMCC) $(CFLAGS) $(ARMCFLAGS) -c -o tgt_support.o tgt_support.c
	$(ARMLD) -dy -b -znointerp -o tgt.elf -e _start -M mapfile \
		tgt_start.o \
		tgt.o \
		bcm2835_uart.o \
		tgt_support.o
	$(ARMOBJCOPY) tgt.elf -O binary $@
