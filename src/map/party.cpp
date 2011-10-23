#include "party.hpp"

#include "../lib/dmap.hpp"

#include "../common/nullpo.hpp"
#include "../common/timer.hpp"
#include "../common/utils.hpp"

#include "battle.hpp"
#include "chrif.hpp"
#include "clif.hpp"
#include "main.hpp"
#include "pc.hpp"
#include "skill.hpp"
#include "tmw.hpp"

constexpr std::chrono::seconds PARTY_SEND_XYHP_INVERVAL(1);

static sint32 party_check_conflict(MapSessionData *sd);
static sint32 party_send_xy_clear(struct party *p);

static DMap<party_t, struct party *> party_db;

static void party_send_xyhp_timer(timer_id tid, tick_t tick);

// 初期化
void do_init_party(void)
{
    add_timer_interval(gettick() + PARTY_SEND_XYHP_INVERVAL,
                       PARTY_SEND_XYHP_INVERVAL, party_send_xyhp_timer);
}

// 検索
struct party *party_search(party_t party_id)
{
    return party_db.get(party_id);
}

// パーティ名検索
struct party *party_searchname(const char *str)
{
    for (auto& pair : party_db)
    {
        struct party *p = pair.second;
        if (strcasecmp(p->name, str) == 0)
            return p;
    }
    return NULL;
}

/* Process a party creation request. */
sint32 party_create(MapSessionData *sd, const char *name)
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
    if (!sd->status.party_id)
        intif_create_party(sd, pname);
    else
        clif_party_created(sd, 2);

    return 0;
}

/* Relay the result of a party creation request. */
void party_created(account_t account_id, bool fail, party_t party_id, const char *name)
{
    MapSessionData *sd = map_id2sd(account_id);

    nullpo_retv(sd);

    /* The party name is valid and not already taken. */
    if (!fail)
    {
        struct party *p = party_db.get(party_id);
        sd->status.party_id = party_id;

        if (p)
        {
            printf("party_created(): ID already exists!\n");
            abort();
        }

        CREATE(p, struct party, 1);
        p->party_id = party_id;
        memcpy(p->name, name, 24);
        party_db.set(party_id, p);

        /* The party was created successfully. */
        clif_party_created(sd, 0);
    }

    else
        clif_party_created(sd, 1);
}

// 情報要求
void party_request_info(party_t party_id)
{
    intif_request_partyinfo(party_id);
}

// 所属キャラの確認
static sint32 party_check_member(const struct party *p)
{
    nullpo_ret(p);

    for (MapSessionData *sd : auth_sessions)
    {
        if (sd->status.party_id != p->party_id)
            continue;
        bool f = 1;
        for (sint32 j = 0; j < MAX_PARTY; j++)
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
            sd->status.party_id = party_t();
            MAP_LOG("party: check_member %d[%s] is not member\n",
                    sd->status.account_id, sd->status.name);
        }
    }
    return 0;
}

// 情報所得失敗（そのIDのキャラを全部未所属にする）
void party_recv_noinfo(party_t party_id)
{
    for (MapSessionData *sd : auth_sessions)
    {
        if (sd->status.party_id == party_id)
            sd->status.party_id = party_t();
    }
}

// 情報所得
sint32 party_recv_info(const struct party *sp)
{
    nullpo_ret(sp);

    struct party *p = party_db.get(sp->party_id);
    sint32 i;

    if (!p)
    {
        CREATE(p, struct party, 1);
        party_db.set(sp->party_id, p);

        // 最初のロードなのでユーザーのチェックを行う
        party_check_member(sp);
    }
    memcpy(p, sp, sizeof(struct party));

    for (i = 0; i < MAX_PARTY; i++)
    {                           // sdの設定
        MapSessionData *sd = map_id2sd(p->member[i].account_id);
        p->member[i].sd = (sd && sd->status.party_id == p->party_id) ? sd : NULL;
    }

    clif_party_info(p, -1);

    for (i = 0; i < MAX_PARTY; i++)
    {                           // 設定情報の送信
//      MapSessionData *sd = map_id2sd(p->member[i].account_id);
        MapSessionData *sd = p->member[i].sd;
        if (sd && !sd->party_sent)
        {
            clif_party_option(p, sd, 0x100);
            sd->party_sent = true;
        }
    }

    return 0;
}

