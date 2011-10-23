include make/char.deps

tmwa-char: obj/char/main.o \
 obj/char/inter.o \
 obj/char/int_party.o \
 obj/char/int_storage.o \
 \
 obj/common/core.o \
 obj/common/db.o \
 obj/common/lock.o \
 obj/common/mt_rand.o \
 obj/common/socket.o \
 obj/common/timer.o \
 obj/common/utils.o \
 \
 obj/lib/ip.o \
 obj/lib/log.o \
