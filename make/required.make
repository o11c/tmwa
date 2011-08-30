# We edit the CXX variable instead of CXXFLAGS
# because CXX is also used for dependency generation and linking

# works on both x86 and x86_64
override CXX += -m32
# We require many c++0x features from G++ 4.6
# It's probably not worth porting even to 4.5, and hopeless before that
override CXX += -std=c++0x
# With -flto, needed at link time *as well* as at compile time
override CXX += ${OPTIMIZATION}

# for clock_gettime
override LDLIBS += -lrt
# Due to a strange and irreproducable bug in GCC
# Note: please don't append anything else after this
override LDLIBS += -Wl,--no-as-needed -lm

# I'm not *that* confident in the code
override CXXFLAGS += -fstack-protector
# I *know* we don't do *this*
override CXXFLAGS += -fno-strict-aliasing

# I'm probably going to get rid of this header eventually
# Also, it is pretty stupid if you only check sanity when you remember
override CXXFLAGS += -include src/common/sanity.hpp
