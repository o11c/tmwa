#include "party.hpp"

#include "../common/nullpo.hpp"
#include "../common/timer.hpp"
#include "../common/utils.hpp"

#include "battle.hpp"
#include "chrif.hpp"
#include "clif.hpp"
#include "map.hpp"
#include "pc.hpp"
#include "skill.hpp"
#include "tmw.hpp"

#define PARTY_SEND_XYHP_INVERVAL        1000    // 座標やＨＰ送信の間隔

static int32_t party_check_conflict(MapSessionData *sd);
static int32_t party_send_xy_clear(struct party *p);

static struct dbt *party_db;

static void party_send_xyhp_timer(timer_id tid, tick_t tick);

// 初期化
void do_init_party(void)
{
    party_db = numdb_init();
    add_timer_interval(gettick() + PARTY_SEND_XYHP_INVERVAL,
                       PARTY_SEND_XYHP_INVERVAL, party_send_xyhp_timer);
}

// 検索
struct party *party_search(int32_t party_id)
{
    return static_cast<struct party *>(numdb_search(party_db, party_id).p);
}

static void party_searchname_sub(db_key_t, db_val_t data, const char *str, struct party **dst)
{
    struct party *p = static_cast<struct party *>(data.p);
    if (strcasecmp(p->name, str) == 0)
        *dst = p;
}

// パーティ名検索
struct party *party_searchname(const char *str)
{
    struct party *p = NULL;
    numdb_foreach(party_db, party_searchname_sub, str, &p);
    return p;
}

/* Process a party creation request. */
int32_t party_create(MapSessionData *sd, const char *name)
{
    char pname[24];
    nullpo_ret(sd);

    strncpy(pname, name, 24);
    pname[23] = '\0';
    tmw_TrimStr(pname);

    /* The party name is empty/invalid. */
    if (!*pname)
        clif_party_created(sd, 1);

    /* Make sure the character isn't already in a party. */
    if (sd->status.party_id == 0)
        intif_create_party(sd, pname);
    else
        clif_party_created(sd, 2);

    return 0;
}

/* Relay the result of a party creation request. */
int32_t party_created(int32_t account_id, int32_t fail, int32_t party_id, const char *name)
{
    MapSessionData *sd;
    sd = map_id2sd(account_id);

    nullpo_ret(sd);

    /* The party name is valid and not already taken. */
    if (!fail)
    {
        struct party *p;
        sd->status.party_id = party_id;

        if ((p = static_cast<struct party *>(numdb_search(party_db, party_id).p)) != NULL)
        {
            printf("party_created(): ID already exists!\n");
            exit(1);
        }

        CREATE(p, struct party, 1);
        p->party_id = party_id;
        memcpy(p->name, name, 24);
        numdb_insert(party_db, party_id, static_cast<void *>(p));

        /* The party was created successfully. */
        clif_party_created(sd, 0);
    }

    else
        clif_party_created(sd, 1);

    return 0;
}

// 情報要求
void party_request_info(int32_t party_id)
{
    intif_request_partyinfo(party_id);
}

// 所属キャラの確認
static int32_t party_check_member(const struct party *p)
{
    nullpo_ret(p);

    for (MapSessionData *sd : auth_sessions)
    {
        if (sd->status.party_id != p->party_id)
            continue;
        bool f = 1;
        for (int32_t j = 0; j < MAX_PARTY; j++)
        {
            if (p->member[j].account_id == sd->status.account_id)
            {
                if (strcmp(p->member[j].name, sd->status.name) == 0)
                    f = 0;
//              else
//                  p->member[j].sd = NULL;
            }
        }
        if (f)
        {
            sd->status.party_id = 0;
            map_log("party: check_member %d[%s] is not member\n",
                    sd->status.account_id, sd->status.name);
        }
    }
    return 0;
}

// 情報所得失敗（そのIDのキャラを全部未所属にする）
int32_t party_recv_noinfo(int32_t party_id)
{
    for (MapSessionData *sd : auth_sessions)
    {
        if (sd->status.party_id == party_id)
            sd->status.party_id = 0;
    }
    return 0;
}

