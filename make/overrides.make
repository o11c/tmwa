# It isn't feasible to fix this single use of strftime with nonconstant format string
obj/map/script.o: override WARNINGS+=-Wno-error=format-nonliteral
# Not our code :(
obj/map/magic-lexer.o: override WARNINGS+=-Wno-unused-but-set-variable -Wno-suggest-attribute=pure

# SIG_DFL or generated: unavoidable
obj/common/core.o \
obj/common/socket.o \
obj/tool/eathena-monitor.o \
obj/map/magic-lexer.o \
obj/map/magic-parser.o \
: override WARNINGS+=-Wno-old-style-cast
