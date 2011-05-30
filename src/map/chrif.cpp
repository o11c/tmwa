#include "chrif.hpp"
#include "intif.hpp"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <stdio.h>
#include <stdlib.h>

#include <sys/time.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <signal.h>
#include <fcntl.h>
#include <string.h>

#include <time.h>

#include "../common/nullpo.hpp"
#include "../common/version.hpp"
#include "../common/socket.hpp"
#include "../common/timer.hpp"
#include "map.hpp"
#include "battle.hpp"
#include "clif.hpp"
#include "npc.hpp"
#include "pc.hpp"
#include "party.hpp"
#include "storage.hpp"
#include "itemdb.hpp"

#include "../login/login.hpp"

int char_fd = -1;

static char char_ip_str[16];
static int char_ip;
static int char_port = 6121;
static char userid[24], passwd[24];
static int chrif_state;

/// Set the name of the account used to connect to the char server
void chrif_setuserid(const char *id)
{
    STRZCPY(userid, id);
}

/// Set the password used to connect to the char server
void chrif_setpasswd(const char *pwd)
{
    STRZCPY(passwd, pwd);
    passwd[sizeof(passwd)-1] = '\0';
}

/// Get the password used to connect to the char server
const char *chrif_getpasswd(void)
{
    return passwd;
}

/// Set the IP of the char server
void chrif_setip(const char *ip)
{
    STRZCPY(char_ip_str, ip);
    char_ip = inet_addr(char_ip_str);
}

/// Set the port of the char server
void chrif_setport(in_port_t port)
{
    char_port = port;
}

/// Are we connected *and* the server got our maps?
bool chrif_isconnect(void)
{
    return chrif_state == 2;
}

/// Request to save a character
void chrif_save(struct map_session_data *sd)
{
    nullpo_retv(sd);

    if (char_fd < 0)
        return;

    pc_makesavestatus(sd);

    WFIFOW(char_fd, 0) = 0x2b01;
    WFIFOW(char_fd, 2) = sizeof(sd->status) + 12;
    WFIFOL(char_fd, 4) = sd->bl.id;
    WFIFOL(char_fd, 8) = sd->char_id;
    memcpy(WFIFOP(char_fd, 12), &sd->status, sizeof(sd->status));
    WFIFOSET(char_fd, WFIFOW(char_fd, 2));

    if (sd->state.storage_flag == 1)
        storage_storage_save(sd->status.account_id, 0);
}

/// Connect to the char server, telling it our stuff
static void chrif_connect(int fd)
{
    WFIFOW(fd, 0) = 0x7530;
    WFIFOSET(fd, 2);

    WFIFOW(fd, 0) = 0x2af8;
    STRZCPY2((char *)WFIFOP(fd, 2), userid);
    STRZCPY2((char *)WFIFOP(fd, 26), passwd);
    WFIFOL(fd, 50) = 0;
    WFIFOL(fd, 54) = clif_getip();
    WFIFOW(fd, 58) = clif_getport();
    WFIFOSET(fd, 60);
}

/// Send our maps to the char server
static void chrif_sendmap(int fd)
{
    WFIFOW(fd, 0) = 0x2afa;
    for (int i = 0; i < map_num; i++)
        STRZCPY2((char *)WFIFOP(fd, 4 + i * 16), maps[i].name);
    WFIFOW(fd, 2) = 4 + map_num * 16;
    WFIFOSET(fd, WFIFOW(fd, 2));
}

/// Receive maps of other map servers
static void chrif_recvmap(int fd)
{
    if (chrif_state < 2)
        return;

    in_addr_t ip = RFIFOL(fd, 4);
    in_port_t port = RFIFOW(fd, 8);
    int j = 0;
    for (int i = 10; j < (RFIFOW(fd, 2) - 10) / 16; i += 16, j++)
    {
        map_setipport((char *)RFIFOP(fd, i), ip, port);
    }
    uint8_t *p = (uint8_t *) &ip;
    map_log("recv map on %hhu.%hhu.%hhu.%hhu:%hu (%d maps)\n", p[0], p[1], p[2], p[3], port, j);
}

/// Arrange for a character to change to another map server
void chrif_changemapserver(struct map_session_data *sd,
                           const char mapname[16], int x, int y,
                           in_addr_t ip, in_port_t port)
{
    nullpo_retv(sd);

    int i = sd->fd;
    in_addr_t s_ip = session[i]->client_addr.sin_addr.s_addr;

    WFIFOW(char_fd, 0) = 0x2b05;
    WFIFOL(char_fd, 2) = sd->bl.id;
    WFIFOL(char_fd, 6) = sd->login_id1;
    WFIFOL(char_fd, 10) = sd->login_id2;
    WFIFOL(char_fd, 14) = sd->status.char_id;
    strzcpy((char *)WFIFOP(char_fd, 18), mapname, 16);
    WFIFOW(char_fd, 34) = x;
    WFIFOW(char_fd, 36) = y;
    WFIFOL(char_fd, 38) = ip;
    WFIFOL(char_fd, 42) = port;
    WFIFOB(char_fd, 44) = sd->status.sex;
    WFIFOL(char_fd, 45) = s_ip;
    WFIFOSET(char_fd, 49);
}

/// The result of telling the char server we want to forward someone
/// we then pass it on to the client
static void chrif_changemapserverack(int fd)
{
    struct map_session_data *sd = map_id2sd(RFIFOL(fd, 2));

    if (!sd || sd->status.char_id != RFIFOL(fd, 14))
        return;

    if (RFIFOL(fd, 6) == 1)
    {
        map_log("map server change failed.\n");
        pc_authfail(sd->fd);
        return;
    }
    clif_changemapserver(sd, (char *)RFIFOP(fd, 18), RFIFOW(fd, 34),
                         RFIFOW(fd, 36), RFIFOL(fd, 38), RFIFOW(fd, 42));
}

