#! /usr/bin/make -f
SHELL=/bin/bash
.DELETE_ON_ERROR:
.PHONY: default all clean common lib

PROGS = tmwa-login tmwa-char tmwa-map tmwa-admin
all: ${PROGS}

include make/suffixes.make
include make/defaults.make
include make/required.make
include make/deps.make
include make/overrides.make
# These probably don't work anyway
#include make/legacy.make

tags: src/*/
	ctags -R src/

clean:
	rm -rf ${PROGS} obj/

tmwa-login: obj/login/login
	cp -f $< $@
tmwa-char: obj/char/char
	cp -f $< $@
tmwa-map: obj/map/map
	cp -f $< $@
tmwa-admin: obj/admin/admin
	cp -f $< $@

install: ${PROGS}
	install -t ${PREFIX_BIN} ${PROGS}

warnings:: warnings.commented
	grep -v '^#' $< > $@
