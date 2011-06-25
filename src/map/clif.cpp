#include "clif.hpp"

#include "../lib/strings.hpp"

#include "../common/md5calc.hpp"
#include "../common/nullpo.hpp"
#include "../common/timer.hpp"
#include "../common/utils.hpp"
#include "../common/version.hpp"

#include "atcommand.hpp"
#include "battle.hpp"
#include "chrif.hpp"
#include "itemdb.hpp"
#include "magic.hpp"
#include "magic-stmt.hpp"
#include "map.hpp"
#include "npc.hpp"
#include "party.hpp"
#include "pc.hpp"
#include "skill.hpp"
#include "storage.hpp"
#include "tmw.hpp"
#include "trade.hpp"

#define STATE_BLIND 0x10
#define EMOTE_IGNORED 0x0e

static void clif_changelook_towards(BlockList *, int, int, MapSessionData *dst);
static void clif_sitting(int fd, MapSessionData *sd);
static void clif_itemlist(MapSessionData *sd);
static void clif_GM_kickack(MapSessionData *sd, int id);

/// I would really like to delete this table
// most of them are not used and such
static const int packet_len_table[0x220] =
{
//#0x0000
    10, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
//#0x0010
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
//#0x0020
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
//#0x0030
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,

//#0x0040
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
//#0x0050
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
//#0x0060
    0, 0, 0, -1,
    55, 17, 3, 37,
    46, -1, 23, -1,
    3, 108, 3, 2,
//#0x0070
    3, 28, 19, 11,
    3, -1, 9, 5,
    54, 53, 58, 60,
    41, 2, 6, 6,

//#0x0080
    // 0x8b unknown... size 2 or 23?
    7, 3, 2, 2,
    2, 5, 16, 12,
    10, 7, 29, 23,
    -1, -1, -1, 0,
//#0x0090
    7, 22, 28, 2,
    6, 30, -1, -1,
    3, -1, -1, 5,
    9, 17, 17, 6,
//#0x00A0
    23, 6, 6, -1,
    -1, -1, -1, 8,
    7, 6, 7, 4,
    7, 0, -1, 6,
//#0x00B0
    8, 8, 3, 3,
    -1, 6, 6, -1,
    7, 6, 2, 5,
    6, 44, 5, 3,

//#0x00C0
    7, 2, 6, 8,
    6, 7, -1, -1,
    -1, -1, 3, 3,
    6, 6, 2, 27,
//#0x00D0
    3, 4, 4, 2,
    -1, -1, 3, -1,
    6, 14, 3, -1,
    28, 29, -1, -1,
//#0x00E0
    30, 30, 26, 2,
    6, 26, 3, 3,
    8, 19, 5, 2,
    3, 2, 2, 2,
//#0x00F0
    3, 2, 6, 8,
    21, 8, 8, 2,
    2, 26, 3, -1,
    6, 27, 30, 10,

//#0x0100
    2, 6, 6, 30,
    79, 31, 10, 10,
    -1, -1, 4, 6,
    6, 2, 11, -1,
//#0x0110
    10, 39, 4, 10,
    31, 35, 10, 18,
    2, 13, 15, 20,
    68, 2, 3, 16,
//#0x0120
    6, 14, -1, -1,
    21, 8, 8, 8,
    8, 8, 2, 2,
    3, 4, 2, -1,
//#0x0130
    6, 86, 6, -1,
    -1, 7, -1, 6,
    3, 16, 4, 4,
    4, 6, 24, 26,

//#0x0140
    22, 14, 6, 10,
    23, 19, 6, 39,
    8, 9, 6, 27,
    -1, 2, 6, 6,
//#0x0150
    110, 6, -1, -1,
    -1, -1, -1, 6,
    -1, 54, 66, 54,
    90, 42, 6, 42,
//#0x0160
    -1, -1, -1, -1,
    -1, 30, -1, 3,
    14, 3, 30, 10,
    43, 14, 186, 182,
//#0x0170
    14, 30, 10, 3,
    -1, 6, 106, -1,
    4, 5, 4, -1,
    6, 7, -1, -1,

//#0x0180
    6, 3, 106, 10,
    10, 34, 0, 6,
    8, 4, 4, 4,
    29, -1, 10, 6,
//#0x0190
    90, 86, 24, 6,
    30, 102, 9, 4,
    8, 4, 14, 10,
    -1, 6, 2, 6,
//#0x01A0
    3, 3, 35, 5,
    11, 26, -1, 4,
    4, 6, 10, 12,
    6, -1, 4, 4,
//#0x01B0
    11, 7, -1, 67,
    12, 18, 114, 6,
    3, 6, 26, 26,
    26, 26, 2, 3,

//#0x01C0
    2, 14, 10, -1,
    22, 22, 4, 2,
    13, 97, 0, 9,
    9, 30, 6, 28,
//#0x01D0
    // Set 0x1d5=-1
    8, 14, 10, 35,
    6, -1, 4, 11,
    54, 53, 60, 2,
    -1, 47, 33, 6,
//#0x01E0
    30, 8, 34, 14,
    2, 6, 26, 2,
    28, 81, 6, 10,
    26, 2, -1, -1,
//#0x01F0
    -1, -1, 20, 10,
    32, 9, 34, 14,
    2, 6, 48, 56,
    -1, 4, 5, 10,

//#0x0200
    26, -1, 26, 10,
    18, 26, 11, 34,
    14, 36, 10, 19,
    10, -1, 24, 0,
//#0x0210
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
};

// local define
enum class Whom
{
    ALL_CLIENT,
    ALL_SAMEMAP,
    AREA,
    AREA_WOS,
    AREA_WOC,
    AREA_CHAT_WOC,
    PARTY,
    PARTY_WOS,
    PARTY_SAMEMAP,
    PARTY_SAMEMAP_WOS,
    PARTY_AREA,
    PARTY_AREA_WOS,
    SELF
};

static void WBUFPOS(uint8_t *p, size_t pos, uint16_t x, uint16_t y, Direction d)
{
    // x = aaaa aaaa bbbb bbbb
    // y = cccc cccc dddd dddd
    p += pos;
    // aabb bbbb
    p[0] = x >> 2;
    // bb00 0000 | (cccc dddd & 0011 1111)
    p[1] = (x << 6) | ((y >> 4) & 0x3f);
    //
    p[2] = (y << 4) | (static_cast<int>(d) & 0xF);
}

static void WBUFPOS2(uint8_t *p, size_t pos, uint16_t x_0, uint16_t y_0, uint16_t x_1, uint16_t y_1)
{
    // x_0 = aaaa aaaa bbbb bbbb
    // y_0 = cccc cccc dddd dddd
    // x_1 = eeee eeee ffff ffff
    // y_1 = gggg gggg hhhh hhhh
    p += pos;
    // aabb bbbb
    p[0] =  x_0 >>2;
    // bb00 0000 | (cccc dddd & 0011 1111)
    p[1] = (x_0 << 6) | ((y_0 >> 4) & 0x3f);
    // dddd 0000 | (eeee eeff & 0000 1111)
    p[2] = (y_0 << 4) | ((x_1 >> 6) & 0x0f);
    // ffff ff00 | (gggg gggg & 0000 0011)
    p[3] = (x_1 << 2) | ((y_1 >> 8) & 0x03);
    // hhhh hhhh
    p[4] = y_1;
}

static void WFIFOPOS(int fd, size_t pos, uint16_t x, uint16_t y, Direction d)
{
    WBUFPOS(WFIFOP(fd, 0), pos, x, y, d);
}

static void WFIFOPOS2(int fd, size_t pos, uint16_t x_0, uint16_t y_0, uint16_t x_1, uint16_t y_1)
{
    WBUFPOS2(WFIFOP(fd, 0), pos, x_0, y_0, x_1, y_1);
}

static IP_Address map_ip;
static in_port_t map_port = 5121;
int map_fd;

/// Our public IP
void clif_setip(IP_Address ip)
{
    map_ip = ip;
}

/// The port on which we listen
void clif_setport(uint16_t port)
{
    map_port = port;
}

/// Our public IP
IP_Address clif_getip(void)
{
    return map_ip;
}

/// The port on which we listen
in_port_t clif_getport(void)
{
    return map_port;
}

/// Count authenticated users
unsigned clif_countusers(void)
{
    unsigned int users = 0;
    for (MapSessionData *sd : auth_sessions)
    {
        if (battle_config.hide_GM_session && pc_isGM(sd))
            continue;
        users++;
    }
    return users;
}

Sessions<true> auth_sessions;
Sessions<false> all_sessions;

static uint8_t *clif_validate_chat(MapSessionData *sd, int type,
                                   char **message, size_t *message_len);

/// Subfunction that checks if the given target is included in the Whom
// (only called for PCs in the area)
static void clif_send_sub(BlockList *bl,
                          uint8_t *buf,
                          uint16_t len,
                          BlockList *src_bl,
                          Whom type)
{
    nullpo_retv(bl);

    MapSessionData *sd = static_cast<MapSessionData *>(bl);

    nullpo_retv(src_bl);

    switch (type)
    {
    case Whom::AREA_WOS:
    case Whom::AREA_CHAT_WOC:
    case Whom::AREA_WOC:
        if (bl && bl == src_bl)
            return;
    }

    memcpy(WFIFOP(sd->fd, 0), buf, len);
    WFIFOSET(sd->fd, len);
}

/// Send a packet to a certain set of people
static void clif_send(uint8_t *buf, uint16_t len, BlockList *bl, Whom type)
{
    // Validate packet
    if (!buf)
        abort();
    if (bl && bl->type == BL_PC && WFIFOP(static_cast<MapSessionData *>(bl)->fd, 0) == buf)
        abort();
    if (len < 2)
        abort();
    int packet_len = packet_len_table[RBUFW(buf, 0)];
    if (packet_len == -1)
    {
        if (len < 4)
            abort();
        packet_len = WBUFW(buf, 2);
    }
    if (packet_len != len)
        abort();


    if (type != Whom::ALL_CLIENT)
    {
        nullpo_retv(bl);

        if (bl->type == BL_PC)
        {
            MapSessionData *sd = static_cast<MapSessionData *>(bl);
            if (sd->status.option & OPTION_INVISIBILITY)
            {
                // Obscure hidden GMs
                switch (type)
                {
                case Whom::AREA:
                case Whom::AREA_WOC:
                    type = Whom::SELF;
                    break;

                case Whom::AREA_WOS:
                    return;
                }
            }
        }
    }

    // TODO put these inside if possible
    int x_0 = 0, x_1 = 0, y_0 = 0, y_1 = 0;

    switch (type)
    {
    case Whom::ALL_CLIENT:
        for (MapSessionData *sd : auth_sessions)
        {
            memcpy(WFIFOP(sd->fd, 0), buf, len);
            WFIFOSET(sd->fd, len);
        }
        break;
    case Whom::ALL_SAMEMAP:
        for (MapSessionData *sd : auth_sessions)
        {
            if (sd->m != bl->m)
                continue;

            memcpy(WFIFOP(sd->fd, 0), buf, len);
            WFIFOSET(sd->fd, len);
        }
        break;
    case Whom::AREA:
    case Whom::AREA_WOS:
    case Whom::AREA_WOC:
        map_foreachinarea(clif_send_sub, bl->m,
                          bl->x - AREA_SIZE, bl->y - AREA_SIZE,
                          bl->x + AREA_SIZE, bl->y + AREA_SIZE,
                          BL_PC, buf, len, bl, type);
        break;
    case Whom::AREA_CHAT_WOC:
        map_foreachinarea(clif_send_sub, bl->m,
                          bl->x - (AREA_SIZE - 5), bl->y - (AREA_SIZE - 5),
                          bl->x + (AREA_SIZE - 5), bl->y + (AREA_SIZE - 5),
                          BL_PC, buf, len, bl, Whom::AREA_CHAT_WOC);
        break;
    case Whom::PARTY_AREA:
    case Whom::PARTY_AREA_WOS:
        x_0 = bl->x - AREA_SIZE;
        y_0 = bl->y - AREA_SIZE;
        x_1 = bl->x + AREA_SIZE;
        y_1 = bl->y + AREA_SIZE;
    case Whom::PARTY:
    case Whom::PARTY_WOS:
    case Whom::PARTY_SAMEMAP:
    case Whom::PARTY_SAMEMAP_WOS:
    {
        struct party *p = NULL;
        if (bl->type == BL_PC)
        {
            MapSessionData *sd = static_cast<MapSessionData *>(bl);
            if (sd->status.party_id > 0)
                p = party_search(sd->status.party_id);
        }
        if (p)
        {
            for (int i = 0; i < MAX_PARTY; i++)
            {
                MapSessionData *sd = p->member[i].sd;
                if (!sd)
                    continue;
                if (sd->id == bl->id
                        && (type == Whom::PARTY_WOS
                                || type == Whom::PARTY_SAMEMAP_WOS
                                || type == Whom::PARTY_AREA_WOS))
                    continue;
                if (type != Whom::PARTY && type != Whom::PARTY_WOS
                        && bl->m != sd->m)
                    continue;
                if ((type == Whom::PARTY_AREA || type == Whom::PARTY_AREA_WOS)
                        && (sd->x < x_0 || sd->y < y_0
                                || sd->x > x_1 || sd->y > y_1))
                    continue;

                memcpy(WFIFOP(sd->fd, 0), buf, len);
                WFIFOSET(sd->fd, len);
            }
        }
        break;
    }
    case Whom::SELF:
        MapSessionData *sd = static_cast<MapSessionData *>(bl);
        memcpy(WFIFOP(sd->fd, 0), buf, len);
        WFIFOSET(sd->fd, len);
        break;
    }
}



/// Tell a client that it's ok
void clif_authok(MapSessionData *sd)
{
    nullpo_retv(sd);

    int fd = sd->fd;

    WFIFOW(fd, 0) = 0x73;
    WFIFOL(fd, 2) = gettick();
    WFIFOPOS(fd, 6, sd->x, sd->y, Direction::S);
    WFIFOB(fd, 9) = 5;
    WFIFOB(fd, 10) = 5;
    WFIFOSET(fd, packet_len_table[0x73]);
}

/// Tell a client it is getting disconnected
// Sent if we don't have a record from char-server ?
// or to all connections if same account tries to connect multiple times
// hm, does that mean that in case of multiple map servers ...?
void clif_authfail_fd(int fd, int type)
{
    if (!session[fd])
        return;

    WFIFOW(fd, 0) = 0x81;
    WFIFOL(fd, 2) = type;
    WFIFOSET(fd, packet_len_table[0x81]);

    clif_setwaitclose(fd);
}

/// The client can go back to the character-selection screen
void clif_charselectok(int id)
{
    MapSessionData *sd = map_id2sd(id);
    if (!sd)
        return;

    int fd = sd->fd;
    WFIFOW(fd, 0) = 0xb3;
    WFIFOB(fd, 2) = 1;
    WFIFOSET(fd, packet_len_table[0xb3]);
}

/// An item is dropped on the floor
void clif_dropflooritem(struct flooritem_data *fitem)
{
    uint8_t buf[64];

    nullpo_retv(fitem);

    if (fitem->item_data.nameid <= 0)
        return;

    WBUFW(buf, 0) = 0x9e;
    WBUFL(buf, 2) = fitem->id;
    WBUFW(buf, 6) = fitem->item_data.nameid;
    WBUFB(buf, 8) = fitem->item_data.identify;
    WBUFW(buf, 9) = fitem->x;
    WBUFW(buf, 11) = fitem->y;
    WBUFB(buf, 13) = fitem->subx;
    WBUFB(buf, 14) = fitem->suby;
    WBUFW(buf, 15) = fitem->item_data.amount;

    clif_send(buf, packet_len_table[0x9e], fitem, Whom::AREA);
}

/// An item disappears (due to timeout or distance)
void clif_clearflooritem(struct flooritem_data *fitem, int fd)
{
    nullpo_retv(fitem);

    uint8_t buf[6];
    WBUFW(buf, 0) = 0xa1;
    WBUFL(buf, 2) = fitem->id;

    if (fd == -1)
    {
        clif_send(buf, packet_len_table[0xa1], fitem, Whom::AREA);
    }
    else
    {
        memcpy(WFIFOP(fd, 0), buf, 6);
        WFIFOSET(fd, packet_len_table[0xa1]);
    }
}

/// An entity is removed (usually, dead)
void clif_being_remove(BlockList *bl, BeingRemoveType type)
{
    nullpo_retv(bl);

    uint8_t buf[16];
    WBUFW(buf, 0) = 0x80;
    WBUFL(buf, 2) = bl->id;
    if (type == BeingRemoveType::DISGUISE)
    {
        WBUFB(buf, 6) = 0;
        clif_send(buf, packet_len_table[0x80], bl, Whom::AREA);
    }
    else if (type == BeingRemoveType::DEAD)
    {
        WBUFB(buf, 6) = 1;
        clif_send(buf, packet_len_table[0x80], bl, Whom::AREA);
    }
    else
    {
        WBUFB(buf, 6) = 0;
        clif_send(buf, packet_len_table[0x80], bl, Whom::AREA_WOS);
    }
}

/// A being disappears to one player
void clif_being_remove_id(uint32_t id, BeingRemoveType type, int fd)
{
    uint8_t buf[16];

    WBUFW(buf, 0) = 0x80;
    WBUFL(buf, 2) = id;
    WBUFB(buf, 6) = type == BeingRemoveType::DEAD;
    memcpy(WFIFOP(fd, 0), buf, 7);
    WFIFOSET(fd, packet_len_table[0x80]);
}

/// A player appears (or updates)
// note: this packet is almost identical to the next
static uint16_t clif_player_update(MapSessionData *sd, uint8_t *buf)
{
    nullpo_ret(sd);

    WBUFW(buf, 0) = 0x1d8;
    WBUFL(buf, 2) = sd->id;
    WBUFW(buf, 6) = sd->speed;
    WBUFW(buf, 8) = sd->opt1;
    WBUFW(buf, 10) = sd->opt2;
    WBUFW(buf, 12) = sd->status.option;
    WBUFW(buf, 14) = 0;// = sd->view_class;
    WBUFW(buf, 16) = sd->status.hair;
    if (sd->attack_spell_override)
        WBUFB(buf, 18) = sd->attack_spell_look_override;
    else
    {
        if (sd->equip_index[9] >= 0 && sd->inventory_data[sd->equip_index[9]])
            WBUFW(buf, 18) = sd->status.inventory[sd->equip_index[9]].nameid;
        else
            WBUFW(buf, 18) = 0;
    }
    if (sd->equip_index[8] >= 0 && sd->equip_index[8] != sd->equip_index[9]
        && sd->inventory_data[sd->equip_index[8]])
        WBUFW(buf, 20) = sd->status.inventory[sd->equip_index[8]].nameid;
    else
        WBUFW(buf, 20) = 0;
    WBUFW(buf, 22) = sd->status.head_bottom;
    WBUFW(buf, 24) = sd->status.head_top;
    WBUFW(buf, 26) = sd->status.head_mid;
    WBUFW(buf, 28) = sd->status.hair_color;
    WBUFW(buf, 30) = 0;//sd->status.clothes_color;
    WBUFW(buf, 32) = static_cast<int>(sd->head_dir);

    WBUFW(buf, 40) = 0;//sd->status.manner;
    WBUFW(buf, 42) = sd->opt3;
    WBUFB(buf, 44) = 0;//sd->status.karma;
    WBUFB(buf, 45) = sd->sex;
    WBUFPOS(buf, 46, sd->x, sd->y, sd->dir);
    WBUFW(buf, 49) = (pc_isGM(sd) == 60 || pc_isGM(sd) == 99) ? 0x80 : 0;
    WBUFB(buf, 51) = sd->state.dead_sit;
    WBUFW(buf, 52) = 0;

    return packet_len_table[0x1d8];
}