/// Result of trying to connect to the char server
static void chrif_connectack(int fd)
{
    if (RFIFOB(fd, 2))
    {
        map_log("Connected to char-server failed %d.\n", RFIFOB(fd, 2));
        exit(1);
    }

    printf("Connected to char-server (connection #%d).\n", fd);
    chrif_state = 1;

    chrif_sendmap(fd);

    printf("chrif: OnCharIfInit event done. (%d events)\n",
           npc_event_doall("OnCharIfInit"));
    printf("chrif: OnInterIfInit event done. (%d events)\n",
           npc_event_doall("OnInterIfInit"));
}

/// Server got our maps
static void chrif_sendmapack(int fd)
{
    if (RFIFOB(fd, 2))
    {
        printf("chrif : send map list to char server failed %d\n", RFIFOB(fd, 2));
        exit(1);
    }

    STRZCPY(wisp_server_name, (const char *)RFIFOP(fd, 3));

    chrif_state = 2;
}

/// Request to authenticate the client
void chrif_authreq(struct map_session_data *sd)
{
    nullpo_retv(sd);

    if (!sd || char_fd < 0 || !sd->bl.id || !sd->login_id1)
        return;

    int i = sd->fd;
    WFIFOW(char_fd, 0) = 0x2afc;
    WFIFOL(char_fd, 2) = sd->bl.id;
    WFIFOL(char_fd, 6) = sd->char_id;
    WFIFOL(char_fd, 10) = sd->login_id1;
    WFIFOL(char_fd, 14) = sd->login_id2;
    WFIFOL(char_fd, 18) = session[i]->client_addr.sin_addr.s_addr;
    WFIFOSET(char_fd, 22);
}

/*==========================================
 *
 *------------------------------------------
 */
void chrif_charselectreq(struct map_session_data *sd)
{
    nullpo_retv(sd);

    if (!sd || char_fd < 0 || !sd->bl.id || !sd->login_id1)
        return;

    int i = sd->fd;
    in_addr_t s_ip = session[i]->client_addr.sin_addr.s_addr;

    WFIFOW(char_fd, 0) = 0x2b02;
    WFIFOL(char_fd, 2) = sd->bl.id;
    WFIFOL(char_fd, 6) = sd->login_id1;
    WFIFOL(char_fd, 10) = sd->login_id2;
    WFIFOL(char_fd, 14) = s_ip;
    WFIFOSET(char_fd, 18);
}

/// Change GM level (@gm)
void chrif_changegm(int id, const char *pass, int len)
{
    map_log("%s: account: %d, password: '%s'.\n", __func__, id, pass);

    WFIFOW(char_fd, 0) = 0x2b0a;
    WFIFOW(char_fd, 2) = len + 8;
    WFIFOL(char_fd, 4) = id;
    memcpy(WFIFOP(char_fd, 8), pass, len);
    WFIFOSET(char_fd, len + 8);
}

/// Change email
void chrif_changeemail(int id, const char *actual_email, const char *new_email)
{
    map_log("%s: account: %d, actual_email: '%s', new_email: '%s'.\n", __func__,
            id, actual_email, new_email);

    WFIFOW(char_fd, 0) = 0x2b0c;
    WFIFOL(char_fd, 2) = id;
    strzcpy((char *)WFIFOP(char_fd, 6), actual_email, 40);
    strzcpy((char *)WFIFOP(char_fd, 46), new_email, 40);
    WFIFOSET(char_fd, 86);
}

/// Do miscellaneous operations on a character
void chrif_char_ask_name(int id, const char *character_name, CharOperation operation_type,
                         int year, int month, int day, int hour, int minute, int second)
{
    WFIFOW(char_fd, 0) = 0x2b0e;
    // who asked, or -1 if server
    WFIFOL(char_fd, 2) = id;
    strzcpy((char *)WFIFOP(char_fd, 6), character_name, 24);
    WFIFOW(char_fd, 30) = (uint16_t)operation_type;
    if (operation_type == CharOperation::BAN)
    {
        WFIFOW(char_fd, 32) = year;
        WFIFOW(char_fd, 34) = month;
        WFIFOW(char_fd, 36) = day;
        WFIFOW(char_fd, 38) = hour;
        WFIFOW(char_fd, 40) = minute;
        WFIFOW(char_fd, 42) = second;
    }
    WFIFOSET(char_fd, 44);
}

/// Reply (only sent when a human asked)
static void chrif_char_ask_name_answer(int fd)
{
    account_t acc = RFIFOL(fd, 2);
    char player_name[24];
    STRZCPY(player_name, (char *)RFIFOP(fd, 6));

    struct map_session_data *sd = map_id2sd(acc);
    if (!sd)
        map_log("%s: failed - player not online.\n", __func__);

    char output[256] = {};

    if (RFIFOW(fd, 32) == 1)
    {
        sprintf(output, "The player '%s' doesn't exist.", player_name);
        clif_displaymessage(sd->fd, output);
        return;
    }

    const char *result;
    switch (RFIFOW(fd, 32))
    {
        case 0: result = "Login-server has been asked"; break;
        case 2: result = "Your GM level doesn't authorise you"; break;
        case 3: result = "Login-server is offline. Impossible"; break;
        default: return;
    }

    const char *what;
    switch (RFIFOW(fd, 30))
    {
    case 1: what = "to block the player"; break;
    case 2: what = "to ban the player"; break;
    case 3: what = "to unblock the player"; break;
    case 4: what = "to unban the player"; break;
    case 5: what = "to change the sex of the player"; break;
    default: return;
    }

    snprintf(output, sizeof(output), "%s %s '%s'.", result, what, player_name);
    clif_displaymessage(sd->fd, output);
}

/// A GM level changed
static void chrif_changedgm(int fd)
{
    account_t acc = RFIFOL(fd, 2);
    gm_level_t level = RFIFOL(fd, 6);

    struct map_session_data *sd = map_id2sd(acc);

    map_log("chrif_changedgm: account: %u, GM level 0 -> %hhu.\n", acc, level);

    if (!sd)
        return;
    if (level)
        clif_displaymessage(sd->fd, "GM modification success.");
    else
        clif_displaymessage(sd->fd, "Failure of GM modification.");
}

