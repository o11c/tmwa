${BUILD_DIR}/char/char.o: src/char/char.cpp src/char/char.hpp \
 src/char/../common/mmo.hpp src/char/../common/utils.hpp \
 src/char/../common/sanity.hpp src/char/../common/sanity.hpp \
 src/char/../common/utils.hpp src/char/../common/core.hpp \
 src/char/../common/socket.hpp src/char/../common/timer.hpp \
 src/char/../common/version.hpp src/char/../common/lock.hpp \
 src/char/inter.hpp src/char/int_party.hpp src/char/int_storage.hpp
${BUILD_DIR}/char/inter.o: src/char/inter.cpp src/char/inter.hpp \
 src/char/../common/mmo.hpp src/char/../common/utils.hpp \
 src/char/../common/sanity.hpp src/char/char.hpp \
 src/char/../common/sanity.hpp src/char/../common/socket.hpp \
 src/char/../common/timer.hpp src/char/../common/db.hpp \
 src/char/int_party.hpp src/char/int_storage.hpp \
 src/char/../common/lock.hpp
${BUILD_DIR}/char/int_party.o: src/char/int_party.cpp \
 src/char/int_party.hpp src/char/../common/sanity.hpp \
 src/char/../common/mmo.hpp src/char/../common/utils.hpp \
 src/char/../common/sanity.hpp src/char/inter.hpp src/char/char.hpp \
 src/char/../common/socket.hpp src/char/../common/db.hpp \
 src/char/../common/lock.hpp
${BUILD_DIR}/char/int_storage.o: src/char/int_storage.cpp \
 src/char/int_storage.hpp src/char/../common/mmo.hpp \
 src/char/../common/utils.hpp src/char/../common/sanity.hpp \
 src/char/../common/socket.hpp src/char/../common/db.hpp \
 src/char/../common/lock.hpp src/char/char.hpp \
 src/char/../common/sanity.hpp src/char/inter.hpp
${BUILD_DIR}/common/core.o: src/common/core.cpp src/common/core.hpp \
 src/common/socket.hpp src/common/sanity.hpp src/common/timer.hpp \
 src/common/version.hpp src/common/mt_rand.hpp src/common/nullpo.hpp
${BUILD_DIR}/common/db.o: src/common/db.cpp src/common/db.hpp \
 src/common/sanity.hpp src/common/utils.hpp
${BUILD_DIR}/common/grfio.o: src/common/grfio.cpp src/common/utils.hpp \
 src/common/sanity.hpp src/common/grfio.hpp src/common/mmo.hpp \
 src/common/socket.hpp
${BUILD_DIR}/common/lock.o: src/common/lock.cpp src/common/lock.hpp \
 src/common/socket.hpp src/common/sanity.hpp
${BUILD_DIR}/common/md5calc.o: src/common/md5calc.cpp \
 src/common/md5calc.hpp src/common/sanity.hpp src/common/mt_rand.hpp
${BUILD_DIR}/common/mt_rand.o: src/common/mt_rand.cpp \
 src/common/mt_rand.hpp src/common/sanity.hpp
${BUILD_DIR}/common/nullpo.o: src/common/nullpo.cpp src/common/nullpo.hpp \
 src/common/sanity.hpp
${BUILD_DIR}/common/socket.o: src/common/socket.cpp src/common/mmo.hpp \
 src/common/utils.hpp src/common/sanity.hpp src/common/socket.hpp
${BUILD_DIR}/common/timer.o: src/common/timer.cpp src/common/timer.hpp \
 src/common/sanity.hpp src/common/utils.hpp
${BUILD_DIR}/common/utils.o: src/common/utils.cpp src/common/sanity.hpp \
 src/common/utils.hpp src/common/socket.hpp
${BUILD_DIR}/ladmin/ladmin.o: src/ladmin/ladmin.cpp \
 src/ladmin/../common/core.hpp src/ladmin/../common/socket.hpp \
 src/ladmin/../common/sanity.hpp src/ladmin/../common/version.hpp \
 src/ladmin/../common/mmo.hpp src/ladmin/../common/utils.hpp \
 src/ladmin/../common/md5calc.hpp
