#include "storage.hpp"

#include "../common/nullpo.hpp"
#include "../common/utils.hpp"

#include "chrif.hpp"
#include "clif.hpp"
#include "itemdb.hpp"
#include "main.hpp"
#include "pc.hpp"

static void storage_delete(account_t account_id);
static sint32 storage_comp_item(const void *_i1, const void *_i2);
static void sortage_sortitem(struct storage *stor);

static DMap<account_t, struct storage *> storage_db;

/*==========================================
 * 倉庫内アイテムソート
 *------------------------------------------
 */
sint32 storage_comp_item(const void *_i1, const void *_i2)
{
    const struct item *i1 = static_cast<const struct item *>(_i1);
    const struct item *i2 = static_cast<const struct item *>(_i2);

    if (i1->nameid == i2->nameid)
        return 0;
    else if (!(i1->nameid) || !(i1->amount))
        return 1;
    else if (!(i2->nameid) || !(i2->amount))
        return -1;
    return i1->nameid - i2->nameid;
}

void sortage_sortitem(struct storage *stor)
{
    nullpo_retv(stor);
    qsort(stor->storage_, MAX_STORAGE, sizeof(struct item),
           storage_comp_item);
}

struct storage *account2storage(account_t account_id)
{
    struct storage *stor = storage_db.get(account_id);
    if (stor == NULL)
    {
        CREATE(stor, struct storage, 1);
        stor->account_id = account_id;
        storage_db.set(account_id, stor);
    }
    return stor;
}

// Just to ask storage, without creation
struct storage *account2storage2(account_t account_id)
{
    return storage_db.get(account_id);
}

void storage_delete(account_t account_id)
{
    free(storage_db.take(account_id));
}

/*==========================================
 * カプラ倉庫を開く
 *------------------------------------------
 */
sint32 storage_storageopen(MapSessionData *sd)
{
    struct storage *stor;
    nullpo_ret(sd);

    if (sd->state.storage_flag)
        return 1;               //Already open?

    if (!(stor = storage_db.get(sd->status.account_id)))
    {                           //Request storage.
        intif_request_storage(sd->status.account_id);
        return 1;
    }

    if (stor->storage_status)
        return 1;               //Already open/player already has it open...

    stor->storage_status = 1;
    sd->state.storage_flag = 1;
    clif_storageitemlist(sd, stor);
    clif_storageequiplist(sd, stor);
    clif_updatestorageamount(sd, stor);
    return 0;
}

/*==========================================
 * Internal add-item function.
 *------------------------------------------
 */
static sint32 storage_additem(MapSessionData *sd, struct storage *stor,
                            struct item *item_data, sint32 amount)
{
    struct item_data *data;
    sint32 i;

    if (item_data->nameid <= 0 || amount <= 0)
        return 1;

    data = itemdb_search(item_data->nameid);

    if (!itemdb_isequip2(data))
    {                           //Stackable
        for (i = 0; i < MAX_STORAGE; i++)
        {
            if (compare_item(&stor->storage_[i], item_data))
            {
                if (amount > MAX_AMOUNT - stor->storage_[i].amount)
                    return 1;
                stor->storage_[i].amount += amount;
                clif_storageitemadded(sd, stor, i, amount);
                stor->dirty = 1;
                return 0;
            }
        }
    }
    //Add item
    for (i = 0; i < MAX_STORAGE && stor->storage_[i].nameid; i++);

    if (i >= MAX_STORAGE)
        return 1;

    memcpy(&stor->storage_[i], item_data, sizeof(stor->storage_[0]));
    stor->storage_[i].amount = amount;
    stor->storage_amount++;
    clif_storageitemadded(sd, stor, i, amount);
    clif_updatestorageamount(sd, stor);
    stor->dirty = 1;
    return 0;
}

/*==========================================
 * Internal del-item function
 *------------------------------------------
 */
static sint32 storage_delitem(MapSessionData *sd, struct storage *stor,
                            sint32 n, sint32 amount)
{

    if (stor->storage_[n].nameid == 0 || stor->storage_[n].amount < amount)
        return 1;

    stor->storage_[n].amount -= amount;
    if (stor->storage_[n].amount == 0)
    {
        memset(&stor->storage_[n], 0, sizeof(stor->storage_[0]));
        stor->storage_amount--;
        clif_updatestorageamount(sd, stor);
    }
    clif_storageitemremoved(sd, n, amount);

    stor->dirty = 1;
    return 0;
}