/// Actually flip someone's sex
static void chrif_changedsex(int fd)
{
    account_t acc = RFIFOL(fd, 2);
    uint32_t sex = RFIFOL(fd, 6);
    map_log("chrif_changedsex %d.\n", acc);
    struct map_session_data *sd = map_id2sd(acc);
    if (!sd || sd->status.sex == sex)
        return;

    if (sd->status.sex == 0)
    {
        sd->status.sex = 1;
        sd->sex = 1;
    }
    else if (sd->status.sex == 1)
    {
        sd->status.sex = 0;
        sd->sex = 0;
    }
    // to avoid any problem with equipment and invalid sex, equipment is unequiped.
    for (int i = 0; i < MAX_INVENTORY; i++)
    {
        if (sd->status.inventory[i].nameid && sd->status.inventory[i].equip)
            pc_unequipitem(sd, i, 0);
    }
    // save character
    chrif_save(sd);
    // ?? to avoid reconnection error when come back from map-server (char-server will ask again the authentication)
    sd->login_id1++;
    clif_displaymessage(sd->fd, "Your sex has been changed (need disconexion by the server)...");
    clif_setwaitclose(sd->fd);
}

/// Save variables
void chrif_saveaccountreg2(struct map_session_data *sd)
{
    nullpo_retv(sd);

    int p = 8;
    for (int j = 0; j < sd->status.account_reg2_num; j++)
    {
        struct global_reg *reg = &sd->status.account_reg2[j];
        if (reg->str[0] && reg->value)
        {
            STRZCPY2((char *)WFIFOP(char_fd, p), reg->str);
            p += 32;
            WFIFOL(char_fd, p) = reg->value;
            p += 4;
        }
    }
    WFIFOW(char_fd, 0) = 0x2b10;
    WFIFOW(char_fd, 2) = p;
    WFIFOL(char_fd, 4) = sd->bl.id;
    WFIFOSET(char_fd, p);
}

/// Load variables
static void chrif_accountreg2(int fd)
{
    struct map_session_data *sd = map_id2sd(RFIFOL(fd, 4));

    if (!sd)
        return;

    int p = 8;
    int j;
    for (j = 0; p < RFIFOW(fd, 2) && j < ACCOUNT_REG2_NUM; j++)
    {
        STRZCPY(sd->status.account_reg2[j].str, (const char *)RFIFOP(fd, p));
        p += 32;
        sd->status.account_reg2[j].value = RFIFOL(fd, p);
        p += 4;
    }
    sd->status.account_reg2_num = j;
}

/**
 * Divorce request from char server
 * triggered on account deletion or as an
 * ack from a map-server divorce request
 */
static void chrif_divorce(int char_id, int partner_id)
{
    if (!char_id || !partner_id)
        return;

    struct map_session_data *sd;
    sd = map_nick2sd(map_charid2nick(char_id));
    if (sd && sd->status.partner_id == partner_id)
    {
        sd->status.partner_id = 0;

        if (sd->npc_flags.divorce)
        {
            sd->npc_flags.divorce = 0;
            map_scriptcont(sd, sd->npc_id);
        }
    }

    sd = map_nick2sd(map_charid2nick(partner_id));

    if (sd && sd->status.partner_id == char_id)
        sd->status.partner_id = 0;
}

/**
 * Tell character server someone is divorced
 * Needed to divorce when partner is not connected to map server
 */
void chrif_send_divorce(int char_id)
{
    if (char_fd < 0)
        return;

    WFIFOW(char_fd, 0) = 0x2b16;
    WFIFOL(char_fd, 2) = char_id;
    WFIFOSET(char_fd, 6);
}

/**
 * Disconnection of a player(account has been deleted in login-server) by [Yor]
 */
static void chrif_accountdeletion(int fd)
{
    account_t acc = RFIFOL(fd, 2);
    map_log("chrif_accountdeletion %d.\n", acc);
    struct map_session_data *sd = map_id2sd(acc);
    if (!sd)
        return;
    sd->login_id1++;
    clif_displaymessage(sd->fd, "Your account has been deleted (disconnection)...");
    clif_setwaitclose(sd->fd);
}

/**
 * Disconnection of a player(account has been banned of has a status, from login-server) by [Yor]
 */
static void chrif_accountban(int fd)
{
    account_t acc = RFIFOL(fd, 2);
    map_log("chrif_accountban %d.\n", acc);
    struct map_session_data *sd = map_id2sd(acc);
    if (!sd)
        return;

    sd->login_id1++;
    if (RFIFOB(fd, 6))
    {
        char tmpstr[2048];
        time_t timestamp = (time_t) RFIFOL(fd, 7);
        sprintf(tmpstr, "Your account has been banished until %s", stamp_time(timestamp));

        clif_displaymessage(sd->fd, tmpstr);
        clif_setwaitclose(sd->fd);
        return;
    }
    // change of status
    switch ((enum auth_failure)RFIFOL(fd, 7))
    {
        case AUTH_UNREGISTERED_ID:
            clif_displaymessage(sd->fd, "Your account has 'Unregistered'.");
            break;
        case AUTH_INVALID_PASSWORD:
            clif_displaymessage(sd->fd, "Your account has an 'Incorrect Password'...");
            break;
        case AUTH_EXPIRED:
            clif_displaymessage(sd->fd, "Your account has expired.");
            break;
        case AUTH_REJECTED_BY_SERVER:
            clif_displaymessage(sd->fd, "Your account has been rejected from server.");
            break;
        case AUTH_BLOCKED_BY_GM:
            clif_displaymessage(sd->fd, "Your account has been blocked by the GM Team.");
            break;
        case AUTH_CLIENT_TOO_OLD:
            clif_displaymessage(sd->fd, "Your Game's EXE file is not the latest version.");
            break;
        case AUTH_BANNED_TEMPORARILY:
            clif_displaymessage(sd->fd, "Your account has been prohibited to log in.");
            break;
        case AUTH_SERVER_OVERPOPULATED:
            clif_displaymessage(sd->fd, "Server is jammed due to over populated.");
            break;
        case AUTH_ID_ERASED:
            clif_displaymessage(sd->fd, "Your account has been totally erased.");
            break;
        default:
            clif_displaymessage(sd->fd, "Your account has not more authorised (?).");
            break;
    }
    clif_setwaitclose(sd->fd);
}