${BUILD_DIR}/login/login.o: src/login/login.cpp src/login/login.hpp \
 src/login/../common/sanity.hpp src/login/../common/mmo.hpp \
 src/login/../common/utils.hpp src/login/../common/sanity.hpp \
 src/login/../common/core.hpp src/login/../common/socket.hpp \
 src/login/../common/timer.hpp src/login/../common/version.hpp \
 src/login/../common/db.hpp src/login/../common/lock.hpp \
 src/login/../common/mt_rand.hpp src/login/../common/md5calc.hpp
${BUILD_DIR}/map/atcommand.o: src/map/atcommand.cpp src/map/atcommand.hpp \
 src/map/map.hpp src/map/../common/mmo.hpp src/map/../common/utils.hpp \
 src/map/../common/sanity.hpp src/map/../common/timer.hpp \
 src/map/../common/db.hpp src/map/script.hpp src/map/../common/socket.hpp \
 src/map/../common/nullpo.hpp src/map/../common/mt_rand.hpp \
 src/map/battle.hpp src/map/clif.hpp src/map/storage.hpp \
 src/map/chrif.hpp src/map/intif.hpp src/map/itemdb.hpp src/map/mob.hpp \
 src/map/npc.hpp src/map/pc.hpp src/map/party.hpp src/map/skill.hpp \
 src/map/magic.hpp src/map/trade.hpp src/map/../common/core.hpp \
 src/map/tmw.hpp
${BUILD_DIR}/map/battle.o: src/map/battle.cpp src/map/battle.hpp \
 src/map/../common/timer.hpp src/map/../common/sanity.hpp \
 src/map/../common/nullpo.hpp src/map/clif.hpp src/map/map.hpp \
 src/map/../common/mmo.hpp src/map/../common/utils.hpp \
 src/map/../common/db.hpp src/map/script.hpp src/map/storage.hpp \
 src/map/itemdb.hpp src/map/mob.hpp src/map/pc.hpp src/map/skill.hpp \
 src/map/magic.hpp src/map/intif.hpp src/map/../common/socket.hpp \
 src/map/../common/mt_rand.hpp
${BUILD_DIR}/map/chat.o: src/map/chat.cpp src/map/chat.hpp \
 src/map/map.hpp src/map/../common/mmo.hpp src/map/../common/utils.hpp \
 src/map/../common/sanity.hpp src/map/../common/timer.hpp \
 src/map/../common/db.hpp src/map/script.hpp src/map/../common/nullpo.hpp \
 src/map/clif.hpp src/map/storage.hpp src/map/pc.hpp src/map/npc.hpp
${BUILD_DIR}/map/chrif.o: src/map/chrif.cpp src/map/chrif.hpp \
 src/map/../common/mmo.hpp src/map/../common/utils.hpp \
 src/map/../common/sanity.hpp src/map/../common/socket.hpp \
 src/map/../common/timer.hpp src/map/map.hpp src/map/../common/db.hpp \
 src/map/script.hpp src/map/battle.hpp src/map/clif.hpp \
 src/map/storage.hpp src/map/intif.hpp src/map/npc.hpp src/map/pc.hpp \
 src/map/../common/nullpo.hpp src/map/itemdb.hpp
${BUILD_DIR}/map/clif.o: src/map/clif.cpp src/map/clif.hpp \
 src/map/map.hpp src/map/../common/mmo.hpp src/map/../common/utils.hpp \
 src/map/../common/sanity.hpp src/map/../common/timer.hpp \
 src/map/../common/db.hpp src/map/script.hpp src/map/storage.hpp \
 src/map/../common/socket.hpp src/map/../common/version.hpp \
 src/map/../common/nullpo.hpp src/map/../common/md5calc.hpp \
 src/map/../common/mt_rand.hpp src/map/atcommand.hpp src/map/battle.hpp \
 src/map/chat.hpp src/map/chrif.hpp src/map/intif.hpp src/map/itemdb.hpp \
 src/map/magic.hpp src/map/mob.hpp src/map/npc.hpp src/map/party.hpp \
 src/map/pc.hpp src/map/skill.hpp src/map/tmw.hpp src/map/trade.hpp
