#ifndef INTER_H
#define INTER_H

void inter_init (const char *file);
void inter_save (void);
int  inter_parse_frommap (int fd);
void inter_mapif_init (int fd);

int  inter_check_length (int fd, int length);

#define inter_cfgName "conf/inter_athena.conf"

extern int party_share_level;

#endif
