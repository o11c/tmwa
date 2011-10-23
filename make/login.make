include make/login.deps

tmwa-login: obj/login/main.o \
 obj/common/core.o \
 obj/common/db.o \
 obj/common/lock.o \
 obj/common/md5calc.o \
 obj/common/mt_rand.o \
 obj/common/socket.o \
 obj/common/timer.o \
 obj/common/utils.o \
 \
 obj/lib/ip.o \
 obj/lib/log.o \