${BUILD_DIR}/map/intif.o: src/map/intif.cpp src/map/intif.hpp \
 src/map/../common/mmo.hpp src/map/../common/utils.hpp \
 src/map/../common/sanity.hpp src/map/../common/nullpo.hpp \
 src/map/../common/socket.hpp src/map/../common/timer.hpp \
 src/map/battle.hpp src/map/chrif.hpp src/map/clif.hpp src/map/map.hpp \
 src/map/../common/db.hpp src/map/script.hpp src/map/storage.hpp \
 src/map/party.hpp src/map/pc.hpp
${BUILD_DIR}/map/itemdb.o: src/map/itemdb.cpp src/map/itemdb.hpp \
 src/map/script.hpp src/map/map.hpp src/map/../common/mmo.hpp \
 src/map/../common/utils.hpp src/map/../common/sanity.hpp \
 src/map/../common/timer.hpp src/map/../common/db.hpp \
 src/map/../common/grfio.hpp src/map/../common/nullpo.hpp \
 src/map/battle.hpp src/map/pc.hpp src/map/../common/socket.hpp \
 src/map/../common/mt_rand.hpp
${BUILD_DIR}/map/magic.o: src/map/magic.cpp src/map/magic-interpreter.hpp \
 src/map/../common/nullpo.hpp src/map/../common/sanity.hpp \
 src/map/battle.hpp src/map/chat.hpp src/map/map.hpp \
 src/map/../common/mmo.hpp src/map/../common/utils.hpp \
 src/map/../common/timer.hpp src/map/../common/db.hpp src/map/script.hpp \
 src/map/chrif.hpp src/map/clif.hpp src/map/storage.hpp src/map/intif.hpp \
 src/map/itemdb.hpp src/map/magic.hpp src/map/mob.hpp src/map/npc.hpp \
 src/map/pc.hpp src/map/party.hpp src/map/skill.hpp src/map/trade.hpp \
 src/map/../common/socket.hpp
${BUILD_DIR}/map/magic-expr.o: src/map/magic-expr.cpp \
 src/map/magic-expr.hpp src/map/magic-interpreter.hpp \
 src/map/../common/nullpo.hpp src/map/../common/sanity.hpp \
 src/map/battle.hpp src/map/chat.hpp src/map/map.hpp \
 src/map/../common/mmo.hpp src/map/../common/utils.hpp \
 src/map/../common/timer.hpp src/map/../common/db.hpp src/map/script.hpp \
 src/map/chrif.hpp src/map/clif.hpp src/map/storage.hpp src/map/intif.hpp \
 src/map/itemdb.hpp src/map/magic.hpp src/map/mob.hpp src/map/npc.hpp \
 src/map/pc.hpp src/map/party.hpp src/map/skill.hpp src/map/trade.hpp \
 src/map/../common/socket.hpp src/map/magic-interpreter-aux.hpp \
 src/map/magic-expr-eval.hpp src/map/../common/mt_rand.hpp
${BUILD_DIR}/map/magic-interpreter-base.o: \
 src/map/magic-interpreter-base.cpp src/map/magic.hpp src/map/clif.hpp \
 src/map/map.hpp src/map/../common/mmo.hpp src/map/../common/utils.hpp \
 src/map/../common/sanity.hpp src/map/../common/timer.hpp \
 src/map/../common/db.hpp src/map/script.hpp src/map/storage.hpp \
 src/map/intif.hpp src/map/magic-interpreter.hpp \
 src/map/../common/nullpo.hpp src/map/battle.hpp src/map/chat.hpp \
 src/map/chrif.hpp src/map/itemdb.hpp src/map/mob.hpp src/map/npc.hpp \
 src/map/pc.hpp src/map/party.hpp src/map/skill.hpp src/map/trade.hpp \
 src/map/../common/socket.hpp src/map/magic-expr.hpp \
 src/map/magic-interpreter-aux.hpp
