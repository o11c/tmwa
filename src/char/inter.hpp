#ifndef INTER_HPP
#define INTER_HPP

#include "../lib/ints.hpp"

void inter_init(const char *file);
void inter_save(void);
sint32 inter_parse_frommap(sint32 fd);
void inter_mapif_init(sint32 fd) __attribute__((deprecated));

sint32 inter_check_length(sint32 fd, sint32 length) __attribute__((pure));

# define inter_cfgName "conf/inter_athena.conf"

extern sint32 party_share_level;

#endif // INTER_HPP