/// A player moves
// note: this packet is almost identical to the previous
static uint16_t clif_player_move(MapSessionData *sd, uint8_t *buf)
{
    nullpo_ret(sd);

    WBUFW(buf, 0) = 0x1da;
    WBUFL(buf, 2) = sd->id;
    WBUFW(buf, 6) = sd->speed;
    WBUFW(buf, 8) = sd->opt1;
    WBUFW(buf, 10) = sd->opt2;
    WBUFW(buf, 12) = sd->status.option;
    WBUFW(buf, 14) = 0; //sd->view_class;
    WBUFW(buf, 16) = sd->status.hair;
    if (sd->equip_index[9] >= 0 && sd->inventory_data[sd->equip_index[9]])
        WBUFW(buf, 18) = sd->status.inventory[sd->equip_index[9]].nameid;
    else
        WBUFW(buf, 18) = 0;
    if (sd->equip_index[8] >= 0 && sd->equip_index[8] != sd->equip_index[9]
            && sd->inventory_data[sd->equip_index[8]])
        WBUFW(buf, 20) = sd->status.inventory[sd->equip_index[8]].nameid;
    else
        WBUFW(buf, 20) = 0;
    WBUFW(buf, 22) = sd->status.head_bottom;
    WBUFL(buf, 24) = gettick();
    WBUFW(buf, 28) = sd->status.head_top;
    WBUFW(buf, 30) = sd->status.head_mid;
    WBUFW(buf, 32) = sd->status.hair_color;
    WBUFW(buf, 34) = 0;//sd->status.clothes_color;
    WBUFW(buf, 36) = static_cast<int>(sd->head_dir);

    WBUFW(buf, 44) = 0;//sd->status.manner;
    WBUFW(buf, 46) = sd->opt3;
    WBUFB(buf, 48) = 0;//sd->status.karma;
    WBUFB(buf, 49) = sd->sex;
    WBUFPOS2(buf, 50, sd->x, sd->y, sd->to_x, sd->to_y);
    // this isn't needed, as players GM status is set in the other packet
    // also, this was missing the 99 case
    WBUFW(buf, 55) = 0; //pc_isGM(sd) == 60 ? 0x80 : 0;
    WBUFB(buf, 57) = 5;
    WBUFW(buf, 58) = 0;

    return packet_len_table[0x1da];
}

/// A mob appears (or updates)
static uint16_t clif_mob_appear(struct mob_data *md, uint8_t *buf)
{
    nullpo_ret(md);

    memset(buf, 0, packet_len_table[0x78]);

    WBUFW(buf, 0) = 0x78;
    WBUFL(buf, 2) = md->id;
    WBUFW(buf, 6) = battle_get_speed(md);
    WBUFW(buf, 8) = md->opt1;
    WBUFW(buf, 10) = md->opt2;
    WBUFW(buf, 12) = md->option;
    WBUFW(buf, 14) = md->mob_class;

    WBUFPOS(buf, 46, md->x, md->y, md->dir);
    WBUFB(buf, 49) = 5;
    WBUFB(buf, 50) = 5;
    WBUFW(buf, 52) = MIN(battle_get_lv(md), battle_config.max_lv);

    return packet_len_table[0x78];
}

/// A mob moves
static uint16_t clif_mob_move(struct mob_data *md, uint8_t *buf)
{
    nullpo_ret(md);

    memset(buf, 0, packet_len_table[0x7b]);

    WBUFW(buf, 0) = 0x7b;
    WBUFL(buf, 2) = md->id;
    WBUFW(buf, 6) = battle_get_speed(md);
    WBUFW(buf, 8) = md->opt1;
    WBUFW(buf, 10) = md->opt2;
    WBUFW(buf, 12) = md->option;
    WBUFW(buf, 14) = md->mob_class;

    WBUFL(buf, 22) = gettick();

    WBUFPOS2(buf, 50, md->x, md->y, md->to_x, md->to_y);
    WBUFB(buf, 56) = 5;
    WBUFB(buf, 57) = 5;
    WBUFW(buf, 58) = MIN(battle_get_lv(md), battle_config.max_lv);

    return packet_len_table[0x7b];
}

/// An NPC appears (or updates)
static uint16_t clif_npc_appear(struct npc_data *nd, uint8_t *buf)
{
    nullpo_ret(nd);

    memset(buf, 0, packet_len_table[0x78]);

    WBUFW(buf, 0) = 0x78;
    WBUFL(buf, 2) = nd->id;
    WBUFW(buf, 6) = nd->speed;
    WBUFW(buf, 14) = nd->npc_class;
    WBUFPOS(buf, 46, nd->x, nd->y, nd->dir);
    WBUFB(buf, 49) = 5;
    WBUFB(buf, 50) = 5;

    return packet_len_table[0x78];
}

/// These indices are derived from equip_pos in pc.c and some guesswork
static int equip_points[LOOK_LAST + 1] =
{
    -1, /// 0: base
    -1, /// 1: hair
    9,  /// 2: weapon
    4,  /// 3: head botom -- leg armour
    6,  /// 4: head top -- hat
    5,  /// 5: head mid -- torso armour
    -1, /// 6: hair colour
    -1, /// 7: clothes colour
    8,  /// 8: shield
    2,  /// 9: shoes
    3,  /// 10: gloves
    1,  /// 11: cape
    7,  /// 12: misc1
    0,  /// 13: misc2
};

/// Send everybody (else) a new PC's appearance
void clif_spawnpc(MapSessionData *sd)
{
    nullpo_retv(sd);

    uint8_t buf[128];
    int len = clif_player_update(sd, buf);

    clif_send(buf, len, sd, Whom::AREA_WOS);
}

/// Send everybody a new NPC
void clif_spawnnpc(struct npc_data *nd)
{
    nullpo_retv(nd);

    if (nd->npc_class < 0 || nd->flag & 1 || nd->npc_class == INVISIBLE_CLASS)
        return;

    uint8_t buf[64];
    memset(buf, 0, packet_len_table[0x7c]);

    WBUFW(buf, 0) = 0x7c;
    WBUFL(buf, 2) = nd->id;
    WBUFW(buf, 6) = nd->speed;
    WBUFW(buf, 20) = nd->npc_class;
    WBUFPOS(buf, 36, nd->x, nd->y, Direction::S);

    clif_send(buf, packet_len_table[0x7c], nd, Whom::AREA);

    int len = clif_npc_appear(nd, buf);
    clif_send(buf, len, nd, Whom::AREA);
}

/// Hack because the client (and protocol) don't support effects at an arbitrary position
void clif_spawn_fake_npc_for_player(MapSessionData *sd, int fake_npc_id)
{
    nullpo_retv(sd);

    int fd = sd->fd;

    WFIFOW(fd, 0) = 0x7c;
    WFIFOL(fd, 2) = fake_npc_id;
    WFIFOW(fd, 6) = 0;
    WFIFOW(fd, 8) = 0;
    WFIFOW(fd, 10) = 0;
    WFIFOW(fd, 12) = 0;
    WFIFOW(fd, 20) = 127;
    WFIFOPOS(fd, 36, sd->x, sd->y, Direction::S);
    WFIFOSET(fd, packet_len_table[0x7c]);

    WFIFOW(fd, 0) = 0x78;
    WFIFOL(fd, 2) = fake_npc_id;
    WFIFOW(fd, 6) = 0;
    WFIFOW(fd, 8) = 0;
    WFIFOW(fd, 10) = 0;
    WFIFOW(fd, 12) = 0;
    WFIFOW(fd, 14) = 127;      // identifies as NPC
    WFIFOW(fd, 20) = 127;
    WFIFOPOS(fd, 46, sd->x, sd->y, Direction::S);
    WFIFOPOS(fd, 36, sd->x, sd->y, Direction::S);
    WFIFOB(fd, 49) = 5;
    WFIFOB(fd, 50) = 5;
    WFIFOSET(fd, packet_len_table[0x78]);
}

/// Show everybody a new mob
void clif_spawnmob(struct mob_data *md)
{
    nullpo_retv(md);

    uint8_t buf[64];

    int len = clif_mob_appear(md, buf);
    clif_send(buf, len, md, Whom::AREA);
}

/// Player can walk to the requested destination
void clif_walkok(MapSessionData *sd)
{
    nullpo_retv(sd);

    int fd = sd->fd;
    WFIFOW(fd, 0) = 0x87;
    WFIFOL(fd, 2) = gettick();;
    WFIFOPOS2(fd, 6, sd->x, sd->y, sd->to_x, sd->to_y);
    WFIFOB(fd, 11) = 0;
    WFIFOSET(fd, packet_len_table[0x87]);
}

/// A player moves
void clif_movechar(MapSessionData *sd)
{
    nullpo_retv(sd);

    uint8_t buf[256];
    uint16_t len = clif_player_move(sd, buf);

    clif_send(buf, len, sd, Whom::AREA_WOS);
}

/// Timer to close a connection
// This exists so the server can finish sending the packets
// surely there is a better way (perhaps a half shutdown?)
static void clif_waitclose(timer_id, tick_t, int fd)
{
    if (session[fd])
        session[fd]->eof = 1;
}

/// Disconnect player after 5 seconds, to finish sending packets
void clif_setwaitclose(int fd)
{
    add_timer(gettick() + 5000, clif_waitclose, fd);
}

/// Player has moved to another map
void clif_changemap(MapSessionData *sd, const Point& point)
{
    nullpo_retv(sd);

    int fd = sd->fd;

    WFIFOW(fd, 0) = 0x91;
    point.map.write_to(sign_cast<char *>(WFIFOP(fd, 2)));
    WFIFOW(fd, 18) = point.x;
    WFIFOW(fd, 20) = point.y;
    WFIFOSET(fd, packet_len_table[0x91]);
}

/// Player has moved to a map on another server
void clif_changemapserver(MapSessionData *sd, const Point& point, IP_Address ip, in_port_t port)
{
    nullpo_retv(sd);

    int fd = sd->fd;
    WFIFOW(fd, 0) = 0x92;
    point.map.write_to(sign_cast<char *>(WFIFOP(fd, 2)));
    WFIFOW(fd, 18) = point.x;
    WFIFOW(fd, 20) = point.y;
    WFIFOL(fd, 22) = ip.to_n();
    WFIFOW(fd, 26) = port;
    WFIFOSET(fd, packet_len_table[0x92]);
}

/// Player has stopped walking
void clif_stop(MapSessionData *bl)
{
    nullpo_retv(bl);

    uint8_t buf[16];
    WBUFW(buf, 0) = 0x88;
    WBUFL(buf, 2) = bl->id;
    WBUFW(buf, 6) = bl->x;
    WBUFW(buf, 8) = bl->y;

    clif_send(buf, packet_len_table[0x88], bl, Whom::AREA);
}

/// An chatted NPC indicates a shop rather than a script
void clif_npcbuysell(MapSessionData *sd, int id)
{
    nullpo_retv(sd);

    int fd = sd->fd;
    WFIFOW(fd, 0) = 0xc4;
    WFIFOL(fd, 2) = id;
    WFIFOSET(fd, packet_len_table[0xc4]);
}

/// Player wants to buy from the NPC: list the NPC's wares
void clif_buylist(MapSessionData *sd, struct npc_data_shop *nd)
{
    nullpo_retv(sd);
    nullpo_retv(nd);

    int fd = sd->fd;
    WFIFOW(fd, 0) = 0xc6;
    for (int i = 0; i < nd->shop_item.size(); i++)
    {
        struct item_data *id = itemdb_search(nd->shop_item[i].nameid);
        int val = nd->shop_item[i].value;
        WFIFOL(fd, 4 + i * 11) = val;
        WFIFOL(fd, 8 + i * 11) = val;
        WFIFOB(fd, 12 + i * 11) = id->type;
        WFIFOW(fd, 13 + i * 11) = nd->shop_item[i].nameid;
    }
    WFIFOW(fd, 2) = nd->shop_item.size() * 11 + 4;
    WFIFOSET(fd, WFIFOW(fd, 2));
}

/// Player wants to sell to an NPC: list sellable items
void clif_selllist(MapSessionData *sd)
{
    nullpo_retv(sd);

    int fd = sd->fd;
    WFIFOW(fd, 0) = 0xc7;
    int c = 0;
    for (int i = 0; i < MAX_INVENTORY; i++)
    {
        if (sd->status.inventory[i].nameid > 0 && sd->inventory_data[i])
        {
            int val = sd->inventory_data[i]->value_sell;
            if (val <= 0)
                continue;
            WFIFOW(fd, 4 + c * 10) = i + 2;
            WFIFOL(fd, 6 + c * 10) = val;
            WFIFOL(fd, 10 + c * 10) = val;
            c++;
        }
    }
    WFIFOW(fd, 2) = c * 10 + 4;
    WFIFOSET(fd, WFIFOW(fd, 2));
}

/// Append another line of NPC dialog
void clif_scriptmes(MapSessionData *sd, int npcid, const char *mes)
{
    nullpo_retv(sd);

    int fd = sd->fd;
    WFIFOW(fd, 0) = 0xb4;
    WFIFOW(fd, 2) = strlen(mes) + 9;
    WFIFOL(fd, 4) = npcid;
    strcpy(sign_cast<char *>(WFIFOP(fd, 8)), mes);
    WFIFOSET(fd, WFIFOW(fd, 2));
}

/// Pause in NPC dialog/script - wait for player to click "next"
void clif_scriptnext(MapSessionData *sd, int npcid)
{
    nullpo_retv(sd);

    int fd = sd->fd;
    WFIFOW(fd, 0) = 0xb5;
    WFIFOL(fd, 2) = npcid;
    WFIFOSET(fd, packet_len_table[0xb5]);
}

/// NPC script ends - wait for client to reply
void clif_scriptclose(MapSessionData *sd, int npcid)
{
    nullpo_retv(sd);

    int fd = sd->fd;
    WFIFOW(fd, 0) = 0xb6;
    WFIFOL(fd, 2) = npcid;
    WFIFOSET(fd, packet_len_table[0xb6]);
}

/// NPC script menu - wait for player to choose something
// the values are separated by colons, so we need to do some magic
void clif_scriptmenu(MapSessionData *sd, int npcid, const std::vector<std::string>& options)
{
    nullpo_retv(sd);

    if (options.empty())
        abort();

    std::string mes;
    for (std::string str : options)
    {
        // "modifer letter colon" looks close enough
        replace_all(str, ":", "\ua789");
        mes += str;
        mes += ':';
    }

    int fd = sd->fd;
    WFIFOW(fd, 0) = 0xb7;
    WFIFOW(fd, 2) = mes.size() + 8;
    WFIFOL(fd, 4) = npcid;
    memcpy(WFIFOP(fd, 8), mes.data(), mes.size());
    WFIFOSET(fd, WFIFOW(fd, 2));
}

/// Request for numeric input
void clif_scriptinput(MapSessionData *sd, int npcid)
{
    nullpo_retv(sd);

    int fd = sd->fd;
    WFIFOW(fd, 0) = 0x142;
    WFIFOL(fd, 2) = npcid;
    WFIFOSET(fd, packet_len_table[0x142]);
}

/// Request for string input
void clif_scriptinputstr(MapSessionData *sd, int npcid)
{
    nullpo_retv(sd);

    int fd = sd->fd;
    WFIFOW(fd, 0) = 0x1d4;
    WFIFOL(fd, 2) = npcid;
    WFIFOSET(fd, packet_len_table[0x1d4]);
}