/// Receive GM account list
static void chrif_recvgmaccounts(int fd)
{
    printf("From login-server: receiving of %d GM accounts information.\n",
           pc_read_gm_account(fd));
}

/*========================================
 * Map item IDs
 *----------------------------------------
 */

static void ladmin_itemfrob_fix_item(int source, int dest, struct item *item)
{
    if (item && item->nameid == source)
    {
        item->nameid = dest;
        item->equip = 0;
    }
}

static void ladmin_itemfrob_c2(struct block_list *bl, int source_id,
                               int dest_id)
{
#define IFIX(v) if (v == source_id) {v = dest_id; }
#define FIX(item) ladmin_itemfrob_fix_item(source_id, dest_id, &item)

    if (!bl)
        return;

    switch (bl->type)
    {
    case BL_PC:
    {
        struct map_session_data *pc = (struct map_session_data *) bl;
        struct storage *stor = account2storage2(pc->status.account_id);

        for (int j = 0; j < MAX_INVENTORY; j++)
            IFIX(pc->status.inventory[j].nameid);
        IFIX(pc->status.weapon);
        IFIX(pc->status.shield);
        IFIX(pc->status.head_top);
        IFIX(pc->status.head_mid);
        IFIX(pc->status.head_bottom);

        if (stor)
            for (int j = 0; j < stor->storage_amount; j++)
                FIX(stor->storage_[j]);

        for (int j = 0; j < MAX_INVENTORY; j++)
        {
            struct item_data *item = pc->inventory_data[j];
            if (item && item->nameid == source_id)
            {
                item->nameid = dest_id;
                if (item->equip)
                    pc_unequipitem(pc, j, 0);
                item->nameid = dest_id;
            }
        }

        break;
    }

    case BL_MOB:
    {
        struct mob_data *mob = (struct mob_data *) bl;
        for (int i = 0; i < mob->lootitem_count; i++)
            FIX(mob->lootitem[i]);
        break;
    }

    case BL_ITEM:
    {
        struct flooritem_data *item = (struct flooritem_data *) bl;
        FIX(item->item_data);
        break;
    }
    }
#undef FIX
#undef IFIX
}

static void ladmin_itemfrob_c(struct block_list *bl, va_list va_args)
{
    int source_id = va_arg(va_args, int);
    int dest_id = va_arg(va_args, int);
    ladmin_itemfrob_c2(bl, source_id, dest_id);
}

static void ladmin_itemfrob(int fd)
{
    int source_id = RFIFOL(fd, 2);
    int dest_id = RFIFOL(fd, 6);
    struct block_list *bl = (struct block_list *) map_get_first_session();

    // flooritems
    map_foreachobject(ladmin_itemfrob_c, BL_NUL, source_id, dest_id);

    // player characters
    while (bl->next)
    {
        ladmin_itemfrob_c2(bl, source_id, dest_id);
        bl = bl->next;
    }
}


//-----------------------------------------------------------------
// inter serverへの送信

// Message for all GMs on all map servers
int intif_GMmessage(const char *mes, int len)
{
    WFIFOW(char_fd, 0) = 0x3000;
    WFIFOW(char_fd, 2) = 4 + len;
    memcpy(WFIFOP(char_fd, 4), mes, len);
    WFIFOSET(char_fd, WFIFOW(char_fd, 2));

    return 0;
}

// The transmission of Wisp/Page to inter-server (player not found on this server)
int intif_wis_message(struct map_session_data *sd, char *nick, char *mes,
                       int mes_len)
{
    nullpo_retr(0, sd);

    WFIFOW(char_fd, 0) = 0x3001;
    WFIFOW(char_fd, 2) = mes_len + 52;
    memcpy(WFIFOP(char_fd, 4), sd->status.name, 24);
    memcpy(WFIFOP(char_fd, 28), nick, 24);
    memcpy(WFIFOP(char_fd, 52), mes, mes_len);
    WFIFOSET(char_fd, WFIFOW(char_fd, 2));

    map_log("inmap_logtif_wis_message from %s to %s (message: '%s')\n",
            sd->status.name, nick, mes);

    return 0;
}

// The reply of Wisp/page
static int intif_wis_replay(int id, int flag)
{
    WFIFOW(char_fd, 0) = 0x3002;
    WFIFOL(char_fd, 2) = id;
    WFIFOB(char_fd, 6) = flag;    // flag: 0: success to send wisper, 1: target character is not loged in?, 2: ignored by target
    WFIFOSET(char_fd, 7);

    map_log("inmap_logtif_wis_replay: id: %d, flag:%d\n", id, flag);

    return 0;
}

// The transmission of GM only Wisp/Page from server to inter-server
int intif_wis_message_to_gm(char *Wisp_name, int min_gm_level, char *mes,
                             int mes_len)
{
    WFIFOW(char_fd, 0) = 0x3003;
    WFIFOW(char_fd, 2) = mes_len + 30;
    memcpy(WFIFOP(char_fd, 4), Wisp_name, 24);
    WFIFOW(char_fd, 28) = (short) min_gm_level;
    memcpy(WFIFOP(char_fd, 30), mes, mes_len);
    WFIFOSET(char_fd, WFIFOW(char_fd, 2));

    map_log("intif_wis_message_to_gm: from: '%s', min level: %d, message: '%s'.\n",
            Wisp_name, min_gm_level, mes);

    return 0;
}

// アカウント変数送信
int intif_saveaccountreg(struct map_session_data *sd)
{
    int j, p;

    nullpo_retr(0, sd);

    WFIFOW(char_fd, 0) = 0x3004;
    WFIFOL(char_fd, 4) = sd->bl.id;
    for (j = 0, p = 8; j < sd->status.account_reg_num; j++, p += 36)
    {
        memcpy(WFIFOP(char_fd, p), sd->status.account_reg[j].str, 32);
        WFIFOL(char_fd, p + 32) = sd->status.account_reg[j].value;
    }
    WFIFOW(char_fd, 2) = p;
    WFIFOSET(char_fd, p);
    return 0;
}