// 情報所得
int32_t party_recv_info(const struct party *sp)
{
    struct party *p;
    int32_t i;

    nullpo_ret(sp);

    if ((p = static_cast<struct party *>(numdb_search(party_db, static_cast<numdb_key_t>(sp->party_id)).p)) == NULL)
    {
        CREATE(p, struct party, 1);
        numdb_insert(party_db, static_cast<numdb_key_t>(sp->party_id), static_cast<void *>(p));

        // 最初のロードなのでユーザーのチェックを行う
        party_check_member(sp);
    }
    memcpy(p, sp, sizeof(struct party));

    for (i = 0; i < MAX_PARTY; i++)
    {                           // sdの設定
        MapSessionData *sd = map_id2sd(p->member[i].account_id);
        p->member[i].sd = (sd != NULL
                           && sd->status.party_id == p->party_id) ? sd : NULL;
    }

    clif_party_info(p, -1);

    for (i = 0; i < MAX_PARTY; i++)
    {                           // 設定情報の送信
//      MapSessionData *sd = map_id2sd(p->member[i].account_id);
        MapSessionData *sd = p->member[i].sd;
        if (sd != NULL && sd->party_sended == 0)
        {
            clif_party_option(p, sd, 0x100);
            sd->party_sended = 1;
        }
    }

    return 0;
}

/* Process party invitation from sd to account_id. */
int32_t party_invite(MapSessionData *sd, int32_t account_id)
{
    MapSessionData *tsd = map_id2sd(account_id);
    struct party *p = party_search(sd->status.party_id);
    int32_t i;
    int32_t full = 1; /* Indicates whether or not there's room for one more. */

    nullpo_ret(sd);

    if (!tsd || !p || !tsd->fd)
        return 0;

    if (!battle_config.invite_request_check)
    {
        /* Disallow the invitation under these conditions. */
        if (tsd->trade_partner || tsd->npc_id
            || tsd->npc_shopid || pc_checkskill(tsd, NV_PARTY) < 1)
        {
            clif_party_inviteack(sd, tsd->status.name, 1);
            return 0;
        }
    }

    /* The target player is already in a party, or has a pending invitation. */
    if (tsd->status.party_id > 0 || tsd->party_invite > 0)
    {
        clif_party_inviteack(sd, tsd->status.name, 0);
        return 0;
    }

    for (i = 0; i < MAX_PARTY; i++)
    {
        /*
         * A character from the target account is already in the same party.
         * The response isn't strictly accurate, as they're separate
         * characters, but we're making do with what was already in place and
         * leaving this(mostly) alone for now.
         */
        if (p->member[i].account_id == account_id)
        {
            clif_party_inviteack(sd, tsd->status.name, 1);
            return 0;
        }

        if (!p->member[i].account_id)
            full = 0;
    }

    /* There isn't enough room for a new member. */
    if (full)
    {
        clif_party_inviteack(sd, tsd->status.name, 3);
        return 0;
    }

    /* Otherwise, relay the invitation to the target player. */
    tsd->party_invite = sd->status.party_id;
    tsd->party_invite_account = sd->status.account_id;

    clif_party_invite(sd, tsd);
    return 0;
}

/* Process response to party invitation. */
int32_t party_reply_invite(MapSessionData *sd, int32_t account_id, int32_t flag)
{
    nullpo_ret(sd);

    /* There is no pending invitation. */
    if (!sd->party_invite || !sd->party_invite_account)
        return 0;

    /*
     * Only one invitation can be pending, so these have to be the same. Since
     * the client continues to send the wrong ID, and it's already managed on
     * this side of things, the sent ID is being ignored.
     */
    account_id = sd->party_invite_account;

    /* The invitation was accepted. */
    if (flag == 1)
        intif_party_addmember(sd->party_invite, sd->status.account_id);
    /* The invitation was rejected. */
    else
    {
        /* This is the player who sent the invitation. */
        MapSessionData *tsd = NULL;

        sd->party_invite = 0;
        sd->party_invite_account = 0;

        if ((tsd = map_id2sd(account_id)))
            clif_party_inviteack(tsd, sd->status.name, 1);
    }
    return 0;
}

// パーティが追加された
int32_t party_member_added(int32_t party_id, int32_t account_id, int32_t flag)
{
    MapSessionData *sd = map_id2sd(account_id), *sd2;
    struct party *p = party_search(party_id);

    if (sd == NULL)
    {
        if (flag == 0)
        {
            map_log("party: memmap_logber added error %d is not online\n",
                    account_id);
            intif_party_leave(party_id, account_id);   // キャラ側に登録できなかったため脱退要求を出す
        }
        return 0;
    }
    sd2 = map_id2sd(sd->party_invite_account);
    sd->party_invite = 0;
    sd->party_invite_account = 0;

    if (p == NULL)
    {
        printf("party_member_added: party %d not found.\n", party_id);
        intif_party_leave(party_id, account_id);
        return 0;
    }

    if (flag == 1)
    {                           // 失敗
        if (sd2 != NULL)
            clif_party_inviteack(sd2, sd->status.name, 0);
        return 0;
    }

    // 成功
    sd->party_sended = 0;
    sd->status.party_id = party_id;

    if (sd2 != NULL)
        clif_party_inviteack(sd2, sd->status.name, 2);

    // いちおう競合確認
    party_check_conflict(sd);

    party_send_xy_clear(p);

    return 0;
}

