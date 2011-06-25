#include "npc.hpp"

#include "../common/nullpo.hpp"
#include "../common/timer.hpp"
#include "../common/utils.hpp"

#include "battle.hpp"
#include "clif.hpp"
#include "itemdb.hpp"
#include "map.hpp"
#include "mob.hpp"
#include "pc.hpp"
#include "script.hpp"

static void npc_event_timer(timer_id, tick_t, uint32_t, char *);
static int npc_checknear(MapSessionData *, int);
static int npc_parse_mob(char *w1, char *w2, char *w3, char *w4);


struct npc_src_list
{
    struct npc_src_list *next;
    struct npc_src_list *prev;
    char name[4];
};

static struct npc_src_list *npc_src_first, *npc_src_last;
static int npc_next_id = START_NPC_NUM;
static int npc_warp, npc_shop, npc_script, npc_mob;

int npc_get_new_npc_id(void)
{
    return npc_next_id++;
}

static struct dbt *ev_db;
static struct dbt *npcname_db;

struct event_data
{
    struct npc_data_script *nd;
    int pos;
};
static struct tm ev_tm_b;       // 時計イベント用

/*==========================================
 * NPCの無効化/有効化
 * npc_enable
 * npc_enable_sub 有効時にOnTouchイベントを実行
 *------------------------------------------
 */
static void npc_enable_sub(BlockList *bl, struct npc_data_script *nd)
{
    nullpo_retv(bl);
    nullpo_retv(nd);

    char *name;
    CREATE(name, char, 50);

    if (bl->type == BL_PC)
    {
        MapSessionData *sd = static_cast<MapSessionData *>(bl);
        if (nd->flag & 1)       // 無効化されている
            return;

        memcpy(name, nd->name, sizeof(nd->name));
        if (sd->areanpc_id == nd->id)
            return;
        sd->areanpc_id = nd->id;
        npc_event(sd, strcat(name, "::OnTouch"), 0);
    }
    free(name);
}

int npc_enable(const char *name, int flag)
{
    struct npc_data *nd = static_cast<struct npc_data *>(strdb_search(npcname_db, name).p);
    if (nd == NULL)
        return -1;

    if (flag & 1)
    {                           // 有効化
        nd->flag &= ~1;
        clif_spawnnpc(nd);
    }
    else if (flag & 2)
    {
        nd->flag &= ~1;
        nd->option = 0x0000;
        clif_changeoption(nd);
    }
    else if (flag & 4)
    {
        nd->flag |= 1;
        nd->option = 0x0002;
        clif_changeoption(nd);
    }
    else
    {                           // 無効化
        nd->flag |= 1;
        clif_being_remove(nd, BeingRemoveType::ZERO);
    }
    if (flag & 3)
    {
        npc_data_script *nds = static_cast<npc_data_script *>(nd);
        if (nds->scr.xs > 0 || nds->scr.ys > 0)
            map_foreachinarea(npc_enable_sub, nds->m,
                              nds->x - nds->scr.xs, nds->y - nds->scr.ys,
                              nds->x + nds->scr.xs, nds->y + nds->scr.ys,
                              BL_PC, nds);
    }

    return 0;
}

/*==========================================
 * NPCを名前で探す
 *------------------------------------------
 */
struct npc_data *npc_name2id(const char *name)
{
    return static_cast<struct npc_data *>(strdb_search(npcname_db, name).p);
}

/*==========================================
 * イベントキューのイベント処理
 *------------------------------------------
 */
int npc_event_dequeue(MapSessionData *sd)
{
    nullpo_ret(sd);

    sd->npc_id = 0;

    if (sd->eventqueue[0][0]) // キューのイベント処理
    {
        if (!pc_addeventtimer(sd, 100, sd->eventqueue[0]))
        {
            printf("npc_event_dequeue(): Event timer is full.\n");
            return 0;
        }

        if (MAX_EVENTQUEUE > 1)
            memmove(sd->eventqueue[0], sd->eventqueue[1],
                     (MAX_EVENTQUEUE - 1) * sizeof(sd->eventqueue[0]));
        sd->eventqueue[MAX_EVENTQUEUE - 1][0] = '\0';
        return 1;
    }

    return 0;
}

/*==========================================
 * イベントの遅延実行
 *------------------------------------------
 */
void npc_event_timer(timer_id, tick_t, uint32_t id, char *data)
{
    MapSessionData *sd = map_id2sd(id);
    if (sd == NULL)
        return;

    npc_event(sd, data, 0);
    free(data);
}

/*==========================================
 * 全てのNPCのOn*イベント実行
 *------------------------------------------
 */
static void npc_event_doall_sub(db_key_t key, db_val_t data, int *c, const char *name, int rid, int argc, argrec_t *argv)
{
    const char *p = key.s;
    struct event_data *ev;

    nullpo_retv(ev = static_cast<struct event_data *>(data.p));
    nullpo_retv(c);

    if ((p = strchr(p, ':')) && p && strcasecmp(name, p) == 0)
    {
        run_script_l(ev->nd->scr.script, ev->pos, rid, ev->nd->id, argc,
                      argv);
        (*c)++;
    }
}

int npc_event_doall_l(const char *name, int rid, int argc, argrec_t * args)
{
    int c = 0;
    char buf[64] = "::";

    strncpy(buf + 2, name, sizeof(buf)-3);
    buf[sizeof(buf)-1] = '\0';
    strdb_foreach(ev_db, npc_event_doall_sub, &c, static_cast<const char *>(buf), rid, argc, args);
    return c;
}

static void npc_event_do_sub(db_key_t key, db_val_t data, int *c, const char *name, int rid, int argc, argrec_t *argv)
{
    const char *p = key.s;
    struct event_data *ev;

    nullpo_retv(ev = static_cast<struct event_data *>(data.p));
    nullpo_retv(c);

    if (p && strcasecmp(name, p) == 0)
    {
        run_script_l(ev->nd->scr.script, ev->pos, rid, ev->nd->id, argc,
                      argv);
        (*c)++;
    }
}

int npc_event_do_l(const char *name, int rid, int argc, argrec_t * args)
{
    int c = 0;

    if (*name == ':' && name[1] == ':')
    {
        return npc_event_doall_l(name + 2, rid, argc, args);
    }

    strdb_foreach(ev_db, npc_event_do_sub, &c, name, rid, argc, args);
    return c;
}

/*==========================================
 * 時計イベント実行
 *------------------------------------------
 */
static void npc_event_do_clock(timer_id, tick_t)
{
    time_t timer;
    struct tm *t;
    char buf[64];
    int c = 0;

    time(&timer);
    t = gmtime(&timer);

    if (t->tm_min != ev_tm_b.tm_min)
    {
        sprintf(buf, "OnMinute%02d", t->tm_min);
        c += npc_event_doall(buf);
        sprintf(buf, "OnClock%02d%02d", t->tm_hour, t->tm_min);
        c += npc_event_doall(buf);
    }
    if (t->tm_hour != ev_tm_b.tm_hour)
    {
        sprintf(buf, "OnHour%02d", t->tm_hour);
        c += npc_event_doall(buf);
    }
    if (t->tm_mday != ev_tm_b.tm_mday)
    {
        sprintf(buf, "OnDay%02d%02d", t->tm_mon + 1, t->tm_mday);
        c += npc_event_doall(buf);
    }
    memcpy(&ev_tm_b, t, sizeof(ev_tm_b));
}