// アカウント変数要求
int intif_request_accountreg(struct map_session_data *sd)
{
    nullpo_retr(0, sd);

    WFIFOW(char_fd, 0) = 0x3005;
    WFIFOL(char_fd, 2) = sd->bl.id;
    WFIFOSET(char_fd, 6);
    return 0;
}

// 倉庫データ要求
int intif_request_storage(int account_id)
{
    WFIFOW(char_fd, 0) = 0x3010;
    WFIFOL(char_fd, 2) = account_id;
    WFIFOSET(char_fd, 6);
    return 0;
}

// 倉庫データ送信
int intif_send_storage(struct storage *stor)
{
    nullpo_retr(0, stor);
    WFIFOW(char_fd, 0) = 0x3011;
    WFIFOW(char_fd, 2) = sizeof(struct storage) + 8;
    WFIFOL(char_fd, 4) = stor->account_id;
    memcpy(WFIFOP(char_fd, 8), stor, sizeof(struct storage));
    WFIFOSET(char_fd, WFIFOW(char_fd, 2));
    return 0;
}

// パーティ作成要求
int intif_create_party(struct map_session_data *sd, char *name)
{
    nullpo_retr(0, sd);

    WFIFOW(char_fd, 0) = 0x3020;
    WFIFOL(char_fd, 2) = sd->status.account_id;
    memcpy(WFIFOP(char_fd, 6), name, 24);
    memcpy(WFIFOP(char_fd, 30), sd->status.name, 24);
    memcpy(WFIFOP(char_fd, 54), maps[sd->bl.m].name, 16);
    WFIFOW(char_fd, 70) = sd->status.base_level;
    WFIFOSET(char_fd, 72);
//  if (battle_config.etc_log)
//      printf("intif: create party\n");
    return 0;
}

// パーティ情報要求
int intif_request_partyinfo(int party_id)
{
    WFIFOW(char_fd, 0) = 0x3021;
    WFIFOL(char_fd, 2) = party_id;
    WFIFOSET(char_fd, 6);
//  if (battle_config.etc_log)
//      printf("intif: request party info\n");
    return 0;
}

// パーティ追加要求
int intif_party_addmember(int party_id, int account_id)
{
    struct map_session_data *sd;
    sd = map_id2sd(account_id);
//  if (battle_config.etc_log)
//      printf("intif: party add member %d %d\n",party_id,account_id);
    if (sd != NULL)
    {
        WFIFOW(char_fd, 0) = 0x3022;
        WFIFOL(char_fd, 2) = party_id;
        WFIFOL(char_fd, 6) = account_id;
        memcpy(WFIFOP(char_fd, 10), sd->status.name, 24);
        memcpy(WFIFOP(char_fd, 34), maps[sd->bl.m].name, 16);
        WFIFOW(char_fd, 50) = sd->status.base_level;
        WFIFOSET(char_fd, 52);
    }
    return 0;
}

// パーティ設定変更
int intif_party_changeoption(int party_id, int account_id, int exp, int item)
{
    WFIFOW(char_fd, 0) = 0x3023;
    WFIFOL(char_fd, 2) = party_id;
    WFIFOL(char_fd, 6) = account_id;
    WFIFOW(char_fd, 10) = exp;
    WFIFOW(char_fd, 12) = item;
    WFIFOSET(char_fd, 14);
    return 0;
}

// パーティ脱退要求
int intif_party_leave(int party_id, int account_id)
{
//  if (battle_config.etc_log)
//      printf("intif: party leave %d %d\n",party_id,account_id);
    WFIFOW(char_fd, 0) = 0x3024;
    WFIFOL(char_fd, 2) = party_id;
    WFIFOL(char_fd, 6) = account_id;
    WFIFOSET(char_fd, 10);
    return 0;
}

// パーティ移動要求
int intif_party_changemap(struct map_session_data *sd, int online)
{
    if (sd != NULL)
    {
        WFIFOW(char_fd, 0) = 0x3025;
        WFIFOL(char_fd, 2) = sd->status.party_id;
        WFIFOL(char_fd, 6) = sd->status.account_id;
        memcpy(WFIFOP(char_fd, 10), maps[sd->bl.m].name, 16);
        WFIFOB(char_fd, 26) = online;
        WFIFOW(char_fd, 27) = sd->status.base_level;
        WFIFOSET(char_fd, 29);
    }
//  if (battle_config.etc_log)
//      printf("party: change map\n");
    return 0;
}

// パーティ会話送信
int intif_party_message(int party_id, int account_id, char *mes, int len)
{
//  if (battle_config.etc_log)
//      printf("intif_party_message: %s\n",mes);
    WFIFOW(char_fd, 0) = 0x3027;
    WFIFOW(char_fd, 2) = len + 12;
    WFIFOL(char_fd, 4) = party_id;
    WFIFOL(char_fd, 8) = account_id;
    memcpy(WFIFOP(char_fd, 12), mes, len);
    WFIFOSET(char_fd, len + 12);
    return 0;
}

// パーティ競合チェック要求
int intif_party_checkconflict(int party_id, int account_id, char *nick)
{
    WFIFOW(char_fd, 0) = 0x3028;
    WFIFOL(char_fd, 2) = party_id;
    WFIFOL(char_fd, 6) = account_id;
    memcpy(WFIFOP(char_fd, 10), nick, 24);
    WFIFOSET(char_fd, 34);
    return 0;
}

//-----------------------------------------------------------------
// Packets receive from inter server

// Wisp/Page reception
static int intif_parse_WisMessage(int fd)
{                               // rewritten by [Yor]
    struct map_session_data *sd;

    map_log("intif_parse_wismessage: id: %d, from: %s, to: %s, message: '%s'\n",
            RFIFOL(fd, 4), RFIFOP(fd, 8), RFIFOP(fd, 32), RFIFOP(fd,
                                                                      56));
    sd = map_nick2sd((char *)RFIFOP(fd, 32)); // Searching destination player
    if (sd != NULL && strcmp(sd->status.name, (char *)RFIFOP(fd, 32)) == 0)
    {                           // exactly same name (inter-server have checked the name before)
        clif_wis_message(sd->fd, (char *)RFIFOP(fd, 8), (char *)RFIFOP(fd, 56),
                          RFIFOW(fd, 2) - 56);
        intif_wis_replay(RFIFOL(fd, 4), 0);   // flag: 0: success to send wisper, 1: target character is not loged in?, 2: ignored by target
    }
    else
        intif_wis_replay(RFIFOL(fd, 4), 1);   // flag: 0: success to send wisper, 1: target character is not loged in?, 2: ignored by target
    return 0;
}