// パーティ除名要求
int32_t party_removemember(MapSessionData *sd, int32_t account_id, const char *)
{
    struct party *p;
    int32_t i;

    nullpo_ret(sd);

    if ((p = party_search(sd->status.party_id)) == NULL)
        return 0;

    for (i = 0; i < MAX_PARTY; i++)
    {                           // リーダーかどうかチェック
        if (p->member[i].account_id == sd->status.account_id)
            if (p->member[i].leader == 0)
                return 0;
    }

    for (i = 0; i < MAX_PARTY; i++)
    {                           // 所属しているか調べる
        if (p->member[i].account_id == account_id)
        {
            intif_party_leave(p->party_id, account_id);
            return 0;
        }
    }
    return 0;
}

// パーティ脱退要求
int32_t party_leave(MapSessionData *sd)
{
    struct party *p;
    int32_t i;

    nullpo_ret(sd);

    if ((p = party_search(sd->status.party_id)) == NULL)
        return 0;

    for (i = 0; i < MAX_PARTY; i++)
    {                           // 所属しているか
        if (p->member[i].account_id == sd->status.account_id)
        {
            intif_party_leave(p->party_id, sd->status.account_id);
            return 0;
        }
    }
    return 0;
}

// パーティメンバが脱退した
int32_t party_member_left(int32_t party_id, int32_t account_id, const char *name)
{
    MapSessionData *sd = map_id2sd(account_id);
    struct party *p = party_search(party_id);
    if (p != NULL)
    {
        int32_t i;
        for (i = 0; i < MAX_PARTY; i++)
            if (p->member[i].account_id == account_id)
            {
                clif_party_left(p, sd, account_id, name, 0x00);
                p->member[i].account_id = 0;
                p->member[i].sd = NULL;
            }
    }
    if (sd != NULL && sd->status.party_id == party_id)
    {
        sd->status.party_id = 0;
        sd->party_sended = 0;
    }
    return 0;
}

// パーティ解散通知
int32_t party_broken(int32_t party_id)
{
    struct party *p;
    int32_t i;
    if ((p = party_search(party_id)) == NULL)
        return 0;

    for (i = 0; i < MAX_PARTY; i++)
    {
        if (p->member[i].sd != NULL)
        {
            clif_party_left(p, p->member[i].sd,
                               p->member[i].account_id, p->member[i].name,
                               0x10);
            p->member[i].sd->status.party_id = 0;
            p->member[i].sd->party_sended = 0;
        }
    }
    numdb_erase(party_db, party_id);
    return 0;
}

// パーティの設定変更要求
int32_t party_changeoption(MapSessionData *sd, int32_t exp, int32_t item)
{
    struct party *p;

    nullpo_ret(sd);

    if (sd->status.party_id == 0
        || (p = party_search(sd->status.party_id)) == NULL)
        return 0;
    intif_party_changeoption(sd->status.party_id, sd->status.account_id, exp,
                              item);
    return 0;
}

// パーティの設定変更通知
int32_t party_optionchanged(int32_t party_id, int32_t account_id, int32_t exp, int32_t item,
                         int32_t flag)
{
    struct party *p;
    MapSessionData *sd = map_id2sd(account_id);
    if ((p = party_search(party_id)) == NULL)
        return 0;

    if (!(flag & 0x01))
        p->exp = exp;
    if (!(flag & 0x10))
        p->item = item;
    clif_party_option(p, sd, flag);
    return 0;
}

// パーティメンバの移動通知
int32_t party_recv_movemap(int32_t party_id, int32_t account_id, const char *map, int32_t online, int32_t lv)
{
    struct party *p;
    int32_t i;
    if ((p = party_search(party_id)) == NULL)
        return 0;
    for (i = 0; i < MAX_PARTY; i++)
    {
        struct party_member *m = &p->member[i];
        if (m == NULL)
        {
            printf("party_recv_movemap nullpo?\n");
            return 0;
        }
        if (m->account_id == account_id)
        {
            memcpy(m->map, map, 16);
            m->online = online;
            m->lv = lv;
            break;
        }
    }
    if (i == MAX_PARTY)
    {
        map_log("party:map_log not found member %d on %d[%s]", account_id,
                party_id, p->name);
        return 0;
    }

    for (i = 0; i < MAX_PARTY; i++)
    {                           // sd再設定
        MapSessionData *sd = map_id2sd(p->member[i].account_id);
        p->member[i].sd = (sd != NULL
                           && sd->status.party_id == p->party_id) ? sd : NULL;
    }

    party_send_xy_clear(p);    // 座標再通知要請

    clif_party_info(p, -1);
    return 0;
}

