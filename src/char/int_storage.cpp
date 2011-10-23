#include "int_storage.hpp"

#include "../common/db.hpp"
#include "../common/lock.hpp"
#include "../common/socket.hpp"
#include "../common/utils.hpp"

#include "main.hpp"

char storage_txt[1024] = "save/storage.txt";

static struct dbt *storage_db;

/// Store items for one account, IF it is not empty
static void storage_tofile(FILE *fp, struct storage *p)
{
    for (sint32 i = 0; i < MAX_STORAGE; i++)
        if (p->storage_[i].nameid && p->storage_[i].amount)
            goto actually_store;
    return;

actually_store:
    FPRINTF(fp, "%u,%hu\t", p->account_id, p->storage_amount);

    for (sint32 i = 0; i < MAX_STORAGE; i++)
        if (p->storage_[i].nameid && p->storage_[i].amount)
        {
            fprintf(fp, "%d,%d,%d,"
                        "%d,%d,%d,%d,"
                        "%d,%d,%d,%d ",
                    0/*id*/, p->storage_[i].nameid, p->storage_[i].amount,
                    static_cast<uint16>(p->storage_[i].equip), 0/*identify*/,
                    0/*refine*/, 0/*attribute*/,
                    0/*card[0]*/, 0/*card[1]*/,
                    0/*card[2]*/, 0/*card[3]*/);
            // Note: in storage, the "broken" field is not stored
        }
    fprintf(fp, "\n");
}

/// Load somebody's storage
static bool storage_fromstr(const char *str, struct storage *p)
{
    sint32 next;
    if (SSCANF(str, "%u,%hd%n", &p->account_id, &p->storage_amount, &next) != 2)
        return 1;
    str += next;
    if (str[0] == '\n' || str[0] == '\r')
        return 0;

    str++;

    if (p->storage_amount > MAX_STORAGE)
    {
        char_log.error("%s: more than %d items on line, %d items dropped", __func__,
                       MAX_STORAGE, p->storage_amount - MAX_STORAGE);
        p->storage_amount = MAX_STORAGE;
    }

    for (sint32 i = 0; i < p->storage_amount; i++)
    {
        if (sscanf(str, "%*d,%hd,%hd,"
                        "%hu,%*d,%*d,%*d,"
                        "%*d,%*d,%*d,%*d%n",
                   /*id,*/ &p->storage_[i].nameid, &p->storage_[i].amount,
                   reinterpret_cast<uint16 *>(&p->storage_[i].equip), /*identify,*/
                   /*refine,*/ /*attribute,*/
                   /*card[0],*/ /*card[1],*/
                   /*card[2],*/ /*card[3],*/
                   &next) != 11)
            return 1;
        str += next;
        if (str[0] == ' ')
            str++;
    }

    return 0;
}

/// Get the storage of an account, creating if it does not exist
struct storage *account2storage(account_t account_id)
{
    struct storage *s = reinterpret_cast<struct storage *>(numdb_search(storage_db, static_cast<numdb_key_t>(account_id)).p);
    if (!s)
    {
        CREATE(s, struct storage, 1);
        s->account_id = account_id;
        numdb_insert(storage_db, static_cast<numdb_key_t>(s->account_id), static_cast<void *>(s));
    }
    return s;
}


/// Read all storage data
bool inter_storage_init(void)
{
    storage_db = numdb_init();

    FILE *fp = fopen_(storage_txt, "r");
    if (!fp)
    {
        printf("cant't read : %s\n", storage_txt);
        return 1;
    }
    char line[65536];
    sint32 c = 0;
    while (fgets(line, sizeof(line), fp))
    {
        c++;
        struct storage *s;
        CREATE(s, struct storage, 1);
        if (storage_fromstr(line, s) == 0)
        {
            numdb_insert(storage_db, static_cast<numdb_key_t>(s->account_id), static_cast<void *>(s));
        }
        else
        {
            printf("int_storage: broken data [%s] line %d\n", storage_txt, c);
            free(s);
        }
    }
    fclose_(fp);
    return 0;
}

static void storage_db_final(db_key_t, db_val_t data)
{
    free(data.p);
}

void inter_storage_final(void)
{
    numdb_final(storage_db, storage_db_final);
}

/// Save somebody's storage
static void inter_storage_save_sub(db_key_t, db_val_t data, FILE *fp)
{
    storage_tofile(fp, reinterpret_cast<struct storage *>(data.p));
}

/// Save everybody's storage
bool inter_storage_save(void)
{
    if (!storage_db)
        return 1;

    sint32 lock;
    FILE *fp = lock_fopen(storage_txt, &lock);
    if (!fp)
    {
        char_log.error("int_storage: %s: %m\n", storage_txt);
        return 1;
    }
    numdb_foreach(storage_db, inter_storage_save_sub, fp);
    lock_fclose(fp, storage_txt, &lock);
    return 0;
}

/// Delete somebody's storage
void inter_storage_delete(account_t account_id)
{
    struct storage *s = reinterpret_cast<struct storage *>(numdb_search(storage_db, static_cast<numdb_key_t>(account_id)).p);
    if (s)
    {
        numdb_erase(storage_db, static_cast<numdb_key_t>(account_id));
        free(s);
    }
}



/// Give map server the storage info
static void mapif_load_storage(sint32 fd)
{
    account_t account_id = account_t(WFIFOL(fd, 2));
    struct storage *s = account2storage(account_id);
    WFIFOW(fd, 0) = 0x3810;
    WFIFOW(fd, 2) = sizeof(struct storage) + 8;
    WFIFOL(fd, 4) = uint32(account_id);
    *reinterpret_cast<struct storage *>(WFIFOP(fd, 8)) = *s;
    WFIFOSET(fd, WFIFOW(fd, 2));
}



/// The map server updates storage
static void mapif_parse_save_storage(sint32 fd)
{
    uint16 len = RFIFOW(fd, 2);
    if (sizeof(struct storage) != len - 8)
    {
        char_log.error("inter storage: data size error %d %d\n",
                       sizeof(struct storage), len - 8);
        return;
    }
    account_t account_id = account_t(RFIFOL(fd, 4));
    struct storage *s = account2storage(account_id);
    *s = *reinterpret_cast<const struct storage*>(RFIFOP(fd, 8));

    WFIFOW(fd, 0) = 0x3811;
    WFIFOL(fd, 2) = uint32(account_id);
    WFIFOB(fd, 6) = 0;
    WFIFOSET(fd, 7);
}

/// Parse one packet from the map server
// return 1 if we handled it
bool inter_storage_parse_frommap(sint32 fd)
{
    switch (RFIFOW(fd, 0))
    {
    case 0x3010:
        mapif_load_storage(fd);
        return 1;
    case 0x3011:
        mapif_parse_save_storage(fd);
        return 1;
    default:
        return 0;
    }
}