include ../../Makefile.master

LIBSV6=../../libuart/libuart-bcm2835.a
LIBSV7=../../libuart/libuart-bcm2836.a

all:
	VER=6 gmake tgt
	VER=7 gmake tgt

clean:
	rm -f tgt-?{,.elf} *.o
	cd ../../libuart; gmake clean

tgt: $(LIBSV$(VER)) tgt-$(VER)
	@echo > /dev/null

../../libuart/libuart-%.a:
	cd ../../libuart; CC=$(ARMCC) gmake

%-$(VER).o: %.s
	$(ARMCC) $(CFLAGS) $(ARMCFLAGS) $(CFLAGSV$(VER)) \
		-x assembler-with-cpp -c -o $@ $<

%-$(VER).o: %.c
	$(ARMCC) $(CFLAGS) $(ARMCFLAGS) $(CFLAGSV$(VER)) \
		$(VERDEF) -c -o $@ $<

tgt-%.elf: tgt-%.o tgt_start-%.o $(LIBSV$(VER))
	$(ARMLD) -dy -b -znointerp -o $@ -e _start -M mapfile $^

tgt-%: tgt-%.elf
	$(ARMOBJCOPY) $< -O binary $@

.KEEP: tgt-6.elf tgt-7.elf
