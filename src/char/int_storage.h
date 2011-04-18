#ifndef INT_STORAGE_H
#define INT_STORAGE_H

int  inter_storage_init (void);
void inter_storage_final (void);
int  inter_storage_save (void);
int  inter_guild_storage_save (void);
int  inter_storage_delete (int account_id);
int  inter_guild_storage_delete (int guild_id);
struct storage *account2storage (int account_id);

int  inter_storage_parse_frommap (int fd);

extern char storage_txt[1024];
extern char guild_storage_txt[1024];

#endif
