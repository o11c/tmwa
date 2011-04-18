#ifndef INTER_H
#define INTER_H

int  inter_init (const char *file);
int  inter_save (void);
int  inter_parse_frommap (int fd);
int  inter_mapif_init (int fd);

int  inter_check_length (int fd, int length);

int  inter_log (const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#define inter_cfgName "conf/inter_athena.conf"

extern int party_share_level;
extern char inter_log_filename[1024];

#endif