/*==========================================
 * OnInitイベント実行(&時計イベント開始)
 *------------------------------------------
 */
int npc_event_do_oninit(void)
{
    int c = npc_event_doall("OnInit");
    printf("npc: OnInit Event done. (%d npc)\n", c);

    add_timer_interval(gettick() + 100, 1000, npc_event_do_clock);

    return 0;
}

/*==========================================
 * OnTimer NPC event - by RoVeRT
 *------------------------------------------
 */
static int npc_addeventtimer(struct npc_data *nd, int tick, const char *name)
{
    int i;
    for (i = 0; i < MAX_EVENTTIMER; i++)
        if (nd->eventtimer[i].tid == NULL)
            break;
    if (i < MAX_EVENTTIMER)
    {
        char *evname;
        CREATE(evname, char, 24);
        memcpy(evname, name, 24);
        nd->eventtimer[i].tid = add_timer(gettick() + tick, npc_event_timer,
                                          nd->id, evname);
        nd->eventtimer[i].name = evname;
    }
    else
        printf("npc_addtimer: event timer is full !\n");

    return 0;
}

static int npc_deleventtimer(struct npc_data *nd, const char *name)
{
    int i;
    for (i = 0; i < MAX_EVENTTIMER; i++)
        if (nd->eventtimer[i].tid && strcmp(nd->eventtimer[i].name, name) == 0)
        {
            delete_timer(nd->eventtimer[i].tid);
            nd->eventtimer[i].tid = NULL;
            break;
        }

    return 0;
}

static void npc_do_ontimer_sub(db_key_t key, db_val_t data, int *c, MapSessionData *, bool option)
{
    const char *p = key.s;
    struct event_data *ev = static_cast<struct event_data *>(data.p);
    int tick = 0;
    char temp[10];
    char event[50];

    if (ev->nd->id == *c && (p = strchr(p, ':')) && p
        && strncasecmp("::OnTimer", p, 8) == 0)
    {
        sscanf(&p[9], "%s", temp);
        tick = atoi(temp);

        strcpy(event, ev->nd->name);
        strcat(event, p);

        if (option != 0)
        {
            npc_addeventtimer(ev->nd, tick, event);
        }
        else
        {
            npc_deleventtimer(ev->nd, event);
        }
    }
}

int npc_do_ontimer(int npc_id, MapSessionData *sd, bool option)
{
    strdb_foreach(ev_db, npc_do_ontimer_sub, &npc_id, sd, option);
    return 0;
}

/*==========================================
 * タイマーイベント実行
 *------------------------------------------
 */
static void npc_timerevent(timer_id, tick_t tick, uint32_t id, int data)
{
    int next, t;
    struct npc_data_script *nd = static_cast<struct npc_data_script *>(map_id2bl(id));
    struct npc_timerevent_list *te;
    if (nd == NULL || nd->scr.nexttimer < 0)
    {
        printf("npc_timerevent: ??\n");
        return;
    }
    nd->scr.timertick = tick;
    te = nd->scr.timer_event + nd->scr.nexttimer;
    nd->scr.timerid = NULL;

    t = nd->scr.timer += data;
    nd->scr.nexttimer++;
    if (nd->scr.timeramount > nd->scr.nexttimer)
    {
        next = nd->scr.timer_event[nd->scr.nexttimer].timer - t;
        nd->scr.timerid = add_timer(tick + next, npc_timerevent, id, next);
    }

    run_script(nd->scr.script, te->pos, 0, nd->id);
}

/*==========================================
 * タイマーイベント開始
 *------------------------------------------
 */
int npc_timerevent_start(struct npc_data_script *nd)
{
    int j, n, next;

    nullpo_ret(nd);

    n = nd->scr.timeramount;
    if (nd->scr.nexttimer >= 0 || n == 0)
        return 0;

    for (j = 0; j < n; j++)
    {
        if (nd->scr.timer_event[j].timer > nd->scr.timer)
            break;
    }
    nd->scr.nexttimer = j;
    nd->scr.timertick = gettick();

    if (j >= n)
        return 0;

    next = nd->scr.timer_event[j].timer - nd->scr.timer;
    nd->scr.timerid = add_timer(gettick() + next, npc_timerevent, nd->id, next);
    return 0;
}

/*==========================================
 * タイマーイベント終了
 *------------------------------------------
 */
int npc_timerevent_stop(struct npc_data_script *nd)
{
    nullpo_ret(nd);

    if (nd->scr.nexttimer >= 0)
    {
        nd->scr.nexttimer = -1;
        nd->scr.timer += gettick() - nd->scr.timertick;
        if (nd->scr.timerid)
            delete_timer(nd->scr.timerid);
        nd->scr.timerid = NULL;
    }
    return 0;
}

/*==========================================
 * タイマー値の所得
 *------------------------------------------
 */
int npc_gettimerevent_tick(struct npc_data_script *nd)
{
    int tick;

    nullpo_ret(nd);

    tick = nd->scr.timer;

    if (nd->scr.nexttimer >= 0)
        tick += gettick() - nd->scr.timertick;
    return tick;
}

/*==========================================
 * タイマー値の設定
 *------------------------------------------
 */
int npc_settimerevent_tick(struct npc_data_script *nd, int newtimer)
{
    int flag;

    nullpo_ret(nd);

    flag = nd->scr.nexttimer;

    npc_timerevent_stop(nd);
    nd->scr.timer = newtimer;
    if (flag >= 0)
        npc_timerevent_start(nd);
    return 0;
}

/*==========================================
 * イベント型のNPC処理
 *------------------------------------------
 */
