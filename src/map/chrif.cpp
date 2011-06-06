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
void chrif_save(MapSessionData *sd)
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
    STRZCPY2(sign_cast<char *>(WFIFOP(fd, 2)), userid);
    STRZCPY2(sign_cast<char *>(WFIFOP(fd, 26)), passwd);
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
        STRZCPY2(sign_cast<char *>(WFIFOP(fd, 4 + i * 16)), maps[i].name);
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
        map_setipport(sign_cast<const char *>(RFIFOP(fd, i)), ip, port);
    }
    uint8_t *p = reinterpret_cast<uint8_t *>(&ip);
    map_log("recv map on %hhu.%hhu.%hhu.%hhu:%hu (%d maps)\n", p[0], p[1], p[2], p[3], port, j);
}

/// Arrange for a character to change to another map server
void chrif_changemapserver(MapSessionData *sd,
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
    strzcpy(sign_cast<char *>(WFIFOP(char_fd, 18)), mapname, 16);
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
    MapSessionData *sd = map_id2sd(RFIFOL(fd, 2));

    if (!sd || sd->status.char_id != RFIFOL(fd, 14))
        return;

    if (RFIFOL(fd, 6) == 1)
    {
        map_log("map server change failed.\n");
        pc_authfail(sd->fd);
        return;
    }
    clif_changemapserver(sd, sign_cast<const char *>(RFIFOP(fd, 18)), RFIFOW(fd, 34),
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

    STRZCPY(whisper_server_name, sign_cast<const char *>(RFIFOP(fd, 3)));

    chrif_state = 2;
}

/// Request to authenticate the client
void chrif_authreq(MapSessionData *sd)
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
void chrif_charselectreq(MapSessionData *sd)
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
    strzcpy(sign_cast<char *>(WFIFOP(char_fd, 6)), actual_email, 40);
    strzcpy(sign_cast<char *>(WFIFOP(char_fd, 46)), new_email, 40);
    WFIFOSET(char_fd, 86);
}

