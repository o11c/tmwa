#ifndef INTER_HPP
#define INTER_HPP

void inter_init(const char *file);
void inter_save(void);
int inter_parse_frommap(int fd);
void inter_mapif_init(int fd) __attribute__((deprecated));

int inter_check_length(int fd, int length) __attribute__((pure));

# define inter_cfgName "conf/inter_athena.conf"

extern int party_share_level;

#endif // INTER_HPP