/* Process party invitation from sd to account_id. */
void party_invite(MapSessionData *sd, account_t account_id)
{
    MapSessionData *tsd = map_id2sd(account_id);
    struct party *p = party_search(sd->status.party_id);
    sint32 i;
    sint32 full = 1; /* Indicates whether or not there's room for one more. */

    nullpo_retv(sd);

    if (!tsd || !p || !tsd->fd)
        return;

    if (!battle_config.invite_request_check)
    {
        /* Disallow the invitation under these conditions. */
        if (tsd->trade_partner || tsd->npc_id
            || tsd->npc_shopid || pc_checkskill(tsd, NV_PARTY) < 1)
        {
            clif_party_inviteack(sd, tsd->status.name, 1);
            return;
        }
    }

    /* The target player is already in a party, or has a pending invitation. */
    if (tsd->status.party_id || tsd->party_invite)
    {
        clif_party_inviteack(sd, tsd->status.name, 0);
        return;
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
            return;
        }

        if (!p->member[i].account_id)
            full = 0;
    }

    /* There isn't enough room for a new member. */
    if (full)
    {
        clif_party_inviteack(sd, tsd->status.name, 3);
        return;
    }

    /* Otherwise, relay the invitation to the target player. */
    tsd->party_invite = sd->status.party_id;
    tsd->party_invite_account = sd->status.account_id;

    clif_party_invite(sd, tsd);
}

/* Process response to party invitation. */
void party_reply_invite(MapSessionData *sd, account_t account_id, bool flag)
{
    nullpo_retv(sd);

    /* There is no pending invitation. */
    if (!sd->party_invite || !sd->party_invite_account)
        return;

    /*
     * Only one invitation can be pending, so these have to be the same. Since
     * the client continues to send the wrong ID, and it's already managed on
     * this side of things, the sent ID is being ignored.
     */
    account_id = sd->party_invite_account;

    /* The invitation was accepted. */
    if (flag)
        intif_party_addmember(sd->party_invite, sd->status.account_id);
    /* The invitation was rejected. */
    else
    {
        /* This is the player who sent the invitation. */
        MapSessionData *tsd = NULL;

        sd->party_invite = party_t();
        sd->party_invite_account = account_t();

        if ((tsd = map_id2sd(account_id)))
            clif_party_inviteack(tsd, sd->status.name, 1);
    }
}

// パーティが追加された
void party_member_added(party_t party_id, account_t account_id, bool flag)
{
    MapSessionData *sd = map_id2sd(account_id), *sd2;
    struct party *p = party_search(party_id);

    if (!sd)
    {
        if (!flag)
        {
            MAP_LOG("party: member added error %d is not online\n", account_id);
            intif_party_leave(party_id, account_id);   // キャラ側に登録できなかったため脱退要求を出す
        }
        return;
    }
    sd2 = map_id2sd(sd->party_invite_account);
    sd->party_invite = party_t();
    sd->party_invite_account = account_t();

    if (!p)
    {
        PRINTF("party_member_added: party %d not found.\n", party_id);
        intif_party_leave(party_id, account_id);
        return;
    }

    if (flag)
    {                           // 失敗
        if (sd2)
            clif_party_inviteack(sd2, sd->status.name, 0);
        return;
    }

    // 成功
    sd->party_sent = false;
    sd->status.party_id = party_id;

    if (sd2)
        clif_party_inviteack(sd2, sd->status.name, 2);

    // いちおう競合確認
    party_check_conflict(sd);

    party_send_xy_clear(p);
}

// パーティ除名要求
void party_removemember(MapSessionData *sd, account_t account_id)
{
    struct party *p;
    sint32 i;

    nullpo_retv(sd);

    if ((p = party_search(sd->status.party_id)) == NULL)
        return;

    for (i = 0; i < MAX_PARTY; i++)
    {                           // リーダーかどうかチェック
        if (p->member[i].account_id == sd->status.account_id)
            if (p->member[i].leader == 0)
                return;
    }

    for (i = 0; i < MAX_PARTY; i++)
    {                           // 所属しているか調べる
        if (p->member[i].account_id == account_id)
        {
            intif_party_leave(p->party_id, account_id);
            return;
        }
    }
}

