SUBDIRS=\
	omnom/host \
	omnom/tgt \
	parrot/tgt

all:
	for x in $(SUBDIRS) ; do gmake -C $$x all ; done

clean:
	for x in $(SUBDIRS) ; do gmake -C $$x clean ; done
