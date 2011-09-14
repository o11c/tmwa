#ifndef INTER_HPP
#define INTER_HPP

void inter_init(const char *file);
void inter_save(void);
int32_t inter_parse_frommap(int32_t fd);
void inter_mapif_init(int32_t fd) __attribute__((deprecated));

int32_t inter_check_length(int32_t fd, int32_t length) __attribute__((pure));

# define inter_cfgName "conf/inter_athena.conf"

extern int32_t party_share_level;

#endif // INTER_HPP