/// Add an item to player's inventory
void clif_additem(MapSessionData *sd, int n, int amount, PickupFail fail)
{
    nullpo_retv(sd);

    int fd = sd->fd;
    if (fail != PickupFail::OKAY)
    {
        WFIFOW(fd, 0) = 0xa0;
        WFIFOW(fd, 2) = n + 2;
        WFIFOW(fd, 4) = amount;
        WFIFOW(fd, 6) = 0;
        WFIFOB(fd, 8) = 0;
        WFIFOB(fd, 9) = 0;
        WFIFOB(fd, 10) = 0;
        WFIFOW(fd, 11) = 0;
        WFIFOW(fd, 13) = 0;
        WFIFOW(fd, 15) = 0;
        WFIFOW(fd, 17) = 0;
        WFIFOW(fd, 19) = 0;
        WFIFOB(fd, 21) = 0;
        WFIFOB(fd, 22) = static_cast<uint8_t>(fail);
    }
    else
    {
        if (n < 0 || n >= MAX_INVENTORY || sd->status.inventory[n].nameid <= 0
            || sd->inventory_data[n] == NULL)
            return;

        WFIFOW(fd, 0) = 0xa0;
        WFIFOW(fd, 2) = n + 2;
        WFIFOW(fd, 4) = amount;
        WFIFOW(fd, 6) = sd->status.inventory[n].nameid;
        WFIFOB(fd, 8) = sd->status.inventory[n].identify;
        if (sd->status.inventory[n].broken == 1)
            WFIFOB(fd, 9) = 1; // is weapon broken [Valaris]
        else
            WFIFOB(fd, 9) = sd->status.inventory[n].attribute;
        WFIFOB(fd, 10) = sd->status.inventory[n].refine;
        WFIFOW(fd, 11) = sd->status.inventory[n].card[0];
        WFIFOW(fd, 13) = sd->status.inventory[n].card[1];
        WFIFOW(fd, 15) = sd->status.inventory[n].card[2];
        WFIFOW(fd, 17) = sd->status.inventory[n].card[3];
        WFIFOW(fd, 19) = pc_equippoint(sd, n);
        WFIFOB(fd, 21) =
            (sd->inventory_data[n]->type == 7) ? 4 : sd->inventory_data[n]->type;
        WFIFOB(fd, 22) = static_cast<uint8_t>(fail);
    }

    WFIFOSET(fd, packet_len_table[0xa0]);
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_delitem(MapSessionData *sd, int n, int amount)
{
    nullpo_retv(sd);

    int fd = sd->fd;
    WFIFOW(fd, 0) = 0xaf;
    WFIFOW(fd, 2) = n + 2;
    WFIFOW(fd, 4) = amount;

    WFIFOSET(fd, packet_len_table[0xaf]);
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_itemlist(MapSessionData *sd)
{
    int i, n, arrow = -1;
    uint8_t *buf;

    nullpo_retv(sd);

    int fd = sd->fd;
    buf = WFIFOP(fd, 0);
    WFIFOW(fd, 0) = 0x1ee;
    for (i = 0, n = 0; i < MAX_INVENTORY; i++)
    {
        if (sd->status.inventory[i].nameid <= 0
            || sd->inventory_data[i] == NULL
            || itemdb_isequip2(sd->inventory_data[i]))
            continue;
        WFIFOW(fd, n * 18 + 4) = i + 2;
        WFIFOW(fd, n * 18 + 6) = sd->status.inventory[i].nameid;
        WFIFOB(fd, n * 18 + 8) = sd->inventory_data[i]->type;
        WFIFOB(fd, n * 18 + 9) = sd->status.inventory[i].identify;
        WFIFOW(fd, n * 18 + 10) = sd->status.inventory[i].amount;
        if (sd->inventory_data[i]->equip == 0x8000)
        {
            WFIFOW(fd, n * 18 + 12) = 0x8000;
            if (sd->status.inventory[i].equip)
                arrow = i;      // ついでに矢装備チェック
        }
        else
            WFIFOW(fd, n * 18 + 12) = 0;
        WFIFOW(fd, n * 18 + 14) = sd->status.inventory[i].card[0];
        WFIFOW(fd, n * 18 + 16) = sd->status.inventory[i].card[1];
        WFIFOW(fd, n * 18 + 18) = sd->status.inventory[i].card[2];
        WFIFOW(fd, n * 18 + 20) = sd->status.inventory[i].card[3];
        n++;
    }
    if (n)
    {
        WFIFOW(fd, 2) = 4 + n * 18;
        WFIFOSET(fd, WFIFOW(fd, 2));
    }
    if (arrow >= 0)
        clif_arrowequip(sd, arrow);
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_equiplist(MapSessionData *sd)
{
    int i, n, fd;
    uint8_t *buf;

    nullpo_retv(sd);

    fd = sd->fd;
    buf = WFIFOP(fd, 0);
    WFIFOW(fd, 0) = 0xa4;
    for (i = 0, n = 0; i < MAX_INVENTORY; i++)
    {
        if (sd->status.inventory[i].nameid <= 0
            || sd->inventory_data[i] == NULL
            || !itemdb_isequip2(sd->inventory_data[i]))
            continue;
        WFIFOW(fd, n * 20 + 4) = i + 2;
        WFIFOW(fd, n * 20 + 6) = sd->status.inventory[i].nameid;
        WFIFOB(fd, n * 20 + 8) =
            (sd->inventory_data[i]->type ==
             7) ? 4 : sd->inventory_data[i]->type;
        WFIFOB(fd, n * 20 + 9) = sd->status.inventory[i].identify;
        WFIFOW(fd, n * 20 + 10) = pc_equippoint(sd, i);
        WFIFOW(fd, n * 20 + 12) = sd->status.inventory[i].equip;
        if (sd->status.inventory[i].broken == 1)
            WFIFOB(fd, n * 20 + 14) = 1;   // is weapon broken [Valaris]
        else
            WFIFOB(fd, n * 20 + 14) = sd->status.inventory[i].attribute;
        WFIFOB(fd, n * 20 + 15) = sd->status.inventory[i].refine;
        WFIFOW(fd, n * 20 + 16) = sd->status.inventory[i].card[0];
        WFIFOW(fd, n * 20 + 18) = sd->status.inventory[i].card[1];
        WFIFOW(fd, n * 20 + 20) = sd->status.inventory[i].card[2];
        WFIFOW(fd, n * 20 + 22) = sd->status.inventory[i].card[3];
        n++;
    }
    if (n)
    {
        WFIFOW(fd, 2) = 4 + n * 20;
        WFIFOSET(fd, WFIFOW(fd, 2));
    }
}

/*==========================================
 * カプラさんに預けてある消耗品&収集品リスト
 *------------------------------------------
 */
void clif_storageitemlist(MapSessionData *sd, struct storage *stor)
{
    struct item_data *id;
    int i, n, fd;
    uint8_t *buf;

    nullpo_retv(sd);
    nullpo_retv(stor);

    fd = sd->fd;
    buf = WFIFOP(fd, 0);
    WFIFOW(fd, 0) = 0x1f0;
    for (i = 0, n = 0; i < MAX_STORAGE; i++)
    {
        if (stor->storage_[i].nameid <= 0)
            continue;
        nullpo_retv(id = itemdb_search(stor->storage_[i].nameid));
        if (itemdb_isequip2(id))
            continue;

        WFIFOW(fd, n * 18 + 4) = i + 1;
        WFIFOW(fd, n * 18 + 6) = stor->storage_[i].nameid;
        WFIFOB(fd, n * 18 + 8) = id->type;
        WFIFOB(fd, n * 18 + 9) = stor->storage_[i].identify;
        WFIFOW(fd, n * 18 + 10) = stor->storage_[i].amount;
        WFIFOW(fd, n * 18 + 12) = 0;
        WFIFOW(fd, n * 18 + 14) = stor->storage_[i].card[0];
        WFIFOW(fd, n * 18 + 16) = stor->storage_[i].card[1];
        WFIFOW(fd, n * 18 + 18) = stor->storage_[i].card[2];
        WFIFOW(fd, n * 18 + 20) = stor->storage_[i].card[3];
        n++;
    }
    if (n)
    {
        WFIFOW(fd, 2) = 4 + n * 18;
        WFIFOSET(fd, WFIFOW(fd, 2));
    }
}

/*==========================================
 * カプラさんに預けてある装備リスト
 *------------------------------------------
 */
void clif_storageequiplist(MapSessionData *sd, struct storage *stor)
{
    struct item_data *id;
    int i, n, fd;
    uint8_t *buf;

    nullpo_retv(sd);
    nullpo_retv(stor);

    fd = sd->fd;
    buf = WFIFOP(fd, 0);
    WFIFOW(fd, 0) = 0xa6;
    for (i = 0, n = 0; i < MAX_STORAGE; i++)
    {
        if (stor->storage_[i].nameid <= 0)
            continue;
        nullpo_retv(id = itemdb_search(stor->storage_[i].nameid));
        if (!itemdb_isequip2(id))
            continue;
        WFIFOW(fd, n * 20 + 4) = i + 1;
        WFIFOW(fd, n * 20 + 6) = stor->storage_[i].nameid;
        WFIFOB(fd, n * 20 + 8) = id->type;
        WFIFOB(fd, n * 20 + 9) = stor->storage_[i].identify;
        WFIFOW(fd, n * 20 + 10) = id->equip;
        WFIFOW(fd, n * 20 + 12) = stor->storage_[i].equip;
        if (stor->storage_[i].broken == 1)
            WFIFOB(fd, n * 20 + 14) = 1;   //is weapon broken [Valaris]
        else
            WFIFOB(fd, n * 20 + 14) = stor->storage_[i].attribute;
        WFIFOB(fd, n * 20 + 15) = stor->storage_[i].refine;
        if (stor->storage_[i].card[0] == 0x00ff
            || stor->storage_[i].card[0] == 0x00fe
            || stor->storage_[i].card[0] == static_cast<short>(0xff00))
        {
            WFIFOW(fd, n * 20 + 16) = stor->storage_[i].card[0];
            WFIFOW(fd, n * 20 + 18) = stor->storage_[i].card[1];
            WFIFOW(fd, n * 20 + 20) = stor->storage_[i].card[2];
            WFIFOW(fd, n * 20 + 22) = stor->storage_[i].card[3];
        }
        else
        {
            WFIFOW(fd, n * 20 + 16) = stor->storage_[i].card[0];
            WFIFOW(fd, n * 20 + 18) = stor->storage_[i].card[1];
            WFIFOW(fd, n * 20 + 20) = stor->storage_[i].card[2];
            WFIFOW(fd, n * 20 + 22) = stor->storage_[i].card[3];
        }
        n++;
    }
    if (n)
    {
        WFIFOW(fd, 2) = 4 + n * 20;
        WFIFOSET(fd, WFIFOW(fd, 2));
    }
}

/*==========================================
 * ステータスを送りつける
 * 表示専用数字はこの中で計算して送る
 *------------------------------------------
 */
void clif_updatestatus(MapSessionData *sd, int type)
{
    int fd, len = 8;

    nullpo_retv(sd);

    fd = sd->fd;

    WFIFOW(fd, 0) = 0xb0;
    WFIFOW(fd, 2) = type;
    switch (type)
    {
            // 00b0
        case SP_WEIGHT:
            WFIFOW(fd, 0) = 0xb0;
            WFIFOW(fd, 2) = type;
            WFIFOL(fd, 4) = sd->weight;
            break;
        case SP_MAXWEIGHT:
            WFIFOL(fd, 4) = sd->max_weight;
            break;
        case SP_SPEED:
            WFIFOL(fd, 4) = sd->speed;
            break;
        case SP_BASELEVEL:
            WFIFOL(fd, 4) = sd->status.base_level;
            break;
        case SP_JOBLEVEL:
            WFIFOL(fd, 4) = 0;
            break;
        case SP_STATUSPOINT:
            WFIFOL(fd, 4) = sd->status.status_point;
            break;
        case SP_SKILLPOINT:
            WFIFOL(fd, 4) = sd->status.skill_point;
            break;
        case SP_HIT:
            WFIFOL(fd, 4) = sd->hit;
            break;
        case SP_FLEE1:
            WFIFOL(fd, 4) = sd->flee;
            break;
        case SP_FLEE2:
            WFIFOL(fd, 4) = sd->flee2 / 10;
            break;
        case SP_MAXHP:
            WFIFOL(fd, 4) = sd->status.max_hp;
            break;
        case SP_MAXSP:
            WFIFOL(fd, 4) = sd->status.max_sp;
            break;
        case SP_HP:
            WFIFOL(fd, 4) = sd->status.hp;
            break;
        case SP_SP:
            WFIFOL(fd, 4) = sd->status.sp;
            break;
        case SP_ASPD:
            WFIFOL(fd, 4) = sd->aspd;
            break;
        case SP_ATK1:
            WFIFOL(fd, 4) = sd->base_atk + sd->watk;
            break;
        case SP_DEF1:
            WFIFOL(fd, 4) = sd->def;
            break;
        case SP_MDEF1:
            WFIFOL(fd, 4) = sd->mdef;
            break;
        case SP_ATK2:
            WFIFOL(fd, 4) = sd->watk2;
            break;
        case SP_DEF2:
            WFIFOL(fd, 4) = sd->def2;
            break;
        case SP_MDEF2:
            WFIFOL(fd, 4) = sd->mdef2;
            break;
        case SP_CRITICAL:
            WFIFOL(fd, 4) = sd->critical / 10;
            break;
        case SP_MATK1:
            WFIFOL(fd, 4) = sd->matk1;
            break;
        case SP_MATK2:
            WFIFOL(fd, 4) = sd->matk2;
            break;

        case SP_ZENY:
            trade_verifyzeny(sd);
            WFIFOW(fd, 0) = 0xb1;
            if (sd->status.zeny < 0)
                sd->status.zeny = 0;
            WFIFOL(fd, 4) = sd->status.zeny;
            break;
        case SP_BASEEXP:
            WFIFOW(fd, 0) = 0xb1;
            WFIFOL(fd, 4) = sd->status.base_exp;
            break;
        case SP_JOBEXP:
            WFIFOW(fd, 0) = 0xb1;
            WFIFOL(fd, 4) = sd->status.job_exp;
            break;
        case SP_NEXTBASEEXP:
            WFIFOW(fd, 0) = 0xb1;
            WFIFOL(fd, 4) = pc_nextbaseexp(sd);
            break;
        case SP_NEXTJOBEXP:
            WFIFOW(fd, 0) = 0xb1;
            WFIFOL(fd, 4) = pc_nextjobexp(sd);
            break;

            // 00be 終了
        case SP_USTR:
        case SP_UAGI:
        case SP_UVIT:
        case SP_UINT:
        case SP_UDEX:
        case SP_ULUK:
            WFIFOW(fd, 0) = 0xbe;
            WFIFOB(fd, 4) =
                pc_need_status_point(sd, type - SP_USTR + SP_STR);
            len = 5;
            break;

            // 013a 終了
        case SP_ATTACKRANGE:
            WFIFOW(fd, 0) = 0x13a;
            WFIFOW(fd, 2) = (sd->attack_spell_override)
                ? sd->attack_spell_range : sd->attackrange;
            len = 4;
            break;

            // 0141 終了
        case SP_STR:
            WFIFOW(fd, 0) = 0x141;
            WFIFOL(fd, 2) = type;
            WFIFOL(fd, 6) = sd->status.str;
            WFIFOL(fd, 10) = sd->paramb[0] + sd->parame[0];
            len = 14;
            break;
        case SP_AGI:
            WFIFOW(fd, 0) = 0x141;
            WFIFOL(fd, 2) = type;
            WFIFOL(fd, 6) = sd->status.agi;
            WFIFOL(fd, 10) = sd->paramb[1] + sd->parame[1];
            len = 14;
            break;
        case SP_VIT:
            WFIFOW(fd, 0) = 0x141;
            WFIFOL(fd, 2) = type;
            WFIFOL(fd, 6) = sd->status.vit;
            WFIFOL(fd, 10) = sd->paramb[2] + sd->parame[2];
            len = 14;
            break;
        case SP_INT:
            WFIFOW(fd, 0) = 0x141;
            WFIFOL(fd, 2) = type;
            WFIFOL(fd, 6) = sd->status.int_;
            WFIFOL(fd, 10) = sd->paramb[3] + sd->parame[3];
            len = 14;
            break;
        case SP_DEX:
            WFIFOW(fd, 0) = 0x141;
            WFIFOL(fd, 2) = type;
            WFIFOL(fd, 6) = sd->status.dex;
            WFIFOL(fd, 10) = sd->paramb[4] + sd->parame[4];
            len = 14;
            break;
        case SP_LUK:
            WFIFOW(fd, 0) = 0x141;
            WFIFOL(fd, 2) = type;
            WFIFOL(fd, 6) = sd->status.luk;
            WFIFOL(fd, 10) = sd->paramb[5] + sd->parame[5];
            len = 14;
            break;

        case SP_GM:
            WFIFOL(fd, 4) = pc_isGM(sd);
            break;

        default:
            map_log("%s: make %d routine\n", __func__, type);
            return;
    }
    WFIFOSET(fd, len);
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_changelook(BlockList *bl, int type, int val)
{
    return clif_changelook_towards(bl, type, val, NULL);
}

void clif_changelook_towards(BlockList *bl, int type, int val,
                             MapSessionData *dstsd)
{
    uint8_t rbuf[32];
    uint8_t *buf = dstsd ? WFIFOP(dstsd->fd, 0) : rbuf;  // pick target buffer or general-purpose one
    MapSessionData *sd = NULL;

    nullpo_retv(bl);

    if (bl->type == BL_PC)
        sd = static_cast<MapSessionData *>(bl);

    if (sd && sd->status.option & OPTION_INVISIBILITY)
        return;

    if (sd
        && (type == LOOK_WEAPON || type == LOOK_SHIELD || type >= LOOK_SHOES))
    {
        WBUFW(buf, 0) = 0x1d7;
        WBUFL(buf, 2) = bl->id;
        if (type >= LOOK_SHOES)
        {
            int equip_point = equip_points[type];

            WBUFB(buf, 6) = type;
            if (sd->equip_index[equip_point] >= 0
                    && sd->inventory_data[sd->equip_index[2]])
                WBUFW(buf, 7) = sd->status.inventory[sd->equip_index[equip_point]].nameid;
            else
                WBUFW(buf, 7) = 0;
            WBUFW(buf, 9) = 0;
        }
        else
        {
            WBUFB(buf, 6) = 2;
            if (sd->attack_spell_override)
                WBUFW(buf, 7) = sd->attack_spell_look_override;
            else
            {
                if (sd->equip_index[9] >= 0
                        && sd->inventory_data[sd->equip_index[9]])
                    WBUFW(buf, 7) = sd->status.inventory[sd->equip_index[9]].nameid;
                else
                    WBUFW(buf, 7) = 0;
            }
            if (sd->equip_index[8] >= 0
                    && sd->equip_index[8] != sd->equip_index[9]
                    && sd->inventory_data[sd->equip_index[8]])
                WBUFW(buf, 9) = sd->status.inventory[sd->equip_index[8]].nameid;
            else
                WBUFW(buf, 9) = 0;
        }
        if (dstsd)
            WFIFOSET(dstsd->fd, packet_len_table[0x1d7]);
        else
            clif_send(buf, packet_len_table[0x1d7], bl, Whom::AREA);
    }
    else
    {
        WBUFW(buf, 0) = 0x1d7;
        WBUFL(buf, 2) = bl->id;
        WBUFB(buf, 6) = type;
        WBUFW(buf, 7) = val;
        WBUFW(buf, 9) = 0;
        if (dstsd)
            WFIFOSET(dstsd->fd, packet_len_table[0x1d7]);
        else
            clif_send(buf, packet_len_table[0x1d7], bl, Whom::AREA);
    }
}

/*==========================================
 *
 *------------------------------------------
 */
static void clif_initialstatus(MapSessionData *sd)
{
    int fd;
    uint8_t *buf;

    nullpo_retv(sd);

    fd = sd->fd;
    buf = WFIFOP(fd, 0);

    WBUFW(buf, 0) = 0xbd;
    WBUFW(buf, 2) = sd->status.status_point;
    WBUFB(buf, 4) = (sd->status.str > 255) ? 255 : sd->status.str;
    WBUFB(buf, 5) = pc_need_status_point(sd, SP_STR);
    WBUFB(buf, 6) = (sd->status.agi > 255) ? 255 : sd->status.agi;
    WBUFB(buf, 7) = pc_need_status_point(sd, SP_AGI);
    WBUFB(buf, 8) = (sd->status.vit > 255) ? 255 : sd->status.vit;
    WBUFB(buf, 9) = pc_need_status_point(sd, SP_VIT);
    WBUFB(buf, 10) = (sd->status.int_ > 255) ? 255 : sd->status.int_;
    WBUFB(buf, 11) = pc_need_status_point(sd, SP_INT);
    WBUFB(buf, 12) = (sd->status.dex > 255) ? 255 : sd->status.dex;
    WBUFB(buf, 13) = pc_need_status_point(sd, SP_DEX);
    WBUFB(buf, 14) = (sd->status.luk > 255) ? 255 : sd->status.luk;
    WBUFB(buf, 15) = pc_need_status_point(sd, SP_LUK);

    WBUFW(buf, 16) = sd->base_atk + sd->watk;
    WBUFW(buf, 18) = sd->watk2;    //atk bonus
    WBUFW(buf, 20) = sd->matk1;
    WBUFW(buf, 22) = sd->matk2;
    WBUFW(buf, 24) = sd->def;  // def
    WBUFW(buf, 26) = sd->def2;
    WBUFW(buf, 28) = sd->mdef; // mdef
    WBUFW(buf, 30) = sd->mdef2;
    WBUFW(buf, 32) = sd->hit;
    WBUFW(buf, 34) = sd->flee;
    WBUFW(buf, 36) = sd->flee2 / 10;
    WBUFW(buf, 38) = sd->critical / 10;
    WBUFW(buf, 40) = 0;//sd->status.karma;
    WBUFW(buf, 42) = 0;//sd->status.manner;

    WFIFOSET(fd, packet_len_table[0xbd]);

    clif_updatestatus(sd, SP_STR);
    clif_updatestatus(sd, SP_AGI);
    clif_updatestatus(sd, SP_VIT);
    clif_updatestatus(sd, SP_INT);
    clif_updatestatus(sd, SP_DEX);
    clif_updatestatus(sd, SP_LUK);

    clif_updatestatus(sd, SP_ATTACKRANGE);
    clif_updatestatus(sd, SP_ASPD);
}

/*==========================================
 *矢装備
 *------------------------------------------
 */
void clif_arrowequip(MapSessionData *sd, int val)
{
    int fd;

    nullpo_retv(sd);

    if (sd->attacktarget && sd->attacktarget > 0)   // [Valaris]
        sd->attacktarget = 0;

    fd = sd->fd;
    WFIFOW(fd, 0) = 0x013c;
    WFIFOW(fd, 2) = val + 2;   //矢のアイテムID

    WFIFOSET(fd, packet_len_table[0x013c]);
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_arrow_fail(MapSessionData *sd, int type)
{
    nullpo_retv(sd);

    int fd = sd->fd;
    WFIFOW(fd, 0) = 0x013b;
    WFIFOW(fd, 2) = type;

    WFIFOSET(fd, packet_len_table[0x013b]);
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_statusupack(MapSessionData *sd, int type, int ok, int val)
{
    nullpo_retv(sd);

    int fd = sd->fd;
    WFIFOW(fd, 0) = 0xbc;
    WFIFOW(fd, 2) = type;
    WFIFOB(fd, 4) = ok;
    WFIFOB(fd, 5) = val;
    WFIFOSET(fd, packet_len_table[0xbc]);
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_equipitemack(MapSessionData *sd, int n, int pos, int ok)
{
    nullpo_retv(sd);

    int fd = sd->fd;
    WFIFOW(fd, 0) = 0xaa;
    WFIFOW(fd, 2) = n + 2;
    WFIFOW(fd, 4) = pos;
    WFIFOB(fd, 6) = ok;
    WFIFOSET(fd, packet_len_table[0xaa]);
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_unequipitemack(MapSessionData *sd, int n, int pos, int ok)
{
    nullpo_retv(sd);

    int fd = sd->fd;
    WFIFOW(fd, 0) = 0xac;
    WFIFOW(fd, 2) = n + 2;
    WFIFOW(fd, 4) = pos;
    WFIFOB(fd, 6) = ok;
    WFIFOSET(fd, packet_len_table[0xac]);
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_misceffect(BlockList *bl, int type)
{
    nullpo_retv(bl);

    uint8_t buf[32];
    WBUFW(buf, 0) = 0x19b;
    WBUFL(buf, 2) = bl->id;
    WBUFL(buf, 6) = type;

    clif_send(buf, packet_len_table[0x19b], bl, Whom::AREA);
}

/*==========================================
 * 表示オプション変更
 *------------------------------------------
 */
void clif_changeoption(BlockList *bl)
{
    nullpo_retv(bl);

    short option = *battle_get_option(bl);

    uint8_t buf[32];
    WBUFW(buf, 0) = 0x119;
    WBUFL(buf, 2) = bl->id;
    WBUFW(buf, 6) = *battle_get_opt1(bl);
    WBUFW(buf, 8) = *battle_get_opt2(bl);
    WBUFW(buf, 10) = option;
    WBUFB(buf, 12) = 0;        // ??

    clif_send(buf, packet_len_table[0x119], bl, Whom::AREA);
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_useitemack(MapSessionData *sd, int idx, int amount,
                     int ok)
{
    nullpo_retv(sd);

    if (!ok)
    {
        int fd = sd->fd;
        WFIFOW(fd, 0) = 0xa8;
        WFIFOW(fd, 2) = idx + 2;
        WFIFOW(fd, 4) = amount;
        WFIFOB(fd, 6) = ok;
        WFIFOSET(fd, packet_len_table[0xa8]);
    }
    else
    {
        uint8_t buf[32];

        WBUFW(buf, 0) = 0x1c8;
        WBUFW(buf, 2) = idx + 2;
        WBUFW(buf, 4) = sd->status.inventory[idx].nameid;
        WBUFL(buf, 6) = sd->id;
        WBUFW(buf, 10) = amount;
        WBUFB(buf, 12) = ok;
        clif_send(buf, packet_len_table[0x1c8], sd, Whom::SELF);
    }
}

/*==========================================
 * 取り引き要請受け
 *------------------------------------------
 */
void clif_traderequest(MapSessionData *sd, char *name)
{
    nullpo_retv(sd);

    int fd = sd->fd;
    WFIFOW(fd, 0) = 0xe5;
    strcpy(sign_cast<char *>(WFIFOP(fd, 2)), name);
    WFIFOSET(fd, packet_len_table[0xe5]);
}

/*==========================================
 * 取り引き要求応答
 *------------------------------------------
 */
void clif_tradestart(MapSessionData *sd, int type)
{
    nullpo_retv(sd);

    int fd = sd->fd;
    WFIFOW(fd, 0) = 0xe7;
    WFIFOB(fd, 2) = type;
    WFIFOSET(fd, packet_len_table[0xe7]);
}

/*==========================================
 * 相手方からのアイテム追加
 *------------------------------------------
 */
void clif_tradeadditem(MapSessionData *sd,
                       MapSessionData *tsd, int idx, int amount)
{
    nullpo_retv(sd);
    nullpo_retv(tsd);

    int fd = tsd->fd;
    WFIFOW(fd, 0) = 0xe9;
    WFIFOL(fd, 2) = amount;
    if (idx == 0)
    {
        WFIFOW(fd, 6) = 0;     // type id
        WFIFOB(fd, 8) = 0;     //identify flag
        WFIFOB(fd, 9) = 0;     // attribute
        WFIFOB(fd, 10) = 0;    //refine
        WFIFOW(fd, 11) = 0;    //card (4w)
        WFIFOW(fd, 13) = 0;    //card (4w)
        WFIFOW(fd, 15) = 0;    //card (4w)
        WFIFOW(fd, 17) = 0;    //card (4w)
    }
    else
    {
        idx -= 2;
        WFIFOW(fd, 6) = sd->status.inventory[idx].nameid;    // type id
        WFIFOB(fd, 8) = sd->status.inventory[idx].identify;  //identify flag
        if (sd->status.inventory[idx].broken == 1)
            WFIFOB(fd, 9) = 1; // is broke weapon [Valaris]
        else
            WFIFOB(fd, 9) = sd->status.inventory[idx].attribute; // attribute
        WFIFOB(fd, 10) = sd->status.inventory[idx].refine;   //refine
        WFIFOW(fd, 11) = sd->status.inventory[idx].card[0];
        WFIFOW(fd, 13) = sd->status.inventory[idx].card[1];
        WFIFOW(fd, 15) = sd->status.inventory[idx].card[2];
        WFIFOW(fd, 17) = sd->status.inventory[idx].card[3];
    }
    WFIFOSET(fd, packet_len_table[0xe9]);
}

/*==========================================
 * アイテム追加成功/失敗
 *------------------------------------------
 */
void clif_tradeitemok(MapSessionData *sd, int idx, int amount, int fail)
{
    nullpo_retv(sd);

    int fd = sd->fd;
    WFIFOW(fd, 0) = 0x1b1;
    WFIFOW(fd, 2) = idx;
    WFIFOW(fd, 4) = amount;
    WFIFOB(fd, 6) = fail;
    WFIFOSET(fd, packet_len_table[0x1b1]);
}

/*==========================================
 * 取り引きok押し
 *------------------------------------------
 */
void clif_tradedeal_lock(MapSessionData *sd, int fail)
{
    nullpo_retv(sd);

    int fd = sd->fd;
    WFIFOW(fd, 0) = 0xec;
    WFIFOB(fd, 2) = fail;      // 0=you 1=the other person
    WFIFOSET(fd, packet_len_table[0xec]);
}

/*==========================================
 * 取り引きがキャンセルされました
 *------------------------------------------
 */
void clif_tradecancelled(MapSessionData *sd)
{
    nullpo_retv(sd);

    int fd = sd->fd;
    WFIFOW(fd, 0) = 0xee;
    WFIFOSET(fd, packet_len_table[0xee]);
}

/*==========================================
 * 取り引き完了
 *------------------------------------------
 */
void clif_tradecompleted(MapSessionData *sd, int fail)
{
    nullpo_retv(sd);

    int fd = sd->fd;
    WFIFOW(fd, 0) = 0xf0;
    WFIFOB(fd, 2) = fail;
    WFIFOSET(fd, packet_len_table[0xf0]);
}

/*==========================================
 * カプラ倉庫のアイテム数を更新
 *------------------------------------------
 */
void clif_updatestorageamount(MapSessionData *sd,
                              struct storage *stor)
{
    nullpo_retv(sd);
    nullpo_retv(stor);

    int fd = sd->fd;
    WFIFOW(fd, 0) = 0xf2;      // update storage amount
    WFIFOW(fd, 2) = stor->storage_amount;  //items
    WFIFOW(fd, 4) = MAX_STORAGE;   //items max
    WFIFOSET(fd, packet_len_table[0xf2]);
}

/*==========================================
 * カプラ倉庫にアイテムを追加する
 *------------------------------------------
 */
void clif_storageitemadded(MapSessionData *sd, struct storage *stor,
                           int idx, int amount)
{
    nullpo_retv(sd);
    nullpo_retv(stor);

    int fd = sd->fd;
    WFIFOW(fd, 0) = 0xf4;      // Storage item added
    WFIFOW(fd, 2) = idx + 1; // index
    WFIFOL(fd, 4) = amount;    // amount
    WFIFOW(fd, 8) = stor->storage_[idx].nameid;
    WFIFOB(fd, 10) = stor->storage_[idx].identify;   //identify flag
    if (stor->storage_[idx].broken == 1)
        WFIFOB(fd, 11) = 1;    // is weapon broken [Valaris]
    else
        WFIFOB(fd, 11) = stor->storage_[idx].attribute;  // attribute
    WFIFOB(fd, 12) = stor->storage_[idx].refine; //refine
    WFIFOW(fd, 13) = stor->storage_[idx].card[0];
    WFIFOW(fd, 15) = stor->storage_[idx].card[1];
    WFIFOW(fd, 17) = stor->storage_[idx].card[2];
    WFIFOW(fd, 19) = stor->storage_[idx].card[3];
    WFIFOSET(fd, packet_len_table[0xf4]);
}

/*==========================================
 * カプラ倉庫からアイテムを取り去る
 *------------------------------------------
 */
void clif_storageitemremoved(MapSessionData *sd, int idx, int amount)
{
    nullpo_retv(sd);

    int fd = sd->fd;
    WFIFOW(fd, 0) = 0xf6;      // Storage item removed
    WFIFOW(fd, 2) = idx + 1;
    WFIFOL(fd, 4) = amount;
    WFIFOSET(fd, packet_len_table[0xf6]);
}

/*==========================================
 * カプラ倉庫を閉じる
 *------------------------------------------
 */
void clif_storageclose(MapSessionData *sd)
{
    nullpo_retv(sd);

    int fd = sd->fd;
    WFIFOW(fd, 0) = 0xf8;      // Storage Closed
    WFIFOSET(fd, packet_len_table[0xf8]);
}

void clif_changelook_accessories(BlockList *bl,
                             MapSessionData *dest)
{
    int i;

    for (i = LOOK_SHOES; i <= LOOK_LAST; i++)
        clif_changelook_towards(bl, i, 0, dest);
}

//
// callback系 ?
//
/*==========================================
 * PC表示
 *------------------------------------------
 */
static void clif_getareachar_pc(MapSessionData *sd,
                          MapSessionData *dstsd)
{
    int len;

    if (dstsd->status.option & OPTION_INVISIBILITY)
        return;

    nullpo_retv(sd);
    nullpo_retv(dstsd);

    if (dstsd->walktimer)
    {
        len = clif_player_move(dstsd, WFIFOP(sd->fd, 0));
        WFIFOSET(sd->fd, len);
    }
    else
    {
        len = clif_player_update(dstsd, WFIFOP(sd->fd, 0));
        WFIFOSET(sd->fd, len);
    }

    clif_changelook_accessories(sd, dstsd);
    clif_changelook_accessories(dstsd, sd);
}

/*==========================================
 * NPC表示
 *------------------------------------------
 */
static void clif_getareachar_npc(MapSessionData *sd, struct npc_data *nd)
{
    int len;

    nullpo_retv(sd);
    nullpo_retv(nd);

    if (nd->npc_class < 0 || nd->flag & 1 || nd->npc_class == INVISIBLE_CLASS)
        return;

    len = clif_npc_appear(nd, WFIFOP(sd->fd, 0));
    WFIFOSET(sd->fd, len);
}

/*==========================================
 * 移動停止
 *------------------------------------------
 */
void clif_movemob(struct mob_data *md)
{
    nullpo_retv(md);

    uint8_t buf[256];
    uint16_t len = clif_mob_move(md, buf);
    clif_send(buf, len, md, Whom::AREA);
}

/*==========================================
 * モンスターの位置修正
 *------------------------------------------
 */
void clif_fixmobpos(struct mob_data *md)
{
    nullpo_retv(md);

    if (md->state.state == MS_WALK)
    {
        uint8_t buf[256];
        int len = clif_mob_move(md, buf);
        clif_send(buf, len, md, Whom::AREA);
    }
    else
    {
        uint8_t buf[256];
        int len = clif_mob_appear(md, buf);
        clif_send(buf, len, md, Whom::AREA);
    }
}

/*==========================================
 * PCの位置修正
 *------------------------------------------
 */
void clif_fixpcpos(MapSessionData *sd)
{
    nullpo_retv(sd);

    if (sd->walktimer)
    {
        uint8_t buf[256];
        int len = clif_player_move(sd, buf);
        clif_send(buf, len, sd, Whom::AREA);
    }
    else
    {
        uint8_t buf[256];
        int len = clif_player_update(sd, buf);
        clif_send(buf, len, sd, Whom::AREA);
    }
    clif_changelook_accessories(sd, NULL);
}

/*==========================================
 * 通常攻撃エフェクト＆ダメージ
 *------------------------------------------
 */
// TODO figure out what div is
void clif_damage(BlockList *src, BlockList *dst,
                 unsigned int tick, int sdelay, int ddelay, int damage,
                 int div_, int type, int damage2)
{
    nullpo_retv(src);
    nullpo_retv(dst);

    uint8_t buf[256];
    WBUFW(buf, 0) = 0x8a;
    WBUFL(buf, 2) = src->id;
    WBUFL(buf, 6) = dst->id;
    WBUFL(buf, 10) = tick;
    WBUFL(buf, 14) = sdelay;
    WBUFL(buf, 18) = ddelay;
    WBUFW(buf, 22) = (damage > 0x7fff) ? 0x7fff : damage;
    WBUFW(buf, 24) = div_;
    WBUFB(buf, 26) = type;
    WBUFW(buf, 27) = damage2;
    clif_send(buf, packet_len_table[0x8a], src, Whom::AREA);
}

/*==========================================
 *
 *------------------------------------------
 */
static void clif_getareachar_mob(MapSessionData *sd, struct mob_data *md)
{
    int len;
    nullpo_retv(sd);
    nullpo_retv(md);

    if (md->state.state == MS_WALK)
    {
        len = clif_mob_move(md, WFIFOP(sd->fd, 0));
        WFIFOSET(sd->fd, len);
    }
    else
    {
        len = clif_mob_appear(md, WFIFOP(sd->fd, 0));
        WFIFOSET(sd->fd, len);
    }
}

/*==========================================
 *
 *------------------------------------------
 */
static void clif_getareachar_item(MapSessionData *sd,
                            struct flooritem_data *fitem)
{
    int fd;

    nullpo_retv(sd);
    nullpo_retv(fitem);

    fd = sd->fd;
    //009d <ID>.l <item ID>.w <identify flag>.B <X>.w <Y>.w <amount>.w <subX>.B <subY>.B
    WFIFOW(fd, 0) = 0x9d;
    WFIFOL(fd, 2) = fitem->id;
    WFIFOW(fd, 6) = fitem->item_data.nameid;
    WFIFOB(fd, 8) = fitem->item_data.identify;
    WFIFOW(fd, 9) = fitem->x;
    WFIFOW(fd, 11) = fitem->y;
    WFIFOW(fd, 13) = fitem->item_data.amount;
    WFIFOB(fd, 15) = fitem->subx;
    WFIFOB(fd, 16) = fitem->suby;

    WFIFOSET(fd, packet_len_table[0x9d]);
}

/*==========================================
 *
 *------------------------------------------
 */
static void clif_getareachar(BlockList *bl, MapSessionData *sd)
{
    nullpo_retv(bl);

    switch (bl->type)
    {
        case BL_PC:
            if (sd == static_cast<MapSessionData *>(bl))
                break;
            clif_getareachar_pc(sd, static_cast<MapSessionData *>(bl));
            break;
        case BL_NPC:
            clif_getareachar_npc(sd, static_cast<struct npc_data *>(bl));
            break;
        case BL_MOB:
            clif_getareachar_mob(sd, static_cast<struct mob_data *>(bl));
            break;
        case BL_ITEM:
            clif_getareachar_item(sd, static_cast<struct flooritem_data *>(bl));
            break;
        default:
            map_log("%s: ??? %d\n", __func__, bl->type);
            break;
    }
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_pcoutsight(BlockList *bl, MapSessionData *sd)
{
    MapSessionData *dstsd;

    nullpo_retv(bl);
    nullpo_retv(sd);

    switch (bl->type)
    {
        case BL_PC:
            dstsd = static_cast<MapSessionData *>(bl);
            if (sd != dstsd)
            {
                clif_being_remove_id(dstsd->id, BeingRemoveType::ZERO, sd->fd);
                clif_being_remove_id(sd->id, BeingRemoveType::ZERO, dstsd->fd);
            }
            break;
        case BL_NPC:
            if (static_cast<struct npc_data *>(bl)->npc_class != INVISIBLE_CLASS)
                clif_being_remove_id(bl->id, BeingRemoveType::ZERO, sd->fd);
            break;
        case BL_MOB:
            clif_being_remove_id(bl->id, BeingRemoveType::ZERO, sd->fd);
            break;
        case BL_ITEM:
            clif_clearflooritem(static_cast<struct flooritem_data *>(bl), sd->fd);
            break;
    }
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_pcinsight(BlockList *bl, MapSessionData *sd)
{
    MapSessionData *dstsd;

    nullpo_retv(bl);
    nullpo_retv(sd);

    switch (bl->type)
    {
        case BL_PC:
            dstsd = static_cast<MapSessionData *>(bl);
            if (sd != dstsd)
            {
                clif_getareachar_pc(sd, dstsd);
                clif_getareachar_pc(dstsd, sd);
            }
            break;
        case BL_NPC:
            clif_getareachar_npc(sd, static_cast<struct npc_data *>(bl));
            break;
        case BL_MOB:
            clif_getareachar_mob(sd, static_cast<struct mob_data *>(bl));
            break;
        case BL_ITEM:
            clif_getareachar_item(sd, static_cast<struct flooritem_data *>(bl));
            break;
    }
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_moboutsight(BlockList *bl, struct mob_data *md)
{
    nullpo_retv(bl);
    nullpo_retv(md);

    if (bl->type == BL_PC)
    {
        MapSessionData *sd = static_cast<MapSessionData *>(bl);
        clif_being_remove_id(md->id, BeingRemoveType::ZERO, sd->fd);
    }
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_mobinsight(BlockList *bl, struct mob_data *md)
{
    nullpo_retv(bl);

    if (bl->type == BL_PC)
    {
        MapSessionData *sd = static_cast<MapSessionData *>(bl);
        clif_getareachar_mob(sd, md);
    }
}

/*==========================================
 * スキルリストを送信する
 *------------------------------------------
 */
void clif_skillinfoblock(MapSessionData *sd)
{
    nullpo_retv(sd);

    int fd = sd->fd;
    WFIFOW(fd, 0) = 0x10f;
    int len = 4;
    for (int i = 0, c = 0; i < MAX_SKILL; i++)
    {
        int id = sd->status.skill[i].id;
        if (id != 0 && (sd->tmw_version >= 1))
        {                       // [Fate] Version 1 and later don't crash because of bad skill IDs anymore
            WFIFOW(fd, len) = id;
            WFIFOW(fd, len + 2) = 0; // skill_get_inf(id);
            WFIFOW(fd, len + 4) = skill_db[i].poolflags | (sd->status.skill[i].flags & SKILL_POOL_ACTIVATED);
            WFIFOW(fd, len + 6) = sd->status.skill[i].lv;
            WFIFOW(fd, len + 8) = 0; //skill_get_sp(id, sd->status.skill[i].lv);
            WFIFOW(fd, len + 10) = 0; //skill_get_range(id, sd->status.skill[i].lv);
            memset(WFIFOP(fd, len + 12), 0, 24);
            WFIFOB(fd, len + 36) = sd->status.skill[i].lv < skill_get_max_raise(id);
            len += 37;
            c++;
        }
    }
    WFIFOW(fd, 2) = len;
    WFIFOSET(fd, len);
}

/*==========================================
 * スキル割り振り通知
 *------------------------------------------
 */
void clif_skillup(MapSessionData *sd, int skill_num)
{
    nullpo_retv(sd);

    int fd = sd->fd;
    WFIFOW(fd, 0) = 0x10e;
    WFIFOW(fd, 2) = skill_num;
    WFIFOW(fd, 4) = sd->status.skill[skill_num].lv;
    WFIFOW(fd, 6) = 0; //skill_get_sp(skill_num, sd->status.skill[skill_num].lv);
    WFIFOW(fd, 8) = 0; //skill_get_range(skill_num, sd->status.skill[skill_num].lv);
    WFIFOB(fd, 10) = sd->status.skill[skill_num].lv < skill_get_max_raise(sd->status.skill[skill_num].id);
    WFIFOSET(fd, packet_len_table[0x10e]);
}

/*==========================================
 * 状態異常アイコン/メッセージ表示
 *------------------------------------------
 */
void clif_status_change(BlockList *bl, int type, int flag)
{
    nullpo_retv(bl);

    uint8_t buf[16];
    WBUFW(buf, 0) = 0x0196;
    WBUFW(buf, 2) = type;
    WBUFL(buf, 4) = bl->id;
    WBUFB(buf, 8) = flag;
    clif_send(buf, packet_len_table[0x196], bl, Whom::AREA);
}

/*==========================================
 * Send message(modified by [Yor])
 *------------------------------------------
 */
void clif_displaymessage(int fd, const char *mes)
{
    int len_mes = strlen(mes);

    if (len_mes > 0)
    {                           // don't send a void message (it's not displaying on the client chat). @help can send void line.
        WFIFOW(fd, 0) = 0x8e;
        WFIFOW(fd, 2) = 5 + len_mes;   // 4 + len + NULL teminate
        memcpy(WFIFOP(fd, 4), mes, len_mes + 1);
        WFIFOSET(fd, 5 + len_mes);
    }
}

/*==========================================
 * 天の声を送信する
 *------------------------------------------
 */
void clif_GMmessage(BlockList *bl, const char *mes, int len, int flag)
{
    uint8_t buf[len + 16];
    WBUFW(buf, 0) = 0x9a;
    WBUFW(buf, 2) = len + 4;
    memcpy(WBUFP(buf, 4), mes, len);
    flag &= 0x07;
    clif_send(buf, WBUFW(buf, 2), bl,
               (flag == 1) ? Whom::ALL_SAMEMAP :
               (flag == 2) ? Whom::AREA : (flag == 3) ? Whom::SELF : Whom::ALL_CLIENT);
}

/*==========================================
 * 復活する
 *------------------------------------------
 */
void clif_resurrection(BlockList *bl, int type)
{
    nullpo_retv(bl);

    uint8_t buf[16];
    WBUFW(buf, 0) = 0x148;
    WBUFL(buf, 2) = bl->id;
    WBUFW(buf, 6) = type;

    clif_send(buf, packet_len_table[0x148], bl, type == 1 ? Whom::AREA : Whom::AREA_WOS);
}

/*==========================================
 * whisper is transmitted to the destination player
 *------------------------------------------
 */
void clif_whisper_message(int fd, const char *nick, const char *mes, int mes_len)   // R 0097 <len>.w <nick>.24B <message>.?B
{
    WFIFOW(fd, 0) = 0x97;
    WFIFOW(fd, 2) = mes_len + 24 + 4;
    memcpy(WFIFOP(fd, 4), nick, 24);
    memcpy(WFIFOP(fd, 28), mes, mes_len);
    WFIFOSET(fd, WFIFOW(fd, 2));
}

/*==========================================
 * The transmission result of whisper is transmitted to the source player
 *------------------------------------------
 */
void clif_whisper_end(int fd, int flag) // R 0098 <type>.B: 0: success to send whisper, 1: target character is not loged in?, 2: ignored by target
{
    WFIFOW(fd, 0) = 0x98;
    WFIFOW(fd, 2) = flag;
    WFIFOSET(fd, packet_len_table[0x98]);
}

/*==========================================
 * パーティ作成完了
 * Relay the result of party creation.
 *
 * (R 00fa <flag>.B)
 *
 * flag:
 *  0 The party was created.
 *  1 The party name is invalid/taken.
 *  2 The character is already in a party.
 *------------------------------------------
 */
void clif_party_created(MapSessionData *sd, int flag)
{
    nullpo_retv(sd);

    int fd = sd->fd;
    WFIFOW(fd, 0) = 0xfa;
    WFIFOB(fd, 2) = flag;
    WFIFOSET(fd, packet_len_table[0xfa]);
}

/*==========================================
 * パーティ情報送信
 *------------------------------------------
 */
void clif_party_info(struct party *p, int fd)
{
    nullpo_retv(p);

    uint8_t buf[1024];
    WBUFW(buf, 0) = 0xfb;
    STRZCPY2(sign_cast<char *>(WBUFP(buf, 4)), p->name);

    MapSessionData *sd = NULL;
    int c = 0;
    for (int i = 0; i < MAX_PARTY; i++)
    {
        struct party_member *m = &p->member[i];
        if (m->account_id > 0)
        {
            if (sd == NULL)
                sd = m->sd;
            WBUFL(buf, 28 + c * 46) = m->account_id;
            memcpy(WBUFP(buf, 28 + c * 46 + 4), m->name, 24);
            memcpy(WBUFP(buf, 28 + c * 46 + 28), m->map, 16);
            WBUFB(buf, 28 + c * 46 + 44) = (m->leader) ? 0 : 1;
            WBUFB(buf, 28 + c * 46 + 45) = (m->online) ? 0 : 1;
            c++;
        }
    }
    WBUFW(buf, 2) = 28 + c * 46;
    if (fd >= 0)
    {                           // fdが設定されてるならそれに送る
        memcpy(WFIFOP(fd, 0), buf, WBUFW(buf, 2));
        WFIFOSET(fd, WFIFOW(fd, 2));
        return;
    }
    if (sd)
        clif_send(buf, WBUFW(buf, 2), sd, Whom::PARTY);
}

/*==========================================
 * パーティ勧誘
 * Relay a party invitation.
 *
 * (R 00fe <sender_ID>.l <party_name>.24B)
 *------------------------------------------
 */
void clif_party_invite(MapSessionData *sd,
                       MapSessionData *tsd)
{
    nullpo_retv(sd);
    nullpo_retv(tsd);

    int fd = tsd->fd;

    struct party *p = party_search(sd->status.party_id);
    if (!p)
        return;

    WFIFOW(fd, 0) = 0xfe;
    WFIFOL(fd, 2) = sd->status.account_id;
    memcpy(WFIFOP(fd, 6), p->name, 24);
    WFIFOSET(fd, packet_len_table[0xfe]);
}

/*==========================================
 * パーティ勧誘結果
 * Relay the response to a party invitation.
 *
 * (R 00fd <name>.24B <flag>.B)
 *
 * flag:
 *  0 The character is already in a party.
 *  1 The invitation was rejected.
 *  2 The invitation was accepted.
 *  3 The party is full.
 *  4 The character is in the same party.
 *------------------------------------------
 */
void clif_party_inviteack(MapSessionData *sd, char *nick, int flag)
{
    nullpo_retv(sd);

    int fd = sd->fd;
    WFIFOW(fd, 0) = 0xfd;
    memcpy(WFIFOP(fd, 2), nick, 24);
    WFIFOB(fd, 26) = flag;
    WFIFOSET(fd, packet_len_table[0xfd]);
}

/*==========================================
 * パーティ設定送信
 * flag & 0x001=exp変更ミス
 *        0x010=item変更ミス
 *        0x100=一人にのみ送信
 *------------------------------------------
 */
void clif_party_option(struct party *p, MapSessionData *sd, int flag)
{
    nullpo_retv(p);

    if (!sd && !flag)
    {
        for (int i = 0; i < MAX_PARTY; i++)
        {
            sd = map_id2sd(p->member[i].account_id);
            if (sd)
                break;
        }
    }
    if (!sd)
        return;

    uint8_t buf[16];
    WBUFW(buf, 0) = 0x101;
    WBUFW(buf, 2) = (flag & 0x01) ? 2 : p->exp;
    WBUFW(buf, 4) = (flag & 0x10) ? 2 : p->item;
    if (flag == 0)
        clif_send(buf, packet_len_table[0x101], sd, Whom::PARTY);
    else
    {
        memcpy(WFIFOP(sd->fd, 0), buf, packet_len_table[0x101]);
        WFIFOSET(sd->fd, packet_len_table[0x101]);
    }
}

/*==========================================
 * パーティ脱退（脱退前に呼ぶこと）
 *------------------------------------------
 */
void clif_party_left(struct party *p, MapSessionData *sd,
                     account_t account_id, const char *name, int flag)
{
    nullpo_retv(p);

    uint8_t buf[64];
    WBUFW(buf, 0) = 0x105;
    WBUFL(buf, 2) = account_id;
    strzcpy(sign_cast<char *>(WBUFP(buf, 6)), name, 24);
    WBUFB(buf, 30) = flag & 0x0f;

    if ((flag & 0xf0) == 0)
    {
        if (!sd)
            for (int i = 0; i < MAX_PARTY; i++)
            {
                sd = p->member[i].sd;
                if (sd != NULL)
                    break;
            }
        if (sd)
            clif_send(buf, packet_len_table[0x105], sd, Whom::PARTY);
    }
    else if (sd)
    {
        memcpy(WFIFOP(sd->fd, 0), buf, packet_len_table[0x105]);
        WFIFOSET(sd->fd, packet_len_table[0x105]);
    }
}

/*==========================================
 * パーティメッセージ送信
 *------------------------------------------
 */
void clif_party_message(struct party *p, int account_id, const char *mes, int len)
{
    nullpo_retv(p);

    MapSessionData *sd = NULL;
    for (int i = 0; i < MAX_PARTY; i++)
    {
        sd = p->member[i].sd;
        if (sd)
            break;
    }
    if (sd)
    {
        uint8_t buf[1024];
        WBUFW(buf, 0) = 0x109;
        WBUFW(buf, 2) = len + 8;
        WBUFL(buf, 4) = account_id;
        memcpy(WBUFP(buf, 8), mes, len);
        clif_send(buf, len + 8, sd, Whom::PARTY);
    }
}

/*==========================================
 * パーティ座標通知
 *------------------------------------------
 */
void clif_party_xy(struct party *, MapSessionData *sd)
{
    nullpo_retv(sd);

    uint8_t buf[16];
    WBUFW(buf, 0) = 0x107;
    WBUFL(buf, 2) = sd->status.account_id;
    WBUFW(buf, 6) = sd->x;
    WBUFW(buf, 8) = sd->y;
    clif_send(buf, packet_len_table[0x107], sd, Whom::PARTY_SAMEMAP_WOS);
}

/*==========================================
 * パーティHP通知
 *------------------------------------------
 */
void clif_party_hp(struct party *, MapSessionData *sd)
{
    nullpo_retv(sd);

    uint8_t buf[16];
    WBUFW(buf, 0) = 0x106;
    WBUFL(buf, 2) = sd->status.account_id;
    WBUFW(buf, 6) = (sd->status.hp > 0x7fff) ? 0x7fff : sd->status.hp;
    WBUFW(buf, 8) =
        (sd->status.max_hp > 0x7fff) ? 0x7fff : sd->status.max_hp;
    clif_send(buf, packet_len_table[0x106], sd, Whom::PARTY_AREA_WOS);
}

/*==========================================
 * パーティ場所移動（未使用）
 *------------------------------------------
 */
void clif_party_move(struct party *p, MapSessionData *sd, bool online)
{
    uint8_t buf[128];

    nullpo_retv(sd);
    nullpo_retv(p);

    WBUFW(buf, 0) = 0x104;
    WBUFL(buf, 2) = sd->status.account_id;
    WBUFL(buf, 6) = 0;
    WBUFW(buf, 10) = sd->x;
    WBUFW(buf, 12) = sd->y;
    WBUFB(buf, 14) = !online;
    STRZCPY2(sign_cast<char *>(WBUFP(buf, 15)), p->name);
    STRZCPY2(sign_cast<char *>(WBUFP(buf, 39)), sd->status.name);
    maps[sd->m].name.write_to(sign_cast<char *>(WBUFP(buf, 63)));
    clif_send(buf, packet_len_table[0x104], sd, Whom::PARTY);
}

/*==========================================
 * 攻撃するために移動が必要
 *------------------------------------------
 */
void clif_movetoattack(MapSessionData *sd, BlockList *bl)
{
    int fd;

    nullpo_retv(sd);
    nullpo_retv(bl);

    fd = sd->fd;
    WFIFOW(fd, 0) = 0x139;
    WFIFOL(fd, 2) = bl->id;
    WFIFOW(fd, 6) = bl->x;
    WFIFOW(fd, 8) = bl->y;
    WFIFOW(fd, 10) = sd->x;
    WFIFOW(fd, 12) = sd->y;
    WFIFOW(fd, 14) = sd->attackrange;
    WFIFOSET(fd, packet_len_table[0x139]);
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_changemapcell(int m, int x, int y, int cell_type, int type)
{
    BlockList bl(BL_NUL);
    uint8_t buf[32];

    bl.m = m;
    bl.x = x;
    bl.y = y;
    WBUFW(buf, 0) = 0x192;
    WBUFW(buf, 2) = x;
    WBUFW(buf, 4) = y;
    WBUFW(buf, 6) = cell_type;
    maps[m].name.write_to(sign_cast<char *>(WBUFP(buf, 8)));
    if (!type)
        clif_send(buf, packet_len_table[0x192], &bl, Whom::AREA);
    else
        clif_send(buf, packet_len_table[0x192], &bl, Whom::ALL_SAMEMAP);
}

/*==========================================
 * エモーション
 *------------------------------------------
 */
void clif_emotion(BlockList *bl, int type)
{
    uint8_t buf[8];

    nullpo_retv(bl);

    WBUFW(buf, 0) = 0xc0;
    WBUFL(buf, 2) = bl->id;
    WBUFB(buf, 6) = type;
    clif_send(buf, packet_len_table[0xc0], bl, Whom::AREA);
}

/*==========================================
 * 座る
 *------------------------------------------
 */
void clif_sitting(int, MapSessionData *sd)
{
    nullpo_retv(sd);

    uint8_t buf[64];
    WBUFW(buf, 0) = 0x8a;
    WBUFL(buf, 2) = sd->id;
    WBUFB(buf, 26) = 2;
    clif_send(buf, packet_len_table[0x8a], sd, Whom::AREA);
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_disp_onlyself(MapSessionData *sd, char *mes, int len)
{
    nullpo_retv(sd);

    uint8_t buf[len + 32];
    WBUFW(buf, 0) = 0x17f;
    WBUFW(buf, 2) = len + 8;
    memcpy(WBUFP(buf, 4), mes, len + 4);

    clif_send(buf, WBUFW(buf, 2), sd, Whom::SELF);
}

/*==========================================
 *
 *------------------------------------------
 */

void clif_GM_kickack(MapSessionData *sd, int id)
{
    nullpo_retv(sd);

    int fd = sd->fd;
    WFIFOW(fd, 0) = 0xcd;
    WFIFOL(fd, 2) = id;
    WFIFOSET(fd, packet_len_table[0xcd]);
}

static void clif_parse_QuitGame(int fd, MapSessionData *sd);

void clif_GM_kick(MapSessionData *sd, MapSessionData *tsd,
                  int type)
{
    nullpo_retv(tsd);

    if (type)
        clif_GM_kickack(sd, tsd->status.account_id);
    tsd->opt1 = tsd->opt2 = 0;
    clif_parse_QuitGame(tsd->fd, tsd);
}

/*==========================================
 * サウンドエフェクト
 *------------------------------------------
 */
// ignored by client
void clif_soundeffect(MapSessionData *sd, BlockList *bl,
                       const char *name, int type)
{
    int fd;

    nullpo_retv(sd);
    nullpo_retv(bl);

    fd = sd->fd;
    WFIFOW(fd, 0) = 0x1d3;
    memcpy(WFIFOP(fd, 2), name, 24);
    WFIFOB(fd, 26) = type;
    WFIFOL(fd, 27) = 0;
    WFIFOL(fd, 31) = bl->id;
    WFIFOSET(fd, packet_len_table[0x1d3]);

    return;
}

// displaying special effects (npcs, weather, etc) [Valaris]
void clif_specialeffect(BlockList *bl, int type, int flag)
{
    uint8_t buf[24];

    nullpo_retv(bl);

    memset(buf, 0, packet_len_table[0x19b]);

    WBUFW(buf, 0) = 0x19b;
    WBUFL(buf, 2) = bl->id;
    WBUFL(buf, 6) = type;

    if (flag == 2)
    {
        for (MapSessionData *sd : auth_sessions)
        {
            if (sd->m == bl->m)
                clif_specialeffect(sd, type, 1);
        }
    }

    else if (flag == 1)
        clif_send(buf, packet_len_table[0x19b], bl, Whom::SELF);
    else if (!flag)
        clif_send(buf, packet_len_table[0x19b], bl, Whom::AREA);
}

// ------------
// clif_parse_*
// ------------
// パケット読み取って色々操作
/*==========================================
 *
 *------------------------------------------
 */
static void clif_parse_WantToConnection(int fd, MapSessionData *sd)
{
    MapSessionData *old_sd;
    int account_id;            // account_id in the packet

    if (sd)
    {
        map_log("%s: invalid request?\n", __func__);
        return;
    }

    if (RFIFOW(fd, 0) == 0x72)
    {
        account_id = RFIFOL(fd, 2);
    }
    else
        return;                 // Not the auth packet

    WFIFOL(fd, 0) = account_id;
    WFIFOSET(fd, 4);

    // if same account already connected, we disconnect the 2 sessions
    if ((old_sd = map_id2sd(account_id)) != NULL)
    {
        clif_authfail_fd(fd, 2);   // same id
        clif_authfail_fd(old_sd->fd, 2);   // same id
        printf
            ("clif_parse_WantToConnection: Double connection for account %d (sessions: #%d (new) and #%d (old)).\n",
             account_id, fd, old_sd->fd);
    }
    else
    {
        sd = new MapSessionData;
        session[fd]->session_data = sd;
        sd->fd = fd;

        charid_t charid = RFIFOL(fd, 6);
        uint32_t login1 = RFIFOL(fd, 10);
        // uint32_t client_tick = RFIFOL(fd, 14);
        uint8_t sex = RFIFOB(fd, 18);
        pc_setnewpc(sd, account_id, charid, login1,
                    sex);

        map_addiddb(sd);

        chrif_authreq(sd);
    }

    return;
}

/*==========================================
 * 007d クライアント側マップ読み込み完了
 * map侵入時に必要なデータを全て送りつける
 *------------------------------------------
 */
static void clif_parse_LoadEndAck(int, MapSessionData *sd)
{
//  struct item_data* item;
    int i;
    nullpo_retv(sd);

    if (sd->prev != NULL)
        return;

    // 接続ok時
    //clif_authok();
    if (sd->npc_id)
        npc_event_dequeue(sd);
    clif_skillinfoblock(sd);
    pc_checkitem(sd);

    // loadendack時
    // next exp
    clif_updatestatus(sd, SP_NEXTBASEEXP);
    clif_updatestatus(sd, SP_NEXTJOBEXP);
    // skill point
    clif_updatestatus(sd, SP_SKILLPOINT);
    // item
    clif_itemlist(sd);
    clif_equiplist(sd);
    // cart
    // param all
    clif_initialstatus(sd);
    // party
    party_send_movemap(sd);
    // 119
    // 78

    if (battle_config.pc_invincible_time > 0)
    {
        pc_setinvincibletimer(sd, battle_config.pc_invincible_time);
    }

    map_addblock(sd);     // ブロック登録
    clif_spawnpc(sd);          // spawn

    // weight max , now
    clif_updatestatus(sd, SP_MAXWEIGHT);
    clif_updatestatus(sd, SP_WEIGHT);

    // pvp
    if (sd->pvp_timer && !battle_config.pk_mode)
        delete_timer(sd->pvp_timer);
    if (maps[sd->m].flag.pvp)
    {
        if (!battle_config.pk_mode)
        {                       // remove pvp stuff for pk_mode [Valaris]
            sd->pvp_timer =
                add_timer(gettick() + 200, pc_calc_pvprank_timer, sd->id);
            sd->pvp_rank = 0;
            sd->pvp_lastusers = 0;
            sd->pvp_point = 5;
        }
    }
    else
    {
        sd->pvp_timer = NULL;
    }

    if (sd->state.connect_new)
    {
        sd->state.connect_new = 0;
    }

    // view equipment item
    clif_changelook(sd, LOOK_WEAPON, 0);

    // option
    clif_changeoption(sd);
    for (i = 0; i < MAX_INVENTORY; i++)
    {
        if (sd->status.inventory[i].equip
            && sd->status.inventory[i].equip & 0x0002
            && sd->status.inventory[i].broken == 1)
            skill_status_change_start(sd, SC_BROKNWEAPON, 0, 0);
        if (sd->status.inventory[i].equip
            && sd->status.inventory[i].equip & 0x0010
            && sd->status.inventory[i].broken == 1)
            skill_status_change_start(sd, SC_BROKNARMOR, 0, 0);
    }

//        clif_changelook_accessories(sd, NULL);

    map_foreachinarea(clif_getareachar, sd->m, sd->x - AREA_SIZE,
                      sd->y - AREA_SIZE, sd->x + AREA_SIZE,
                      sd->y + AREA_SIZE, BL_NUL, sd);
}

/*==========================================
 *
 *------------------------------------------
 */
static void clif_parse_WalkToXY(int fd, MapSessionData *sd)
{
    int x, y;

    nullpo_retv(sd);

    if (pc_isdead(sd))
    {
        clif_being_remove(sd, BeingRemoveType::DEAD);
        return;
    }

    if (sd->npc_id != 0 || sd->state.storage_flag)
        return;

    if (sd->canmove_tick > gettick())
        return;

    if (sd->opt1 > 0 && sd->opt1 != 6)
        return;
    if (sd->status.option & 2)
        return;

    if (sd->invincible_timer)
        pc_delinvincibletimer(sd);

    pc_stopattack(sd);

    x = RFIFOB(fd, 2) * 4 + (RFIFOB(fd, 3) >> 6);
    y = ((RFIFOB(fd, 3) & 0x3f) << 4) + (RFIFOB(fd, 4) >> 4);
    pc_walktoxy(sd, x, y);

}

/*==========================================
 *
 *------------------------------------------
 */
void clif_parse_QuitGame(int fd, MapSessionData *sd)
{
    unsigned int tick = gettick();
    nullpo_retv(sd);

    WFIFOW(fd, 0) = 0x18b;
    if ((!pc_isdead(sd) && (sd->opt1 || sd->opt2))
        || (DIFF_TICK(tick, sd->canact_tick) < 0))
    {
        WFIFOW(fd, 2) = 1;
        WFIFOSET(fd, packet_len_table[0x18b]);
        return;
    }

    /*  Rovert's prevent logout option fixed [Valaris]  */
    if ((battle_config.prevent_logout
         && (gettick() - sd->canlog_tick) >= 10000)
        || (!battle_config.prevent_logout))
    {
        clif_setwaitclose(fd);
        WFIFOW(fd, 2) = 0;
    }
    else
    {
        WFIFOW(fd, 2) = 1;
    }
    WFIFOSET(fd, packet_len_table[0x18b]);

}

/*==========================================
 *
 *------------------------------------------
 */
static void clif_parse_GetCharNameRequest(int fd, MapSessionData *sd)
{
    BlockList *bl;
    int account_id;

    account_id = RFIFOL(fd, 2);
    bl = map_id2bl(account_id);
    if (bl == NULL)
        return;

    WFIFOW(fd, 0) = 0x95;
    WFIFOL(fd, 2) = account_id;

    switch (bl->type)
    {
        case BL_PC:
        {
            MapSessionData *ssd = static_cast<MapSessionData *>(bl);

            nullpo_retv(ssd);

            if (ssd->state.shroud_active)
                memset(WFIFOP(fd, 6), 0, 24);
            else
                memcpy(WFIFOP(fd, 6), ssd->status.name, 24);
            WFIFOSET(fd, packet_len_table[0x95]);

            struct party *p = NULL;

            const char *party_name = "";

            bool will_send = 0;

            if (ssd->status.party_id > 0 && (p = party_search(ssd->status.party_id)) != NULL)
            {
                party_name = p->name;
                will_send = 1;
            }

            if (will_send)
            {
                WFIFOW(fd, 0) = 0x195;
                WFIFOL(fd, 2) = account_id;
                memcpy(WFIFOP(fd, 6), party_name, 24);
                memset(WFIFOP(fd, 30), 0, 24);
                memset(WFIFOP(fd, 54), 0, 24);
                memset(WFIFOP(fd, 78), 0, 24); // We send this value twice because the client expects it
                WFIFOSET(fd, packet_len_table[0x195]);

            }

            if (pc_isGM(sd) >= battle_config.hack_info_GM_level)
            {
                in_addr_t ip = session[ssd->fd]->client_addr.to_n();
                WFIFOW(fd, 0) = 0x20C;

                // Mask the IP using the char-server password
                if (battle_config.mask_ip_gms)
                    ip = MD5_ip(chrif_getpasswd(), ip);

                WFIFOL(fd, 2) = account_id;
                WFIFOL(fd, 6) = ip;
                WFIFOSET(fd, packet_len_table[0x20C]);
             }

        }
            break;
        case BL_NPC:
            memcpy(WFIFOP(fd, 6), static_cast<struct npc_data *>(bl)->name, 24);
            {
                char *start = sign_cast<char *>(WFIFOP(fd, 6));
                char *end = strchr(start, '#');    // [fate] elim hashed out/invisible names for the client
                if (end)
                    while (*end)
                        *end++ = 0;

                // [fate] Elim preceding underscores for (hackish) name position fine-tuning
                while (*start == '_')
                    *start++ = ' ';
            }
            WFIFOSET(fd, packet_len_table[0x95]);
            break;
        case BL_MOB:
        {
            struct mob_data *md = static_cast<struct mob_data *>(bl);

            nullpo_retv(md);

            memcpy(WFIFOP(fd, 6), md->name, 24);
            WFIFOSET(fd, packet_len_table[0x95]);
        }
            break;
        default:
            map_log("%s: bad type %d(%d)\n", __func__, bl->type, account_id);
            break;
    }
}

/*==========================================
 * Validate and process transmission of a
 * global/public message.
 *
 * (S 008c <len>.w <message>.?B)
 *------------------------------------------
 */
static void clif_parse_GlobalMessage(int fd, MapSessionData *sd)
{
    int msg_len = RFIFOW(fd, 2) - 4; /* Header (2) + length (2). */
    size_t message_len = 0;
    uint8_t *buf = NULL;
    char *message = NULL;   /* The message text only. */

    nullpo_retv(sd);

    if (!(buf = clif_validate_chat(sd, 2, &message, &message_len)))
    {
        clif_displaymessage(fd, "Your message could not be sent.");
        return;
    }

    if (is_atcommand(fd, sd, message, 0))
    {
        free(buf);
        return;
    }

    if (!magic_message(sd, sign_cast<char *>(buf), msg_len))
    {
        /* Don't send chat that results in an automatic ban. */
        if (tmw_CheckChatSpam(sd, message))
        {
            free(buf);
            clif_displaymessage(fd, "Your message could not be sent.");
            return;
        }

        /* It's not a spell/magic message, so send the message to others. */
        WBUFW(buf, 0) = 0x8d;
        WBUFW(buf, 2) = msg_len + 8;   /* Header (2) + length (2) + ID (4). */
        WBUFL(buf, 4) = sd->id;

        clif_send(buf, msg_len + 8, sd, Whom::AREA_CHAT_WOC);
    }

    /* Send the message back to the speaker. */
    memcpy(WFIFOP(fd, 0), RFIFOP(fd, 0), RFIFOW(fd, 2));
    WFIFOW(fd, 0) = 0x8e;
    WFIFOSET(fd, WFIFOW(fd, 2));

    free(buf);
}

void clif_message(BlockList *bl, const char *msg)
{
    unsigned short msg_len = strlen(msg) + 1;
    uint8_t buf[512];

    if (msg_len + 16 > 512)
        return;

    nullpo_retv(bl);

    WBUFW(buf, 0) = 0x8d;
    WBUFW(buf, 2) = msg_len + 8;
    WBUFL(buf, 4) = bl->id;
    memcpy(WBUFP(buf, 8), msg, msg_len);

    clif_send(buf, WBUFW(buf, 2), bl, Whom::AREA);
}

/*==========================================
 *
 *------------------------------------------
 */
static void clif_parse_ChangeDir(int fd, MapSessionData *sd)
{
    uint8_t buf[64];

    nullpo_retv(sd);

    Direction dir = static_cast<Direction>(RFIFOB(fd, 4));

    if (dir == sd->dir)
        return;

    pc_setdir(sd, dir);

    WBUFW(buf, 0) = 0x9c;
    WBUFL(buf, 2) = sd->id;
    WBUFW(buf, 6) = 0;
    WBUFB(buf, 8) = static_cast<int>(dir);
    clif_send(buf, packet_len_table[0x9c], sd, Whom::AREA_WOS);

}

/*==========================================
 *
 *------------------------------------------
 */
static void clif_parse_Emotion(int fd, MapSessionData *sd)
{
    uint8_t buf[64];

    nullpo_retv(sd);

    if (pc_checkskill(sd, NV_EMOTE) >= 1)
    {
        WBUFW(buf, 0) = 0xc0;
        WBUFL(buf, 2) = sd->id;
        WBUFB(buf, 6) = RFIFOB(fd, 2);
        clif_send(buf, packet_len_table[0xc0], sd, Whom::AREA);
    }
}

/*==========================================
 *
 *------------------------------------------
 */
static void clif_parse_HowManyConnections(int fd, MapSessionData *)
{
    WFIFOW(fd, 0) = 0xc2;
    WFIFOL(fd, 2) = map_getusers();
    WFIFOSET(fd, packet_len_table[0xc2]);
}

/*==========================================
 *
 *------------------------------------------
 */
static void clif_parse_ActionRequest(int fd, MapSessionData *sd)
{
    unsigned int tick;
    uint8_t buf[64];
    int action_type, target_id;

    nullpo_retv(sd);

    if (pc_isdead(sd))
    {
        clif_being_remove(sd, BeingRemoveType::DEAD);
        return;
    }
    if (sd->npc_id != 0 || sd->opt1 > 0 || sd->status.option & 2 || sd->state.storage_flag)
        return;

    tick = gettick();

    pc_stop_walking(sd, 0);
    pc_stopattack(sd);

    target_id = RFIFOL(fd, 2);
    action_type = RFIFOB(fd, 6);

    switch (action_type)
    {
        case 0x00:             // once attack
        case 0x07:             // continuous attack
            if (sd->status.option & OPTION_HIDE)
                return;
            if (!battle_config.sdelay_attack_enable)
            {
                if (DIFF_TICK(tick, sd->canact_tick) < 0)
                {
                    return;
                }
            }
            if (sd->invincible_timer)
                pc_delinvincibletimer(sd);
            if (sd->attacktarget > 0)   // [Valaris]
                sd->attacktarget = 0;
            pc_attack(sd, target_id, action_type != 0);
            break;
        case 0x02:             // sitdown
            pc_stop_walking(sd, 1);
            pc_setsit(sd);
            clif_sitting(fd, sd);
            break;
        case 0x03:             // standup
            pc_setstand(sd);
            WBUFW(buf, 0) = 0x8a;
            WBUFL(buf, 2) = sd->id;
            WBUFB(buf, 26) = 3;
            clif_send(buf, packet_len_table[0x8a], sd, Whom::AREA);
            break;
    }
}

/*==========================================
 *
 *------------------------------------------
 */
static void clif_parse_Restart(int fd, MapSessionData *sd)
{
    nullpo_retv(sd);

    switch (RFIFOB(fd, 2))
    {
        case 0x00:
            if (pc_isdead(sd))
            {
                pc_setstand(sd);
                pc_setrestartvalue(sd, 3);
                pc_setpos(sd, sd->status.save_point, BeingRemoveType::QUIT);
            }
            break;
        case 0x01:
            /*if (!pc_isdead(sd) && (sd->opt1 || (sd->opt2 && !(night_flag == 1 && sd->opt2 == STATE_BLIND))))
             * return; */

            /*  Rovert's Prevent logout option - Fixed [Valaris]    */
            if ((battle_config.prevent_logout
                 && (gettick() - sd->canlog_tick) >= 10000)
                || (!battle_config.prevent_logout))
            {
                chrif_charselectreq(sd);
            }
            else
            {
                WFIFOW(fd, 0) = 0x18b;
                WFIFOW(fd, 2) = 1;

                WFIFOSET(fd, packet_len_table[0x018b]);
            }
            break;
    }
}

/*==========================================
 * Validate and process transmission of a
 * whisper/private message.
 *
 * (S 0096 <len>.w <nick>.24B <message>.?B)
 *
 * rewritten by [Yor], then partially by
 * [remoitnane]
 *------------------------------------------
 */
static void clif_parse_Wis(int fd, MapSessionData *sd)
{
    size_t message_len = 0;
    uint8_t *buf = NULL;
    char *message = NULL;   /* The message text only. */
    MapSessionData *dstsd = NULL;

    nullpo_retv(sd);

    if (!(buf = clif_validate_chat(sd, 1, &message, &message_len)))
    {
        clif_displaymessage(fd, "Your message could not be sent.");
        return;
    }

    if (is_atcommand(fd, sd, message, 0))
    {
        free(buf);
        return;
    }

    /* Don't send chat that results in an automatic ban. */
    if (tmw_CheckChatSpam(sd, message))
    {
        free(buf);
        clif_displaymessage(fd, "Your message could not be sent.");
        return;
    }

    /*
     * The player is not on this server. Only send the whisper if the name is
     * exactly the same, because if there are multiple map-servers and a name
     * conflict(for instance, "Test" versus "test"), the char-server must
     * settle the discrepancy.
     */
    if (!(dstsd = map_nick2sd(sign_cast<const char *>(RFIFOP(fd, 4))))
            || strcmp(dstsd->status.name, sign_cast<const char *>(RFIFOP(fd, 4))) != 0)
        intif_whisper_message(sd, sign_cast<const char *>(RFIFOP(fd, 4)), message,  RFIFOW(fd, 2) - 28);
    else
    {
        /* Refuse messages addressed to self. */
        if (dstsd->fd == fd)
        {
            const char *mes = "You cannot page yourself.";
            clif_whisper_message(fd, whisper_server_name, mes, strlen(mes) + 1);
        }
        else
        {
            clif_whisper_message(dstsd->fd, sd->status.name, message,
                              RFIFOW(fd, 2) - 28);
            /* The whisper was sent successfully. */
            clif_whisper_end(fd, 0);
        }
    }

    free(buf);
}

/*==========================================
 *
 *------------------------------------------
 */
static void clif_parse_TakeItem(int fd, MapSessionData *sd)
{
    struct flooritem_data *fitem;
    int map_object_id;

    nullpo_retv(sd);

    map_object_id = RFIFOL(fd, 2);
    fitem = static_cast<struct flooritem_data *>(map_id2bl(map_object_id));

    if (pc_isdead(sd))
    {
        clif_being_remove(sd, BeingRemoveType::DEAD);
        return;
    }

    if (sd->npc_id != 0 || sd->opt1 > 0)
        return;

    if (fitem == NULL || fitem->m != sd->m)
        return;

    if (abs(sd->x - fitem->x) >= 2
        || abs(sd->y - fitem->y) >= 2)
        return;                 // too far away to pick up

    if (sd->state.shroud_active && sd->state.shroud_disappears_on_pickup)
        magic_unshroud(sd);

    pc_takeitem(sd, fitem);
}

/*==========================================
 *
 *------------------------------------------
 */
static void clif_parse_DropItem(int fd, MapSessionData *sd)
{
    int item_index, item_amount;

    nullpo_retv(sd);

    if (pc_isdead(sd))
    {
        clif_being_remove(sd, BeingRemoveType::DEAD);
        return;
    }
    if (sd->npc_id != 0 || sd->opt1 > 0 || maps[sd->m].flag.no_player_drops)
        return;

    item_index = RFIFOW(fd, 2) - 2;
    item_amount = RFIFOW(fd, 4);

    pc_dropitem(sd, item_index, item_amount);
}

/*==========================================
 *
 *------------------------------------------
 */
static void clif_parse_UseItem(int fd, MapSessionData *sd)
{
    nullpo_retv(sd);

    if (pc_isdead(sd))
    {
        clif_being_remove(sd, BeingRemoveType::DEAD);
        return;
    }
    if (sd->npc_id != 0 || sd->opt1 > 0)
        return;

    if (sd->invincible_timer)
        pc_delinvincibletimer(sd);

    pc_useitem(sd, RFIFOW(fd, 2) - 2);
}

/*==========================================
 *
 *------------------------------------------
 */
static void clif_parse_EquipItem(int fd, MapSessionData *sd)
{
    int idx;

    nullpo_retv(sd);

    if (pc_isdead(sd))
    {
        clif_being_remove(sd, BeingRemoveType::DEAD);
        return;
    }
    idx = RFIFOW(fd, 2) - 2;
    if (sd->npc_id != 0)
        return;

    if (sd->status.inventory[idx].identify != 1)
    {                           // 未鑑定
        // Bjorn: Auto-identify items when equipping them as there
        //  is no nice way to do this in the client yet.
        sd->status.inventory[idx].identify = 1;
        //clif_equipitemack(sd,idx,0,0);  // fail
        //return;
    }
    //ペット用装備であるかないか
    if (sd->inventory_data[idx])
    {
        uint16_t what = RFIFOW(fd, 4);
        if (sd->inventory_data[idx]->type == 10)
            what = 0x8000;    // 矢を無理やり装備できるように（−−；
        pc_equipitem(sd, idx, what);
    }
}

/*==========================================
 *
 *------------------------------------------
 */
static void clif_parse_UnequipItem(int fd, MapSessionData *sd)
{
    int idx;

    nullpo_retv(sd);

    if (pc_isdead(sd))
    {
        clif_being_remove(sd, BeingRemoveType::DEAD);
        return;
    }
    idx = RFIFOW(fd, 2) - 2;
    if (sd->status.inventory[idx].broken == 1 && sd->sc_data
        && sd->sc_data[SC_BROKNWEAPON].timer)
        skill_status_change_end(sd, SC_BROKNWEAPON, NULL);
    if (sd->status.inventory[idx].broken == 1 && sd->sc_data
        && sd->sc_data[SC_BROKNARMOR].timer)
        skill_status_change_end(sd, SC_BROKNARMOR, NULL);

    if (sd->npc_id != 0 || sd->opt1 > 0)
        return;
    pc_unequipitem(sd, idx, 0);
}

/*==========================================
 *
 *------------------------------------------
 */
static void clif_parse_NpcClicked(int fd, MapSessionData *sd)
{
    nullpo_retv(sd);

    if (pc_isdead(sd))
    {
        clif_being_remove(sd, BeingRemoveType::DEAD);
        return;
    }
    if (sd->npc_id != 0)
        return;
    npc_click(sd, RFIFOL(fd, 2));
}

/*==========================================
 *
 *------------------------------------------
 */
static void clif_parse_NpcBuySellSelected(int fd, MapSessionData *sd)
{
    npc_buysellsel(sd, RFIFOL(fd, 2), RFIFOB(fd, 6));
}

/*==========================================
 *
 *------------------------------------------
 */
static void clif_parse_NpcBuyListSend(int fd, MapSessionData *sd)
{
    int fail = 0, n;
    const uint16_t *item_list;

    n = (RFIFOW(fd, 2) - 4) / 4;
    item_list = reinterpret_cast<const uint16_t *>(RFIFOP(fd, 4));

    fail = npc_buylist(sd, n, item_list);

    WFIFOW(fd, 0) = 0xca;
    WFIFOB(fd, 2) = fail;
    WFIFOSET(fd, packet_len_table[0xca]);
}

/*==========================================
 *
 *------------------------------------------
 */
static void clif_parse_NpcSellListSend(int fd, MapSessionData *sd)
{
    int fail = 0, n;
    const uint16_t *item_list;

    n = (RFIFOW(fd, 2) - 4) / 4;
    item_list = reinterpret_cast<const uint16_t *>(RFIFOP(fd, 4));

    fail = npc_selllist(sd, n, item_list);

    WFIFOW(fd, 0) = 0xcb;
    WFIFOB(fd, 2) = fail;
    WFIFOSET(fd, packet_len_table[0xcb]);
}

/*==========================================
 * 取引要請を相手に送る
 *------------------------------------------
 */
static void clif_parse_TradeRequest(int, MapSessionData *sd)
{
    nullpo_retv(sd);

    if (pc_checkskill(sd, NV_TRADE) >= 1)
    {
        trade_traderequest(sd, RFIFOL(sd->fd, 2));
    }
}

/*==========================================
 * 取引要請
 *------------------------------------------
 */
static void clif_parse_TradeAck(int, MapSessionData *sd)
{
    nullpo_retv(sd);

    trade_tradeack(sd, RFIFOB(sd->fd, 2));
}

/*==========================================
 * アイテム追加
 *------------------------------------------
 */
static void clif_parse_TradeAddItem(int, MapSessionData *sd)
{
    nullpo_retv(sd);

    trade_tradeadditem(sd, RFIFOW(sd->fd, 2), RFIFOL(sd->fd, 4));
}

/*==========================================
 * アイテム追加完了(ok押し)
 *------------------------------------------
 */
static void clif_parse_TradeOk(int, MapSessionData *sd)
{
    trade_tradeok(sd);
}

/*==========================================
 * 取引キャンセル
 *------------------------------------------
 */
static void clif_parse_TradeCansel(int, MapSessionData *sd)
{
    trade_tradecancel(sd);
}

/*==========================================
 * 取引許諾(trade押し)
 *------------------------------------------
 */
static void clif_parse_TradeCommit(int, MapSessionData *sd)
{
    trade_tradecommit(sd);
}

/*==========================================
 * ステータスアップ
 *------------------------------------------
 */
static void clif_parse_StatusUp(int fd, MapSessionData *sd)
{
    pc_statusup(sd, RFIFOW(fd, 2));
}

/*==========================================
 * スキルレベルアップ
 *------------------------------------------
 */
static void clif_parse_SkillUp(int fd, MapSessionData *sd)
{
    pc_skillup(sd, RFIFOW(fd, 2));
}

/*==========================================
 *
 *------------------------------------------
 */
static void clif_parse_NpcSelectMenu(int fd, MapSessionData *sd)
{
    nullpo_retv(sd);

    sd->npc_menu = RFIFOB(fd, 6);
    map_scriptcont(sd, RFIFOL(fd, 2));
}

/*==========================================
 *
 *------------------------------------------
 */
static void clif_parse_NpcNextClicked(int fd, MapSessionData *sd)
{
    map_scriptcont(sd, RFIFOL(fd, 2));
}

/*==========================================
 *
 *------------------------------------------
 */
static void clif_parse_NpcAmountInput(int fd, MapSessionData *sd)
{
    nullpo_retv(sd);

    sd->npc_amount = RFIFOL(fd, 6);

    map_scriptcont(sd, RFIFOL(fd, 2));
}

/*==========================================
 * Process string-based input for an NPC.
 *
 * (S 01d5 <len>.w <npc_ID>.l <message>.?B)
 *------------------------------------------
 */
static void clif_parse_NpcStringInput(int fd, MapSessionData *sd)
{
    int len;
    nullpo_retv(sd);

    len = RFIFOW(fd, 2) - 8;

    /*
     * If we check for equal to 0, too, we'll freeze clients that send(or
     * claim to have sent) an "empty" message.
     */
    if (len < 0)
        return;

    if (len >= sizeof(sd->npc_str) - 1)
    {
        printf("clif_parse_NpcStringInput(): Input string too long!\n");
        len = sizeof(sd->npc_str) - 1;
    }

    if (len > 0)
        strncpy(sd->npc_str, sign_cast<const char *>(RFIFOP(fd, 8)), len);
    sd->npc_str[len] = '\0';

    map_scriptcont(sd, RFIFOL(fd, 4));
}

/*==========================================
 *
 *------------------------------------------
 */
static void clif_parse_NpcCloseClicked(int fd, MapSessionData *sd)
{
    map_scriptcont(sd, RFIFOL(fd, 2));
}

/*==========================================
 * カプラ倉庫へ入れる
 *------------------------------------------
 */
static void clif_parse_MoveToKafra(int fd, MapSessionData *sd)
{
    int item_index, item_amount;

    nullpo_retv(sd);

    item_index = RFIFOW(fd, 2) - 2;
    item_amount = RFIFOL(fd, 4);

    if ((sd->npc_id != 0 && !sd->npc_flags.storage) || sd->trade_partner != 0
        || !sd->state.storage_flag)
        return;

    if (sd->state.storage_flag == 1)
        storage_storageadd(sd, item_index, item_amount);
}

/*==========================================
 * カプラ倉庫から出す
 *------------------------------------------
 */
static void clif_parse_MoveFromKafra(int fd, MapSessionData *sd)
{
    int item_index, item_amount;

    nullpo_retv(sd);

    item_index = RFIFOW(fd, 2) - 1;
    item_amount = RFIFOL(fd, 4);

    if ((sd->npc_id != 0 && !sd->npc_flags.storage) || sd->trade_partner != 0
        || !sd->state.storage_flag)
        return;

    if (sd->state.storage_flag == 1)
        storage_storageget(sd, item_index, item_amount);
}

/*==========================================
 * カプラ倉庫を閉じる
 *------------------------------------------
 */
static void clif_parse_CloseKafra(int, MapSessionData *sd)
{
    nullpo_retv(sd);

    if (sd->state.storage_flag == 1)
        storage_storageclose(sd);
}

/*==========================================
 * パーティを作る
 * Process request to create a party.
 *
 * (S 00f9 <party_name>.24B)
 *------------------------------------------
 */
static void clif_parse_CreateParty(int fd, MapSessionData *sd)
{
    if (pc_checkskill(sd, NV_PARTY) >= 2)
    {
        party_create(sd, sign_cast<const char *>(RFIFOP(fd, 2)));
    }
}

/*==========================================
 * パーティに勧誘
 * Process invitation to join a party.
 *
 * (S 00fc <account_ID>.l)
 *------------------------------------------
 */
static void clif_parse_PartyInvite(int fd, MapSessionData *sd)
{
    party_invite(sd, RFIFOL(fd, 2));
}

/*==========================================
 * パーティ勧誘返答
 * Process reply to party invitation.
 *
 * (S 00ff <account_ID>.l <flag>.l)
 *------------------------------------------
 */
static void clif_parse_ReplyPartyInvite(int fd, MapSessionData *sd)
{
    if (pc_checkskill(sd, NV_PARTY) >= 1)
    {
        party_reply_invite(sd, RFIFOL(fd, 2), RFIFOL(fd, 6));
    }
    else
    {
        party_reply_invite(sd, RFIFOL(fd, 2), 0);
    }
}

/*==========================================
 * パーティ脱退要求
 *------------------------------------------
 */
static void clif_parse_LeaveParty(int, MapSessionData *sd)
{
    party_leave(sd);
}

/*==========================================
 * パーティ除名要求
 *------------------------------------------
 */
static void clif_parse_RemovePartyMember(int fd, MapSessionData *sd)
{
    party_removemember(sd, RFIFOL(fd, 2), sign_cast<const char *>(RFIFOP(fd, 6)));
}

/*==========================================
 * パーティ設定変更要求
 *------------------------------------------
 */
static void clif_parse_PartyChangeOption(int fd, MapSessionData *sd)
{
    party_changeoption(sd, RFIFOW(fd, 2), RFIFOW(fd, 4));
}

/*==========================================
 * パーティメッセージ送信要求
 * Validate and process transmission of a
 * party message.
 *
 * (S 0108 <len>.w <message>.?B)
 *------------------------------------------
 */
static void clif_parse_PartyMessage(int fd, MapSessionData *sd)
{
    size_t message_len = 0;
    uint8_t *buf = NULL;
    char *message = NULL;   /* The message text only. */

    nullpo_retv(sd);

    if (!(buf = clif_validate_chat(sd, 0, &message, &message_len)))
    {
        clif_displaymessage(fd, "Your message could not be sent.");
        return;
    }

    if (is_atcommand(fd, sd, message, 0))
    {
        free(buf);
        return;
    }

    /* Don't send chat that results in an automatic ban. */
    if (tmw_CheckChatSpam(sd, message))
    {
        free(buf);
        clif_displaymessage(fd, "Your message could not be sent.");
        return;
    }

    party_send_message(sd, message, RFIFOW(fd, 2) - 4);
    free(buf);
}

// functions list. Rate is how many milliseconds are required between
// calls. Packets exceeding this rate will be dropped. flood_rates in
// map.h must be the same length as this table. rate 0 is default
// rate -1 is unlimited
typedef struct func_table
{
    int rate;
    void (*func)(int fd, MapSessionData *sd);
} func_table;
func_table clif_parse_func_table[0x220] =
{
    { 0, 0 }, // 0
    { 0, 0 }, // 1
    { 0, 0 }, // 2
    { 0, 0 }, // 3
    { 0, 0 }, // 4
    { 0, 0 }, // 5
    { 0, 0 }, // 6
    { 0, 0 }, // 7
    { 0, 0 }, // 8
    { 0, 0 }, // 9
    { 0, 0 }, // a
    { 0, 0 }, // b
    { 0, 0 }, // c
    { 0, 0 }, // d
    { 0, 0 }, // e
    { 0, 0 }, // f
    { 0, 0 }, // 10
    { 0, 0 }, // 11
    { 0, 0 }, // 12
    { 0, 0 }, // 13
    { 0, 0 }, // 14
    { 0, 0 }, // 15
    { 0, 0 }, // 16
    { 0, 0 }, // 17
    { 0, 0 }, // 18
    { 0, 0 }, // 19
    { 0, 0 }, // 1a
    { 0, 0 }, // 1b
    { 0, 0 }, // 1c
    { 0, 0 }, // 1d
    { 0, 0 }, // 1e
    { 0, 0 }, // 1f
    { 0, 0 }, // 20
    { 0, 0 }, // 21
    { 0, 0 }, // 22
    { 0, 0 }, // 23
    { 0, 0 }, // 24
    { 0, 0 }, // 25
    { 0, 0 }, // 26
    { 0, 0 }, // 27
    { 0, 0 }, // 28
    { 0, 0 }, // 29
    { 0, 0 }, // 2a
    { 0, 0 }, // 2b
    { 0, 0 }, // 2c
    { 0, 0 }, // 2d
    { 0, 0 }, // 2e
    { 0, 0 }, // 2f
    { 0, 0 }, // 30
    { 0, 0 }, // 31
    { 0, 0 }, // 32
    { 0, 0 }, // 33
    { 0, 0 }, // 34
    { 0, 0 }, // 35
    { 0, 0 }, // 36
    { 0, 0 }, // 37
    { 0, 0 }, // 38
    { 0, 0 }, // 39
    { 0, 0 }, // 3a
    { 0, 0 }, // 3b
    { 0, 0 }, // 3c
    { 0, 0 }, // 3d
    { 0, 0 }, // 3e
    { 0, 0 }, // 3f
    { 0, 0 }, // 40
    { 0, 0 }, // 41
    { 0, 0 }, // 42
    { 0, 0 }, // 43
    { 0, 0 }, // 44
    { 0, 0 }, // 45
    { 0, 0 }, // 46
    { 0, 0 }, // 47
    { 0, 0 }, // 48
    { 0, 0 }, // 49
    { 0, 0 }, // 4a
    { 0, 0 }, // 4b
    { 0, 0 }, // 4c
    { 0, 0 }, // 4d
    { 0, 0 }, // 4e
    { 0, 0 }, // 4f
    { 0, 0 }, // 50
    { 0, 0 }, // 51
    { 0, 0 }, // 52
    { 0, 0 }, // 53
    { 0, 0 }, // 54
    { 0, 0 }, // 55
    { 0, 0 }, // 56
    { 0, 0 }, // 57
    { 0, 0 }, // 58
    { 0, 0 }, // 59
    { 0, 0 }, // 5a
    { 0, 0 }, // 5b
    { 0, 0 }, // 5c
    { 0, 0 }, // 5d
    { 0, 0 }, // 5e
    { 0, 0 }, // 5f
    { 0, 0 }, // 60
    { 0, 0 }, // 61
    { 0, 0 }, // 62
    { 0, 0 }, // 63
    { 0, 0 }, // 64
    { 0, 0 }, // 65
    { 0, 0 }, // 66
    { 0, 0 }, // 67
    { 0, 0 }, // 68
    { 0, 0 }, // 69
    { 0, 0 }, // 6a
    { 0, 0 }, // 6b
    { 0, 0 }, // 6c
    { 0, 0 }, // 6d
    { 0, 0 }, // 6e
    { 0, 0 }, // 6f
    { 0, 0 }, // 70
    { 0, 0 }, // 71
    { 0, clif_parse_WantToConnection }, // 72
    { 0, 0 }, // 73
    { 0, 0 }, // 74
    { 0, 0 }, // 75
    { 0, 0 }, // 76
    { 0, 0 }, // 77
    { 0, 0 }, // 78
    { 0, 0 }, // 79
    { 0, 0 }, // 7a
    { 0, 0 }, // 7b
    { 0, 0 }, // 7c
    { -1, clif_parse_LoadEndAck }, // 7d
    { 0, 0 }, // 7e
    { 0, 0 }, // 7f
    { 0, 0 }, // 80
    { 0, 0 }, // 81
    { 0, 0 }, // 82
    { 0, 0 }, // 83
    { 0, 0 }, // 84
    { -1, clif_parse_WalkToXY }, // 85 Walk code limits this on it's own
    { 0, 0 }, // 86
    { 0, 0 }, // 87
    { 0, 0 }, // 88
    { 1000, clif_parse_ActionRequest }, // 89 Special case - see below
    { 0, 0 }, // 8a
    { 0, 0 }, // 8b
    { 300, clif_parse_GlobalMessage }, // 8c
    { 0, 0 }, // 8d
    { 0, 0 }, // 8e
    { 0, 0 }, // 8f
    { 500, clif_parse_NpcClicked }, // 90
    { 0, 0 }, // 91
    { 0, 0 }, // 92
    { 0, 0 }, // 93
    { -1, clif_parse_GetCharNameRequest }, // 94
    { 0, 0 }, // 95
    { 300, clif_parse_Wis }, // 96
    { 0, 0 }, // 97
    { 0, 0 }, // 98
    { 0, 0 }, // 99
    { 0, 0 }, // 9a
    { -1, clif_parse_ChangeDir }, // 9b
    { 0, 0 }, // 9c
    { 0, 0 }, // 9d
    { 0, 0 }, // 9e
    { 400, clif_parse_TakeItem }, // 9f
    { 0, 0 }, // a0
    { 0, 0 }, // a1
    { 50, clif_parse_DropItem }, // a2
    { 0, 0 }, // a3
    { 0, 0 }, // a4
    { 0, 0 }, // a5
    { 0, 0 }, // a6
    { 0, clif_parse_UseItem }, // a7
    { 0, 0 }, // a8
    { -1, clif_parse_EquipItem }, // a9 Special case - outfit window (not implemented yet - needs to allow bursts)
    { 0, 0 }, // aa
    { -1, clif_parse_UnequipItem }, // ab Special case - outfit window (not implemented yet - needs to allow bursts)
    { 0, 0 }, // ac
    { 0, 0 }, // ad
    { 0, 0 }, // ae
    { 0, 0 }, // af
    { 0, 0 }, // b0
    { 0, 0 }, // b1
    { 0, clif_parse_Restart }, // b2
    { 0, 0 }, // b3
    { 0, 0 }, // b4
    { 0, 0 }, // b5
    { 0, 0 }, // b6
    { 0, 0 }, // b7
    { 0, clif_parse_NpcSelectMenu }, // b8
    { -1, clif_parse_NpcNextClicked }, // b9
    { 0, 0 }, // ba
    { -1, clif_parse_StatusUp }, // bb People click this very quickly
    { 0, 0 }, // bc
    { 0, 0 }, // bd
    { 0, 0 }, // be
    { 1000, clif_parse_Emotion }, // bf
    { 0, 0 }, // c0
    { 0, clif_parse_HowManyConnections }, // c1
    { 0, 0 }, // c2
    { 0, 0 }, // c3
    { 0, 0 }, // c4
    { 0, clif_parse_NpcBuySellSelected }, // c5
    { 0, 0 }, // c6
    { 0, 0 }, // c7
    { -1, clif_parse_NpcBuyListSend }, // c8
    { -1, clif_parse_NpcSellListSend }, // c9 Selling multiple 1-slot items
    { 0, 0 }, // ca
    { 0, 0 }, // cb
    { 0, 0 }, // cc
    { 0, 0 }, // cd
    { 0, 0 }, // ce
    { 0, 0 }, // cf
    { 0, 0 }, // d0
    { 0, 0 }, // d1
    { 0, 0 }, // d2
    { 0, 0 }, // d3
    { 0, 0 }, // d4
    { 0, 0 }, // d5
    { 0, 0 }, // d6
    { 0, 0 }, // d7
    { 0, 0 }, // d8
    { 0, 0 }, // d9
    { 0, 0 }, // da
    { 0, 0 }, // db
    { 0, 0 }, // dc
    { 0, 0 }, // dd
    { 0, 0 }, // de
    { 0, 0 }, // df
    { 0, 0 }, // e0
    { 0, 0 }, // e1
    { 0, 0 }, // e2
    { 0, 0 }, // e3
    { 2000, clif_parse_TradeRequest }, // e4
    { 0, 0 }, // e5
    { 0, clif_parse_TradeAck }, // e6
    { 0, 0 }, // e7
    { 0, clif_parse_TradeAddItem }, // e8
    { 0, 0 }, // e9
    { 0, 0 }, // ea
    { 0, clif_parse_TradeOk }, // eb
    { 0, 0 }, // ec
    { 0, clif_parse_TradeCansel }, // ed
    { 0, 0 }, // ee
    { 0, clif_parse_TradeCommit }, // ef
    { 0, 0 }, // f0
    { 0, 0 }, // f1
    { 0, 0 }, // f2
    { -1, clif_parse_MoveToKafra }, // f3
    { 0, 0 }, // f4
    { -1, clif_parse_MoveFromKafra }, // f5
    { 0, 0 }, // f6
    { 0, clif_parse_CloseKafra }, // f7
    { 0, 0 }, // f8
    { 2000, clif_parse_CreateParty }, // f9
    { 0, 0 }, // fa
    { 0, 0 }, // fb
    { 2000, clif_parse_PartyInvite }, // fc
    { 0, 0 }, // fd
    { 0, 0 }, // fe
    { 0, clif_parse_ReplyPartyInvite }, // ff
    { 0, clif_parse_LeaveParty }, // 100
    { 0, 0 }, // 101
    { 0, clif_parse_PartyChangeOption }, // 102
    { 0, clif_parse_RemovePartyMember }, // 103
    { 0, 0 }, // 104
    { 0, 0 }, // 105
    { 0, 0 }, // 106
    { 0, 0 }, // 107
    { 300, clif_parse_PartyMessage }, // 108
    { 0, 0 }, // 109
    { 0, 0 }, // 10a
    { 0, 0 }, // 10b
    { 0, 0 }, // 10c
    { 0, 0 }, // 10d
    { 0, 0 }, // 10e
    { 0, 0 }, // 10f
    { 0, 0 }, // 110
    { 0, 0 }, // 111
    { -1, clif_parse_SkillUp }, // 112
    { 0, 0 }, // 113
    { 0, 0 }, // 114
    { 0, 0 }, // 115
    { 0, 0 }, // 116
    { 0, 0 }, // 117
    { 0, 0 }, // 118
    { 0, 0 }, // 119
    { 0, 0 }, // 11a
    { 0, 0 }, // 11b
    { 0, 0 }, // 11c
    { 0, 0 }, // 11d
    { 0, 0 }, // 11e
    { 0, 0 }, // 11f
    { 0, 0 }, // 120
    { 0, 0 }, // 121
    { 0, 0 }, // 122
    { 0, 0 }, // 123
    { 0, 0 }, // 124
    { 0, 0 }, // 125
    { 0, 0 }, // 126
    { 0, 0 }, // 127
    { 0, 0 }, // 128
    { 0, 0 }, // 129
    { 0, 0 }, // 12a
    { 0, 0 }, // 12b
    { 0, 0 }, // 12c
    { 0, 0 }, // 12d
    { 0, 0 }, // 12e
    { 0, 0 }, // 12f
    { 0, 0 }, // 130
    { 0, 0 }, // 131
    { 0, 0 }, // 132
    { 0, 0 }, // 133
    { 0, 0 }, // 134
    { 0, 0 }, // 135
    { 0, 0 }, // 136
    { 0, 0 }, // 137
    { 0, 0 }, // 138
    { 0, 0 }, // 139
    { 0, 0 }, // 13a
    { 0, 0 }, // 13b
    { 0, 0 }, // 13c
    { 0, 0 }, // 13d
    { 0, 0 }, // 13e
    { 0, 0 }, // 13f
    { 0, 0 }, // 140
    { 0, 0 }, // 141
    { 0, 0 }, // 142
    { 300, clif_parse_NpcAmountInput }, // 143
    { 0, 0 }, // 144
    { 0, 0 }, // 145
    { 300, clif_parse_NpcCloseClicked }, // 146
    { 0, 0 }, // 147
    { 0, 0 }, // 148
    { 0, 0 }, // 149
    { 0, 0 }, // 14a
    { 0, 0 }, // 14b
    { 0, 0 }, // 14c
    { 0, 0 }, // 14d
    { 0, 0 }, // 14e
    { 0, 0 }, // 14f
    { 0, 0 }, // 150
    { 0, 0 }, // 151
    { 0, 0 }, // 152
    { 0, 0 }, // 153
    { 0, 0 }, // 154
    { 0, 0 }, // 155
    { 0, 0 }, // 156
    { 0, 0 }, // 157
    { 0, 0 }, // 158
    { 0, 0 }, // 159
    { 0, 0 }, // 15a
    { 0, 0 }, // 15b
    { 0, 0 }, // 15c
    { 0, 0 }, // 15d
    { 0, 0 }, // 15e
    { 0, 0 }, // 15f
    { 0, 0 }, // 160
    { 0, 0 }, // 161
    { 0, 0 }, // 162
    { 0, 0 }, // 163
    { 0, 0 }, // 164
    { 0, 0 }, // 165
    { 0, 0 }, // 166
    { 0, 0 }, // 167
    { 0, 0 }, // 168
    { 0, 0 }, // 169
    { 0, 0 }, // 16a
    { 0, 0 }, // 16b
    { 0, 0 }, // 16c
    { 0, 0 }, // 16d
    { 0, 0 }, // 16e
    { 0, 0 }, // 16f
    { 0, 0 }, // 170
    { 0, 0 }, // 171
    { 0, 0 }, // 172
    { 0, 0 }, // 173
    { 0, 0 }, // 174
    { 0, 0 }, // 175
    { 0, 0 }, // 176
    { 0, 0 }, // 177
    { 0, 0 }, // 178
    { 0, 0 }, // 179
    { 0, 0 }, // 17a
    { 0, 0 }, // 17b
    { 0, 0 }, // 17c
    { 0, 0 }, // 17d
    { 0, 0 }, // 17e
    { 0, 0 }, // 17f
    { 0, 0 }, // 180
    { 0, 0 }, // 181
    { 0, 0 }, // 182
    { 0, 0 }, // 183
    { 0, 0 }, // 184
    { 0, 0 }, // 185
    { 0, 0 }, // 186
    { 0, 0 }, // 187
    { 0, 0 }, // 188
    { 0, 0 }, // 189
    { 0, clif_parse_QuitGame }, // 18a
    { 0, 0 }, // 18b
    { 0, 0 }, // 18c
    { 0, 0 }, // 18d
    { 0, 0 }, // 18e
    { 0, 0 }, // 18f
    { 0, 0 }, // 190
    { 0, 0 }, // 191
    { 0, 0 }, // 192
    { 0, 0 }, // 193
    { 0, 0 }, // 194
    { 0, 0 }, // 195
    { 0, 0 }, // 196
    { 0, 0 }, // 197
    { 0, 0 }, // 198
    { 0, 0 }, // 199
    { 0, 0 }, // 19a
    { 0, 0 }, // 19b
    { 0, 0 }, // 19c
    { 0, 0 }, // 19d
    { 0, 0 }, // 19e
    { 0, 0 }, // 19f
    { 0, 0 }, // 1a0
    { 0, 0 }, // 1a1
    { 0, 0 }, // 1a2
    { 0, 0 }, // 1a3
    { 0, 0 }, // 1a4
    { 0, 0 }, // 1a5
    { 0, 0 }, // 1a6
    { 0, 0 }, // 1a7
    { 0, 0 }, // 1a8
    { 0, 0 }, // 1a9
    { 0, 0 }, // 1aa
    { 0, 0 }, // 1ab
    { 0, 0 }, // 1ac
    { 0, 0 }, // 1ad
    { 0, 0 }, // 1ae
    { 0, 0 }, // 1af
    { 0, 0 }, // 1b0
    { 0, 0 }, // 1b1
    { 0, 0 }, // 1b2
    { 0, 0 }, // 1b3
    { 0, 0 }, // 1b4
    { 0, 0 }, // 1b5
    { 0, 0 }, // 1b6
    { 0, 0 }, // 1b7
    { 0, 0 }, // 1b8
    { 0, 0 }, // 1b9
    { 0, 0 }, // 1ba
    { 0, 0 }, // 1bb
    { 0, 0 }, // 1bc
    { 0, 0 }, // 1bd
    { 0, 0 }, // 1be
    { 0, 0 }, // 1bf
    { 0, 0 }, // 1c0
    { 0, 0 }, // 1c1
    { 0, 0 }, // 1c2
    { 0, 0 }, // 1c3
    { 0, 0 }, // 1c4
    { 0, 0 }, // 1c5
    { 0, 0 }, // 1c6
    { 0, 0 }, // 1c7
    { 0, 0 }, // 1c8
    { 0, 0 }, // 1c9
    { 0, 0 }, // 1ca
    { 0, 0 }, // 1cb
    { 0, 0 }, // 1cc
    { 0, 0 }, // 1cd
    { 0, 0 }, // 1ce
    { 0, 0 }, // 1cf
    { 0, 0 }, // 1d0
    { 0, 0 }, // 1d1
    { 0, 0 }, // 1d2
    { 0, 0 }, // 1d3
    { 0, 0 }, // 1d4
    { 300, clif_parse_NpcStringInput }, // 1d5
    { 0, 0 }, // 1d6
    { 0, 0 }, // 1d7
    { 0, 0 }, // 1d8
    { 0, 0 }, // 1d9
    { 0, 0 }, // 1da
    { 0, 0 }, // 1db
    { 0, 0 }, // 1dc
    { 0, 0 }, // 1dd
    { 0, 0 }, // 1de
    { 0, 0 }, // 1df
    { 0, 0 }, // 1e0
    { 0, 0 }, // 1e1
    { 0, 0 }, // 1e2
    { 0, 0 }, // 1e3
    { 0, 0 }, // 1e4
    { 0, 0 }, // 1e5
    { 0, 0 }, // 1e6
    { 0, 0 }, // 1e7
    { 0, 0 }, // 1e8
    { 0, 0 }, // 1e9
    { 0, 0 }, // 1ea
    { 0, 0 }, // 1eb
    { 0, 0 }, // 1ec
    { 0, 0 }, // 1ed
    { 0, 0 }, // 1ee
    { 0, 0 }, // 1ef
    { 0, 0 }, // 1f0
    { 0, 0 }, // 1f1
    { 0, 0 }, // 1f2
    { 0, 0 }, // 1f3
    { 0, 0 }, // 1f4
    { 0, 0 }, // 1f5
    { 0, 0 }, // 1f6
    { 0, 0 }, // 1f7
    { 0, 0 }, // 1f8
    { 0, 0 }, // 1f9
    { 0, 0 }, // 1fa
    { 0, 0 }, // 1fb
    { 0, 0 }, // 1fc
    { 0, 0 }, // 1fd
    { 0, 0 }, // 1fe
    { 0, 0 }, // 1ff
    { 0, 0 }, // 200
    { 0, 0 }, // 201
    { 0, 0 }, // 202
    { 0, 0 }, // 203
    { 0, 0 }, // 204
    { 0, 0 }, // 205
    { 0, 0 }, // 206
    { 0, 0 }, // 207
    { 0, 0 }, // 208
    { 0, 0 }, // 209
    { 0, 0 }, // 20a
    { 0, 0 }, // 20b
    { 0, 0 }, // 20c
    { 0, 0 }, // 20d
    { 0, 0 }, // 20e
    { 0, 0 }, // 20f
    { 0, 0 }, // 210
    { 0, 0 }, // 211
    { 0, 0 }, // 212
    { 0, 0 }, // 213
    { 0, 0 }, // 214
    { 0, 0 }, // 215
    { 0, 0 }, // 216
    { 0, 0 }, // 217
    { 0, 0 }, // 218
    { 0, 0 }, // 219
    { 0, 0 }, // 21a
    { 0, 0 }, // 21b
    { 0, 0 }, // 21c
    { 0, 0 }, // 21d
    { 0, 0 }, // 21e
    { 0, 0 }, // 21f
};

// Checks for packet flooding
static bool clif_check_packet_flood(int fd, int cmd)
{
    MapSessionData *sd = static_cast<MapSessionData *>(session[fd]->session_data);
    unsigned int rate, tick = gettick();

    // sd will not be set if the client hasn't requested
    // WantToConnection yet. Do not apply flood logic to GMs
    // as approved bots (GMlvl1) should not have to work around
    // flood logic.
    if (!sd || pc_isGM(sd) || clif_parse_func_table[cmd].rate == -1)
        return 0;

    // Timer has wrapped
    if (tick < sd->flood_rates[cmd])
    {
        sd->flood_rates[cmd] = tick;
        return 0;
    }

    // Default rate is 100ms
    if ((rate = clif_parse_func_table[cmd].rate) == 0)
        rate = 100;

    // ActionRequest - attacks are allowed a faster rate than sit/stand
    if (cmd == 0x89)
    {
        int action_type = RFIFOB(fd, 6);
        if (action_type == 0x00 || action_type == 0x07)
            rate = 20;
        else
            rate = 1000;
    }

// Restore this code when mana1.0 is released
#if 0
    // ChangeDir - only apply limit if not walking
    if (cmd == 0x9b)
    {
        // .29 clients spam this packet when walking into a blocked tile
        if (RFIFOB(fd, 4) == sd->dir || sd->walktimer != -1)
            return 0;

        rate = 500;
    }
#endif

    // They are flooding
    if (tick < sd->flood_rates[cmd] + rate)
    {
        time_t now = time(NULL);

        // If it's a nasty flood we log and possibly kick
        if (now > sd->packet_flood_reset_due)
        {
            sd->packet_flood_reset_due = now + battle_config.packet_spam_threshold;
            sd->packet_flood_in = 0;
        }

        sd->packet_flood_in++;

        if (sd->packet_flood_in >= battle_config.packet_spam_flood)
        {
            printf("packet flood detected from %s [0x%x]\n", sd->status.name, cmd);
            if (battle_config.packet_spam_kick)
            {
                session[fd]->eof = 1; // Kick
                return 1;
            }
            sd->packet_flood_in = 0;
        }

        return 1;
    }

    sd->flood_rates[cmd] = tick;
    return 0;
}

#define WARN_MALFORMED_MSG(sd, msg)                             \
    printf("clif_validate_chat(): %s (ID %d) sent a malformed" \
            " message: %s.\n", sd->status.name, sd->status.account_id, msg)
/**
 * Validate message integrity(inspired by upstream source(eAthena)).
 *
 * @param sd active session data
 * @param type message type:
 *  0 for when the sender's name is not included(party chat)
 *  1 for when the target's name is included(whisper chat)
 *  2 for when the sender's name is given("sender : text", public/guild chat)
 * @param[out] message the message text(pointing within return value, or NULL)
 * @param[out] message_len the length of the actual text, excluding NUL
 * @return a dynamically allocated copy of the message, or NULL upon failure
 */
static uint8_t *clif_validate_chat(MapSessionData *sd, int type,
                                 char **message, size_t *message_len)
{
    int fd;
    unsigned int buf_len;       /* Actual message length. */
    unsigned int msg_len;       /* Reported message length. */
    unsigned int min_len;       /* Minimum message length. */
    size_t name_len;            /* Sender's name length. */
    uint8_t *buf = NULL;           /* Copy of actual message data. */

    *message = NULL;
    *message_len = 0;

    nullpo_retr(NULL, sd);
    /*
     * Don't send chat in the period between the ban and the connection's
     * closure.
     */
    if (type < 0 || type > 2 || sd->auto_ban_info.in_progress)
        return NULL;

    fd = sd->fd;
    msg_len = RFIFOW(fd, 2) - 4;
    name_len = strlen(sd->status.name);
    /*
     * At least one character is required in all instances.
     * Notes for length checks:
     *
     * For all types, header(2) + length(2) is considered empty.
     * For type 1, the message must be longer than the maximum name length(24)
     *      to be valid.
     * For type 2, the message must be longer than the sender's name length
     *      plus the length of the separator(" : ").
     */
    min_len = (type == 1) ? 24 : (type == 2) ? name_len + 3 : 0;

    /* The player just sent the header (2) and length (2) words. */
    if (!msg_len)
    {
        WARN_MALFORMED_MSG(sd, "no message sent");
        return NULL;
    }

    /* The client sent (or claims to have sent) an empty message. */
    if (msg_len == min_len)
    {
        WARN_MALFORMED_MSG(sd, "empty message");
        return NULL;
    }

    /* The protocol specifies that the target must be 24 bytes long. */
    if (type == 1 && msg_len < min_len)
    {
        /* Disallow malformed messages. */
        clif_setwaitclose(fd);
        WARN_MALFORMED_MSG(sd, "illegal target name");
        return NULL;
    }

    const char *cp = sign_cast<const char *>((type != 1) ? RFIFOP(fd, 4) : RFIFOP(fd, 28));
    buf_len = (type == 1) ? msg_len - min_len: msg_len;

    /*
     * The client attempted to exceed the maximum message length.
     *
     * The conf suggests up to chat_maxline characters, after which the message
     * is truncated. But the previous behavior was to drop the message, so
     * we'll do that, too.
     */
    if (buf_len >= battle_config.chat_maxline)
    {
        WARN_MALFORMED_MSG(sd, "exceeded maximum message length");
        return NULL;
    }

    /* We're leaving an extra eight bytes for public/global chat, 1 for NUL. */
    buf_len += (type == 2) ? 8 + 1 : 1;

    CREATE(buf, uint8_t, buf_len);
    memcpy((type != 2) ? buf : buf + 8, cp,
            (type != 2) ? buf_len - 1 : buf_len - 8 - 1);
    buf[buf_len - 1] = '\0';
    char *p = sign_cast<char *>((type != 2) ? buf : buf + 8);

    if (type != 2)
    {
        *message = sign_cast<char *>(buf);
        /* Don't count the NUL. */
        *message_len = buf_len - 1;
    }
    else
    {
        char *pos = NULL;
        if (!(pos = strstr(p, " : "))
                || strncmp(p, sd->status.name, name_len)
                || pos - p != name_len)
        {
            free(buf);
            /* Disallow malformed/spoofed messages. */
            clif_setwaitclose(fd);
            WARN_MALFORMED_MSG(sd, "spoofed name/invalid format");
            return NULL;
        }
        /* Step beyond the separator. */
        *message = pos + 3;
        /* Don't count the sender's name, the extra eight bytes, or the NUL. */
        *message_len = buf_len - min_len - 8 - 1;
    }

    return buf;
}

/*==========================================
 * クライアントからのパケット解析
 * socket.cのdo_parsepacketから呼び出される
 *------------------------------------------
 */
static void clif_parse(int fd)
{
    int packet_len = 0, cmd = 0;
    MapSessionData *sd = static_cast<MapSessionData *>(session[fd]->session_data);

    if (!sd || (sd && !sd->state.auth))
    {
        if (RFIFOREST(fd) < 2)
        {                       // too small a packet disconnect
            session[fd]->eof = 1;
        }
        if (RFIFOW(fd, 0) != 0x72)
        {                       // first packet not auth, disconnect
            session[fd]->eof = 1;
        }
    }

    // 接続が切れてるので後始末
    if (!chrif_isconnect() || session[fd]->eof)
    {                           // char鯖に繋がってない間は接続禁止 (!chrif_isconnect())
        if (sd && sd->state.auth)
        {
            pc_logout(sd);
            map_quit(sd);
            if (sd->status.name != NULL)
                printf("Player [%s] has logged off your server.\n", sd->status.name);  // Player logout display [Valaris]
            else
                printf("Player with account [%d] has logged off your server.\n", sd->id);   // Player logout display [Yor]
        }
        else if (sd)
        {                       // not authentified! (refused by char-server or disconnect before to be authentified)
            printf("Player with account [%d] has logged off your server (not auth account).\n", sd->id);    // Player logout display [Yor]
            map_deliddb(sd);  // account_id has been included in the DB before auth answer
        }
        if (fd)
            close(fd);
        if (fd)
            delete_session(fd);
        return;
    }

    if (RFIFOREST(fd) < 2)
        return;               // Too small (no packet number)

    cmd = RFIFOW(fd, 0);

    if (cmd == 0x7530)
    {
            WFIFOW(fd, 0) = 0x7531;
            memcpy(WFIFOP(fd, 2), &tmwAthenaVersion, 8);
            // WFIFOB(fd, 6) = 0;
            WFIFOB(fd, 7) = ATHENA_SERVER_MAP;
            WFIFOSET(fd, 10);
            RFIFOSKIP(fd, 2);
            return;
    }
    if (cmd >= ARRAY_SIZEOF(packet_len_table))
    {
        session[fd]->eof = 1;
        return;
    }
    // パケット長を計算
    packet_len = packet_len_table[cmd];
    if (packet_len == -1)
    {
        if (RFIFOREST(fd) < 4)
        {
            return;           // Runt packet (variable length without a length sent)
        }
        packet_len = RFIFOW(fd, 2);
        if (packet_len < 4 || packet_len > 32768)
        {
            session[fd]->eof = 1;
            return;           // Runt packet (variable out of bounds)
        }
    }

    if (RFIFOREST(fd) < packet_len)
    {
        return;               // Runt packet (sent legnth is too small)
    }

    if (sd && sd->state.auth == 1 && sd->state.waitingdisconnect == 1)
    {                           // 切断待ちの場合パケットを処理しない

    }
    else if (clif_parse_func_table[cmd].func)
    {
        if (clif_check_packet_flood(fd, cmd))
        {
            // Flood triggered. Skip packet.
            RFIFOSKIP(sd->fd, packet_len);
            return;
        }

        clif_parse_func_table[cmd].func(fd, sd);
    }
    else
    {
        {
            if (fd)
                printf("\nclif_parse: session #%d, packet 0x%x, lenght %d\n",
                        fd, cmd, packet_len);
            {
                int i;
                FILE *fp;
                char packet_txt[256] = "save/packet.txt";
                time_t now;
                printf
                    ("---- 00-01-02-03-04-05-06-07-08-09-0A-0B-0C-0D-0E-0F");
                for (i = 0; i < packet_len; i++)
                {
                    if ((i & 15) == 0)
                        printf("\n%04X ", i);
                    printf("%02X ", RFIFOB(fd, i));
                }
                if (sd && sd->state.auth)
                {
                    if (sd->status.name != NULL)
                        printf
                            ("\nAccount ID %d, character ID %d, player name %s.\n",
                             sd->status.account_id, sd->status.char_id,
                             sd->status.name);
                    else
                        printf("\nAccount ID %d.\n", sd->id);
                }
                else if (sd)    // not authentified! (refused by char-server or disconnect before to be authentified)
                    printf("\nAccount ID %d.\n", sd->id);

                if ((fp = fopen_(packet_txt, "a")) == NULL)
                {
                    printf("clif.c: cant write [%s] !!! data is lost !!!\n",
                            packet_txt);
                    return;
                }
                else
                {
                    time(&now);
                    if (sd && sd->state.auth)
                    {
                        if (sd->status.name != NULL)
                            fprintf(fp,
                                     "%sPlayer with account ID %d (character ID %d, player name %s) sent wrong packet:\n",
                                     asctime(gmtime(&now)),
                                     sd->status.account_id,
                                     sd->status.char_id, sd->status.name);
                        else
                            fprintf(fp,
                                     "%sPlayer with account ID %d sent wrong packet:\n",
                                     asctime(gmtime(&now)), sd->id);
                    }
                    else if (sd)    // not authentified! (refused by char-server or disconnect before to be authentified)
                        fprintf(fp,
                                 "%sPlayer with account ID %d sent wrong packet:\n",
                                 asctime(gmtime(&now)), sd->id);

                    fprintf(fp,
                             "\t---- 00-01-02-03-04-05-06-07-08-09-0A-0B-0C-0D-0E-0F");
                    for (i = 0; i < packet_len; i++)
                    {
                        if ((i & 15) == 0)
                            fprintf(fp, "\n\t%04X ", i);
                        fprintf(fp, "%02X ", RFIFOB(fd, i));
                    }
                    fprintf(fp, "\n\n");
                    fclose_(fp);
                }
            }
        }
    }
    RFIFOSKIP(fd, packet_len);
}

/*==========================================
 *
 *------------------------------------------
 */
void do_init_clif(void)
{
    set_defaultparse(clif_parse);
    for (int i = 0; i < 10; i++)
    {
        if (make_listen_port(map_port))
            return;
        sleep(20);
    }
    map_log("can't bind port %hu\n", map_port);
    exit(1);
}