${BUILD_DIR}/map/magic-interpreter-lexer.o: \
 src/map/magic-interpreter-lexer.cpp src/map/magic-interpreter.hpp \
 src/map/../common/nullpo.hpp src/map/../common/sanity.hpp \
 src/map/battle.hpp src/map/chat.hpp src/map/map.hpp \
 src/map/../common/mmo.hpp src/map/../common/utils.hpp \
 src/map/../common/timer.hpp src/map/../common/db.hpp src/map/script.hpp \
 src/map/chrif.hpp src/map/clif.hpp src/map/storage.hpp src/map/intif.hpp \
 src/map/itemdb.hpp src/map/magic.hpp src/map/mob.hpp src/map/npc.hpp \
 src/map/pc.hpp src/map/party.hpp src/map/skill.hpp src/map/trade.hpp \
 src/map/../common/socket.hpp src/map/magic-interpreter-parser.hpp
${BUILD_DIR}/map/magic-interpreter-parser.o: \
 src/map/magic-interpreter-parser.cpp src/map/magic-interpreter.hpp \
 src/map/../common/nullpo.hpp src/map/../common/sanity.hpp \
 src/map/battle.hpp src/map/chat.hpp src/map/map.hpp \
 src/map/../common/mmo.hpp src/map/../common/utils.hpp \
 src/map/../common/timer.hpp src/map/../common/db.hpp src/map/script.hpp \
 src/map/chrif.hpp src/map/clif.hpp src/map/storage.hpp src/map/intif.hpp \
 src/map/itemdb.hpp src/map/magic.hpp src/map/mob.hpp src/map/npc.hpp \
 src/map/pc.hpp src/map/party.hpp src/map/skill.hpp src/map/trade.hpp \
 src/map/../common/socket.hpp src/map/magic-expr.hpp \
 src/map/magic-interpreter-aux.hpp
${BUILD_DIR}/map/magic-stmt.o: src/map/magic-stmt.cpp \
 src/map/magic-interpreter.hpp src/map/../common/nullpo.hpp \
 src/map/../common/sanity.hpp src/map/battle.hpp src/map/chat.hpp \
 src/map/map.hpp src/map/../common/mmo.hpp src/map/../common/utils.hpp \
 src/map/../common/timer.hpp src/map/../common/db.hpp src/map/script.hpp \
 src/map/chrif.hpp src/map/clif.hpp src/map/storage.hpp src/map/intif.hpp \
 src/map/itemdb.hpp src/map/magic.hpp src/map/mob.hpp src/map/npc.hpp \
 src/map/pc.hpp src/map/party.hpp src/map/skill.hpp src/map/trade.hpp \
 src/map/../common/socket.hpp src/map/magic-expr.hpp \
 src/map/magic-interpreter-aux.hpp src/map/magic-expr-eval.hpp
${BUILD_DIR}/map/map.o: src/map/map.cpp src/map/map.hpp \
 src/map/../common/mmo.hpp src/map/../common/utils.hpp \
 src/map/../common/sanity.hpp src/map/../common/timer.hpp \
 src/map/../common/db.hpp src/map/script.hpp src/map/../common/core.hpp \
 src/map/../common/grfio.hpp src/map/../common/mt_rand.hpp \
 src/map/chrif.hpp src/map/clif.hpp src/map/storage.hpp src/map/intif.hpp \
 src/map/npc.hpp src/map/pc.hpp src/map/mob.hpp src/map/chat.hpp \
 src/map/itemdb.hpp src/map/skill.hpp src/map/magic.hpp src/map/trade.hpp \
 src/map/party.hpp src/map/battle.hpp src/map/atcommand.hpp \
 src/map/../common/nullpo.hpp src/map/../common/socket.hpp
