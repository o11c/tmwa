# Overrides for code beyond our control.
# If it's code within our control instead use #pragma GCC diagnostic
obj/map/magic-lexer.o: override WARNINGS+=-Wno-unused-but-set-variable -Wno-suggest-attribute=pure
obj/map/magic-lexer.o \
obj/map/magic-parser.o \
: override WARNINGS+=-Wno-old-style-cast
