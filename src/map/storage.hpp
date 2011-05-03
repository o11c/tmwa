#ifndef STORAGE_H
#define STORAGE_H

#include "../common/mmo.hpp"

int  storage_storageopen (struct map_session_data *sd);
int  storage_storageadd (struct map_session_data *sd, int index, int amount);
int  storage_storageget (struct map_session_data *sd, int index, int amount);
int  storage_storageaddfromcart (struct map_session_data *sd, int index,
                                 int amount);
int  storage_storagegettocart (struct map_session_data *sd, int index,
                               int amount);
int  storage_storageclose (struct map_session_data *sd);
int  do_init_storage (void);
void do_final_storage (void);
void do_reconnect_storage (void);
struct storage *account2storage (int account_id);
struct storage *account2storage2 (int account_id);
int  storage_delete (int account_id);
int  storage_storage_quit (struct map_session_data *sd);
int  storage_storage_save (int account_id, int final);
int  storage_storage_saved (int account_id);    //Ack from char server that guild store was saved.
void storage_storage_dirty (struct map_session_data *sd);

int  storage_comp_item (const void *_i1, const void *_i2);
//int storage_comp_item(const struct item* i1, const struct item* i2);
void sortage_sortitem (struct storage *stor);

#endif // STORAGE_H