// パーティメンバの移動
int32_t party_send_movemap(MapSessionData *sd)
{
    struct party *p;

    nullpo_ret(sd);

    if (sd->status.party_id == 0)
        return 0;
    intif_party_changemap(sd, 1);

    if (sd->party_sended != 0)  // もうパーティデータは送信済み
        return 0;

    // 競合確認
    party_check_conflict(sd);

    // あるならパーティ情報送信
    if ((p = party_search(sd->status.party_id)) != NULL)
    {
        party_check_member(p); // 所属を確認する
        if (sd->status.party_id == p->party_id)
        {
            clif_party_info(p, sd->fd);
            clif_party_option(p, sd, 0x100);
            sd->party_sended = 1;
        }
    }

    return 0;
}

// パーティメンバのログアウト
int32_t party_send_logout(MapSessionData *sd)
{
    struct party *p;

    nullpo_ret(sd);

    if (sd->status.party_id > 0)
        intif_party_changemap(sd, 0);

    // sdが無効になるのでパーティ情報から削除
    if ((p = party_search(sd->status.party_id)) != NULL)
    {
        int32_t i;
        for (i = 0; i < MAX_PARTY; i++)
            if (p->member[i].sd == sd)
                p->member[i].sd = NULL;
    }

    return 0;
}

// パーティメッセージ送信
int32_t party_send_message(MapSessionData *sd, char *mes, int32_t len)
{
    if (sd->status.party_id == 0)
        return 0;
    intif_party_message(sd->status.party_id, sd->status.account_id, mes,
                         len);
    return 0;
}

// パーティメッセージ受信
int32_t party_recv_message(int32_t party_id, int32_t account_id, const char *mes, int32_t len)
{
    struct party *p;
    if ((p = party_search(party_id)) == NULL)
        return 0;
    clif_party_message(p, account_id, mes, len);
    return 0;
}

// パーティ競合確認
int32_t party_check_conflict(MapSessionData *sd)
{
    nullpo_ret(sd);

    intif_party_checkconflict(sd->status.party_id, sd->status.account_id,
                               sd->status.name);
    return 0;
}

// 位置やＨＰ通知用
static void party_send_xyhp_timer_sub(db_key_t, db_val_t data)
{
    struct party *p = static_cast<struct party *>(data.p);
    int32_t i;

    nullpo_retv(p);

    for (i = 0; i < MAX_PARTY; i++)
    {
        MapSessionData *sd;
        if ((sd = p->member[i].sd) != NULL)
        {
            // 座標通知
            if (sd->party_x != sd->x || sd->party_y != sd->y)
            {
                clif_party_xy(p, sd);
                sd->party_x = sd->x;
                sd->party_y = sd->y;
            }
            // ＨＰ通知
            if (sd->party_hp != sd->status.hp)
            {
                clif_party_hp(p, sd);
                sd->party_hp = sd->status.hp;
            }

        }
    }
}

// 位置やＨＰ通知
void party_send_xyhp_timer(timer_id, tick_t)
{
    numdb_foreach(party_db, party_send_xyhp_timer_sub);
}

// 位置通知クリア
int32_t party_send_xy_clear(struct party *p)
{
    int32_t i;

    nullpo_ret(p);

    for (i = 0; i < MAX_PARTY; i++)
    {
        MapSessionData *sd;
        if ((sd = p->member[i].sd) != NULL)
        {
            sd->party_x = -1;
            sd->party_y = -1;
            sd->party_hp = -1;
        }
    }
    return 0;
}

// HP通知の必要性検査用（map_foreachinmoveareaから呼ばれる）
void party_send_hp_check(BlockList *bl, party_t party_id, bool *flag)
{
    nullpo_retv(bl);
    MapSessionData *sd = static_cast<MapSessionData *>(bl);

    if (sd->status.party_id == party_id)
    {
        *flag = 1;
        sd->party_hp = -1;
    }
}

// 経験値公平分配
int32_t party_exp_share(struct party *p, int32_t map, int32_t base_exp, int32_t job_exp)
{
    MapSessionData *sd;
    int32_t i, c;

    nullpo_ret(p);

    for (i = c = 0; i < MAX_PARTY; i++)
        if ((sd = p->member[i].sd) != NULL && sd->m == map)
            c++;
    if (c == 0)
        return 0;
    for (i = 0; i < MAX_PARTY; i++)
        if ((sd = p->member[i].sd) != NULL && sd->m == map)
            pc_gainexp(sd, base_exp / c + 1, job_exp / c + 1);
    return 0;
}