/*==========================================
 * Add an item to the storage from the inventory.
 *------------------------------------------
 */
sint32 storage_storageadd(MapSessionData *sd, sint32 idx, sint32 amount)
{
    struct storage *stor;

    nullpo_ret(sd);
    nullpo_ret(stor = account2storage2(sd->status.account_id));

    if ((stor->storage_amount > MAX_STORAGE) || !stor->storage_status)
        return 0;               // storage full / storage closed

    if (idx < 0 || idx >= MAX_INVENTORY)
        return 0;

    if (sd->status.inventory[idx].nameid <= 0)
        return 0;               //No item on that spot

    if (amount < 1 || amount > sd->status.inventory[idx].amount)
        return 0;

//  log_tostorage(sd, idx, 0);
    if (storage_additem(sd, stor, &sd->status.inventory[idx], amount) == 0)
    {
        // remove item from inventory
        pc_unequipinvyitem(sd, idx, CalcStatus::NOW);
        pc_delitem(sd, idx, amount, 0);
    }

    return 1;
}

/*==========================================
 * Retrieve an item from the storage.
 *------------------------------------------
 */
sint32 storage_storageget(MapSessionData *sd, sint32 idx, sint32 amount)
{
    struct storage *stor;

    nullpo_ret(sd);
    nullpo_ret(stor = account2storage2(sd->status.account_id));

    if (idx < 0 || idx >= MAX_STORAGE)
        return 0;

    if (stor->storage_[idx].nameid <= 0)
        return 0;               //Nothing there

    if (amount < 1 || amount > stor->storage_[idx].amount)
        return 0;

    PickupFail flag = pc_additem(sd, &stor->storage_[idx], amount);
    if (flag == PickupFail::OKAY)
        storage_delitem(sd, stor, idx, amount);
    else
        clif_additem(sd, 0, 0, flag);
//  log_fromstorage(sd, idx, 0);
    return 1;
}

/*==========================================
 * Modified By Valaris to save upon closing [massdriller]
 *------------------------------------------
 */
sint32 storage_storageclose(MapSessionData *sd)
{
    struct storage *stor;

    nullpo_ret(sd);
    nullpo_ret(stor = account2storage2(sd->status.account_id));

    clif_storageclose(sd);
    if (stor->storage_status)
    {
        chrif_save(sd);
    }
    stor->storage_status = 0;
    sd->state.storage_flag = 0;

    if (sd->npc_flags.storage)
    {
        sd->npc_flags.storage = 0;
        map_scriptcont(sd, sd->npc_id);
    }

    return 0;
}

/*==========================================
 * When quitting the game.
 *------------------------------------------
 */
sint32 storage_storage_quit(MapSessionData *sd)
{
    struct storage *stor;

    nullpo_ret(sd);

    stor = account2storage2(sd->status.account_id);
    if (stor)
    {
        chrif_save(sd);        //Invokes the storage saving as well.
        stor->storage_status = 0;
        sd->state.storage_flag = 0;
    }

    return 0;
}

sint32 storage_storage_save(account_t account_id, sint32 final)
{
    struct storage *stor;

    stor = account2storage2(account_id);
    if (!stor)
        return 0;

    if (stor->dirty)
    {
        if (final)
        {
            stor->dirty = 2;
            stor->storage_status = 0;   //To prevent further manipulation of it.
        }
        intif_send_storage(stor);
        return 1;
    }
    if (final)
    {                           //Clear storage from memory. Nothing to save.
        storage_delete(account_id);
        return 1;
    }

    return 0;
}

//Ack from Char-server indicating the storage was saved. [Skotlex]
sint32 storage_storage_saved(account_t account_id)
{
    struct storage *stor;

    if ((stor = account2storage2(account_id)) != NULL)
    {                           //Only mark it clean if it's not in use. [Skotlex]
        if (stor->dirty && stor->storage_status == 0)
        {
            stor->dirty = 0;
            sortage_sortitem(stor);
        }
        return 1;
    }
    return 0;
}
