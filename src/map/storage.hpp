#ifndef STORAGE_HPP
#define STORAGE_HPP

# include "../common/mmo.hpp"

# include "main.structs.hpp"

sint32 storage_storageopen(MapSessionData *sd);
sint32 storage_storageadd(MapSessionData *sd, sint32 index, sint32 amount);
sint32 storage_storageget(MapSessionData *sd, sint32 index, sint32 amount);
sint32 storage_storageclose(MapSessionData *sd);
struct storage *account2storage(account_t account_id);
__attribute__((pure))
struct storage *account2storage2(account_t account_id);
sint32 storage_storage_quit(MapSessionData *sd);
sint32 storage_storage_save(account_t account_id, sint32 final);
sint32 storage_storage_saved(account_t account_id);    //Ack from char server that guild store was saved.

#endif // STORAGE_HPP