/// Do miscellaneous operations on a character
void chrif_char_ask_name(int id, const char *character_name, CharOperation operation_type,
                         int year, int month, int day, int hour, int minute, int second)
{
    WFIFOW(char_fd, 0) = 0x2b0e;
    // who asked, or -1 if server
    WFIFOL(char_fd, 2) = id;
    strzcpy(sign_cast<char *>(WFIFOP(char_fd, 6)), character_name, 24);
    WFIFOW(char_fd, 30) = static_cast<uint16_t>(operation_type);
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
    STRZCPY(player_name, sign_cast<const char *>(RFIFOP(fd, 6)));

    MapSessionData *sd = map_id2sd(acc);
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

    MapSessionData *sd = map_id2sd(acc);

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
    MapSessionData *sd = map_id2sd(acc);
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
void chrif_saveaccountreg2(MapSessionData *sd)
{
    nullpo_retv(sd);

    int p = 8;
    for (int j = 0; j < sd->status.account_reg2_num; j++)
    {
        struct global_reg *reg = &sd->status.account_reg2[j];
        if (reg->str[0] && reg->value)
        {
            STRZCPY2(sign_cast<char *>(WFIFOP(char_fd, p)), reg->str);
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
    MapSessionData *sd = map_id2sd(RFIFOL(fd, 4));

    if (!sd)
        return;

    int p = 8;
    int j;
    for (j = 0; p < RFIFOW(fd, 2) && j < ACCOUNT_REG2_NUM; j++)
    {
        STRZCPY(sd->status.account_reg2[j].str, sign_cast<const char *>(RFIFOP(fd, p)));
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

    MapSessionData *sd;
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
    MapSessionData *sd = map_id2sd(acc);
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
    MapSessionData *sd = map_id2sd(acc);
    if (!sd)
        return;

    sd->login_id1++;
    if (RFIFOB(fd, 6))
    {
        char tmpstr[2048];
        time_t timestamp = RFIFOL(fd, 7);
        sprintf(tmpstr, "Your account has been banished until %s", stamp_time(timestamp));

        clif_displaymessage(sd->fd, tmpstr);
        clif_setwaitclose(sd->fd);
        return;
    }
    // change of status
    switch (static_cast<enum auth_failure>(RFIFOL(fd, 7)))
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

static void ladmin_itemfrob_c(struct block_list *bl, int source_id,
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
        MapSessionData *pc = reinterpret_cast<MapSessionData *>(bl);
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
        struct mob_data *mob = reinterpret_cast<struct mob_data *>(bl);
        for (int i = 0; i < mob->lootitem_count; i++)
            FIX(mob->lootitem[i]);
        break;
    }

    case BL_ITEM:
    {
        struct flooritem_data *item = reinterpret_cast<struct flooritem_data *>(bl);
        FIX(item->item_data);
        break;
    }
    }
#undef FIX
#undef IFIX
}

static void ladmin_itemfrob(int fd)
{
    int source_id = RFIFOL(fd, 2);
    int dest_id = RFIFOL(fd, 6);
    struct block_list *bl = reinterpret_cast<struct block_list *>(map_get_first_session());

    // flooritems
    map_foreachobject(ladmin_itemfrob_c, source_id, dest_id);

    // player characters
    while (bl->next)
    {
        ladmin_itemfrob_c(bl, source_id, dest_id);
        bl = bl->next;
    }
}



/// Message for all GMs on all map servers
void intif_GMmessage(const char *mes, int len)
{
    WFIFOW(char_fd, 0) = 0x3000;
    WFIFOW(char_fd, 2) = 4 + len;
    memcpy(WFIFOP(char_fd, 4), mes, len);
    WFIFOSET(char_fd, WFIFOW(char_fd, 2));
}

/// Whisper via inter-server (player not found on this server)
void intif_whisper_message(MapSessionData *sd, const char *nick,
                           const char *mes, int mes_len)
{
    nullpo_retv(sd);

    WFIFOW(char_fd, 0) = 0x3001;
    WFIFOW(char_fd, 2) = mes_len + 52;
    STRZCPY2(sign_cast<char *>(WFIFOP(char_fd, 4)), sd->status.name);
    strzcpy(sign_cast<char *>(WFIFOP(char_fd, 28)), nick, 24);
    memcpy(WFIFOP(char_fd, 52), mes, mes_len);
    WFIFOSET(char_fd, WFIFOW(char_fd, 2));
}

/// Reply to a multicast whisper
static void intif_whisper_reply(int id, int flag)
{
    WFIFOW(char_fd, 0) = 0x3002;
    WFIFOL(char_fd, 2) = id;
    WFIFOB(char_fd, 6) = flag;
    WFIFOSET(char_fd, 7);
}

/// @wgm for other map servers
void intif_whisper_message_to_gm(const char *whisper_name, gm_level_t min_gm_level,
                                 const char *mes, int mes_len)
{
    WFIFOW(char_fd, 0) = 0x3003;
    WFIFOW(char_fd, 2) = mes_len + 30;
    strzcpy(sign_cast<char *>(WFIFOP(char_fd, 4)), whisper_name, 24);
    WFIFOW(char_fd, 28) = min_gm_level;
    memcpy(WFIFOP(char_fd, 30), mes, mes_len);
    WFIFOSET(char_fd, WFIFOW(char_fd, 2));

    map_log("intif_whisper_message_to_gm: from: '%s', min level: %d, message: '%s'.\n",
            whisper_name, min_gm_level, mes);
}

/// Save variables
void intif_saveaccountreg(MapSessionData *sd)
{
    nullpo_retv(sd);

    WFIFOW(char_fd, 0) = 0x3004;
    WFIFOL(char_fd, 4) = sd->bl.id;
    int p = 8;
    for (int j = 0; j < sd->status.account_reg_num; j++)
    {
        memcpy(WFIFOP(char_fd, p), sd->status.account_reg[j].str, 32);
        p += 32;
        WFIFOL(char_fd, p) = sd->status.account_reg[j].value;
        p += 4;
    }
    WFIFOW(char_fd, 2) = p;
    WFIFOSET(char_fd, p);
}

/// Request someone's variables
void intif_request_accountreg(MapSessionData *sd)
{
    nullpo_retv(sd);

    WFIFOW(char_fd, 0) = 0x3005;
    WFIFOL(char_fd, 2) = sd->bl.id;
    WFIFOSET(char_fd, 6);
}

/// Request someone's storage
void intif_request_storage(account_t account_id)
{
    WFIFOW(char_fd, 0) = 0x3010;
    WFIFOL(char_fd, 2) = account_id;
    WFIFOSET(char_fd, 6);
}

/// Update someone's storage
void intif_send_storage(struct storage *stor)
{
    nullpo_retv(stor);
    WFIFOW(char_fd, 0) = 0x3011;
    WFIFOW(char_fd, 2) = sizeof(struct storage) + 8;
    WFIFOL(char_fd, 4) = stor->account_id;
    memcpy(WFIFOP(char_fd, 8), stor, sizeof(struct storage));
    WFIFOSET(char_fd, WFIFOW(char_fd, 2));
}

/// Create a party
void intif_create_party(MapSessionData *sd, const char *name)
{
    nullpo_retv(sd);

    WFIFOW(char_fd, 0) = 0x3020;
    WFIFOL(char_fd, 2) = sd->status.account_id;
    strzcpy(sign_cast<char *>(WFIFOP(char_fd, 6)), name, 24);
    STRZCPY2(sign_cast<char *>(WFIFOP(char_fd, 30)), sd->status.name);
    STRZCPY2(sign_cast<char *>(WFIFOP(char_fd, 54)), maps[sd->bl.m].name);
    WFIFOW(char_fd, 70) = sd->status.base_level;
    WFIFOSET(char_fd, 72);
}

/// Request party info
void intif_request_partyinfo(party_t party_id)
{
    WFIFOW(char_fd, 0) = 0x3021;
    WFIFOL(char_fd, 2) = party_id;
    WFIFOSET(char_fd, 6);
}

/// Add someone to a party
void intif_party_addmember(party_t party_id, account_t account_id)
{
    MapSessionData *sd = map_id2sd(account_id);
    if (!sd)
        return;
    WFIFOW(char_fd, 0) = 0x3022;
    WFIFOL(char_fd, 2) = party_id;
    WFIFOL(char_fd, 6) = account_id;
    STRZCPY2(sign_cast<char *>(WFIFOP(char_fd, 10)), sd->status.name);
    STRZCPY2(sign_cast<char *>(WFIFOP(char_fd, 34)), maps[sd->bl.m].name);
    WFIFOW(char_fd, 50) = sd->status.base_level;
    WFIFOSET(char_fd, 52);
}

/// A player has changed the item/exp sharing of a party
void intif_party_changeoption(party_t party_id, account_t account_id, bool exp, bool item)
{
    WFIFOW(char_fd, 0) = 0x3023;
    WFIFOL(char_fd, 2) = party_id;
    WFIFOL(char_fd, 6) = account_id;
    WFIFOW(char_fd, 10) = exp;
    WFIFOW(char_fd, 12) = item;
    WFIFOSET(char_fd, 14);
}

/// Someone leaves a party
void intif_party_leave(party_t party_id, account_t account_id)
{
    WFIFOW(char_fd, 0) = 0x3024;
    WFIFOL(char_fd, 2) = party_id;
    WFIFOL(char_fd, 6) = account_id;
    WFIFOSET(char_fd, 10);
}

/// Update someone's location in the party window
void intif_party_changemap(MapSessionData *sd, bool online)
{
    if (!sd)
        return;

    WFIFOW(char_fd, 0) = 0x3025;
    WFIFOL(char_fd, 2) = sd->status.party_id;
    WFIFOL(char_fd, 6) = sd->status.account_id;
    memcpy(WFIFOP(char_fd, 10), maps[sd->bl.m].name, 16);
    WFIFOB(char_fd, 26) = online;
    WFIFOW(char_fd, 27) = sd->status.base_level;
    WFIFOSET(char_fd, 29);
}

/// Party chat
void intif_party_message(party_t party_id, account_t account_id, const char *mes, int len)
{
    WFIFOW(char_fd, 0) = 0x3027;
    WFIFOW(char_fd, 2) = len + 12;
    WFIFOL(char_fd, 4) = party_id;
    WFIFOL(char_fd, 8) = account_id;
    memcpy(WFIFOP(char_fd, 12), mes, len);
    WFIFOSET(char_fd, len + 12);
}

/// Check that the player isn't already in a different party
void intif_party_checkconflict(party_t party_id, account_t account_id, const char *nick)
{
    WFIFOW(char_fd, 0) = 0x3028;
    WFIFOL(char_fd, 2) = party_id;
    WFIFOL(char_fd, 6) = account_id;
    memcpy(WFIFOP(char_fd, 10), nick, 24);
    WFIFOSET(char_fd, 34);
}



/// Whisper
static void intif_parse_whisper(int fd)
{
    MapSessionData *sd = map_nick2sd(sign_cast<const char *>(RFIFOP(fd, 32)));
    if (!sd)
        return;
    map_log("intif_parse_whispermessage: id: %d, from: %s, to: %s, message: '%s'\n",
            RFIFOL(fd, 4), RFIFOP(fd, 8), RFIFOP(fd, 32), RFIFOP(fd, 56));
    if (strcmp(sd->status.name, sign_cast<const char *>(RFIFOP(fd, 32))) != 0)
    {
        intif_whisper_reply(RFIFOL(fd, 4), 1);
        return;
    }
    clif_whisper_message(sd->fd, sign_cast<const char *>(RFIFOP(fd, 8)), sign_cast<const char *>(RFIFOP(fd, 56)),
                         RFIFOW(fd, 2) - 56);
    intif_whisper_reply(RFIFOL(fd, 4), 0);
}

/// We sent a whisper to the inter server and get this result
static void intif_parse_whisper_end(int fd)
{
    MapSessionData *sd = map_nick2sd(sign_cast<const char *>(RFIFOP(fd, 2)));

    map_log("intif_parse_whisperend: player: %s, flag: %d\n",
            sign_cast<const char *>(RFIFOP(fd, 2)), RFIFOB(fd, 26));
    if (sd)
        clif_whisper_end(sd->fd, RFIFOB(fd, 26));
}

/// forwarded @wgm
static void mapif_parse_WhisperToGM(int fd)
{
    int len = RFIFOW(fd, 2) - 30;

    if (len <= 0)
        return;

    gm_level_t min_gm_level = RFIFOW(fd, 28);

    char whisper_name[24];
    STRZCPY(whisper_name, sign_cast<const char *>(RFIFOP(fd, 4)));

    char message[len];
    STRZCPY(message, sign_cast<const char *>(RFIFOP(fd, 30)));

    for (MapSessionData *pl_sd : sessions)
    {
        if (pc_isGM(pl_sd) < min_gm_level)
            continue;
        clif_whisper_message(pl_sd->fd, whisper_name, message, len);
    }
}

/// Load somebody's variables
static void intif_parse_AccountReg(int fd)
{
    MapSessionData *sd = map_id2sd(RFIFOL(fd, 4));

    if (!sd)
        return;

    int p = 8;
    int j;
    for (j = 0; p < RFIFOW(fd, 2) && j < ACCOUNT_REG_NUM; j++)
    {
        STRZCPY(sd->status.account_reg[j].str, sign_cast<const char *>(RFIFOP(fd, p)));
        p += 32;
        sd->status.account_reg[j].value = RFIFOL(fd, p);
        p += 4;
    }
    sd->status.account_reg_num = j;
}

/// Load somebody's storage
static void intif_parse_LoadStorage(int fd)
{
    MapSessionData *sd = map_id2sd(RFIFOL(fd, 4));
    if (!sd)
    {
        map_log("%s: user not found %d\n", __func__, RFIFOL(fd, 4));
        return;
    }
    struct storage *stor = account2storage(RFIFOL(fd, 4));
    if (stor->storage_status == 1)
    {                           // Already open.. lets ignore this update
        map_log("%s: storage received for a client already open (User %d:%d)\n",
                __func__, sd->status.account_id, sd->status.char_id);
        return;
    }
    if (stor->dirty)
    {                           // Already have storage, and it has been modified and not saved yet! Exploit! [Skotlex]
        map_log("%s: received storage for an already modified non-saved storage! (User %d:%d)\n",
                __func__, sd->status.account_id, sd->status.char_id);
        return;
    }

    if (RFIFOW(fd, 2) - 8 != sizeof(struct storage))
    {
        map_log("%s: data size error %d %d\n", __func__,
                RFIFOW(fd, 2) - 8, sizeof(struct storage));
        return;
    }
    map_log("%s: %d\n", __func__, RFIFOL(fd, 4));
    memcpy(stor, RFIFOP(fd, 8), sizeof(struct storage));
    stor->dirty = 0;
    stor->storage_status = 1;
    sd->state.storage_flag = 1;
    clif_storageitemlist(sd, stor);
    clif_storageequiplist(sd, stor);
    clif_updatestorageamount(sd, stor);
}

/// Somebody's storage has been saved
static void intif_parse_SaveStorage(int fd)
{
    map_log("%s: done %d %d\n", __func__, RFIFOL(fd, 2), RFIFOB(fd, 6));
    storage_storage_saved(RFIFOL(fd, 2));
}

/// A party was created
static void intif_parse_PartyCreated(int fd)
{
    map_log("%s: party created\n", __func__);
    party_created(RFIFOL(fd, 2), RFIFOB(fd, 6), RFIFOL(fd, 7),
                   sign_cast<const char *>(RFIFOP(fd, 11)));
}

/// Parse party info
static void intif_parse_PartyInfo(int fd)
{
    if (RFIFOW(fd, 2) == 8)
    {
        map_log("intif: party noinfo %d\n", RFIFOL(fd, 4));
        party_recv_noinfo(RFIFOL(fd, 4));
        return;
    }

    if (RFIFOW(fd, 2) != sizeof(struct party) + 4)
    {
        map_log("intif: party info : data size error %d %d %d\n",
                RFIFOL(fd, 4), RFIFOW(fd, 2), sizeof(struct party) + 4);
        return;
    }

    party_recv_info(reinterpret_cast<const struct party *>(RFIFOP(fd, 4)));
}

/// Notification of party member added
static void intif_parse_PartyMemberAdded(int fd)
{
    map_log("intif: party member added %d %d %d\n", RFIFOL(fd, 2),
            RFIFOL(fd, 6), RFIFOB(fd, 10));
    party_member_added(RFIFOL(fd, 2), RFIFOL(fd, 6), RFIFOB(fd, 10));
}

/// Notification of party exp/item sharing changed
static void intif_parse_PartyOptionChanged(int fd)
{
    party_optionchanged(RFIFOL(fd, 2), RFIFOL(fd, 6), RFIFOW(fd, 10),
                        RFIFOW(fd, 12), RFIFOB(fd, 14));
}

/// Someone has left the party
static void intif_parse_PartyMemberLeft(int fd)
{
    map_log("intif: party member left %d %d %s\n", RFIFOL(fd, 2),
            RFIFOL(fd, 6), RFIFOP(fd, 10));
    party_member_left(RFIFOL(fd, 2), RFIFOL(fd, 6), sign_cast<const char *>(RFIFOP(fd, 10)));
}

/// A party no longer exists
static void intif_parse_PartyBroken(int fd)
{
    party_broken(RFIFOL(fd, 2));
}

/// Notification that somebody in the party has changed maps
static void intif_parse_PartyMove(int fd)
{
    party_recv_movemap(RFIFOL(fd, 2), RFIFOL(fd, 6), sign_cast<const char *>(RFIFOP(fd, 10)),
                       RFIFOB(fd, 26), RFIFOW(fd, 27));
}

/// Receive party chat
static void intif_parse_PartyMessage(int fd)
{
    party_recv_message(RFIFOL(fd, 4), RFIFOL(fd, 8), sign_cast<const char *>(RFIFOP(fd, 12)),
                       RFIFOW(fd, 2) - 12);
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
            const Version& server_version = *reinterpret_cast<const Version *>(RFIFOP(fd, 2));
            if (!(server_version.what_server & ATHENA_SERVER_CHAR))
            {
                map_log("Not a char server!");
                abort();
            }
            if (!(server_version.what_server & ATHENA_SERVER_INTER))
            {
                map_log("Not an inter server!");
                abort();
            }
            if (server_version != tmwAthenaVersion)
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
                      RFIFOL(fd, 12), RFIFOW(fd, 16),
                      reinterpret_cast<const struct mmo_charstatus *>(RFIFOP(fd, 18)));
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
            map_addchariddb(RFIFOL(fd, 2), sign_cast<const char *>(RFIFOP(fd, 6)));
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
            clif_GMmessage(NULL, sign_cast<const char *>(RFIFOP(fd, 4)), RFIFOW(fd, 2) - 4, 0);
            RFIFOSKIP(fd, RFIFOW(fd, 2));
            break;
        case 0x3801:
            if (RFIFOREST(fd) < 4 || RFIFOREST(fd) < RFIFOW(fd, 2))
                return;
            intif_parse_whisper(fd);
            RFIFOSKIP(fd, RFIFOW(fd, 2));
            break;
        case 0x3802:
            if (RFIFOREST(fd) < 27)
                return;
            intif_parse_whisper_end(fd);
            RFIFOSKIP(fd, 27);
            break;
        case 0x3803:
            if (RFIFOREST(fd) < 4 || RFIFOREST(fd) < RFIFOW(fd, 2))
                return;
            mapif_parse_WhisperToGM(fd);
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
            intif_parse_PartyMemberLeft(fd);
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
static void send_users_tochar(timer_id, tick_t)
{
    if (char_fd < 0)
        return;

    WFIFOW(char_fd, 0) = 0x2aff;

    int users = 0;
    for (MapSessionData *sd : sessions)
    {
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
static void check_connect_char_server(timer_id, tick_t)
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
    add_timer_interval(gettick() + 1000, 10 * 1000, check_connect_char_server);
    add_timer_interval(gettick() + 1000, 5 * 1000, send_users_tochar);
}
