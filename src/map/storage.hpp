#ifndef STORAGE_H
#define STORAGE_H

#include "../common/mmo.hpp"

int storage_storageopen(MapSessionData *sd);
int storage_storageadd(MapSessionData *sd, int index, int amount);
int storage_storageget(MapSessionData *sd, int index, int amount);
int storage_storageclose(MapSessionData *sd);
int do_init_storage(void);
struct storage *account2storage(int account_id);
struct storage *account2storage2(int account_id);
int storage_storage_quit(MapSessionData *sd);
int storage_storage_save(int account_id, int final);
int storage_storage_saved(int account_id);    //Ack from char server that guild store was saved.

#endif // STORAGE_H
