# defaults for compilation
BISON = bison
WARNINGS= @warnings
DEBUG=-g3
OPTIMIZATION=-O2 -pipe -flto
CXXFLAGS = ${DEBUG} ${WARNINGS}
LDFLAGS = -Wl,--as-needed

# defaults for installation
PREFIX=/usr/local
PREFIX_BIN=${PREFIX}/bin

# Root of the filesystem to install onto
ROOT=/