int npc_event(MapSessionData *sd, const char *eventname,
               int mob_kill)
{
    struct event_data *ev = static_cast<struct event_data *>(strdb_search(ev_db, eventname).p);
    struct npc_data_script *nd;
    int xs, ys;
    char mobevent[100];

    if (sd == NULL)
    {
        printf("npc_event nullpo?\n");
    }

    if (ev == NULL && eventname
        && strcmp(((eventname) + strlen(eventname) - 9), "::OnTouch") == 0)
        return 1;

    if (ev == NULL || (nd = ev->nd) == NULL)
    {
        if (mob_kill && (ev == NULL || (nd = ev->nd) == NULL))
        {
            strcpy(mobevent, eventname);
            strcat(mobevent, "::OnMyMobDead");
            ev = static_cast<struct event_data *>(strdb_search(ev_db, mobevent).p);
            if (ev == NULL || (nd = ev->nd) == NULL)
            {
                if (strncasecmp(eventname, "GM_MONSTER", 10) != 0)
                    printf("npc_event: event not found [%s]\n", mobevent);
                return 0;
            }
        }
        else
        {
            map_log("npc_event:map_log event not found [%s]\n", eventname);
            return 0;
        }
    }

    xs = nd->scr.xs;
    ys = nd->scr.ys;
    if (xs >= 0 && ys >= 0)
    {
        if (nd->m != sd->m)
            return 1;
        if (xs > 0
            && (sd->x < nd->x - xs / 2 || nd->x + xs / 2 < sd->x))
            return 1;
        if (ys > 0
            && (sd->y < nd->y - ys / 2 || nd->y + ys / 2 < sd->y))
            return 1;
    }

    if (sd->npc_id != 0)
    {
//      if (battle_config.error_log)
//          printf("npc_event: npc_id != 0\n");
        int i;
        for (i = 0; i < MAX_EVENTQUEUE; i++)
            if (!sd->eventqueue[i][0])
                break;
        if (i == MAX_EVENTQUEUE)
        {
            map_log("npc_event:map_log event queue is full !\n");
        }
        else
        {
//          if (battle_config.etc_log)
//              printf("npc_event: enqueue\n");
            strncpy(sd->eventqueue[i], eventname, 50);
            sd->eventqueue[i][49] = '\0';
        }
        return 1;
    }
    if (nd->flag & 1)
    {                           // 無効化されている
        npc_event_dequeue(sd);
        return 0;
    }

    sd->npc_id = nd->id;
    sd->npc_pos = run_script(nd->scr.script, ev->pos, sd->id, nd->id);
    return 0;
}

static void npc_command_sub(db_key_t key, db_val_t data, const char *npcname, const char *command)
{
    const char *p = key.s;
    struct event_data *ev = static_cast<struct event_data *>(data.p);
    char temp[100];

    if (strcmp(ev->nd->name, npcname) == 0 && (p = strchr(p, ':')) && p
        && strncasecmp("::OnCommand", p, 10) == 0)
    {
        sscanf(&p[11], "%s", temp);

        if (strcmp(command, temp) == 0)
            run_script(ev->nd->scr.script, ev->pos, 0, ev->nd->id);
    }
}

int npc_command(MapSessionData *, const char *npcname, const char *command)
{
    strdb_foreach(ev_db, npc_command_sub, npcname, command);

    return 0;
}

/*==========================================
 * 接触型のNPC処理
 *------------------------------------------
 */
int npc_touch_areanpc(MapSessionData *sd, int m, int x, int y)
{
    int i, f = 1;
    int xs, ys;

    nullpo_retr(1, sd);

    if (sd->npc_id)
        return 1;

    for (i = 0; i < maps[m].npc_num; i++)
    {
        if (maps[m].npc[i]->flag & 1)
        {                       // 無効化されている
            f = 0;
            continue;
        }

        switch (maps[m].npc[i]->subtype)
        {
            case WARP:
                xs = static_cast<npc_data_warp *>(maps[m].npc[i])->warp.xs;
                ys = static_cast<npc_data_warp *>(maps[m].npc[i])->warp.ys;
                break;
            case MESSAGE:
                break;
            case SCRIPT:
                xs = static_cast<npc_data_script *>(maps[m].npc[i])->scr.xs;
                ys = static_cast<npc_data_script *>(maps[m].npc[i])->scr.ys;
                break;
            default:
                continue;
        }
        if (x >= maps[m].npc[i]->x - xs / 2
            && x < maps[m].npc[i]->x - xs / 2 + xs
            && y >= maps[m].npc[i]->y - ys / 2
            && y < maps[m].npc[i]->y - ys / 2 + ys)
            break;
    }
    if (i == maps[m].npc_num)
    {
        if (f)
        {
            map_log("npc_touch_map_logareanpc : some bug \n");
        }
        return 1;
    }
    switch (maps[m].npc[i]->subtype)
    {
        case WARP:
            pc_setpos(sd, static_cast<npc_data_warp *>(maps[m].npc[i])->warp.dst,
                      BeingRemoveType::ZERO);
            break;
        case MESSAGE:
        case SCRIPT:
        {
            char *name;
            CREATE(name, char, 50);

            memcpy(name, maps[m].npc[i]->name, 50);
            if (sd->areanpc_id == maps[m].npc[i]->id)
                return 1;
            sd->areanpc_id = maps[m].npc[i]->id;
            if (npc_event(sd, strcat(name, "::OnTouch"), 0) > 0)
                npc_click(sd, maps[m].npc[i]->id);
            free(name);
            break;
        }
    }
    return 0;
}

/*==========================================
 * 近くかどうかの判定
 *------------------------------------------
 */
int npc_checknear(MapSessionData *sd, int id)
{
    struct npc_data *nd;

    nullpo_ret(sd);

    nd = static_cast<struct npc_data *>(map_id2bl(id));
    if (nd == NULL || nd->type != BL_NPC)
    {
        map_log("no sucmap_logh npc : %d\n", id);
        return 1;
    }

    if (nd->npc_class < 0)          // イベント系は常にOK
        return 0;

    // エリア判定
    if (nd->m != sd->m ||
        nd->x < sd->x - AREA_SIZE - 1
        || nd->x > sd->x + AREA_SIZE + 1
        || nd->y < sd->y - AREA_SIZE - 1
        || nd->y > sd->y + AREA_SIZE + 1)
        return 1;

    return 0;
}

/*==========================================
 * クリック時のNPC処理
 *------------------------------------------
 */
