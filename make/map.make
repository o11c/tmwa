make/map.deps: src/map/magic-parser.cpp src/map/magic-lexer.cpp

include make/map.deps

tmwa-map: obj/map/main.o \
 obj/map/magic-lexer.o \
 obj/map/magic-parser.o \
 obj/map/magic.o \
 obj/map/magic-expr.o \
 obj/map/magic-base.o \
 obj/map/magic-stmt.o \
 \
 obj/map/atcommand.o \
 obj/map/battle.o \
 obj/map/chrif.o \
 obj/map/clif.o \
 obj/map/grfio.o \
 obj/map/itemdb.o \
 obj/map/mob.o \
 obj/map/npc.o \
 obj/map/party.o \
 obj/map/path.o \
 obj/map/pc.o \
 obj/map/script.o \
 obj/map/script-builtins.o \
 obj/map/storage.o \
 obj/map/skill.o \
 obj/map/tmw.o \
 obj/map/trade.o \
 \
 obj/common/core.o \
 obj/common/db.o \
 obj/common/lock.o \
 obj/common/md5calc.o \
 obj/common/mt_rand.o \
 obj/common/nullpo.o \
 obj/common/socket.o \
 obj/common/timer.o \
 obj/common/utils.o \
 \
 obj/lib/ip.o \
 obj/lib/log.o \
 obj/lib/string.o \
 obj/lib/darray.o \
 obj/lib/lfsr.o \
