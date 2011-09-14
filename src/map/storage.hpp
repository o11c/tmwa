#ifndef STORAGE_HPP
#define STORAGE_HPP

# include "../common/mmo.hpp"

# include "map.structs.hpp"

int32_t storage_storageopen(MapSessionData *sd);
int32_t storage_storageadd(MapSessionData *sd, int32_t index, int32_t amount);
int32_t storage_storageget(MapSessionData *sd, int32_t index, int32_t amount);
int32_t storage_storageclose(MapSessionData *sd);
int32_t do_init_storage(void);
struct storage *account2storage(int32_t account_id);
struct storage *account2storage2(int32_t account_id) __attribute__((pure));
int32_t storage_storage_quit(MapSessionData *sd);
int32_t storage_storage_save(int32_t account_id, int32_t final);
int32_t storage_storage_saved(int32_t account_id);    //Ack from char server that guild store was saved.

#endif // STORAGE_HPP
