#! /usr/bin/make -f
SHELL=/bin/bash
.DELETE_ON_ERROR:
.PHONY: all easy clean common lib install

# The map server is slow to build and link.
# Put it first for the illusion of better parallel builds.
all: obj/map/map easy
# Convenience target to rebuild stuff unlikely to break
# after I change something in src/lib or src/common
# (Note also that char server is slower to build than the others)
easy: obj/char/char obj/login/login obj/admin/admin

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
	rm -rf obj/

install:
	install obj/login/login ${ROOT}/${PREFIX_BIN}/tmwa-login
	install obj/char/char   ${ROOT}/${PREFIX_BIN}/tmwa-char
	install obj/map/map     ${ROOT}/${PREFIX_BIN}/tmwa-map
	install obj/admin/admin ${ROOT}/${PREFIX_BIN}/tmwa-admin

warnings:: warnings.commented
	grep -v '^#' $< > $@
