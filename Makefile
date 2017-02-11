SRCDIR = ./src
CC = gcc

OUTDIR = ./out
DEPSDIR =./deps
LIBMILLDIR = libmill

FILES = $(SRCDIR)/main.c
DEPS = $(DEPSDIR)/$(LIBMILLDIR)/.libs/libmill.a

main: clean
	mkdir ./out && $(CC) $(FILES) -o $(OUTDIR)/$@ $(DEPS)

.PHONY: test deps deps-clean libmill-clean clean

test: main
	cd $(OUTDIR) && ./main

deps: libmill

deps-clean: libmill-clean

libmill:
	cd $(DEPSDIR)/$(LIBMILLDIR) && ./configure && $(MAKE) && $(MAKE) check

libmill-clean:
	cd $(DEPSDIR)/$(LIBMILLDIR) && $(MAKE) distclean

clean:
	rm -rf $(OUTDIR)