${BUILD_DIR}/map/mob.o: src/map/mob.cpp src/map/mob.hpp \
 src/map/../common/mmo.hpp src/map/../common/utils.hpp \
 src/map/../common/sanity.hpp src/map/map.hpp src/map/../common/timer.hpp \
 src/map/../common/db.hpp src/map/script.hpp src/map/../common/socket.hpp \
 src/map/../common/nullpo.hpp src/map/../common/mt_rand.hpp \
 src/map/clif.hpp src/map/storage.hpp src/map/intif.hpp src/map/pc.hpp \
 src/map/itemdb.hpp src/map/skill.hpp src/map/magic.hpp \
 src/map/battle.hpp src/map/party.hpp src/map/npc.hpp
${BUILD_DIR}/map/npc.o: src/map/npc.cpp src/map/npc.hpp \
 src/map/../common/mmo.hpp src/map/../common/utils.hpp \
 src/map/../common/sanity.hpp src/map/../common/timer.hpp \
 src/map/../common/nullpo.hpp src/map/battle.hpp src/map/clif.hpp \
 src/map/map.hpp src/map/../common/db.hpp src/map/script.hpp \
 src/map/storage.hpp src/map/intif.hpp src/map/itemdb.hpp src/map/mob.hpp \
 src/map/pc.hpp src/map/skill.hpp src/map/magic.hpp \
 src/map/../common/socket.hpp
${BUILD_DIR}/map/party.o: src/map/party.cpp src/map/party.hpp \
 src/map/../common/db.hpp src/map/../common/sanity.hpp \
 src/map/../common/timer.hpp src/map/../common/socket.hpp \
 src/map/../common/nullpo.hpp src/map/pc.hpp src/map/map.hpp \
 src/map/../common/mmo.hpp src/map/../common/utils.hpp src/map/script.hpp \
 src/map/battle.hpp src/map/intif.hpp src/map/clif.hpp \
 src/map/storage.hpp src/map/skill.hpp src/map/magic.hpp src/map/tmw.hpp
${BUILD_DIR}/map/path.o: src/map/path.cpp src/map/map.hpp \
 src/map/../common/mmo.hpp src/map/../common/utils.hpp \
 src/map/../common/sanity.hpp src/map/../common/timer.hpp \
 src/map/../common/db.hpp src/map/script.hpp src/map/battle.hpp \
 src/map/../common/nullpo.hpp
${BUILD_DIR}/map/pc.o: src/map/pc.cpp src/map/pc.hpp src/map/map.hpp \
 src/map/../common/mmo.hpp src/map/../common/utils.hpp \
 src/map/../common/sanity.hpp src/map/../common/timer.hpp \
 src/map/../common/db.hpp src/map/script.hpp src/map/../common/socket.hpp \
 src/map/../common/nullpo.hpp src/map/../common/mt_rand.hpp \
 src/map/atcommand.hpp src/map/battle.hpp src/map/chat.hpp \
 src/map/chrif.hpp src/map/clif.hpp src/map/storage.hpp src/map/intif.hpp \
 src/map/itemdb.hpp src/map/mob.hpp src/map/npc.hpp src/map/party.hpp \
 src/map/skill.hpp src/map/magic.hpp src/map/trade.hpp
${BUILD_DIR}/map/script.o: src/map/script.cpp src/map/script.hpp \
 src/map/../common/socket.hpp src/map/../common/sanity.hpp \
 src/map/../common/timer.hpp src/map/../common/lock.hpp \
 src/map/../common/mt_rand.hpp src/map/atcommand.hpp src/map/map.hpp \
 src/map/../common/mmo.hpp src/map/../common/utils.hpp \
 src/map/../common/db.hpp src/map/battle.hpp src/map/chat.hpp \
 src/map/chrif.hpp src/map/clif.hpp src/map/storage.hpp src/map/intif.hpp \
 src/map/itemdb.hpp src/map/mob.hpp src/map/npc.hpp src/map/party.hpp \
 src/map/pc.hpp src/map/skill.hpp src/map/magic.hpp
