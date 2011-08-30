# defaults for compilation
BISON = bison
WARNINGS= @warnings
DEBUG=-g3
OPTIMIZATION=-O2 -pipe
CXXFLAGS = ${DEBUG} ${WARNINGS}
LDFLAGS = -Wl,--as-needed

# Since I'm depending on GCC 4.6 for now, I might as well take advantage
CXXFLAGS += -flto
LDFLAGS += -flto

# defaults for installation
PREFIX=/usr/local
PREFIX_BIN=${PREFIX}/bin
