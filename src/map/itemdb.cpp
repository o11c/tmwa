#include "itemdb.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/db.hpp"
#include "../common/grfio.hpp"
#include "../common/nullpo.hpp"
#include "map.hpp"
#include "battle.hpp"
#include "script.hpp"
#include "pc.hpp"
#include "../common/socket.hpp"
#include "../common/mt_rand.hpp"

#define MAX_RANDITEM    2000

static struct dbt *item_db;

static struct random_item_data blue_box[MAX_RANDITEM],
    violet_box[MAX_RANDITEM], card_album[MAX_RANDITEM],
    gift_box[MAX_RANDITEM], scroll[MAX_RANDITEM];
static int blue_box_count = 0, violet_box_count = 0, card_album_count =
    0, gift_box_count = 0, scroll_count = 0;
static int blue_box_default = 0, violet_box_default = 0, card_album_default =
    0, gift_box_default = 0, scroll_default = 0;

// Function declarations

static void itemdb_read(void);
static int itemdb_readdb(void);
static int itemdb_read_randomitem(void);
static int itemdb_read_itemavail(void);
static int itemdb_read_noequip(void);

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
 * 箱系アイテム検索
 *------------------------------------------
 */
int itemdb_searchrandomid(int flags)
{
    int nameid = 0, i, idx, count;
    struct random_item_data *list = NULL;

    struct
    {
        int nameid, count;
        struct random_item_data *list;
    } data[] =
    {
        {0, 0, NULL},
        {blue_box_default, blue_box_count, blue_box},
        {violet_box_default, violet_box_count, violet_box},
        {card_album_default, card_album_count, card_album},
        {gift_box_default, gift_box_count, gift_box},
        {scroll_default, scroll_count, scroll},
    };

    if (flags >= 1 && flags <= 5)
    {
        nameid = data[flags].nameid;
        count = data[flags].count;
        list = data[flags].list;

        if (count > 0)
        {
            for (i = 0; i < 1000; i++)
            {
                idx = MRAND(count);
                if (MRAND(1000000) < list[idx].per)
                {
                    nameid = list[idx].nameid;
                    break;
                }
            }
        }
    }
    return nameid;
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
static int itemdb_readdb(void)
{
    FILE *fp;
    char line[1024];
    int ln = 0, lines = 0;
    int nameid, j;
    char *str[32];
    char *p, *np;
    struct item_data *id;
    int i = 0;
    const char *filename[] = { "db/item_db.txt", "db/item_db2.txt" };

    for (i = 0; i < 2; i++)
    {

        fp = fopen_(filename[i], "r");
        if (fp == NULL)
        {
            if (i > 0)
                continue;
            printf("can't read %s\n", filename[i]);
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
        printf("read %s done (count=%d)\n", filename[i], ln);
    }
    return 0;
}

// Removed item_value_db, don't re-add!

/*==========================================
 * ランダムアイテム出現データの読み込み
 *------------------------------------------
 */
static int itemdb_read_randomitem(void)
{
    FILE *fp;
    char line[1024];
    int ln = 0;
    int nameid, i, j;
    char *str[10], *p;

    const struct
    {
        char filename[64];
        struct random_item_data *pdata;
        int *pcount, *pdefault;
    } data[] =
    {
        {
        "db/item_bluebox.txt", blue_box, &blue_box_count,
                &blue_box_default},
        {
        "db/item_violetbox.txt", violet_box, &violet_box_count,
                &violet_box_default},
        {
        "db/item_cardalbum.txt", card_album, &card_album_count,
                &card_album_default},
        {
        "db/item_giftbox.txt", gift_box, &gift_box_count,
                &gift_box_default},
        {
    "db/item_scroll.txt", scroll, &scroll_count, &scroll_default},};

    for (i = 0; i < sizeof(data) / sizeof(data[0]); i++)
    {
        struct random_item_data *pd = data[i].pdata;
        int *pc = data[i].pcount;
        int *pdefault = data[i].pdefault;
        const char *fn = data[i].filename;

        *pdefault = 0;
        if ((fp = fopen_(fn, "r")) == NULL)
        {
            printf("can't read %s\n", fn);
            continue;
        }

        while (fgets(line, 1020, fp))
        {
            if (line[0] == '/' && line[1] == '/')
                continue;
            memset(str, 0, sizeof(str));
            for (j = 0, p = line; j < 3 && p; j++)
            {
                str[j] = p;
                p = strchr(p, ',');
                if (p)
                    *p++ = 0;
            }

            if (str[0] == NULL)
                continue;

            nameid = atoi(str[0]);
            if (nameid < 0 || nameid >= 20000)
                continue;
            if (nameid == 0)
            {
                if (str[2])
                    *pdefault = atoi(str[2]);
                continue;
            }

            if (str[2])
            {
                pd[*pc].nameid = nameid;
                pd[(*pc)++].per = atoi(str[2]);
            }

            if (ln >= MAX_RANDITEM)
                break;
            ln++;
        }
        fclose_(fp);
        printf("read %s done (count=%d)\n", fn, *pc);
    }

    return 0;
}

/*==========================================
 * アイテム使用可能フラグのオーバーライド
 *------------------------------------------
 */
static int itemdb_read_itemavail(void)
{
    FILE *fp;
    char line[1024];
    int ln = 0;
    int nameid, j;
    char *str[10], *p;

    if ((fp = fopen_("db/item_avail.txt", "r")) == NULL)
    {
        printf("can't read db/item_avail.txt\n");
        return -1;
    }

    while (fgets(line, 1020, fp))
    {
        struct item_data *id;
        if (line[0] == '/' && line[1] == '/')
            continue;
        memset(str, 0, sizeof(str));
        for (j = 0, p = line; j < 2 && p; j++)
        {
            str[j] = p;
            p = strchr(p, ',');
            if (p)
                *p++ = 0;
        }

        if (str[0] == NULL)
            continue;

        nameid = atoi(str[0]);
        if (nameid < 0 || nameid >= 20000 || !(id = itemdb_exists(nameid)))
            continue;

        id->flag.available = 0;
        ln++;
    }
    fclose_(fp);
    printf("read db/item_avail.txt done (count=%d)\n", ln);
    return 0;
}

/*==========================================
 * 装備制限ファイル読み出し
 *------------------------------------------
 */
static int itemdb_read_noequip(void)
{
    FILE *fp;
    char line[1024];
    int ln = 0;
    int nameid, j;
    char *str[32], *p;
    struct item_data *id;

    if ((fp = fopen_("db/item_noequip.txt", "r")) == NULL)
    {
        printf("can't read db/item_noequip.txt\n");
        return -1;
    }
    while (fgets(line, 1020, fp))
    {
        if (line[0] == '/' && line[1] == '/')
            continue;
        memset(str, 0, sizeof(str));
        for (j = 0, p = line; j < 2 && p; j++)
        {
            str[j] = p;
            p = strchr(p, ',');
            if (p)
                *p++ = 0;
        }
        if (str[0] == NULL)
            continue;

        nameid = atoi(str[0]);
        if (nameid <= 0 || nameid >= 20000 || !(id = itemdb_exists(nameid)))
            continue;

        id->flag.no_equip = atoi(str[1]);

        ln++;

    }
    fclose_(fp);
    printf("read db/item_noequip.txt done (count=%d)\n", ln);
    return 0;
}

/*
static FILE *dfp;
static int itemdebug(void *key,void *data,va_list ap){
//      struct item_data *id=(struct item_data *)data;
        fprintf(dfp,"%6d",(int)key);
        return 0;
}
void itemdebugtxt()
{
        dfp=fopen_("itemdebug.txt","wt");
        numdb_foreach(item_db,itemdebug);
        fclose_(dfp);
}
*/

/*====================================
 * Removed item_value_db, don't re-add
 *------------------------------------
 */
static void itemdb_read(void)
{
    itemdb_readdb();
    itemdb_read_randomitem();
    itemdb_read_itemavail();
    itemdb_read_noequip();
}

/*==========================================
 *
 *------------------------------------------
 */
int do_init_itemdb(void)
{
    item_db = numdb_init();

    itemdb_read();

    return 0;
}