${BUILD_DIR}/map/skill.o: src/map/skill.cpp src/map/skill.hpp \
 src/map/../common/timer.hpp src/map/../common/sanity.hpp src/map/map.hpp \
 src/map/../common/mmo.hpp src/map/../common/utils.hpp \
 src/map/../common/db.hpp src/map/script.hpp src/map/magic.hpp \
 src/map/clif.hpp src/map/storage.hpp src/map/intif.hpp \
 src/map/../common/nullpo.hpp src/map/../common/mt_rand.hpp \
 src/map/battle.hpp src/map/itemdb.hpp src/map/mob.hpp src/map/party.hpp \
 src/map/pc.hpp src/map/../common/socket.hpp
${BUILD_DIR}/map/skill-pools.o: src/map/skill-pools.cpp \
 src/map/../common/timer.hpp src/map/../common/sanity.hpp \
 src/map/../common/nullpo.hpp src/map/../common/mt_rand.hpp \
 src/map/magic.hpp src/map/clif.hpp src/map/map.hpp \
 src/map/../common/mmo.hpp src/map/../common/utils.hpp \
 src/map/../common/db.hpp src/map/script.hpp src/map/storage.hpp \
 src/map/intif.hpp src/map/battle.hpp src/map/itemdb.hpp src/map/mob.hpp \
 src/map/party.hpp src/map/pc.hpp src/map/skill.hpp \
 src/map/../common/socket.hpp
${BUILD_DIR}/map/storage.o: src/map/storage.cpp src/map/storage.hpp \
 src/map/../common/mmo.hpp src/map/../common/utils.hpp \
 src/map/../common/sanity.hpp src/map/../common/db.hpp \
 src/map/../common/nullpo.hpp src/map/chrif.hpp src/map/itemdb.hpp \
 src/map/script.hpp src/map/map.hpp src/map/../common/timer.hpp \
 src/map/clif.hpp src/map/intif.hpp src/map/pc.hpp src/map/battle.hpp \
 src/map/atcommand.hpp
${BUILD_DIR}/map/tmw.o: src/map/tmw.cpp src/map/tmw.hpp src/map/map.hpp \
 src/map/../common/mmo.hpp src/map/../common/utils.hpp \
 src/map/../common/sanity.hpp src/map/../common/timer.hpp \
 src/map/../common/db.hpp src/map/script.hpp src/map/../common/socket.hpp \
 src/map/../common/version.hpp src/map/../common/nullpo.hpp \
 src/map/atcommand.hpp src/map/battle.hpp src/map/chat.hpp \
 src/map/chrif.hpp src/map/clif.hpp src/map/storage.hpp src/map/intif.hpp \
 src/map/itemdb.hpp src/map/magic.hpp src/map/mob.hpp src/map/npc.hpp \
 src/map/party.hpp src/map/pc.hpp src/map/skill.hpp src/map/trade.hpp
${BUILD_DIR}/map/trade.o: src/map/trade.cpp src/map/trade.hpp \
 src/map/map.hpp src/map/../common/mmo.hpp src/map/../common/utils.hpp \
 src/map/../common/sanity.hpp src/map/../common/timer.hpp \
 src/map/../common/db.hpp src/map/script.hpp src/map/clif.hpp \
 src/map/storage.hpp src/map/itemdb.hpp src/map/pc.hpp src/map/npc.hpp \
 src/map/battle.hpp src/map/../common/nullpo.hpp
${BUILD_DIR}/tool/eathena-monitor.o: src/tool/eathena-monitor.cpp
${BUILD_DIR}/tool/itemsearch.o: src/tool/itemsearch.cpp
${BUILD_DIR}/webserver/generate.o: src/webserver/generate.cpp
${BUILD_DIR}/webserver/htmlstyle.o: src/webserver/htmlstyle.cpp
${BUILD_DIR}/webserver/logs.o: src/webserver/logs.cpp
${BUILD_DIR}/webserver/main.o: src/webserver/main.cpp
${BUILD_DIR}/webserver/pages/about.o: src/webserver/pages/about.cpp
${BUILD_DIR}/webserver/pages/notdone.o: src/webserver/pages/notdone.cpp
${BUILD_DIR}/webserver/pages/sample.o: src/webserver/pages/sample.cpp
${BUILD_DIR}/webserver/parse.o: src/webserver/parse.cpp
