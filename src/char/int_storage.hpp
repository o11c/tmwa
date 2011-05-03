#ifndef INT_STORAGE_H
#define INT_STORAGE_H

#include "../common/mmo.hpp"

bool inter_storage_init (void);
void inter_storage_final (void);
bool inter_storage_save (void);
void inter_storage_delete (account_t account_id);
struct storage *account2storage (account_t account_id);

bool inter_storage_parse_frommap (int fd);

extern char storage_txt[1024];

#endif
