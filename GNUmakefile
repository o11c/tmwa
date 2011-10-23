#! /usr/bin/make -f
SHELL=/bin/bash
.DELETE_ON_ERROR:
.PHONY: all easy clean tmwa-common tmwa-lib install

# The map server is slow to build and link.
# Put it first for the illusion of better parallel builds.
all: tmwa-map easy
# Convenience target to rebuild stuff unlikely to break
# after I change something in src/lib or src/common
# (Note also that char server is slower to build than the others)
easy: tmwa-char tmwa-login tmwa-admin tmwa-monitor

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
	rm -rf obj/ tmwa-*

install:
	install tmwa-login      ${ROOT}/${PREFIX_BIN}/tmwa-login
	install tmwa-char       ${ROOT}/${PREFIX_BIN}/tmwa-char
	install tmwa-map        ${ROOT}/${PREFIX_BIN}/tmwa-map
	install tmwa-admin      ${ROOT}/${PREFIX_BIN}/tmwa-admin
# Awaiting conversion and generification
#	install tmwa-monitor    ${ROOT}/${PREFIX_BIN}/tmwa-monitor

warnings:: warnings.commented
	grep -v '^#' $< > $@
