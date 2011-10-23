# defaults for compilation
#CXX=g++
BISON = bison

WARNINGS= @warnings
DEBUG=-g3
# the combination of -flto and --as-needed makes the link stage fail
FLTO=-flto -Wl,--no-as-needed
OPTIMIZATION=-O2 -pipe ${FLTO}
CXXFLAGS = ${WARNINGS}
#LDFLAGS=

# defaults for installation
PREFIX=/usr/local
PREFIX_BIN=${PREFIX}/bin

# Root of the filesystem to install onto
ROOT=/