// パーティ脱退要求
sint32 party_leave(MapSessionData *sd)
{
    struct party *p;
    sint32 i;

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
void party_member_left(party_t party_id, account_t account_id, const char *name)
{
    MapSessionData *sd = map_id2sd(account_id);
    struct party *p = party_search(party_id);
    if (p != NULL)
    {
        sint32 i;
        for (i = 0; i < MAX_PARTY; i++)
            if (p->member[i].account_id == account_id)
            {
                clif_party_left(p, sd, account_id, name, 0x00);
                p->member[i].account_id = account_t();
                p->member[i].sd = NULL;
            }
    }
    if (sd != NULL && sd->status.party_id == party_id)
    {
        sd->status.party_id = party_t();
        sd->party_sent = false;
    }
}

// パーティ解散通知
void party_broken(party_t party_id)
{
    struct party *p;
    sint32 i;
    if ((p = party_search(party_id)) == NULL)
        return;

    for (i = 0; i < MAX_PARTY; i++)
    {
        if (p->member[i].sd)
        {
            clif_party_left(p, p->member[i].sd, p->member[i].account_id,
                            p->member[i].name, 0x10);
            p->member[i].sd->status.party_id = party_t();
            p->member[i].sd->party_sent = false;
        }
    }
    party_db.remove(party_id);
}

// パーティの設定変更要求
void party_changeoption(MapSessionData *sd, bool exp, bool item)
{
    struct party *p;

    nullpo_retv(sd);

    if (!sd->status.party_id
        || (p = party_search(sd->status.party_id)) == NULL)
        return;
    intif_party_changeoption(sd->status.party_id, sd->status.account_id, exp, item);
}

// パーティの設定変更通知
void party_optionchanged(party_t party_id, account_t account_id,
                         bool exp, bool item, uint8 flag)
{
    struct party *p;
    MapSessionData *sd = map_id2sd(account_id);
    if ((p = party_search(party_id)) == NULL)
        return;

    if (!(flag & 0x01))
        p->exp = exp;
    if (!(flag & 0x10))
        p->item = item;
    clif_party_option(p, sd, flag);
}

// パーティメンバの移動通知
void party_recv_movemap(party_t party_id, account_t account_id,
                        const char *map, bool online, level_t lv)
{
    struct party *p;
    sint32 i;
    if ((p = party_search(party_id)) == NULL)
        return;
    for (i = 0; i < MAX_PARTY; i++)
    {
        struct party_member *m = &p->member[i];
        if (m == NULL)
        {
            printf("party_recv_movemap nullpo?\n");
            return;
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
        MAP_LOG("party: not found member %d on %d[%s]",
                account_id, party_id, p->name);
        return;
    }

    for (i = 0; i < MAX_PARTY; i++)
    {                           // sd再設定
        MapSessionData *sd = map_id2sd(p->member[i].account_id);
        p->member[i].sd = (sd != NULL
                           && sd->status.party_id == p->party_id) ? sd : NULL;
    }

    party_send_xy_clear(p);    // 座標再通知要請

    clif_party_info(p, -1);
}

// パーティメンバの移動
void party_send_movemap(MapSessionData *sd)
{
    struct party *p;

    nullpo_retv(sd);

    if (!sd->status.party_id)
        return;
    intif_party_changemap(sd, 1);

    if (sd->party_sent)  // もうパーティデータは送信済み
        return;

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
            sd->party_sent = true;
        }
    }
}

// パーティメンバのログアウト
void party_send_logout(MapSessionData *sd)
{
    struct party *p;

    nullpo_retv(sd);

    if (sd->status.party_id)
        intif_party_changemap(sd, 0);

    // sdが無効になるのでパーティ情報から削除
    if ((p = party_search(sd->status.party_id)) != NULL)
    {
        sint32 i;
        for (i = 0; i < MAX_PARTY; i++)
            if (p->member[i].sd == sd)
                p->member[i].sd = NULL;
    }
}

// パーティメッセージ送信
void party_send_message(MapSessionData *sd, char *mes, sint32 len)
{
    if (!sd->status.party_id)
        return;
    intif_party_message(sd->status.party_id, sd->status.account_id, mes, len);
}

// パーティメッセージ受信
void party_recv_message(party_t party_id, account_t account_id, const char *mes, sint32 len)
{
    struct party *p;
    if ((p = party_search(party_id)) == NULL)
        return;
    clif_party_message(p, account_id, mes, len);
}

// パーティ競合確認
sint32 party_check_conflict(MapSessionData *sd)
{
    nullpo_ret(sd);

    intif_party_checkconflict(sd->status.party_id, sd->status.account_id,
                               sd->status.name);
    return 0;
}

// 位置やＨＰ通知用
static void party_send_xyhp_timer_sub(party_t, struct party *p)
{
    sint32 i;

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
    for (auto& pair : party_db)
        party_send_xyhp_timer_sub(pair.first, pair.second);
}

// 位置通知クリア
sint32 party_send_xy_clear(struct party *p)
{
    sint32 i;

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
sint32 party_exp_share(struct party *p, sint32 map, sint32 base_exp, sint32 job_exp)
{
    MapSessionData *sd;
    sint32 i, c;

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