int npc_click(MapSessionData *sd, int id)
{
    struct npc_data *nd;

    nullpo_retr(1, sd);

    if (sd->npc_id != 0)
    {
        map_log("npc_clmap_logick: npc_id != 0\n");
        return 1;
    }

    if (npc_checknear(sd, id)) {
        clif_scriptclose(sd, id);
        return 1;
    }

    nd = static_cast<struct npc_data *>(map_id2bl(id));

    if (nd->flag & 1)           // 無効化されている
        return 1;

    sd->npc_id = id;
    switch (nd->subtype)
    {
        case SHOP:
            clif_npcbuysell(sd, id);
            npc_event_dequeue(sd);
            break;
        case SCRIPT:
            sd->npc_pos = run_script(static_cast<npc_data_script *>(nd)->scr.script, 0, sd->id, id);
            break;
        case MESSAGE:
            if (static_cast<npc_data_message *>(nd)->message)
            {
                clif_scriptmes(sd, id, static_cast<npc_data_message *>(nd)->message);
                clif_scriptclose(sd, id);
            }
            break;
    }

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int npc_scriptcont(MapSessionData *sd, int id)
{
    struct npc_data_script *nd;

    nullpo_retr(1, sd);

    if (id != sd->npc_id)
        return 1;
    if (npc_checknear(sd, id)) {
        clif_scriptclose(sd, id);
        return 1;
    }

    nd = static_cast<struct npc_data_script *>(map_id2bl(id));

    if (!nd /* NPC was disposed? */  || nd->subtype == MESSAGE)
    {
        clif_scriptclose(sd, id);
        npc_event_dequeue(sd);
        return 0;
    }

    sd->npc_pos = run_script(nd->scr.script, sd->npc_pos, sd->id, id);

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int npc_buysellsel(MapSessionData *sd, int id, int type)
{
    struct npc_data_shop *nd;

    nullpo_retr(1, sd);

    if (npc_checknear(sd, id))
        return 1;

    nd = static_cast<struct npc_data_shop *>(map_id2bl(id));
    if (nd->subtype != SHOP)
    {
        map_log("no such shop npc : %d\n", id);
        sd->npc_id = 0;
        return 1;
    }
    if (nd->flag & 1)           // 無効化されている
        return 1;

    sd->npc_shopid = id;
    if (type == 0)
    {
        clif_buylist(sd, nd);
    }
    else
    {
        clif_selllist(sd);
    }
    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int npc_buylist(MapSessionData *sd, int n,
                const unsigned short *item_list)
{
    struct npc_data_shop *nd;
    double z;
    int i, j, w, itemamount = 0, new_stacks = 0;

    nullpo_retr(3, sd);
    nullpo_retr(3, item_list);

    if (npc_checknear(sd, sd->npc_shopid))
        return 3;

    nd = static_cast<struct npc_data_shop *>(map_id2bl(sd->npc_shopid));
    if (nd->subtype != SHOP)
        return 3;

    for (i = 0, w = 0, z = 0; i < n; i++)
    {
        for (j = 0; j < nd->shop_item.size(); j++)
        {
            if (nd->shop_item[j].nameid == item_list[i * 2 + 1])
                break;
        }
        if (j == nd->shop_item.size())
            return 3;

        z += static_cast<double>(nd->shop_item[j].value) * item_list[i * 2];
        itemamount += item_list[i * 2];

        switch (pc_checkadditem(sd, item_list[i * 2 + 1], item_list[i * 2]))
        {
            case ADDITEM_EXIST:
                break;
            case ADDITEM_NEW:
                if (itemdb_isequip(item_list[i * 2 + 1]))
                    new_stacks += item_list[i * 2];
                else
                    new_stacks++;
                break;
            case ADDITEM_OVERAMOUNT:
                return 2;
        }

        w += itemdb_weight(item_list[i * 2 + 1]) * item_list[i * 2];
    }

    if (z > sd->status.zeny)
        return 1;               // zeny不足
    if (w + sd->weight > sd->max_weight)
        return 2;               // 重量超過
    if (pc_inventoryblank(sd) < new_stacks)
        return 3;               // 種類数超過
    if (sd->trade_partner != 0)
        return 4;               // cant buy while trading

    pc_payzeny(sd, static_cast<int>(z));

    for (i = 0; i < n; i++)
    {
        struct item_data *item_data;
        if ((item_data = itemdb_exists(item_list[i * 2 + 1])) != NULL)
        {
            int amount = item_list[i * 2];
            struct item item_tmp;
            memset(&item_tmp, 0, sizeof(item_tmp));

            item_tmp.nameid = item_data->nameid;
            item_tmp.identify = 1;  // npc販売アイテムは鑑定済み

            if (amount > 1
                && (item_data->type == 4 || item_data->type == 5
                    || item_data->type == 7 || item_data->type == 8))
            {
                for (j = 0; j < amount; j++)
                {
                    pc_additem(sd, &item_tmp, 1);
                }
            }
            else
            {
                pc_additem(sd, &item_tmp, amount);
            }
        }
    }
    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int npc_selllist(MapSessionData *sd, int n,
                 const unsigned short *item_list)
{
    double z;
    int i, itemamount = 0;

    nullpo_retr(1, sd);
    nullpo_retr(1, item_list);

    if (npc_checknear(sd, sd->npc_shopid))
        return 1;
    for (i = 0, z = 0; i < n; i++)
    {
        int nameid;
        if (item_list[i * 2] - 2 < 0 || item_list[i * 2] - 2 >= MAX_INVENTORY)
            return 1;
        nameid = sd->status.inventory[item_list[i * 2] - 2].nameid;
        if (nameid == 0 ||
            sd->status.inventory[item_list[i * 2] - 2].amount <
            item_list[i * 2 + 1])
            return 1;
        if (sd->trade_partner != 0)
            return 2;           // cant sell while trading
        z += static_cast<double>(itemdb_value_sell(nameid)) * item_list[i * 2 + 1];
        itemamount += item_list[i * 2 + 1];
    }

    if (z > MAX_ZENY)
        z = MAX_ZENY;
    pc_getzeny(sd, static_cast<int>(z));
    for (i = 0; i < n; i++)
    {
        int item_id = item_list[i * 2] - 2;
        pc_delitem(sd, item_id, item_list[i * 2 + 1], 0);
    }
    return 0;

}

//
// 初期化関係
//

/*==========================================
 * 読み込むnpcファイルのクリア
 *------------------------------------------
 */
static void npc_clearsrcfile(void)
{
    struct npc_src_list *p = npc_src_first;

    while (p)
    {
        struct npc_src_list *p2 = p;
        p = p->next;
        free(p2);
    }
    npc_src_first = NULL;
    npc_src_last = NULL;
}

/*==========================================
 * 読み込むnpcファイルの追加
 *------------------------------------------
 */
void npc_addsrcfile(char *name)
{
    struct npc_src_list *new_src;
    size_t len;

    if (strcasecmp(name, "clear") == 0)
    {
        npc_clearsrcfile();
        return;
    }

    len = sizeof(*new_src) + strlen(name);
    new_src = static_cast<struct npc_src_list *>(calloc(1, len));
    new_src->next = NULL;
    strncpy(new_src->name, name, strlen(name) + 1);
    if (npc_src_first == NULL)
        npc_src_first = new_src;
    if (npc_src_last)
        npc_src_last->next = new_src;

    npc_src_last = new_src;
}

/*==========================================
 * warp行解析
 *------------------------------------------
 */
int npc_parse_warp(char *w1, const char *, char *w3, char *w4)
{
    int x, y, xs, ys, to_x, to_y, m;
    int i, j;
    fixed_string<16> mapname, to_mapname;
    struct npc_data_warp *nd;

    // 引数の個数チェック
    if (sscanf(w1, "%[^,],%d,%d", &mapname, &x, &y) != 3 ||
        sscanf(w4, "%d,%d,%[^,],%d,%d", &xs, &ys, &to_mapname, &to_x, &to_y) != 5)
    {
        printf("bad warp line : %s\n", w3);
        return 1;
    }

    m = map_mapname2mapid(mapname);

    nd = new npc_data_warp;
    nd->id = npc_get_new_npc_id();
    nd->n = map_addnpc(m, nd);

    nd->prev = nd->next = NULL;
    nd->m = m;
    nd->x = x;
    nd->y = y;
    nd->dir = Direction::S;
    nd->flag = 0;
    memcpy(nd->name, w3, 24);
    memcpy(nd->exname, w3, 24);

    nd->npc_class = WARP_CLASS;

    nd->speed = 200;
    nd->option = 0;
    nd->opt1 = 0;
    nd->opt2 = 0;
    nd->opt3 = 0;
    nd->warp.dst.map = to_mapname;
    xs += 2;
    ys += 2;
    nd->warp.dst.x = to_x;
    nd->warp.dst.y = to_y;
    nd->warp.xs = xs;
    nd->warp.ys = ys;

    for (i = 0; i < ys; i++)
    {
        for (j = 0; j < xs; j++)
        {
            int t;
            t = map_getcell(m, x - xs / 2 + j, y - ys / 2 + i);
            if (t == 1 || t == 5)
                continue;
            map_setcell(m, x - xs / 2 + j, y - ys / 2 + i, t | 0x80);
        }
    }

//  printf("warp npc %s %d read done\n",mapname,nd->id);
    npc_warp++;
    map_addblock(nd);
    clif_spawnnpc(nd);
    strdb_insert(npcname_db, nd->name, static_cast<void *>(nd));

    return 0;
}

/*==========================================
 * shop行解析
 *------------------------------------------
 */
static int npc_parse_shop(char *w1, char *, char *w3, char *w4)
{
    char *p;
    int x, y, dir, m;
    fixed_string<16> mapname;
    struct npc_data_shop *nd;

    // 引数の個数チェック
    if (sscanf(w1, "%[^,],%d,%d,%d", &mapname, &x, &y, &dir) != 4 ||
        strchr(w4, ',') == NULL)
    {
        printf("bad shop line : %s\n", w3);
        return 1;
    }
    m = map_mapname2mapid(mapname);

    nd = new npc_data_shop;
    std::vector<npc_item_list>& shop_items = nd->shop_item;

    p = strchr(w4, ',');

    while (p)
    {
        int nameid, value;
        char name[24];
        struct item_data *id = NULL;
        p++;
        if (sscanf(p, "%d:%d", &nameid, &value) == 2)
        {
        }
        else if (sscanf(p, "%s :%d", name, &value) == 2)
        {
            id = itemdb_searchname(name);
            if (id == NULL)
                nameid = -1;
            else
                nameid = id->nameid;
        }
        else
            break;

        if (nameid > 0)
        {
            npc_item_list newitem;
            newitem.nameid = nameid;
            if (value < 0)
            {
                if (id == NULL)
                    id = itemdb_search(nameid);
                value = id->value_buy * abs(value);

            }
            newitem.value = value;
            shop_items.push_back(newitem);
        }
        p = strchr(p, ',');
    }
    if (shop_items.empty())
    {
        delete nd;
        return 1;
    }

    nd->prev = nd->next = NULL;
    nd->m = m;
    nd->x = x;
    nd->y = y;
    nd->id = npc_get_new_npc_id();
    nd->dir = static_cast<Direction>(dir);
    nd->flag = 0;
    memcpy(nd->name, w3, 24);
    nd->npc_class = atoi(w4);
    nd->speed = 200;
    nd->option = 0;
    nd->opt1 = 0;
    nd->opt2 = 0;
    nd->opt3 = 0;

    //printf("shop npc %s %d read done\n",mapname,nd->id);
    npc_shop++;
    nd->n = map_addnpc(m, nd);
    map_addblock(nd);
    clif_spawnnpc(nd);
    strdb_insert(npcname_db, nd->name, static_cast<void *>(nd));

    return 0;
}

/*==========================================
 * NPCのラベルデータコンバート
 *------------------------------------------
 */
static void npc_convertlabel_db(db_key_t key, db_val_t data, struct npc_data_script *nd)
{
    const char *lname = key.s;
    int pos = data.i;
    struct npc_label_list *lst;
    int num;
    const char *p = strchr(lname, ':');

    nullpo_retv(nd);

    lst = nd->scr.label_list;
    num = nd->scr.label_list_num;
    if (!lst)
    {
        CREATE(lst, struct npc_label_list, 1);
        num = 0;
    }
    else
    {
        RECREATE(lst, struct npc_label_list, num + 1);
    }
    strzcpy(lst[num].name, lname, MIN(static_cast<size_t>(p - lname), sizeof(lst[num].name)-1));

    lst[num].pos = pos;
    nd->scr.label_list = lst;
    nd->scr.label_list_num = num + 1;
}

/*==========================================
 * script行解析
 *------------------------------------------
 */
static int npc_parse_script(char *w1, char *w2, char *w3, char *w4,
                             char *first_line, FILE * fp, int *lines)
{
    int x, y, dir = 0, m, xs = 0, ys = 0, npc_class = 0;   // [Valaris] thanks to fov
    fixed_string<16> mapname;
    char *srcbuf = NULL, *script;
    int srcsize = 65536;
    int startline = 0;
    char line[1024];
    struct npc_data_script *nd;
    int evflag = 0;
    struct dbt *label_db;
    char *p;
    struct npc_label_list *label_dup = NULL;
    int label_dupnum = 0;
    int src_id = 0;

    if (strcmp(w1, "-") == 0)
    {
        x = 0;
        y = 0;
        m = -1;
    }
    else
    {
        // 引数の個数チェック
        if (sscanf(w1, "%[^,],%d,%d,%d", &mapname, &x, &y, &dir) != 4 ||
            (strcmp(w2, "script") == 0 && strchr(w4, ',') == NULL))
        {
            printf("bad script line : %s\n", w3);
            return 1;
        }
        m = map_mapname2mapid(mapname);
    }

    if (strcmp(w2, "script") == 0)
    {
        // スクリプトの解析
        CREATE(srcbuf, char, srcsize);
        if (strchr(first_line, '{'))
        {
            strcpy(srcbuf, strchr(first_line, '{'));
            startline = *lines;
        }
        else
            srcbuf[0] = 0;
        while (1)
        {
            int i;
            for (i = strlen(srcbuf) - 1; i >= 0 && isspace(srcbuf[i]); i--);
            if (i >= 0 && srcbuf[i] == '}')
                break;
            if (!fgets(line, 1020, fp))
                break;
            (*lines)++;
            if (feof(fp))
                break;
            if (strlen(srcbuf) + strlen(line) + 1 >= srcsize)
            {
                srcsize += 65536;
                RECREATE(srcbuf, char, srcsize);
                memset(srcbuf + srcsize - 65536, '\0', 65536);
            }
            if (srcbuf[0] != '{')
            {
                if (strchr(line, '{'))
                {
                    strcpy(srcbuf, strchr(line, '{'));
                    startline = *lines;
                }
            }
            else
                strcat(srcbuf, line);
        }
        script = parse_script(srcbuf, startline);
        if (script == NULL)
        {
            // script parse error?
            free(srcbuf);
            return 1;
        }

    }
    else
    {
        // duplicateする

        char srcname[128];
        struct npc_data_script *nd2;
        if (sscanf(w2, "duplicate(%[^)])", srcname) != 1)
        {
            printf("bad duplicate name! : %s", w2);
            return 0;
        }
        if ((nd2 = static_cast<npc_data_script *>(npc_name2id(srcname))) == NULL
            || nd2->subtype != SCRIPT
        )
        {
            printf("bad duplicate name! (not exist) : %s\n", srcname);
            return 0;
        }
        script = nd2->scr.script;
        label_dup = nd2->scr.label_list;
        label_dupnum = nd2->scr.label_list_num;
        src_id = nd2->id;

    }                           // end of スクリプト解析

    nd = new npc_data_script;

    if (m == -1)
    {
        // スクリプトコピー用のダミーNPC

    }
    else if (sscanf(w4, "%d,%d,%d", &npc_class, &xs, &ys) == 3)
    {
        // 接触型NPC
        int i, j;

        if (xs >= 0)
            xs = xs * 2 + 1;
        if (ys >= 0)
            ys = ys * 2 + 1;

        if (npc_class >= 0)
        {

            for (i = 0; i < ys; i++)
            {
                for (j = 0; j < xs; j++)
                {
                    int t;
                    t = map_getcell(m, x - xs / 2 + j, y - ys / 2 + i);
                    if (t == 1 || t == 5)
                        continue;
                    map_setcell(m, x - xs / 2 + j, y - ys / 2 + i, t | 0x80);
                }
            }
        }

        nd->scr.xs = xs;
        nd->scr.ys = ys;
    }
    else
    {                           // クリック型NPC
        npc_class = atoi(w4);
        nd->scr.xs = 0;
        nd->scr.ys = 0;
    }

    if (npc_class < 0 && m >= 0)
    {                           // イベント型NPC
        evflag = 1;
    }

    while ((p = strchr(w3, ':')))
    {
        if (p[1] == ':')
            break;
    }
    if (p)
    {
        *p = 0;
        memcpy(nd->name, w3, 24);
        memcpy(nd->exname, p + 2, 24);
    }
    else
    {
        memcpy(nd->name, w3, 24);
        memcpy(nd->exname, w3, 24);
    }

    nd->prev = nd->next = NULL;
    nd->m = m;
    nd->x = x;
    nd->y = y;
    nd->id = npc_get_new_npc_id();
    nd->dir = static_cast<Direction>(dir);
    nd->flag = 0;
    nd->npc_class = npc_class;
    nd->speed = 200;
    nd->scr.script = script;
    nd->scr.src_id = src_id;
    nd->option = 0;
    nd->opt1 = 0;
    nd->opt2 = 0;
    nd->opt3 = 0;

    //printf("script npc %s %d %d read done\n",mapname,nd->id,nd->class);
    npc_script++;
    if (m >= 0)
    {
        nd->n = map_addnpc(m, nd);
        map_addblock(nd);

        if (evflag)
        {                       // イベント型
            struct event_data *ev;
            CREATE(ev, struct event_data, 1);
            ev->nd = nd;
            ev->pos = 0;
            strdb_insert(ev_db, nd->exname, static_cast<void *>(ev));
        }
        else
            clif_spawnnpc(nd);
    }
    strdb_insert(npcname_db, nd->exname, static_cast<void *>(nd));

    //-----------------------------------------
    // ラベルデータの準備
    if (srcbuf)
    {
        // script本体がある場合の処理

        // ラベルデータのコンバート
        label_db = script_get_label_db();
        // FIXME why is this needed?
        strdb_foreach(label_db, npc_convertlabel_db, nd);

        // もう使わないのでバッファ解放
        free(srcbuf);

    }
    else
    {
        // duplicate

//      nd->label_list=malloc(sizeof(struct npc_label_list)*label_dupnum);
//      memcpy(nd->label_list,label_dup,sizeof(struct npc_label_list)*label_dupnum);

        nd->scr.label_list = label_dup;   // ラベルデータ共有
        nd->scr.label_list_num = label_dupnum;
    }

    //-----------------------------------------
    // イベント用ラベルデータのエクスポート
    for (int i = 0; i < nd->scr.label_list_num; i++)
    {
        char *lname = nd->scr.label_list[i].name;
        int pos = nd->scr.label_list[i].pos;

        if ((lname[0] == 'O' || lname[0] == 'o')
            && (lname[1] == 'N' || lname[1] == 'n'))
        {
            struct event_data *ev;
            char *buf;
            // エクスポートされる
            CREATE(ev, struct event_data, 1);
            CREATE(buf, char, 50);
            if (strlen(lname) > 24)
            {
                printf("npc_parse_script: label name error !\n");
                exit(1);
            }
            else
            {
                ev->nd = nd;
                ev->pos = pos;
                sprintf(buf, "%s::%s", nd->exname, lname);
                strdb_insert(ev_db, buf, static_cast<void *>(ev));
            }
        }
    }

    //-----------------------------------------
    // ラベルデータからタイマーイベント取り込み
    for (int i = 0; i < nd->scr.label_list_num; i++)
    {
        int t = 0, n = 0;
        char *lname = nd->scr.label_list[i].name;
        int pos = nd->scr.label_list[i].pos;
        if (sscanf(lname, "OnTimer%d%n", &t, &n) == 1 && lname[n] == '\0')
        {
            // タイマーイベント
            struct npc_timerevent_list *te = nd->scr.timer_event;
            int j, k = nd->scr.timeramount;
            if (te == NULL)
            {
                CREATE(te, struct npc_timerevent_list, 1);
            }
            else
            {
                RECREATE(te, struct npc_timerevent_list, k + 1);
            }
            for (j = 0; j < k; j++)
            {
                if (te[j].timer > t)
                {
                    memmove(te + j + 1, te + j,
                             sizeof(struct npc_timerevent_list) * (k - j));
                    break;
                }
            }
            te[j].timer = t;
            te[j].pos = pos;
            nd->scr.timer_event = te;
            nd->scr.timeramount = k + 1;
        }
    }
    nd->scr.nexttimer = -1;
    nd->scr.timerid = NULL;

    return 0;
}

/*==========================================
 * function行解析
 *------------------------------------------
 */
static int npc_parse_function(char *, char *, char *w3, char *,
                               char *first_line, FILE * fp, int *lines)
{
    char *srcbuf = NULL, *script;
    int srcsize = 65536;
    int startline = 0;
    char line[1024];
    int i;
//  struct dbt *label_db;
    char *p;

    // スクリプトの解析
    CREATE(srcbuf, char, srcsize);
    if (strchr(first_line, '{'))
    {
        strcpy(srcbuf, strchr(first_line, '{'));
        startline = *lines;
    }
    else
        srcbuf[0] = 0;
    while (1)
    {
        for (i = strlen(srcbuf) - 1; i >= 0 && isspace(srcbuf[i]); i--);
        if (i >= 0 && srcbuf[i] == '}')
            break;
        if (!fgets(line, 1020, fp))
            break;
        (*lines)++;
        if (feof(fp))
            break;
        if (strlen(srcbuf) + strlen(line) + 1 >= srcsize)
        {
            srcsize += 65536;
            RECREATE(srcbuf, char, srcsize);
            memset(srcbuf + srcsize - 65536, '\0', 65536);
        }
        if (srcbuf[0] != '{')
        {
            if (strchr(line, '{'))
            {
                strcpy(srcbuf, strchr(line, '{'));
                startline = *lines;
            }
        }
        else
            strcat(srcbuf, line);
    }
    script = parse_script(srcbuf, startline);
    if (script == NULL)
    {
        // script parse error?
        free(srcbuf);
        return 1;
    }

    CREATE(p, char, 50);

    strncpy(p, w3, 49);
    strdb_insert(script_get_userfunc_db(), p, static_cast<void *>(script));

//  label_db=script_get_label_db();

    // もう使わないのでバッファ解放
    free(srcbuf);

//  printf("function %s => %p\n",p,script);

    return 0;
}

/*==========================================
 * mob行解析
 *------------------------------------------
 */
int npc_parse_mob(char *w1, char *, char *w3, char *w4)
{
    int m, x, y, xs, ys, mob_class, num, delay_1, delay2;
    int i;
    fixed_string<16> mapname;
    char eventname[24] = "";
    struct mob_data *md;

    xs = ys = 0;
    delay_1 = delay2 = 0;
    // 引数の個数チェック
    if (sscanf(w1, "%[^,],%d,%d,%d,%d", &mapname, &x, &y, &xs, &ys) < 3 ||
        sscanf(w4, "%d,%d,%d,%d,%s", &mob_class, &num, &delay_1, &delay2,
                eventname) < 2)
    {
        printf("bad monster line : %s\n", w3);
        return 1;
    }

    m = map_mapname2mapid(mapname);

    if (num > 1 && battle_config.mob_count_rate != 100)
    {
        if ((num = num * battle_config.mob_count_rate / 100) < 1)
            num = 1;
    }

    for (i = 0; i < num; i++)
    {
        md = new mob_data;

        md->prev = NULL;
        md->next = NULL;
        md->m = m;
        md->x = x;
        md->y = y;
        if (strcmp(w3, "--en--") == 0)
            memcpy(md->name, mob_db[mob_class].name, 24);
        else if (strcmp(w3, "--ja--") == 0)
            memcpy(md->name, mob_db[mob_class].jname, 24);
        else
            memcpy(md->name, w3, 24);

        md->n = i;
        md->base_class = md->mob_class = mob_class;
        md->id = npc_get_new_npc_id();
        md->m_0 = m;
        md->x_0 = x;
        md->y_0 = y;
        md->xs = xs;
        md->ys = ys;
        md->spawndelay_1 = delay_1;
        md->spawndelay2 = delay2;

        memset(&md->state, 0, sizeof(md->state));
        md->timer = NULL;
        md->target_id = 0;
        md->attacked_id = 0;

        if (mob_db[mob_class].mode & 0x02)
        {
            CREATE(md->lootitem, struct item, LOOTITEM_SIZE);
        }
        else
            md->lootitem = NULL;

        if (strlen(eventname) >= 4)
        {
            memcpy(md->npc_event, eventname, 24);
        }
        else
            memset(md->npc_event, 0, 24);

        map_addiddb(md);
        mob_spawn(md->id);

        npc_mob++;
    }
    //printf("warp npc %s %d read done\n",mapname,nd->id);

    return 0;
}

/*==========================================
 * マップフラグ行の解析
 *------------------------------------------
 */
static int npc_parse_mapflag(char *w1, char *, char *w3, char *w4)
{
    int m;
    fixed_string<16> mapname, savemap;
    int savex, savey;
    char drop_arg1[16], drop_arg2[16];
    int drop_id = 0, drop_type = 0, drop_per = 0;

    // 引数の個数チェック
//  if (    sscanf(w1,"%[^,],%d,%d,%d",mapname,&x,&y,&dir) != 4 )
    if (sscanf(w1, "%[^,]", &mapname) != 1)
        return 1;

    m = map_mapname2mapid(mapname);
    if (m < 0)
        return 1;

//マップフラグ
    if (strcasecmp(w3, "nosave") == 0)
    {
        if (strcmp(w4, "SavePoint") == 0)
        {
            maps[m].save.map.copy_from("SavePoint");
            maps[m].save.x = -1;
            maps[m].save.y = -1;
        }
        else if (sscanf(w4, "%[^,],%d,%d", &savemap, &savex, &savey) == 3)
        {
            maps[m].save.map = savemap;
            maps[m].save.x = savex;
            maps[m].save.y = savey;
        }
        maps[m].flag.nosave = 1;
    }
    else if (strcasecmp(w3, "nomemo") == 0)
    {
        maps[m].flag.nomemo = 1;
    }
    else if (strcasecmp(w3, "noteleport") == 0)
    {
        maps[m].flag.noteleport = 1;
    }
    else if (strcasecmp(w3, "nowarp") == 0)
    {
        maps[m].flag.nowarp = 1;
    }
    else if (strcasecmp(w3, "nowarpto") == 0)
    {
        maps[m].flag.nowarpto = 1;
    }
    else if (strcasecmp(w3, "noreturn") == 0)
    {
        maps[m].flag.noreturn = 1;
    }
    else if (strcasecmp(w3, "monster_noteleport") == 0)
    {
        maps[m].flag.monster_noteleport = 1;
    }
    else if (strcasecmp(w3, "nobranch") == 0)
    {
        maps[m].flag.nobranch = 1;
    }
    else if (strcasecmp(w3, "nopenalty") == 0)
    {
        maps[m].flag.nopenalty = 1;
    }
    else if (strcasecmp(w3, "pvp") == 0)
    {
        maps[m].flag.pvp = 1;
    }
    else if (strcasecmp(w3, "pvp_noparty") == 0)
    {
        maps[m].flag.pvp_noparty = 1;
    }
    else if (strcasecmp(w3, "pvp_nightmaredrop") == 0)
    {
        if (sscanf(w4, "%[^,],%[^,],%d", drop_arg1, drop_arg2, &drop_per) ==
            3)
        {
            int i;
            if (strcmp(drop_arg1, "random") == 0)
                drop_id = -1;
            else if (itemdb_exists((drop_id = atoi(drop_arg1))) == NULL)
                drop_id = 0;
            if (strcmp(drop_arg2, "inventory") == 0)
                drop_type = 1;
            else if (strcmp(drop_arg2, "equip") == 0)
                drop_type = 2;
            else if (strcmp(drop_arg2, "all") == 0)
                drop_type = 3;

            if (drop_id != 0)
            {
                for (i = 0; i < MAX_DROP_PER_MAP; i++)
                {
                    if (maps[m].drop_list[i].drop_id == 0)
                    {
                        maps[m].drop_list[i].drop_id = drop_id;
                        maps[m].drop_list[i].drop_type = drop_type;
                        maps[m].drop_list[i].drop_per = drop_per;
                        break;
                    }
                }
                maps[m].flag.pvp_nightmaredrop = 1;
            }
        }
    }
    else if (strcasecmp(w3, "pvp_nocalcrank") == 0)
    {
        maps[m].flag.pvp_nocalcrank = 1;
    }
    else if (strcasecmp(w3, "nozenypenalty") == 0)
    {
        maps[m].flag.nozenypenalty = 1;
    }
    else if (strcasecmp(w3, "notrade") == 0)
    {
        maps[m].flag.notrade = 1;
    }
    else if (battle_config.pk_mode && strcasecmp(w3, "nopvp") == 0)
    {                           // nopvp for pk mode [Valaris]
        maps[m].flag.nopvp = 1;
        maps[m].flag.pvp = 0;
    }
    else if (strcasecmp(w3, "no_player_drops") == 0)
    {                           // no player drops [Jaxad0127]
        maps[m].flag.no_player_drops = 1;
    }
    else if (strcasecmp(w3, "town") == 0)
    {                           // town/safe zone [remoitnane]
        maps[m].flag.town = 1;
    }

    return 0;
}

struct npc_data *npc_spawn_text(int m, int x, int y,
                                 int npc_class, const char *name, const char *message)
{
    struct npc_data_message *retval = new npc_data_message;
    retval->id = npc_get_new_npc_id();
    retval->x = x;
    retval->y = y;
    retval->m = m;

    strncpy(retval->name, name, 23);
    strncpy(retval->exname, name, 23);
    retval->name[15] = 0;
    retval->exname[15] = 0;
    retval->message = message ? strdup(message) : NULL;

    retval->npc_class = npc_class;
    retval->speed = 200;

    clif_spawnnpc(retval);
    map_addblock(retval);
    map_addiddb(retval);
    if (retval->name && retval->name[0])
        strdb_insert(npcname_db, retval->name, static_cast<void *>(retval));

    return retval;
}

static void npc_propagate_update(struct npc_data_script *nd)
{
    map_foreachinarea(npc_enable_sub, nd->m,
                      nd->x - nd->scr.xs, nd->y - nd->scr.ys,
                      nd->x + nd->scr.xs, nd->y + nd->scr.ys,
                      BL_PC, nd);
}

npc_data::~npc_data()
{
    clif_being_remove(this, BeingRemoveType::ZERO);
    map_deliddb(this);
    map_delblock(this);
}

npc_data_script::~npc_data_script()
{
    free(scr.timer_event);
    if (scr.src_id == 0)
    {
        free(scr.script);
        free(scr.label_list);
    }
    npc_propagate_update(this);
}

npc_data_message::~npc_data_message()
{
    free(message);
}

static void ev_release(db_key_t key, db_val_t val)
{
    free(const_cast<char *>(key.s));
    free(val.p);
}

/*==========================================
 * npc初期化
 *------------------------------------------
 */
int do_init_npc(void)
{
    struct npc_src_list *nsl;
    FILE *fp;
    char line[1024];
    int m, lines;

    ev_db = strdb_init();
    npcname_db = strdb_init();

    ev_db->release = ev_release;

    memset(&ev_tm_b, -1, sizeof(ev_tm_b));

    for (nsl = npc_src_first; nsl; nsl = nsl->next)
    {
        if (nsl->prev)
        {
            free(nsl->prev);
            nsl->prev = NULL;
        }
        fp = fopen_(nsl->name, "r");
        if (fp == NULL)
        {
            printf("file not found : %s\n", nsl->name);
            exit(1);
        }
        lines = 0;
        while (fgets(line, 1020, fp))
        {
            char w1[1024], w2[1024], w3[1024], w4[1024];
            fixed_string<16> mapname;
            int i, j, w4pos, count;
            lines++;

            if (line[0] == '/' && line[1] == '/')
                continue;
            // 不要なスペースやタブの連続は詰める
            for (i = j = 0; line[i]; i++)
            {
                if (line[i] == ' ')
                {
                    if (!
                        ((line[i + 1]
                          && (isspace(line[i + 1]) || line[i + 1] == ','))
                         || (j && line[j - 1] == ',')))
                        line[j++] = ' ';
                }
                else if (line[i] == '\t')
                {
                    if (!(j && line[j - 1] == '\t'))
                        line[j++] = '\t';
                }
                else
                    line[j++] = line[i];
            }
            // 最初はタブ区切りでチェックしてみて、ダメならスペース区切りで確認
            if ((count =
                 sscanf(line, "%[^\t]\t%[^\t]\t%[^\t\r\n]\t%n%[^\t\r\n]", w1,
                         w2, w3, &w4pos, w4)) < 3
                && (count =
                    sscanf(line, "%s%s%s%n%s", w1, w2, w3, &w4pos, w4)) < 3)
            {
                continue;
            }
            // マップの存在確認
            if (strcmp(w1, "-") != 0 && strcasecmp(w1, "function") != 0)
            {
                sscanf(w1, "%[^,]", &mapname);
                m = map_mapname2mapid(mapname);
                if (m < 0)
                {
                    // "mapname" is not assigned to this server
                    continue;
                }
            }
            if (strcasecmp(w2, "warp") == 0 && count > 3)
            {
                npc_parse_warp(w1, w2, w3, w4);
            }
            else if (strcasecmp(w2, "shop") == 0 && count > 3)
            {
                npc_parse_shop(w1, w2, w3, w4);
            }
            else if (strcasecmp(w2, "script") == 0 && count > 3)
            {
                if (strcasecmp(w1, "function") == 0)
                {
                    npc_parse_function(w1, w2, w3, w4, line + w4pos, fp,
                                        &lines);
                }
                else
                {
                    npc_parse_script(w1, w2, w3, w4, line + w4pos, fp,
                                      &lines);
                }
            }
            else if ((i =
                      0, sscanf(w2, "duplicate%n", &i), (i > 0
                                                          && w2[i] == '('))
                     && count > 3)
            {
                npc_parse_script(w1, w2, w3, w4, line + w4pos, fp, &lines);
            }
            else if (strcasecmp(w2, "monster") == 0 && count > 3)
            {
                npc_parse_mob(w1, w2, w3, w4);
            }
            else if (strcasecmp(w2, "mapflag") == 0 && count >= 3)
            {
                npc_parse_mapflag(w1, w2, w3, w4);
            }
        }
        fclose_(fp);
        printf("\rLoading NPCs [%d]: %-54s", npc_next_id - START_NPC_NUM,
                nsl->name);
        fflush(stdout);
    }
    printf("\rNPCs Loaded: %d [Warps:%d Shops:%d Scripts:%d Mobs:%d]\n",
            npc_next_id - START_NPC_NUM, npc_warp, npc_shop, npc_script, npc_mob);

    return 0;
}
