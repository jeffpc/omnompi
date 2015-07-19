include ../../Makefile.master

all: host host-raw

clean:
	rm -f host host-raw *.o

host: host.c host-xfer.c ../lz4.c ../atag.c
	$(CC) $(CFLAGS) $(VERDEF) -lumem -lcrypto -lpthread -o $@ $^ /usr/lib/libavl.so.1

host-raw: host-raw.c host-xfer.c ../lz4.c ../atag.c
	$(CC) $(CFLAGS) $(VERDEF) -lumem -lcrypto -lpthread -o $@ $^ /usr/lib/libavl.so.1