// Wisp/page transmission result reception
static int intif_parse_WisEnd(int fd)
{
    struct map_session_data *sd;

    map_log("inmap_logtif_parse_wisend: player: %s, flag: %d\n",
            (char *)RFIFOP(fd, 2), RFIFOB(fd, 26)); // flag: 0: success to send wisper, 1: target character is not loged in?, 2: ignored by target
    sd = map_nick2sd((char *)RFIFOP(fd, 2));
    if (sd != NULL)
        clif_wis_end(sd->fd, RFIFOB(fd, 26));

    return 0;
}

// Received wisp message from map-server via char-server for ALL gm
static int mapif_parse_WisToGM(int fd)
{                               // 0x3003/0x3803 <packet_len>.w <wispname>.24B <min_gm_level>.w <message>.?B
    int i, min_gm_level, len;
    struct map_session_data *pl_sd;
    char Wisp_name[24];
    char mbuf[255];

    if (RFIFOW(fd, 2) - 30 <= 0)
        return 0;

    len = RFIFOW(fd, 2) - 30;
    char *message = ((len) >= 255) ? (char *) malloc(len) : mbuf;

    min_gm_level = (int) RFIFOW(fd, 28);
    memcpy(Wisp_name, RFIFOP(fd, 4), 24);
    Wisp_name[23] = '\0';
    memcpy(message, RFIFOP(fd, 30), len);
    message[len - 1] = '\0';
    // information is sended to all online GM
    for (i = 0; i < fd_max; i++)
        if (session[i] && (pl_sd = (struct map_session_data *)session[i]->session_data)
            && pl_sd->state.auth)
            if (pc_isGM(pl_sd) >= min_gm_level)
                clif_wis_message(i, Wisp_name, message,
                                  strlen(message) + 1);

    if (message != mbuf)
        free(message);

    return 0;
}

// アカウント変数通知
static int intif_parse_AccountReg(int fd)
{
    int j, p;
    struct map_session_data *sd;

    if ((sd = map_id2sd(RFIFOL(fd, 4))) == NULL)
        return 1;
    for (p = 8, j = 0; p < RFIFOW(fd, 2) && j < ACCOUNT_REG_NUM;
         p += 36, j++)
    {
        memcpy(sd->status.account_reg[j].str, RFIFOP(fd, p), 32);
        sd->status.account_reg[j].value = RFIFOL(fd, p + 32);
    }
    sd->status.account_reg_num = j;
//  printf("intif: accountreg\n");

    return 0;
}

// 倉庫データ受信
static int intif_parse_LoadStorage(int fd)
{
    struct storage *stor;
    struct map_session_data *sd;

    sd = map_id2sd(RFIFOL(fd, 4));
    if (sd == NULL)
    {
        map_log("intif_map_logparse_LoadStorage: user not found %d\n",
                RFIFOL(fd, 4));
        return 1;
    }
    stor = account2storage(RFIFOL(fd, 4));
    if (stor->storage_status == 1)
    {                           // Already open.. lets ignore this update
        map_log("intif_parse_LoadStorage: storage received for a client already open (User %d:%d)\n",
                sd->status.account_id, sd->status.char_id);
        return 1;
    }
    if (stor->dirty)
    {                           // Already have storage, and it has been modified and not saved yet! Exploit! [Skotlex]
        map_log("intif_parse_LoadStorage: received storage for an already modified non-saved storage! (User %d:%d)\n",
                sd->status.account_id, sd->status.char_id);
        return 1;
    }

    if (RFIFOW(fd, 2) - 8 != sizeof(struct storage))
    {
        map_log("intif_map_logparse_LoadStorage: data size error %d %d\n",
                RFIFOW(fd, 2) - 8, sizeof(struct storage));
        return 1;
    }
    map_log("inmap_logtif_openstorage: %d\n", RFIFOL(fd, 4));
    memcpy(stor, RFIFOP(fd, 8), sizeof(struct storage));
    stor->dirty = 0;
    stor->storage_status = 1;
    sd->state.storage_flag = 1;
    clif_storageitemlist(sd, stor);
    clif_storageequiplist(sd, stor);
    clif_updatestorageamount(sd, stor);

    return 0;
}

// 倉庫データ送信成功
static int intif_parse_SaveStorage(int fd)
{
    map_log("inmap_logtif_savestorage: done %d %d\n", RFIFOL(fd, 2),
            RFIFOB(fd, 6));
    storage_storage_saved(RFIFOL(fd, 2));
    return 0;
}
// パーティ作成可否
static int intif_parse_PartyCreated(int fd)
{
    map_log("inmap_logtif: party created\n");
    party_created(RFIFOL(fd, 2), RFIFOB(fd, 6), RFIFOL(fd, 7),
                   (char *)RFIFOP(fd, 11));
    return 0;
}

// パーティ情報
static int intif_parse_PartyInfo(int fd)
{
    if (RFIFOW(fd, 2) == 8)
    {
        map_log("intif:map_log party noinfo %d\n", RFIFOL(fd, 4));
        party_recv_noinfo(RFIFOL(fd, 4));
        return 0;
    }

//  printf("intif: party info %d\n",RFIFOL(fd,4));
    if (RFIFOW(fd, 2) != sizeof(struct party) + 4)
    {
        map_log("intif:map_log party info : data size error %d %d %d\n",
                RFIFOL(fd, 4), RFIFOW(fd, 2), sizeof(struct party) + 4);
    }
    party_recv_info((struct party *) RFIFOP(fd, 4));
    return 0;
}

