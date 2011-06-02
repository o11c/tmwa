#! /usr/bin/make -f
SHELL=/bin/bash
default: login-server char-server map-server ladmin eathena-monitor
.DELETE_ON_ERROR:
include make.defs

.PHONY: default all clean common
.PRECIOUS: %/
%/:
	+mkdir -p $@

# The default recipe is suboptimal
%.cpp: %.lpp
	$(LEX) -o $@ $<
%.cpp %.hpp: %.ypp
	$(BISON) -d -o $*.cpp $<

tags:
	ctags -R src/

# All this duplication is required because make handles pattern rules specially
obj/char/%.o: src/char/%.cpp | obj/char/
	$(COMPILE.cpp) -o $@ $<
obj/common/%.o: src/common/%.cpp | obj/common/
	$(COMPILE.cpp) -o $@ $<
obj/ladmin/%.o: src/ladmin/%.cpp | obj/ladmin/
	$(COMPILE.cpp) -o $@ $<
obj/login/%.o: src/login/%.cpp | obj/login/
	$(COMPILE.cpp) -o $@ $<
obj/map/%.o: src/map/%.cpp | obj/map/
	$(COMPILE.cpp) -o $@ $<
obj/tool/%.o: src/tool/%.cpp | obj/tool/
	$(COMPILE.cpp) -o $@ $<
obj/webserver/%.o: src/webserver/%.cpp | obj/webserver/
	$(COMPILE.cpp) -o $@ $<
obj/webserver/pages/%.o: src/webserver/pages/%.cpp | obj/webserver/pages/
	$(COMPILE.cpp) -o $@ $<

PROGS = login-server char-server map-server ladmin eathena-monitor webserver
# Things to actually make
all: ${PROGS}
clean:
	rm -rf ${PROGS} obj/
common: obj/common/core.o obj/common/db.o obj/common/grfio.o obj/common/lock.o obj/common/md5calc.o obj/common/mt_rand.o obj/common/nullpo.o obj/common/socket.o obj/common/timer.o obj/common/utils.o

# Top level programs
login-server: obj/login/login
	cp -f $< $@
char-server: obj/char/char
	cp -f $< $@
map-server: obj/map/map
	cp -f $< $@
ladmin: obj/ladmin/ladmin
	cp -f $< $@
tools: eathena-monitor
eathena-monitor: obj/tool/eathena-monitor
	cp -f $< $@
webserver: obj/webserver/main
	cp -f $< $@

# Executable dependencies - generated by hand
obj/char/char: obj/char/char.o \
 obj/char/inter.o \
 obj/char/int_party.o \
 obj/char/int_storage.o \
 obj/common/core.o \
 obj/common/socket.o \
 obj/common/timer.o \
 obj/common/db.o \
 obj/common/lock.o \
 obj/common/mt_rand.o \
 obj/common/utils.o
obj/ladmin/ladmin: obj/ladmin/ladmin.o \
 obj/common/md5calc.o \
 obj/common/core.o \
 obj/common/socket.o \
 obj/common/timer.o \
 obj/common/db.o \
 obj/common/mt_rand.o \
 obj/common/utils.o
obj/login/login: obj/login/login.o \
 obj/common/core.o \
 obj/common/socket.o \
 obj/common/timer.o \
 obj/common/db.o \
 obj/common/lock.o \
 obj/common/mt_rand.o \
 obj/common/md5calc.o \
 obj/common/utils.o
obj/map/map: obj/map/map.o \
 obj/map/tmw.o \
 obj/map/magic-interpreter-lexer.o \
 obj/map/magic-interpreter-parser.o \
 obj/map/magic-interpreter-base.o \
 obj/map/magic-expr.o \
 obj/map/magic-stmt.o \
 obj/map/magic.o \
 obj/map/map.o \
 obj/map/chrif.o \
 obj/map/clif.o \
 obj/map/pc.o \
 obj/map/npc.o \
 obj/map/path.o \
 obj/map/itemdb.o \
 obj/map/mob.o \
 obj/map/script.o \
 obj/map/storage.o \
 obj/map/skill.o \
 obj/map/skill-pools.o \
 obj/map/atcommand.o \
 obj/map/battle.o \
 obj/map/trade.o \
 obj/map/party.o \
 obj/common/core.o \
 obj/common/socket.o \
 obj/common/timer.o \
 obj/common/grfio.o \
 obj/common/db.o \
 obj/common/lock.o \
 obj/common/nullpo.o \
 obj/common/mt_rand.o \
 obj/common/md5calc.o \
 obj/common/utils.o
obj/tool/eathena-monitor: obj/tool/eathena-monitor.o
obj/webserver/main: obj/webserver/main.o \
 obj/webserver/parse.o \
 obj/webserver/generate.o \
 obj/webserver/htmlstyle.o \
 obj/webserver/logs.o \
 obj/webserver/pages/about.o \
 obj/webserver/pages/sample.o \
 obj/webserver/pages/notdone.o

map.deps: src/map/magic-interpreter-parser.cpp src/map/magic-interpreter-lexer.cpp
%.deps: src/%/
	for F in `find $< -name '*.cpp' | sort`; do \
	    ${CXX} -MM "$$F" -MT "$$(sed 's/src/obj/;s/\.cpp/.o/' <<< "$$F")" \
	        | sed 's/[^\]$$/& \\/;s/\([^:]\) \([^\]\)/\1 \\\n \2/g;s_/[a-z]\+/../_/_g' \
	        | sort -u; \
	    echo; \
	done > $@

include common.deps login.deps char.deps map.deps ladmin.deps

# It isn't feaible to fix this single use of strftime with nonconstant format string
obj/map/script.o: override WARNINGS+=-Wno-error=format-nonliteral
obj/map/magic-interpreter-lexer.o: override WARNINGS+=-Wno-error=unused-but-set-variable

obj/common/core.o \
obj/common/socket.o \
obj/tool/eathena-monitor.o \
obj/map/magic-interpreter-lexer.o \
obj/map/magic-interpreter-parser.o \
: override WARNINGS+=-Wno-error=old-style-cast
