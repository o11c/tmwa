#include "int_storage.hpp"

#include <string.h>
#include <stdlib.h>

#include "../common/mmo.hpp"
#include "../common/socket.hpp"
#include "../common/db.hpp"
#include "../common/lock.hpp"
#include "char.hpp"
#include "inter.hpp"

char storage_txt[1024] = "save/storage.txt";

static struct dbt *storage_db;

/// Store items for one account, IF it is not empty
static void storage_tofile (FILE *fp, struct storage *p)
{
    for (int i = 0; i < MAX_STORAGE; i++)
        if (p->storage_[i].nameid && p->storage_[i].amount)
            goto actually_store;
    return;

actually_store:
    fprintf (fp, "%u,%hu\t", p->account_id, p->storage_amount);

    for (int i = 0; i < MAX_STORAGE; i++)
        if (p->storage_[i].nameid && p->storage_[i].amount)
        {
            fprintf (fp, "%d,%d,%d,"
                         "%d,%d,%d,%d,"
                         "%d,%d,%d,%d ",
                     p->storage_[i].id, p->storage_[i].nameid, p->storage_[i].amount,
                     p->storage_[i].equip, p->storage_[i].identify,
                     p->storage_[i].refine,p->storage_[i].attribute,
                     p->storage_[i].card[0], p->storage_[i].card[1],
                     p->storage_[i].card[2], p->storage_[i].card[3]);
        }
    fprintf (fp, "\n");
}

/// Load somebody's storage
static bool storage_fromstr (const char *str, struct storage *p)
{
    int next;
    if (sscanf (str, "%u,%hd%n", &p->account_id, &p->storage_amount, &next) != 2)
        return 1;
    str += next;
    if (str[0] == '\n' || str[0] == '\r')
        return 0;

    str++;

    if (p->storage_amount > MAX_STORAGE)
    {
        char_log ("%s: more than %d items on line, %d items dropped", __func__,
                  MAX_STORAGE, p->storage_amount - MAX_STORAGE);
        p->storage_amount = MAX_STORAGE;
    }

    for (int i = 0; i < p->storage_amount; i++)
    {
        if (sscanf (str, "%d,%hd,%hd,"
                         "%hu,%hhd,%hhd,%hhd,"
                         "%hd,%hd,%hd,%hd%n",
                    &p->storage_[i].id, &p->storage_[i].nameid, &p->storage_[i].amount,
                    &p->storage_[i].equip, &p->storage_[i].identify,
                    &p->storage_[i].refine, &p->storage_[i].attribute,
                    &p->storage_[i].card[0], &p->storage_[i].card[1],
                    &p->storage_[i].card[2], &p->storage_[i].card[3],
                    &next) != 11)
            return 1;
        str += next;
        if (str[0] == ' ')
            str++;
    }

    return 0;
}

/// Get the storage of an account, creating if it does not exist
struct storage *account2storage (account_t account_id)
{
    struct storage *s = (struct storage *) numdb_search (storage_db, (numdb_key_t)account_id).p;
    if (!s)
    {
        CREATE (s, struct storage, 1);
        s->account_id = account_id;
        numdb_insert (storage_db, (numdb_key_t)s->account_id, (void *)s);
    }
    return s;
}


/// Read all storage data
bool inter_storage_init (void)
{
    storage_db = numdb_init ();

    FILE *fp = fopen_ (storage_txt, "r");
    if (!fp)
    {
        printf ("cant't read : %s\n", storage_txt);
        return 1;
    }
    char line[65536];
    int c = 0;
    while (fgets (line, sizeof (line), fp))
    {
        c++;
        struct storage *s;
        CREATE (s, struct storage, 1);
        if (storage_fromstr (line, s) == 0)
        {
            numdb_insert (storage_db, (numdb_key_t)s->account_id, (void *)s);
        }
        else
        {
            printf ("int_storage: broken data [%s] line %d\n", storage_txt, c);
            free (s);
        }
    }
    fclose_ (fp);
    return 0;
}

static void storage_db_final (db_key_t UNUSED, db_val_t data, va_list UNUSED)
{
    free (data.p);
}

void inter_storage_final (void)
{
    numdb_final (storage_db, storage_db_final);
}

/// Save somebody's storage
static void inter_storage_save_sub (db_key_t UNUSED, db_val_t data, va_list ap)
{
    FILE *fp = va_arg (ap, FILE *);
    storage_tofile (fp, (struct storage *) data.p);
}

/// Save everybody's storage
bool inter_storage_save (void)
{
    if (!storage_db)
        return 1;

    int lock;
    FILE *fp = lock_fopen (storage_txt, &lock);
    if (!fp)
    {
        char_log ("int_storage: %s: %m\n", storage_txt);
        return 1;
    }
    numdb_foreach (storage_db, inter_storage_save_sub, fp);
    lock_fclose (fp, storage_txt, &lock);
    return 0;
}

/// Delete somebody's storage
void inter_storage_delete (account_t account_id)
{
    struct storage *s = (struct storage *) numdb_search (storage_db, (numdb_key_t)account_id).p;
    if (s)
    {
        numdb_erase (storage_db, (numdb_key_t)account_id);
        free (s);
    }
}



/// Give map server the storage info
static void mapif_load_storage (int fd)
{
    account_t account_id = WFIFOL (fd, 2);
    struct storage *s = account2storage (account_id);
    WFIFOW (fd, 0) = 0x3810;
    WFIFOW (fd, 2) = sizeof (struct storage) + 8;
    WFIFOL (fd, 4) = account_id;
    *(struct storage *)WFIFOP (fd, 8) = *s;
    WFIFOSET (fd, WFIFOW (fd, 2));
}



/// The map server updates storage
static void mapif_parse_save_storage (int fd)
{
    uint16_t len = RFIFOW (fd, 2);
    if (sizeof (struct storage) != len - 8)
    {
        char_log ("inter storage: data size error %d %d\n",
                  sizeof (struct storage), len - 8);
        return;
    }
    account_t account_id = RFIFOL (fd, 4);
    struct storage *s = account2storage (account_id);
    *s = *(struct storage*) RFIFOP (fd, 8);

    WFIFOW (fd, 0) = 0x3811;
    WFIFOL (fd, 2) = account_id;
    WFIFOB (fd, 6) = 0;
    WFIFOSET (fd, 7);
}

/// Parse one packet from the map server
// return 1 if we handled it
bool inter_storage_parse_frommap (int fd)
{
    switch (RFIFOW (fd, 0))
    {
    case 0x3010:
        mapif_load_storage (fd);
        return 1;
    case 0x3011:
        mapif_parse_save_storage (fd);
        return 1;
    default:
        return 0;
    }
}