// パーティ追加通知
static int intif_parse_PartyMemberAdded(int fd)
{
    map_log("inmap_logtif: party member added %d %d %d\n", RFIFOL(fd, 2),
            RFIFOL(fd, 6), RFIFOB(fd, 10));
    party_member_added(RFIFOL(fd, 2), RFIFOL(fd, 6), RFIFOB(fd, 10));
    return 0;
}

// パーティ設定変更通知
static int intif_parse_PartyOptionChanged(int fd)
{
    party_optionchanged(RFIFOL(fd, 2), RFIFOL(fd, 6), RFIFOW(fd, 10),
                         RFIFOW(fd, 12), RFIFOB(fd, 14));
    return 0;
}

// パーティ脱退通知
static int intif_parse_PartyMemberLeaved(int fd)
{
    map_log("inmap_logtif: party member left %d %d %s\n", RFIFOL(fd, 2),
            RFIFOL(fd, 6), RFIFOP(fd, 10));
    party_member_left(RFIFOL(fd, 2), RFIFOL(fd, 6), (char *)RFIFOP(fd, 10));
    return 0;
}

// パーティ解散通知
static int intif_parse_PartyBroken(int fd)
{
    party_broken(RFIFOL(fd, 2));
    return 0;
}

// パーティ移動通知
static int intif_parse_PartyMove(int fd)
{
    party_recv_movemap(RFIFOL(fd, 2), RFIFOL(fd, 6), (char *)RFIFOP(fd, 10),
                        RFIFOB(fd, 26), RFIFOW(fd, 27));
    return 0;
}

// パーティメッセージ
static int intif_parse_PartyMessage(int fd)
{
    party_recv_message(RFIFOL(fd, 4), RFIFOL(fd, 8), (char *)RFIFOP(fd, 12),
                        RFIFOW(fd, 2) - 12);
    return 0;
}


/// Parse packets from the char server
static void chrif_parse(int fd)
{
    if (fd != char_fd)
        abort();


    if (session[fd]->eof)
    {
        printf("Map-server can't connect to char-server (connection #%d).\n", fd);
        char_fd = -1;

        close(fd);
        delete_session(fd);
        return;
    }

    while (RFIFOREST(fd) >= 2)
    {
        switch (RFIFOW(fd, 0))
        {
        case 0x7531:
            if (RFIFOREST(fd) < 10)
                return;
        {
            Version *server_version = (Version *)RFIFOP(fd, 2);
            if (!(server_version->what_server & ATHENA_SERVER_CHAR))
            {
                map_log("Not a char server!");
                abort();
            }
            if (!(server_version->what_server & ATHENA_SERVER_INTER))
            {
                map_log("Not an inter server!");
                abort();
            }
            if (server_version->major != tmwAthenaVersion.major
                || server_version->minor != tmwAthenaVersion.minor
                || server_version->rev != tmwAthenaVersion.rev)
            {
                map_log("Version mismatch!");
                abort();
            }
            RFIFOSKIP(fd, 10);
            continue;
        }
        case 0x2af9:
            if (RFIFOREST(fd) < 3)
                return;
            chrif_connectack(fd);
            RFIFOSKIP(fd, 3);
            break;
        case 0x2afa:
            if (RFIFOREST(fd) < 10)
                return;
            ladmin_itemfrob(fd);
            RFIFOSKIP(fd, 10);
            break;
        case 0x2afb:
            if (RFIFOREST(fd) < 27)
                return;
            chrif_sendmapack(fd);
            RFIFOSKIP(fd, 27);
            break;
        case 0x2afd:
            if (RFIFOREST(fd) < 4)
                return;
            if (RFIFOREST(fd) < RFIFOW(fd, 2))
                return;
            pc_authok(RFIFOL(fd, 4), RFIFOL(fd, 8),
                      (time_t) RFIFOL(fd, 12), RFIFOW(fd, 16),
                      (struct mmo_charstatus *) RFIFOP(fd, 18));
            RFIFOSKIP(fd, RFIFOW(fd, 2));
            break;
        case 0x2afe:
            if (RFIFOREST(fd) < 6)
                return;
            pc_authfail(RFIFOL(fd, 2));
            RFIFOSKIP(fd, 6);
            break;
        case 0x2b00:
            if (RFIFOREST(fd) < 6)
                return;
            map_setusers(RFIFOL(fd, 2));
            RFIFOSKIP(fd, 6);
            break;
        case 0x2b03:
            if (RFIFOREST(fd) < 7)
                return;
            clif_charselectok(RFIFOL(fd, 2));
            RFIFOSKIP(fd, 7);
            break;
        case 0x2b04:
            if (RFIFOREST(fd) < 4)
                return;
            if (RFIFOREST(fd) < RFIFOW(fd, 2))
                return;
            chrif_recvmap(fd);
            RFIFOSKIP(fd, RFIFOW(fd, 2));
            break;
        case 0x2b06:
            if (RFIFOREST(fd) < 44)
                return;
            chrif_changemapserverack(fd);
            RFIFOSKIP(fd, 44);
            break;
        case 0x2b09:
            if (RFIFOREST(fd) < 30)
                return;
            map_addchariddb(RFIFOL(fd, 2), (char *)RFIFOP(fd, 6));
            RFIFOSKIP(fd, 30);
            break;
        case 0x2b0b:
            if (RFIFOREST(fd) < 10)
                return;
            chrif_changedgm(fd);
            RFIFOSKIP(fd, 10);
            break;
        case 0x2b0d:
            if (RFIFOREST(fd) < 7)
                return;
            chrif_changedsex(fd);
            RFIFOSKIP(fd, 7);
            break;
        case 0x2b0f:
            if (RFIFOREST(fd) < 34)
                return;
            chrif_char_ask_name_answer(fd);
            RFIFOSKIP(fd, 34);
            break;
        case 0x2b11:
            if (RFIFOREST(fd) < 4)
                return;
            if (RFIFOREST(fd) < RFIFOW(fd, 2))
                return;
            chrif_accountreg2(fd);
            RFIFOSKIP(fd, RFIFOW(fd, 2));
            break;
        case 0x2b12:
            if (RFIFOREST(fd) < 10)
                return;
            chrif_divorce(RFIFOL(fd, 2), RFIFOL(fd, 6));
            RFIFOSKIP(fd, 10);
            break;
        case 0x2b13:
            if (RFIFOREST(fd) < 6)
                return;
            chrif_accountdeletion(fd);
            RFIFOSKIP(fd, 6);
            break;
        case 0x2b14:
            if (RFIFOREST(fd) < 11)
                return;
            chrif_accountban(fd);
            RFIFOSKIP(fd, 11);
            break;
        case 0x2b15:
            if (RFIFOREST(fd) < 4)
                return;
            if (RFIFOREST(fd) < RFIFOW(fd, 2))
                return;
            chrif_recvgmaccounts(fd);
            RFIFOSKIP(fd, RFIFOW(fd, 2));
            break;
        case 0x3800:
            if (RFIFOREST(fd) < 4 || RFIFOREST(fd) < RFIFOW(fd, 2))
                return;
            clif_GMmessage(NULL, (char *)RFIFOP(fd, 4), RFIFOW(fd, 2) - 4, 0);
            RFIFOSKIP(fd, RFIFOW(fd, 2));
            break;
        case 0x3801:
            if (RFIFOREST(fd) < 4 || RFIFOREST(fd) < RFIFOW(fd, 2))
                return;
            intif_parse_WisMessage(fd);
            RFIFOSKIP(fd, RFIFOW(fd, 2));
            break;
        case 0x3802:
            if (RFIFOREST(fd) < 27)
                return;
            intif_parse_WisEnd(fd);
            RFIFOSKIP(fd, 27);
            break;
        case 0x3803:
            if (RFIFOREST(fd) < 4 || RFIFOREST(fd) < RFIFOW(fd, 2))
                return;
            mapif_parse_WisToGM(fd);
            RFIFOSKIP(fd, RFIFOW(fd, 2));
            break;
        case 0x3804:
            if (RFIFOREST(fd) < 4 || RFIFOREST(fd) < RFIFOW(fd, 2))
                return;
            intif_parse_AccountReg(fd);
            RFIFOSKIP(fd, RFIFOW(fd, 2));
            break;
        case 0x3810:
            if (RFIFOREST(fd) < 4 || RFIFOREST(fd) < RFIFOW(fd, 2))
                return;
            intif_parse_LoadStorage(fd);
            RFIFOSKIP(fd, RFIFOW(fd, 2));
            break;
        case 0x3811:
            if (RFIFOREST(fd) < 7)
                return;
            intif_parse_SaveStorage(fd);
            RFIFOSKIP(fd, 7);
            break;
        case 0x3820:
            if (RFIFOREST(fd) < 35)
                return;
            intif_parse_PartyCreated(fd);
            RFIFOSKIP(fd, 35);
            break;
        case 0x3821:
            if (RFIFOREST(fd) < 4 || RFIFOREST(fd) < RFIFOW(fd, 2))
                return;
            intif_parse_PartyInfo(fd);
            RFIFOSKIP(fd, RFIFOW(fd, 2));
            break;
        case 0x3822:
            if (RFIFOREST(fd) < 11)
                return;
            intif_parse_PartyMemberAdded(fd);
            RFIFOSKIP(fd, 11);
            break;
        case 0x3823:
            if (RFIFOREST(fd) < 15)
                return;
            intif_parse_PartyOptionChanged(fd);
            RFIFOSKIP(fd, 15);
            break;
        case 0x3824:
            if (RFIFOREST(fd) < 34)
                return;
            intif_parse_PartyMemberLeaved(fd);
            RFIFOSKIP(fd, 34);
            break;
        case 0x3825:
            if (RFIFOREST(fd) < 29)
                return;
            intif_parse_PartyMove(fd);
            RFIFOSKIP(fd, 29);
            break;
        case 0x3826:
            if (RFIFOREST(fd) < 7)
                return;
            intif_parse_PartyBroken(fd);
            RFIFOSKIP(fd, 7);
            break;
        case 0x3827:
            if (RFIFOREST(fd) < 4 || RFIFOREST(fd) < RFIFOW(fd, 2))
                return;
            intif_parse_PartyMessage(fd);
            RFIFOSKIP(fd, RFIFOW(fd, 2));
            break;
        default:
            map_log("%s: %d: unknown packet %04x\n", __func__, fd, RFIFOW(fd, 0));
            session[fd]->eof = 1;
            return;
        }
    }
}

