include make/admin.deps

tmwa-admin: obj/admin/main.o \
 obj/common/core.o \
 obj/common/md5calc.o \
 obj/common/mt_rand.o \
 obj/common/socket.o \
 obj/common/timer.o \
 obj/common/utils.o \
 \
 obj/lib/ip.o \
 obj/lib/log.o \
