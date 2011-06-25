#include "itemdb.hpp"

#include "../common/db.hpp"
#include "../common/socket.hpp"
#include "../common/utils.hpp"

#include "script.hpp"

#define MAX_RANDITEM    2000

static struct dbt *item_db;

// Function declarations

static void itemdb_readdb(void);

/*==========================================
 * 名前で検索用
 *------------------------------------------
 */
// name = item alias, so we should find items aliases first. if not found then look for "jname" (full name)
static void itemdb_searchname_sub(db_key_t, db_val_t data, const char *str, struct item_data **dst)
{
    struct item_data *item = static_cast<struct item_data *>(data.p);
    if (strcasecmp(item->name, str) == 0)
        *dst = item;
}

/*==========================================
 * 名前で検索
 *------------------------------------------
 */
struct item_data *itemdb_searchname(const char *str)
{
    struct item_data *item = NULL;
    numdb_foreach(item_db, itemdb_searchname_sub, str, &item);
    return item;
}

/*==========================================
 * DBの存在確認
 *------------------------------------------
 */
struct item_data *itemdb_exists(int nameid)
{
    return static_cast<struct item_data *>(numdb_search(item_db, nameid).p);
}

/*==========================================
 * DBの検索
 *------------------------------------------
 */
struct item_data *itemdb_search(int nameid)
{
    struct item_data *id = static_cast<struct item_data *>(numdb_search(item_db, nameid).p);
    if (id)
        return id;

    CREATE(id, struct item_data, 1);
    numdb_insert(item_db, nameid, static_cast<void *>(id));

    id->nameid = nameid;
    id->value_buy = 10;
    id->value_sell = id->value_buy / 2;
    id->weight = 10;
    id->sex = 2;
    id->elv = 0;
    id->flag.available = 0;
    id->flag.no_equip = 0;

    if (nameid > 500 && nameid < 600)
        id->type = 0;           //heal item
    else if (nameid > 600 && nameid < 700)
        id->type = 2;           //use item
    else if ((nameid > 700 && nameid < 1100) ||
             (nameid > 7000 && nameid < 8000))
        id->type = 3;           //correction
    else if (nameid >= 1750 && nameid < 1771)
        id->type = 10;          //arrow
    else if (nameid > 1100 && nameid < 2000)
        id->type = 4;           //weapon
    else if ((nameid > 2100 && nameid < 3000) ||
             (nameid > 5000 && nameid < 6000))
        id->type = 5;           //armor
    else if (nameid > 4000 && nameid < 5000)
        id->type = 6;           //card

    return id;
}

/*==========================================
 *
 *------------------------------------------
 */
int itemdb_isequip(int nameid)
{
    int type = itemdb_type(nameid);
    if (type == 0 || type == 2 || type == 3 || type == 6 || type == 10)
        return 0;
    return 1;
}

/*==========================================
 *
 *------------------------------------------
 */
int itemdb_isequip2(struct item_data *data)
{
    if (data)
    {
        int type = data->type;
        if (type == 0 || type == 2 || type == 3 || type == 6 || type == 10)
            return 0;
        else
            return 1;
    }
    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int itemdb_isequip3(int nameid)
{
    int type = itemdb_type(nameid);
    if (type == 4 || type == 5 || type == 8)
        return 1;
    return 0;
}

/*==========================================
 * アイテムデータベースの読み込み
 *------------------------------------------
 */
static void itemdb_readdb(void)
{
    FILE *fp;
    char line[1024];
    int ln = 0, lines = 0;
    int nameid, j;
    char *str[32];
    char *p, *np;
    struct item_data *id;
    const char *filename = "db/item_db.txt";

    {

        fp = fopen_(filename, "r");
        if (fp == NULL)
        {
            printf("can't read %s\n", filename);
            exit(1);
        }

        lines = 0;
        while (fgets(line, 1020, fp))
        {
            lines++;
            if (line[0] == '/' && line[1] == '/')
                continue;
            memset(str, 0, sizeof(str));
            for (j = 0, np = p = line; j < 17 && p; j++)
            {
                while (*p == '\t' || *p == ' ')
                    p++;
                str[j] = p;
                p = strchr(p, ',');
                if (p)
                {
                    *p++ = 0;
                    np = p;
                }
            }
            if (str[0] == NULL)
                continue;

            nameid = atoi(str[0]);
            if (nameid <= 0 || nameid >= 20000)
                continue;
            ln++;

            //ID,Name,Jname,Type,Price,Sell,Weight,ATK,DEF,Range,Slot,Job,Gender,Loc,wLV,eLV,View
            id = itemdb_search(nameid);
            memcpy(id->name, str[1], 24);
            memcpy(id->jname, str[2], 24);
            id->type = atoi(str[3]);
            id->value_buy = atoi(str[4]);
            id->value_sell = atoi(str[5]);
            if (id->value_buy == 0 && id->value_sell == 0)
            {
            }
            else if (id->value_buy == 0)
            {
                id->value_buy = id->value_sell * 2;
            }
            else if (id->value_sell == 0)
            {
                id->value_sell = id->value_buy / 2;
            }
            id->weight = atoi(str[6]);
            id->atk = atoi(str[7]);
            id->def = atoi(str[8]);
            id->range = atoi(str[9]);
            id->magic_bonus = atoi(str[10]);
            id->slot = atoi(str[11]);
            id->sex = atoi(str[12]);
            id->equip = atoi(str[13]);
            id->wlv = atoi(str[14]);
            id->elv = atoi(str[15]);
            id->look = atoi(str[16]);
            id->flag.available = 1;

            id->use_script = NULL;
            id->equip_script = NULL;

            if ((p = strchr(np, '{')) == NULL)
                continue;
            id->use_script = parse_script(p, lines);

            if ((p = strchr(p + 1, '{')) == NULL)
                continue;
            id->equip_script = parse_script(p, lines);
        }
        fclose_(fp);
        printf("read %s done (count=%d)\n", filename, ln);
    }
}

/// Initialize the items
void do_init_itemdb(void)
{
    item_db = numdb_init();
    itemdb_readdb();
}