/// Timer to update the char server with how many and which players we have
static void send_users_tochar(timer_id, tick_t, custom_id_t, custom_data_t)
{
    if (char_fd < 0)
        return;

    WFIFOW(char_fd, 0) = 0x2aff;

    int users = 0;
    for (int i = 0; i < fd_max; i++)
    {
        if (!session[i])
            continue;
        struct map_session_data *sd = (struct map_session_data *)session[i]->session_data;
        if (!sd || !sd->state.auth)
            continue;
        if ((battle_config.hide_GM_session
                || sd->state.shroud_active
                || (sd->status.option & OPTION_HIDE)
                ) && pc_isGM(sd))
            continue;

        WFIFOL(char_fd, 6 + 4 * users) = sd->status.char_id;
        users++;
    }
    WFIFOW(char_fd, 2) = 6 + 4 * users;
    WFIFOW(char_fd, 4) = users;
    WFIFOSET(char_fd, 6 + 4 * users);
}

/// Timer to check if we're connected to the char server
static void check_connect_char_server(timer_id, tick_t, custom_id_t, custom_data_t)
{
    if (char_fd < 0)
    {
        printf("Attempt to connect to char-server...\n");
        chrif_state = 0;
        char_fd = make_connection(char_ip, char_port);
        if (char_fd < 0)
            return;
        session[char_fd]->func_parse = chrif_parse;
        realloc_fifo(char_fd, FIFOSIZE_SERVERLINK, FIFOSIZE_SERVERLINK);

        chrif_connect(char_fd);
    }
}

void do_init_chrif(void)
{
    add_timer_interval(gettick() + 1000, check_connect_char_server, 0, 0, 10 * 1000);
    add_timer_interval(gettick() + 1000, send_users_tochar, 0, 0, 5 * 1000);
}
