include make/admin.deps

obj/admin/admin: obj/admin/admin.o \
 obj/common/core.o \
 obj/common/db.o \
 obj/common/md5calc.o \
 obj/common/mt_rand.o \
 obj/common/socket.o \
 obj/common/timer.o \
 obj/common/utils.o \
 \
 obj/lib/ip.o \
 obj/lib/log.o \
