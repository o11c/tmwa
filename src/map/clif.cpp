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
#include "main.hpp"
#include "npc.hpp"
#include "party.hpp"
#include "pc.hpp"
#include "skill.hpp"
#include "storage.hpp"
#include "tmw.hpp"
#include "trade.hpp"

template class std::vector<std::string>;

#define STATE_BLIND 0x10
#define EMOTE_IGNORED 0x0e

static void clif_changelook_towards(BlockList *, LOOK, sint32, MapSessionData *dst);
static void clif_sitting(sint32 fd, MapSessionData *sd);
static void clif_itemlist(MapSessionData *sd);
static void clif_GM_kickack(MapSessionData *sd, account_t id);

typedef sint32 packet_len_t;
constexpr packet_len_t VAR = -1;
/// I would really like to delete this table
// most of them are not used and such
static const packet_len_t packet_len_table[0x220] =
{
//#0x0000
     10,  0,  0,  0,
      0,  0,  0,  0,
      0,  0,  0,  0,
      0,  0,  0,  0,
//#0x0010
      0,  0,  0,  0,
      0,  0,  0,  0,
      0,  0,  0,  0,
      0,  0,  0,  0,
//#0x0020
      0,  0,  0,  0,
      0,  0,  0,  0,
      0,  0,  0,  0,
      0,  0,  0,  0,
//#0x0030
      0,  0,  0,  0,
      0,  0,  0,  0,
      0,  0,  0,  0,
      0,  0,  0,  0,

//#0x0040
      0,  0,  0,  0,
      0,  0,  0,  0,
      0,  0,  0,  0,
      0,  0,  0,  0,
//#0x0050
      0,  0,  0,  0,
      0,  0,  0,  0,
      0,  0,  0,  0,
      0,  0,  0,  0,
//#0x0060
      0,  0,  0,VAR,
     55, 17,  3, 37,
     46,VAR, 23,VAR,
      3,108,  3,  2,
//#0x0070
      3, 28, 19, 11,
      3,VAR,  9,  5,
     54, 53, 58, 60,
     41,  2,  6,  6,

//#0x0080
    // 0x8b unknown... size 2 or 23?
      7,  3,  2,  2,
      2,  5, 16, 12,
     10,  7, 29, 23,
    VAR,VAR,VAR,  0,
//#0x0090
      7, 22, 28,  2,
      6, 30,VAR,VAR,
      3,VAR,VAR,  5,
      9, 17, 17,  6,
//#0x00A0
     23,  6,  6,VAR,
    VAR,VAR,VAR,  8,
      7,  6,  7,  4,
      7,  0,VAR,  6,
//#0x00B0
      8,  8,  3,  3,
    VAR,  6,  6,VAR,
      7,  6,  2,  5,
      6, 44,  5,  3,

//#0x00C0
      7,  2,  6,  8,
      6,  7,VAR,VAR,
    VAR,VAR,  3,  3,
      6,  6,  2, 27,
//#0x00D0
      3,  4,  4,  2,
    VAR,VAR,  3,VAR,
      6, 14,  3,VAR,
     28, 29,VAR,VAR,
//#0x00E0
     30, 30, 26,  2,
      6, 26,  3,  3,
      8, 19,  5,  2,
      3,  2,  2,  2,
//#0x00F0
      3,  2,  6,  8,
     21,  8,  8,  2,
      2, 26,  3,VAR,
      6, 27, 30, 10,

//#0x0100
      2,  6,  6, 30,
     79, 31, 10, 10,
    VAR,VAR,  4,  6,
      6,  2, 11,VAR,
//#0x0110
     10, 39,  4, 10,
     31, 35, 10, 18,
      2, 13, 15, 20,
     68,  2,  3, 16,
//#0x0120
      6, 14,VAR,VAR,
     21,  8,  8,  8,
      8,  8,  2,  2,
      3,  4,  2,VAR,
//#0x0130
      6, 86,  6,VAR,
    VAR,  7,VAR,  6,
      3, 16,  4,  4,
      4,  6, 24, 26,

//#0x0140
     22, 14,  6, 10,
     23, 19,  6, 39,
      8,  9,  6, 27,
    VAR,  2,  6,  6,
//#0x0150
    110,  6,VAR,VAR,
    VAR,VAR,VAR,  6,
    VAR, 54, 66, 54,
     90, 42,  6, 42,
//#0x0160
    VAR,VAR,VAR,VAR,
    VAR, 30,VAR,  3,
     14,  3, 30, 10,
     43, 14,186,182,
//#0x0170
     14, 30, 10,  3,
    VAR,  6,106,VAR,
      4,  5,  4,VAR,
      6,  7,VAR,VAR,

//#0x0180
      6,  3,106, 10,
     10, 34,  0,  6,
      8,  4,  4,  4,
     29,VAR, 10,  6,
//#0x0190
     90, 86, 24,  6,
     30,102,  9,  4,
      8,  4, 14, 10,
    VAR,  6,  2,  6,
//#0x01A0
      3,  3, 35,  5,
     11, 26,VAR,  4,
      4,  6, 10, 12,
      6,VAR,  4,  4,
//#0x01B0
     11,  7,VAR, 67,
     12, 18,114,  6,
      3,  6, 26, 26,
     26, 26,  2,  3,

//#0x01C0
      2, 14, 10,VAR,
     22, 22,  4,  2,
     13, 97,  0,  9,
      9, 30,  6, 28,
//#0x01D0
    // Set 0x1d5=-1
      8, 14, 10, 35,
      6,VAR,  4, 11,
     54, 53, 60,  2,
    VAR, 47, 33,  6,
//#0x01E0
     30,  8, 34, 14,
      2,  6, 26,  2,
     28, 81,  6, 10,
     26,  2,VAR,VAR,
//#0x01F0
    VAR,VAR, 20, 10,
     32,  9, 34, 14,
      2,  6, 48, 56,
    VAR,  4,  5, 10,

//#0x0200
     26,VAR, 26, 10,
     18, 26, 11, 34,
     14, 36, 10, 19,
     10,VAR, 24,  0,
//#0x0210
      0,  0,  0,  0,
      0,  0,  0,  0,
      0,  0,  0,  0,
      0,  0,  0,  0,
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

static void WBUFPOS(uint8 *p, size_t pos, uint16 x, uint16 y, Direction d)
{
    // x = aaaa aaaa bbbb bbbb
    // y = cccc cccc dddd dddd
    p += pos;
    // aabb bbbb
    p[0] = x >> 2;
    // bb00 0000 | (cccc dddd & 0011 1111)
    p[1] = (x << 6) | ((y >> 4) & 0x3f);
    //
    p[2] = (y << 4) | (static_cast<sint32>(d) & 0xF);
}

static void WBUFPOS2(uint8 *p, size_t pos, uint16 x_0, uint16 y_0, uint16 x_1, uint16 y_1)
{
    // x_0 = aaaa aaaa bbbb bbbb
    // y_0 = cccc cccc dddd dddd
    // x_1 = eeee eeee ffff ffff
    // y_1 = gggg gggg hhhh hhhh
    p += pos;
    // aabb bbbb
    p[0] =  x_0 >> 2;
    // bb00 0000 | (cccc dddd & 0011 1111)
    p[1] = (x_0 << 6) | ((y_0 >> 4) & 0x3f);
    // dddd 0000 | (eeee eeff & 0000 1111)
    p[2] = (y_0 << 4) | ((x_1 >> 6) & 0x0f);
    // ffff ff00 | (gggg gggg & 0000 0011)
    p[3] = (x_1 << 2) | ((y_1 >> 8) & 0x03);
    // hhhh hhhh
    p[4] = y_1;
}

static void WFIFOPOS(sint32 fd, size_t pos, uint16 x, uint16 y, Direction d)
{
    WBUFPOS(WFIFOP(fd, 0), pos, x, y, d);
}

static void WFIFOPOS2(sint32 fd, size_t pos, uint16 x_0, uint16 y_0, uint16 x_1, uint16 y_1)
{
    WBUFPOS2(WFIFOP(fd, 0), pos, x_0, y_0, x_1, y_1);
}

static IP_Address map_ip;
static in_port_t map_port = 5121;
sint32 map_fd;

/// Our public IP
void clif_setip(IP_Address ip)
{
    map_ip = ip;
}

/// The port on which we listen
void clif_setport(uint16 port)
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
uint32 clif_countusers(void)
{
    uint32 users = 0;
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

static uint8 *clif_validate_chat(MapSessionData *sd, sint32 type,
                                   char **message, size_t *message_len);

/// Subfunction that checks if the given target is included in the Whom
// (only called for PCs in the area)
static void clif_send_sub(BlockList *bl,
                          uint8 *buf,
                          uint16 len,
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
static void clif_send(uint8 *buf, uint16 len, BlockList *bl, Whom type)
{
    // Validate packet
    if (!buf)
        abort();
    if (bl && bl->type == BL_PC && WFIFOP(static_cast<MapSessionData *>(bl)->fd, 0) == buf)
        abort();
    if (len < 2)
        abort();
    packet_len_t packet_len = packet_len_table[RBUFW(buf, 0)];
    if (packet_len == VAR)
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
            if (sd->status.option & OPTION::INVISIBILITY)
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
    sint32 x_0 = 0, x_1 = 0, y_0 = 0, y_1 = 0;

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
            if (sd->status.party_id)
                p = party_search(sd->status.party_id);
        }
        if (p)
        {
            for (sint32 i = 0; i < MAX_PARTY; i++)
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

    sint32 fd = sd->fd;

    WFIFOW(fd, 0) = 0x73;
    WFIFOL(fd, 2) = std::chrono::duration_cast<std::chrono::milliseconds>(gettick().time_since_epoch()).count();
    WFIFOPOS(fd, 6, sd->x, sd->y, Direction::S);
    WFIFOB(fd, 9) = 5;
    WFIFOB(fd, 10) = 5;
    WFIFOSET(fd, packet_len_table[0x73]);
}

/// Tell a client it is getting disconnected
// Sent if we don't have a record from char-server ?
// or to all connections if same account tries to connect multiple times
// hm, does that mean that in case of multiple map servers ...?
void clif_authfail_fd(sint32 fd, sint32 type)
{
    if (!session[fd])
        return;

    WFIFOW(fd, 0) = 0x81;
    WFIFOL(fd, 2) = type;
    WFIFOSET(fd, packet_len_table[0x81]);

    clif_setwaitclose(fd);
}

/// The client can go back to the character-selection screen
void clif_charselectok(BlockID id)
{
    MapSessionData *sd = map_id2sd(id);
    if (!sd)
        return;

    sint32 fd = sd->fd;
    WFIFOW(fd, 0) = 0xb3;
    WFIFOB(fd, 2) = 1;
    WFIFOSET(fd, packet_len_table[0xb3]);
}

/// An item is dropped on the floor
void clif_dropflooritem(struct flooritem_data *fitem)
{
    uint8 buf[64];

    nullpo_retv(fitem);

    if (fitem->item_data.nameid <= 0)
        return;

    WBUFW(buf, 0) = 0x9e;
    WBUFL(buf, 2) = uint32(fitem->id);
    WBUFW(buf, 6) = fitem->item_data.nameid;
    WBUFB(buf, 8) = 0; //identify;
    WBUFW(buf, 9) = fitem->x;
    WBUFW(buf, 11) = fitem->y;
    WBUFB(buf, 13) = fitem->subx;
    WBUFB(buf, 14) = fitem->suby;
    WBUFW(buf, 15) = fitem->item_data.amount;

    clif_send(buf, packet_len_table[0x9e], fitem, Whom::AREA);
}

/// An item disappears (due to timeout or distance)
void clif_clearflooritem(struct flooritem_data *fitem, sint32 fd)
{
    nullpo_retv(fitem);

    uint8 buf[6];
    WBUFW(buf, 0) = 0xa1;
    WBUFL(buf, 2) = uint32(fitem->id);

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

    uint8 buf[16];
    WBUFW(buf, 0) = 0x80;
    WBUFL(buf, 2) = uint32(bl->id);
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
void clif_being_remove_id(BlockID id, BeingRemoveType type, sint32 fd)
{
    uint8 buf[16];

    WBUFW(buf, 0) = 0x80;
    WBUFL(buf, 2) = uint32(id);
    WBUFB(buf, 6) = type == BeingRemoveType::DEAD;
    memcpy(WFIFOP(fd, 0), buf, 7);
    WFIFOSET(fd, packet_len_table[0x80]);
}

/// A player appears (or updates)
// note: this packet is almost identical to the next
static uint16 clif_player_update(MapSessionData *sd, uint8 *buf)
{
    nullpo_ret(sd);

    WBUFW(buf, 0) = 0x1d8;
    WBUFL(buf, 2) = uint32(sd->id);
    WBUFW(buf, 6) = std::chrono::duration_cast<std::chrono::milliseconds>(sd->speed).count();
    WBUFW(buf, 8) = sd->opt1;
    WBUFW(buf, 10) = sd->opt2;
    WBUFW(buf, 12) = uint16(sd->status.option);
    WBUFW(buf, 14) = 0; // = sd->view_class;
    WBUFW(buf, 16) = sd->status.hair;
    if (sd->attack_spell_override)
        WBUFB(buf, 18) = sd->attack_spell_look_override;
    else
    {
        if (sd->equip_index[EQUIP::WEAPON] >= 0 && sd->inventory_data[sd->equip_index[EQUIP::WEAPON]])
            WBUFW(buf, 18) = sd->status.inventory[sd->equip_index[EQUIP::WEAPON]].nameid;
        else
            WBUFW(buf, 18) = 0;
    }
    if (sd->equip_index[EQUIP::SHIELD] >= 0 && sd->equip_index[EQUIP::SHIELD] != sd->equip_index[EQUIP::WEAPON]
        && sd->inventory_data[sd->equip_index[EQUIP::SHIELD]])
        WBUFW(buf, 20) = sd->status.inventory[sd->equip_index[EQUIP::SHIELD]].nameid;
    else
        WBUFW(buf, 20) = 0;
    WBUFW(buf, 22) = sd->status.legs;
    WBUFW(buf, 24) = sd->status.head;
    WBUFW(buf, 26) = sd->status.chest;
    WBUFW(buf, 28) = sd->status.hair_color;
    WBUFW(buf, 30) = 0; //sd->status.clothes_color;
    WBUFW(buf, 32) = static_cast<sint32>(sd->head_dir);

    WBUFW(buf, 40) = 0; //sd->status.manner;
    WBUFW(buf, 42) = sd->opt3;
    WBUFB(buf, 44) = 0; //sd->status.karma;
    WBUFB(buf, 45) = sd->sex;
    WBUFPOS(buf, 46, sd->x, sd->y, sd->dir);
    WBUFW(buf, 49) = (pc_isGM(sd) == gm_level_t(60) || pc_isGM(sd) == gm_level_t(99)) ? 0x80 : 0;
    WBUFB(buf, 51) = sd->state.dead_sit;
    WBUFW(buf, 52) = 0;

    return packet_len_table[0x1d8];
}

/// A player moves
// note: this packet is almost identical to the previous
static uint16 clif_player_move(MapSessionData *sd, uint8 *buf)
{
    nullpo_ret(sd);

    WBUFW(buf, 0) = 0x1da;
    WBUFL(buf, 2) = uint32(sd->id);
    WBUFW(buf, 6) = std::chrono::duration_cast<std::chrono::milliseconds>(sd->speed).count();
    WBUFW(buf, 8) = sd->opt1;
    WBUFW(buf, 10) = sd->opt2;
    WBUFW(buf, 12) = uint16(sd->status.option);
    WBUFW(buf, 14) = 0; //sd->view_class;
    WBUFW(buf, 16) = sd->status.hair;
    if (sd->equip_index[EQUIP::WEAPON] >= 0 && sd->inventory_data[sd->equip_index[EQUIP::WEAPON]])
        WBUFW(buf, 18) = sd->status.inventory[sd->equip_index[EQUIP::WEAPON]].nameid;
    else
        WBUFW(buf, 18) = 0;
    if (sd->equip_index[EQUIP::SHIELD] >= 0 && sd->equip_index[EQUIP::SHIELD] != sd->equip_index[EQUIP::WEAPON]
            && sd->inventory_data[sd->equip_index[EQUIP::SHIELD]])
        WBUFW(buf, 20) = sd->status.inventory[sd->equip_index[EQUIP::SHIELD]].nameid;
    else
        WBUFW(buf, 20) = 0;
    WBUFW(buf, 22) = sd->status.legs;
    WBUFL(buf, 24) = std::chrono::duration_cast<std::chrono::milliseconds>(gettick().time_since_epoch()).count();
    WBUFW(buf, 28) = sd->status.head;
    WBUFW(buf, 30) = sd->status.chest;
    WBUFW(buf, 32) = sd->status.hair_color;
    WBUFW(buf, 34) = 0; //sd->status.clothes_color;
    WBUFW(buf, 36) = static_cast<sint32>(sd->head_dir);

    WBUFW(buf, 44) = 0; //sd->status.manner;
    WBUFW(buf, 46) = sd->opt3;
    WBUFB(buf, 48) = 0; //sd->status.karma;
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
static uint16 clif_mob_appear(struct mob_data *md, uint8 *buf)
{
    nullpo_ret(md);

    memset(buf, 0, packet_len_table[0x78]);

    WBUFW(buf, 0) = 0x78;
    WBUFL(buf, 2) = uint32(md->id);
    WBUFW(buf, 6) = std::chrono::duration_cast<std::chrono::milliseconds>(battle_get_speed(md)).count();
    WBUFW(buf, 8) = md->opt1;
    WBUFW(buf, 10) = md->opt2;
    WBUFW(buf, 12) = uint16(md->option);
    WBUFW(buf, 14) = md->mob_class;

    WBUFPOS(buf, 46, md->x, md->y, md->dir);
    WBUFB(buf, 49) = 5;
    WBUFB(buf, 50) = 5;
    WBUFW(buf, 52) = uint8(battle_get_level(md));

    return packet_len_table[0x78];
}

/// A mob moves
static uint16 clif_mob_move(struct mob_data *md, uint8 *buf)
{
    nullpo_ret(md);

    memset(buf, 0, packet_len_table[0x7b]);

    WBUFW(buf, 0) = 0x7b;
    WBUFL(buf, 2) = uint32(md->id);
    WBUFW(buf, 6) = std::chrono::duration_cast<std::chrono::milliseconds>(battle_get_speed(md)).count();
    WBUFW(buf, 8) = md->opt1;
    WBUFW(buf, 10) = md->opt2;
    WBUFW(buf, 12) = uint16(md->option);
    WBUFW(buf, 14) = md->mob_class;

    WBUFL(buf, 22) = std::chrono::duration_cast<std::chrono::milliseconds>(gettick().time_since_epoch()).count();

    WBUFPOS2(buf, 50, md->x, md->y, md->to_x, md->to_y);
    WBUFB(buf, 56) = 5;
    WBUFB(buf, 57) = 5;
    WBUFW(buf, 58) = uint8(battle_get_level(md));

    return packet_len_table[0x7b];
}

/// An NPC appears (or updates)
static uint16 clif_npc_appear(struct npc_data *nd, uint8 *buf)
{
    nullpo_ret(nd);

    memset(buf, 0, packet_len_table[0x78]);

    WBUFW(buf, 0) = 0x78;
    WBUFL(buf, 2) = uint32(nd->id);
    WBUFW(buf, 6) = std::chrono::duration_cast<std::chrono::milliseconds>(nd->speed).count();
    WBUFW(buf, 14) = nd->npc_class;
    WBUFPOS(buf, 46, nd->x, nd->y, nd->dir);
    WBUFB(buf, 49) = 5;
    WBUFB(buf, 50) = 5;

    return packet_len_table[0x78];
}

/// These indices are derived from equip_pos in pc.c and some guesswork
static earray<EQUIP, LOOK, LOOK::COUNT> equip_points =
{
    EQUIP::NONE,        /// 0: base
    EQUIP::NONE,        /// 1: hair
    EQUIP::WEAPON,      /// 2: weapon
    EQUIP::LEGS,        /// 3: legs
    EQUIP::HELMET,      /// 4: helmet
    EQUIP::CHEST,       /// 5: chest
    EQUIP::NONE,        /// 6: hair colour
    EQUIP::NONE,        /// 7: clothes colour
    EQUIP::SHIELD,      /// 8: shield
    EQUIP::SHOES,       /// 9: shoes
    EQUIP::GLOVES,      /// 10: gloves
    EQUIP::CAPE,        /// 11: cape
    EQUIP::MISC1,       /// 12: misc1
    EQUIP::MISC2,       /// 13: misc2
};

/// Send everybody (else) a new PC's appearance
void clif_spawnpc(MapSessionData *sd)
{
    nullpo_retv(sd);

    uint8 buf[128];
    sint32 len = clif_player_update(sd, buf);

    clif_send(buf, len, sd, Whom::AREA_WOS);
}

/// Send everybody a new NPC
void clif_spawnnpc(struct npc_data *nd)
{
    nullpo_retv(nd);

    if (nd->npc_class < 0 || nd->flag & 1 || nd->npc_class == INVISIBLE_CLASS)
        return;

    uint8 buf[64];
    memset(buf, 0, packet_len_table[0x7c]);

    WBUFW(buf, 0) = 0x7c;
    WBUFL(buf, 2) = uint32(nd->id);
    WBUFW(buf, 6) = std::chrono::duration_cast<std::chrono::milliseconds>(nd->speed).count();
    WBUFW(buf, 20) = nd->npc_class;
    WBUFPOS(buf, 36, nd->x, nd->y, Direction::S);

    clif_send(buf, packet_len_table[0x7c], nd, Whom::AREA);

    sint32 len = clif_npc_appear(nd, buf);
    clif_send(buf, len, nd, Whom::AREA);
}

/// Hack because the client (and protocol) don't support effects at an arbitrary position
void clif_spawn_fake_npc_for_player(MapSessionData *sd, BlockID fake_npc_id)
{
    nullpo_retv(sd);

    sint32 fd = sd->fd;

    WFIFOW(fd, 0) = 0x7c;
    WFIFOL(fd, 2) = uint32(fake_npc_id);
    WFIFOW(fd, 6) = 0;
    WFIFOW(fd, 8) = 0;
    WFIFOW(fd, 10) = 0;
    WFIFOW(fd, 12) = 0;
    WFIFOW(fd, 20) = 127;
    WFIFOPOS(fd, 36, sd->x, sd->y, Direction::S);
    WFIFOSET(fd, packet_len_table[0x7c]);

    WFIFOW(fd, 0) = 0x78;
    WFIFOL(fd, 2) = uint32(fake_npc_id);
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

    uint8 buf[64];

    sint32 len = clif_mob_appear(md, buf);
    clif_send(buf, len, md, Whom::AREA);
}

/// Player can walk to the requested destination
void clif_walkok(MapSessionData *sd)
{
    nullpo_retv(sd);

    sint32 fd = sd->fd;
    WFIFOW(fd, 0) = 0x87;
    WFIFOL(fd, 2) = std::chrono::duration_cast<std::chrono::milliseconds>(gettick().time_since_epoch()).count();
    WFIFOPOS2(fd, 6, sd->x, sd->y, sd->to_x, sd->to_y);
    WFIFOB(fd, 11) = 0;
    WFIFOSET(fd, packet_len_table[0x87]);
}

/// A player moves
void clif_movechar(MapSessionData *sd)
{
    nullpo_retv(sd);

    uint8 buf[256];
    uint16 len = clif_player_move(sd, buf);

    clif_send(buf, len, sd, Whom::AREA_WOS);
}

/// Timer to close a connection
// This exists so the server can finish sending the packets
// surely there is a better way (perhaps a half shutdown?)
static void clif_waitclose(timer_id, tick_t, sint32 fd)
{
    if (session[fd])
        session[fd]->eof = 1;
}

/// Disconnect player after 5 seconds, to finish sending packets
void clif_setwaitclose(sint32 fd)
{
    add_timer(gettick() + std::chrono::seconds(5), clif_waitclose, fd);
}

/// Player has moved to another map
void clif_changemap(MapSessionData *sd, const Point& point)
{
    nullpo_retv(sd);

    sint32 fd = sd->fd;

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

    sint32 fd = sd->fd;
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

    uint8 buf[16];
    WBUFW(buf, 0) = 0x88;
    WBUFL(buf, 2) = uint32(bl->id);
    WBUFW(buf, 6) = bl->x;
    WBUFW(buf, 8) = bl->y;

    clif_send(buf, packet_len_table[0x88], bl, Whom::AREA);
}

/// An chatted NPC indicates a shop rather than a script
void clif_npcbuysell(MapSessionData *sd, BlockID id)
{
    nullpo_retv(sd);

    sint32 fd = sd->fd;
    WFIFOW(fd, 0) = 0xc4;
    WFIFOL(fd, 2) = uint32(id);
    WFIFOSET(fd, packet_len_table[0xc4]);
}

/// Player wants to buy from the NPC: list the NPC's wares
void clif_buylist(MapSessionData *sd, struct npc_data_shop *nd)
{
    nullpo_retv(sd);
    nullpo_retv(nd);

    sint32 fd = sd->fd;
    WFIFOW(fd, 0) = 0xc6;
    for (sint32 i = 0; i < nd->shop_item.size(); i++)
    {
        struct item_data *id = itemdb_search(nd->shop_item[i].nameid);
        sint32 val = nd->shop_item[i].value;
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

    sint32 fd = sd->fd;
    WFIFOW(fd, 0) = 0xc7;
    sint32 c = 0;
    for (sint32 i = 0; i < MAX_INVENTORY; i++)
    {
        if (sd->status.inventory[i].nameid > 0 && sd->inventory_data[i])
        {
            sint32 val = sd->inventory_data[i]->value_sell;
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
void clif_scriptmes(MapSessionData *sd, BlockID npcid, const char *mes)
{
    nullpo_retv(sd);

    sint32 fd = sd->fd;
    WFIFOW(fd, 0) = 0xb4;
    WFIFOW(fd, 2) = strlen(mes) + 9;
    WFIFOL(fd, 4) = uint32(npcid);
    strcpy(sign_cast<char *>(WFIFOP(fd, 8)), mes);
    WFIFOSET(fd, WFIFOW(fd, 2));
}

/// Pause in NPC dialog/script - wait for player to click "next"
void clif_scriptnext(MapSessionData *sd, BlockID npcid)
{
    nullpo_retv(sd);

    sint32 fd = sd->fd;
    WFIFOW(fd, 0) = 0xb5;
    WFIFOL(fd, 2) = uint32(npcid);
    WFIFOSET(fd, packet_len_table[0xb5]);
}

/// NPC script ends - wait for client to reply
void clif_scriptclose(MapSessionData *sd, BlockID npcid)
{
    nullpo_retv(sd);

    sint32 fd = sd->fd;
    WFIFOW(fd, 0) = 0xb6;
    WFIFOL(fd, 2) = uint32(npcid);
    WFIFOSET(fd, packet_len_table[0xb6]);
}

/// NPC script menu - wait for player to choose something
// the values are separated by colons, so we need to do some magic
void clif_scriptmenu(MapSessionData *sd, BlockID npcid, const std::vector<std::string>& options)
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

    sint32 fd = sd->fd;
    WFIFOW(fd, 0) = 0xb7;
    WFIFOW(fd, 2) = mes.size() + 8;
    WFIFOL(fd, 4) = unwrap(npcid);
    memcpy(WFIFOP(fd, 8), mes.data(), mes.size());
    WFIFOSET(fd, WFIFOW(fd, 2));
}

/// Request for numeric input
void clif_scriptinput(MapSessionData *sd, BlockID npcid)
{
    nullpo_retv(sd);

    sint32 fd = sd->fd;
    WFIFOW(fd, 0) = 0x142;
    WFIFOL(fd, 2) = uint32(npcid);
    WFIFOSET(fd, packet_len_table[0x142]);
}

/// Request for string input
void clif_scriptinputstr(MapSessionData *sd, BlockID npcid)
{
    nullpo_retv(sd);

    sint32 fd = sd->fd;
    WFIFOW(fd, 0) = 0x1d4;
    WFIFOL(fd, 2) = uint32(npcid);
    WFIFOSET(fd, packet_len_table[0x1d4]);
}

/// Add an item to player's inventory
void clif_additem(MapSessionData *sd, sint32 n, sint32 amount, PickupFail fail)
{
    nullpo_retv(sd);

    sint32 fd = sd->fd;
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
        WFIFOB(fd, 22) = static_cast<uint8>(fail);
    }
    else
    {
        if (n < 0 || n >= MAX_INVENTORY)
            return;
        if (! sd->status.inventory[n].nameid || !sd->inventory_data[n])
            return;

        WFIFOW(fd, 0) = 0xa0;
        WFIFOW(fd, 2) = n + 2;
        WFIFOW(fd, 4) = amount;
        WFIFOW(fd, 6) = sd->status.inventory[n].nameid;
        WFIFOB(fd, 8) = 0; //identify;
        WFIFOB(fd, 9) = 0; //broken or attribute;
        WFIFOB(fd, 10) = 0; //refine;
        WFIFOW(fd, 11) = 0; //card[0];
        WFIFOW(fd, 13) = 0; //card[1];
        WFIFOW(fd, 15) = 0; //card[2];
        WFIFOW(fd, 17) = 0; //card[3];
        WFIFOW(fd, 19) = static_cast<uint16>(pc_equippoint(sd, n));
        WFIFOB(fd, 21) = (sd->inventory_data[n]->type == 7) ? 4 : sd->inventory_data[n]->type;
        WFIFOB(fd, 22) = static_cast<uint8>(fail);
    }

    WFIFOSET(fd, packet_len_table[0xa0]);
}

/// Delete a slot of player's inventory
void clif_delitem(MapSessionData *sd, sint32 n, sint32 amount)
{
    nullpo_retv(sd);

    sint32 fd = sd->fd;
    WFIFOW(fd, 0) = 0xaf;
    WFIFOW(fd, 2) = n + 2;
    WFIFOW(fd, 4) = amount;

    WFIFOSET(fd, packet_len_table[0xaf]);
}

/// List the player's inventory, excluding equipment, but including equipped arrows?
void clif_itemlist(MapSessionData *sd)
{
    nullpo_retv(sd);

    sint32 fd = sd->fd;
    WFIFOW(fd, 0) = 0x1ee;

    sint32 n = 0;
    sint32 arrow = -1;
    for (sint32 i = 0; i < MAX_INVENTORY; i++)
    {
        if (!sd->status.inventory[i].nameid || !sd->inventory_data[i])
            continue;
        if (itemdb_isequip2(sd->inventory_data[i]))
            continue;
        WFIFOW(fd, n * 18 + 4) = i + 2;
        WFIFOW(fd, n * 18 + 6) = sd->status.inventory[i].nameid;
        WFIFOB(fd, n * 18 + 8) = sd->inventory_data[i]->type;
        WFIFOB(fd, n * 18 + 9) = 0; //identify;
        WFIFOW(fd, n * 18 + 10) = sd->status.inventory[i].amount;
        if (sd->inventory_data[i]->equip == EPOS::ARROW)
        {
            WFIFOW(fd, n * 18 + 12) = static_cast<uint16>(EPOS::ARROW);
            if (sd->status.inventory[i].equip != EPOS::NONE)
                arrow = i;
        }
        else
            WFIFOW(fd, n * 18 + 12) = static_cast<uint16>(EPOS::NONE);
        WFIFOW(fd, n * 18 + 14) = 0; //card[0]
        WFIFOW(fd, n * 18 + 16) = 0; //card[1]
        WFIFOW(fd, n * 18 + 18) = 0; //card[2]
        WFIFOW(fd, n * 18 + 20) = 0; //card[3]
        n++;
    }

    // are there actually any items?
    if (n)
    {
        WFIFOW(fd, 2) = 4 + n * 18;
        WFIFOSET(fd, WFIFOW(fd, 2));
    }

    // why is this here instead of below with the other equipment?
    if (arrow >= 0)
        clif_arrowequip(sd, arrow);
}

/// List equipment in inventory
void clif_equiplist(MapSessionData *sd)
{
    nullpo_retv(sd);

    sint32 fd = sd->fd;
    WFIFOW(fd, 0) = 0xa4;

    sint32 n = 0;
    for (sint32 i = 0; i < MAX_INVENTORY; i++)
    {
        if (!sd->status.inventory[i].nameid || !sd->inventory_data[i])
            continue;
        if (!itemdb_isequip2(sd->inventory_data[i]))
            continue;
        WFIFOW(fd, n * 20 + 4) = i + 2;
        WFIFOW(fd, n * 20 + 6) = sd->status.inventory[i].nameid;
        WFIFOB(fd, n * 20 + 8) = (sd->inventory_data[i]->type == 7) ? 4 : sd->inventory_data[i]->type;
        WFIFOB(fd, n * 20 + 9) = 0; //identify;
        WFIFOW(fd, n * 20 + 10) = static_cast<uint16>(pc_equippoint(sd, i));
        WFIFOW(fd, n * 20 + 12) = static_cast<uint16>(sd->status.inventory[i].equip);
        WFIFOB(fd, n * 20 + 14) = 0; //broken or attribute;
        WFIFOB(fd, n * 20 + 15) = 0; //refine;
        WFIFOW(fd, n * 20 + 16) = 0; //card[0];
        WFIFOW(fd, n * 20 + 18) = 0; //card[1];
        WFIFOW(fd, n * 20 + 20) = 0; //card[2];
        WFIFOW(fd, n * 20 + 22) = 0; //card[3];
        n++;
    }

    // are there actually any items?
    if (n)
    {
        WFIFOW(fd, 2) = 4 + n * 20;
        WFIFOSET(fd, WFIFOW(fd, 2));
    }
}

/// List all items in a storage, excluding equipment
void clif_storageitemlist(MapSessionData *sd, struct storage *stor)
{
    nullpo_retv(sd);
    nullpo_retv(stor);

    sint32 fd = sd->fd;
    WFIFOW(fd, 0) = 0x1f0;

    sint32 n = 0;
    for (sint32 i = 0; i < MAX_STORAGE; i++)
    {
        if (!stor->storage_[i].nameid)
            continue;
        struct item_data *id = itemdb_search(stor->storage_[i].nameid);
        nullpo_retv(id);
        if (itemdb_isequip2(id))
            continue;

        WFIFOW(fd, n * 18 + 4) = i + 1;
        WFIFOW(fd, n * 18 + 6) = stor->storage_[i].nameid;
        WFIFOB(fd, n * 18 + 8) = id->type;
        WFIFOB(fd, n * 18 + 9) = 0; //identify;
        WFIFOW(fd, n * 18 + 10) = stor->storage_[i].amount;
        WFIFOW(fd, n * 18 + 12) = 0;
        WFIFOW(fd, n * 18 + 14) = 0; //card[0];
        WFIFOW(fd, n * 18 + 16) = 0; //card[1];
        WFIFOW(fd, n * 18 + 18) = 0; //card[2];
        WFIFOW(fd, n * 18 + 20) = 0; //card[3];
        n++;
    }

    // are there actually any items?
    if (n)
    {
        WFIFOW(fd, 2) = 4 + n * 18;
        WFIFOSET(fd, WFIFOW(fd, 2));
    }
}

/// List equipment in storage
void clif_storageequiplist(MapSessionData *sd, struct storage *stor)
{
    nullpo_retv(sd);
    nullpo_retv(stor);

    sint32 fd = sd->fd;
    WFIFOW(fd, 0) = 0xa6;
    sint32 n = 0;
    for (sint32 i = 0; i < MAX_STORAGE; i++)
    {
        if (!stor->storage_[i].nameid)
            continue;
        struct item_data *id = itemdb_search(stor->storage_[i].nameid);
        nullpo_retv(id);
        if (!itemdb_isequip2(id))
            continue;
        WFIFOW(fd, n * 20 + 4) = i + 1;
        WFIFOW(fd, n * 20 + 6) = stor->storage_[i].nameid;
        WFIFOB(fd, n * 20 + 8) = id->type;
        WFIFOB(fd, n * 20 + 9) = //identify;
        WFIFOW(fd, n * 20 + 10) = static_cast<uint16>(id->equip);
        WFIFOW(fd, n * 20 + 12) = static_cast<uint16>(stor->storage_[i].equip);
        WFIFOB(fd, n * 20 + 14) = 0; //broken or attribute;
        WFIFOB(fd, n * 20 + 15) = 0; //refine;
        WFIFOW(fd, n * 20 + 16) = 0; //card[0];
        WFIFOW(fd, n * 20 + 18) = 0; //card[1];
        WFIFOW(fd, n * 20 + 20) = 0; //card[2];
        WFIFOW(fd, n * 20 + 22) = 0; //card[3];
        n++;
    }
    if (n)
    {
        WFIFOW(fd, 2) = 4 + n * 20;
        WFIFOSET(fd, WFIFOW(fd, 2));
    }
}

/// Inform client of a change in some kind of status
void clif_updatestatus(MapSessionData *sd, SP type)
{
    nullpo_retv(sd);

    sint32 fd = sd->fd;
    WFIFOW(fd, 0) = 0xb0;
    WFIFOW(fd, 2) = static_cast<uint16>(type);
    switch (type)
    {
    // The first set use packet 0xb0
    case SP::WEIGHT:
        WFIFOL(fd, 4) = sd->weight;
        break;
    case SP::MAXWEIGHT:
        WFIFOL(fd, 4) = sd->max_weight;
        break;
    case SP::SPEED:
        WFIFOL(fd, 4) = std::chrono::duration_cast<std::chrono::milliseconds>(sd->speed).count();
        break;
    case SP::BASELEVEL:
        WFIFOL(fd, 4) = uint8(sd->status.base_level);
        break;
    case SP::JOBLEVEL:
        WFIFOL(fd, 4) = 0;
        break;
    case SP::STATUSPOINT:
        WFIFOL(fd, 4) = sd->status.status_point;
        break;
    case SP::SKILLPOINT:
        WFIFOL(fd, 4) = sd->status.skill_point;
        break;
    case SP::HIT:
        WFIFOL(fd, 4) = sd->hit;
        break;
    case SP::FLEE1:
        WFIFOL(fd, 4) = sd->flee;
        break;
    case SP::FLEE2:
        WFIFOL(fd, 4) = sd->flee2 / 10;
        break;
    case SP::MAXHP:
        WFIFOL(fd, 4) = sd->status.max_hp;
        break;
    case SP::MAXSP:
        WFIFOL(fd, 4) = sd->status.max_sp;
        break;
    case SP::HP:
        WFIFOL(fd, 4) = sd->status.hp;
        break;
    case SP::SP:
        WFIFOL(fd, 4) = sd->status.sp;
        break;
    case SP::ASPD:
        WFIFOL(fd, 4) = std::chrono::duration_cast<std::chrono::milliseconds>(sd->aspd).count();
        break;
    case SP::ATK1:
        WFIFOL(fd, 4) = sd->base_atk + sd->watk;
        break;
    case SP::DEF1:
        WFIFOL(fd, 4) = sd->def;
        break;
    case SP::MDEF1:
        WFIFOL(fd, 4) = sd->mdef;
        break;
    case SP::ATK2:
        WFIFOL(fd, 4) = sd->watk2;
        break;
    case SP::DEF2:
        WFIFOL(fd, 4) = sd->def2;
        break;
    case SP::MDEF2:
        WFIFOL(fd, 4) = sd->mdef2;
        break;
    case SP::CRITICAL:
        WFIFOL(fd, 4) = sd->critical / 10;
        break;
    case SP::MATK1:
        WFIFOL(fd, 4) = sd->matk1;
        break;
    case SP::MATK2:
        WFIFOL(fd, 4) = sd->matk2;
        break;

    // The next set use packet 0xb1
    case SP::ZENY:
        trade_verifyzeny(sd);
        WFIFOW(fd, 0) = 0xb1;
        if (sd->status.zeny < 0)
            sd->status.zeny = 0;
        WFIFOL(fd, 4) = sd->status.zeny;
        break;
    case SP::BASEEXP:
        WFIFOW(fd, 0) = 0xb1;
        WFIFOL(fd, 4) = sd->status.base_exp;
        break;
    case SP::JOBEXP:
        WFIFOW(fd, 0) = 0xb1;
        WFIFOL(fd, 4) = sd->status.job_exp;
        break;
    case SP::NEXTBASEEXP:
        WFIFOW(fd, 0) = 0xb1;
        WFIFOL(fd, 4) = pc_nextbaseexp(sd);
        break;
    case SP::NEXTJOBEXP:
        WFIFOW(fd, 0) = 0xb1;
        WFIFOL(fd, 4) = pc_nextjobexp(sd);
        break;

    // These few use packet 0xbe
    case SP::USTR:
    case SP::UAGI:
    case SP::UVIT:
    case SP::UINT:
    case SP::UDEX:
    case SP::ULUK:
        WFIFOW(fd, 0) = 0xbe;
        WFIFOB(fd, 4) = pc_need_status_point(sd, ATTR_TO_SP_BASE(ATTR_FROM_SP_UP(type)));
        break;

    // ... packet 0x013a, with no payload
    case SP::ATTACKRANGE:
        WFIFOW(fd, 0) = 0x13a;
        WFIFOW(fd, 2) = sd->attack_spell_override ? sd->attack_spell_range : sd->attackrange;
        break;

    // main status use 0x0141 with an extended payload
    case SP::STR:
    case SP::AGI:
    case SP::VIT:
    case SP::INT:
    case SP::DEX:
    case SP::LUK:
        WFIFOW(fd, 0) = 0x141;
        // the client reads type as a 4-byte integer, so zero the top word
        WFIFOW(fd, 4) = 0;
        WFIFOL(fd, 6) = sd->status.stats[ATTR_FROM_SP_BASE(type)];
        WFIFOL(fd, 10) = sd->paramb[ATTR_FROM_SP_BASE(type)] + sd->parame[ATTR_FROM_SP_BASE(type)];
        break;

    // back to 0xb0
    case SP::GM:
        WFIFOL(fd, 4) = uint8(pc_isGM(sd));
        break;

    default:
        map_log("%s: make %d routine\n", __func__, static_cast<sint32>(type));
        return;
    }
    WFIFOSET(fd, packet_len_table[WFIFOW(fd, 0)]);
}

/// Change a being's LOOK towards everybody
void clif_changelook(BlockList *bl, LOOK type, sint32 val)
{
    return clif_changelook_towards(bl, type, val, NULL);
}

/// Change a being's LOOK towards somebody in particular (or everybody if NULL)
void clif_changelook_towards(BlockList *bl, LOOK type, sint32 val,
                             MapSessionData *dstsd)
{
    nullpo_retv(bl);

    uint8 rbuf[32];
    uint8 *buf = dstsd ? WFIFOP(dstsd->fd, 0) : rbuf;

    MapSessionData *sd = NULL;
    if (bl->type == BL_PC)
        sd = static_cast<MapSessionData *>(bl);

    if (sd && sd->status.option & OPTION::INVISIBILITY)
        return;

    if (sd && (type == LOOK::WEAPON || type == LOOK::SHIELD || type >= LOOK::SHOES))
    {
        WBUFW(buf, 0) = 0x1d7;
        WBUFL(buf, 2) = uint32(bl->id);
        if (type >= LOOK::SHOES)
        {
            // shoes, gloves, cape, misc1, misc2
            EQUIP equip_point = equip_points[type];

            WBUFB(buf, 6) = uint8(type);
            if (sd->equip_index[equip_point] >= 0
                    && sd->inventory_data[sd->equip_index[equip_point]])
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
                if (sd->equip_index[EQUIP::WEAPON] >= 0
                        && sd->inventory_data[sd->equip_index[EQUIP::WEAPON]])
                    WBUFW(buf, 7) = sd->status.inventory[sd->equip_index[EQUIP::WEAPON]].nameid;
                else
                    WBUFW(buf, 7) = 0;
            }
            if (sd->equip_index[EQUIP::SHIELD] >= 0
                    && sd->equip_index[EQUIP::SHIELD] != sd->equip_index[EQUIP::WEAPON]
                    && sd->inventory_data[sd->equip_index[EQUIP::SHIELD]])
                WBUFW(buf, 9) = sd->status.inventory[sd->equip_index[EQUIP::SHIELD]].nameid;
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
        WBUFL(buf, 2) = uint32(bl->id);
        WBUFB(buf, 6) = uint8(type);
        WBUFW(buf, 7) = val;
        WBUFW(buf, 9) = 0;
        if (dstsd)
            WFIFOSET(dstsd->fd, packet_len_table[0x1d7]);
        else
            clif_send(buf, packet_len_table[0x1d7], bl, Whom::AREA);
    }
}

/// show player their status
// actually sent whenever the client has changed map
static void clif_initialstatus(MapSessionData *sd)
{
    nullpo_retv(sd);

    sint32 fd = sd->fd;

    WFIFOW(fd, 0) = 0xbd;
    WFIFOW(fd, 2) = sd->status.status_point;
    WFIFOB(fd, 4) = min(255, sd->status.stats[ATTR::STR]);
    WFIFOB(fd, 5) = pc_need_status_point(sd, SP::STR);
    WFIFOB(fd, 6) = min(255, sd->status.stats[ATTR::AGI]);
    WFIFOB(fd, 7) = pc_need_status_point(sd, SP::AGI);
    WFIFOB(fd, 8) = min(255, sd->status.stats[ATTR::VIT]);
    WFIFOB(fd, 9) = pc_need_status_point(sd, SP::VIT);
    WFIFOB(fd, 10) = min(255, sd->status.stats[ATTR::INT]);
    WFIFOB(fd, 11) = pc_need_status_point(sd, SP::INT);
    WFIFOB(fd, 12) = min(255, sd->status.stats[ATTR::DEX]);
    WFIFOB(fd, 13) = pc_need_status_point(sd, SP::DEX);
    WFIFOB(fd, 14) = min(255, sd->status.stats[ATTR::LUK]);
    WFIFOB(fd, 15) = pc_need_status_point(sd, SP::LUK);

    WFIFOW(fd, 16) = sd->base_atk + sd->watk;
    WFIFOW(fd, 18) = sd->watk2;    //atk bonus
    WFIFOW(fd, 20) = sd->matk1;
    WFIFOW(fd, 22) = sd->matk2;
    WFIFOW(fd, 24) = sd->def;  // def
    WFIFOW(fd, 26) = sd->def2;
    WFIFOW(fd, 28) = sd->mdef; // mdef
    WFIFOW(fd, 30) = sd->mdef2;
    WFIFOW(fd, 32) = sd->hit;
    WFIFOW(fd, 34) = sd->flee;
    WFIFOW(fd, 36) = sd->flee2 / 10;
    WFIFOW(fd, 38) = sd->critical / 10;
    WFIFOW(fd, 40) = 0; //sd->status.karma;
    WFIFOW(fd, 42) = 0; //sd->status.manner;

    WFIFOSET(fd, packet_len_table[0xbd]);

    // These repeat some of the above, but also provide bonus information.
    clif_updatestatus(sd, SP::STR);
    clif_updatestatus(sd, SP::AGI);
    clif_updatestatus(sd, SP::VIT);
    clif_updatestatus(sd, SP::INT);
    clif_updatestatus(sd, SP::DEX);
    clif_updatestatus(sd, SP::LUK);

    clif_updatestatus(sd, SP::ATTACKRANGE);
    clif_updatestatus(sd, SP::ASPD);
}

/// inform client that arrows are now equipped
void clif_arrowequip(MapSessionData *sd, sint32 val)
{
    nullpo_retv(sd);

    sd->attacktarget = DEFAULT;

    sint32 fd = sd->fd;
    WFIFOW(fd, 0) = 0x013c;
    WFIFOW(fd, 2) = val + 2;

    WFIFOSET(fd, packet_len_table[0x013c]);
}

/// Sent when there aren't any arrow, OR when arrows are equipped
void clif_arrow_fail(MapSessionData *sd, ArrowFail type)
{
    nullpo_retv(sd);

    sint32 fd = sd->fd;
    WFIFOW(fd, 0) = 0x013b;
    WFIFOW(fd, 2) = static_cast<uint16>(type);

    WFIFOSET(fd, packet_len_table[0x013b]);
}

/// What happened when player tried to increase a stat
void clif_statusupack(MapSessionData *sd, SP type, bool ok, sint32 val)
{
    nullpo_retv(sd);

    sint32 fd = sd->fd;
    WFIFOW(fd, 0) = 0xbc;
    WFIFOW(fd, 2) = static_cast<uint16>(type);
    WFIFOB(fd, 4) = ok;
    WFIFOB(fd, 5) = val;
    WFIFOSET(fd, packet_len_table[0xbc]);
}

/// What happened when player tried to equip an item
void clif_equipitemack(MapSessionData *sd, sint32 n, EPOS pos, bool ok)
{
    nullpo_retv(sd);

    sint32 fd = sd->fd;
    WFIFOW(fd, 0) = 0xaa;
    WFIFOW(fd, 2) = n + 2;
    WFIFOW(fd, 4) = static_cast<uint16>(pos);
    WFIFOB(fd, 6) = ok;
    WFIFOSET(fd, packet_len_table[0xaa]);
}

// TODO resume here
/*==========================================
 *
 *------------------------------------------
 */
void clif_unequipitemack(MapSessionData *sd, sint32 n, EPOS pos, bool ok)
{
    nullpo_retv(sd);

    sint32 fd = sd->fd;
    WFIFOW(fd, 0) = 0xac;
    WFIFOW(fd, 2) = n + 2;
    WFIFOW(fd, 4) = static_cast<uint16>(pos);
    WFIFOB(fd, 6) = ok;
    WFIFOSET(fd, packet_len_table[0xac]);
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_misceffect(BlockList *bl, sint32 type)
{
    nullpo_retv(bl);

    uint8 buf[32];
    WBUFW(buf, 0) = 0x19b;
    WBUFL(buf, 2) = uint32(bl->id);
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

    OPTION option = *battle_get_option(bl);

    uint8 buf[32];
    WBUFW(buf, 0) = 0x119;
    WBUFL(buf, 2) = uint32(bl->id);
    WBUFW(buf, 6) = *battle_get_opt1(bl);
    WBUFW(buf, 8) = *battle_get_opt2(bl);
    WBUFW(buf, 10) = uint16(option);
    WBUFB(buf, 12) = 0;        // ??

    clif_send(buf, packet_len_table[0x119], bl, Whom::AREA);
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_useitemack(MapSessionData *sd, sint32 idx, sint32 amount,
                     sint32 ok)
{
    nullpo_retv(sd);

    if (!ok)
    {
        sint32 fd = sd->fd;
        WFIFOW(fd, 0) = 0xa8;
        WFIFOW(fd, 2) = idx + 2;
        WFIFOW(fd, 4) = amount;
        WFIFOB(fd, 6) = ok;
        WFIFOSET(fd, packet_len_table[0xa8]);
    }
    else
    {
        uint8 buf[32];

        WBUFW(buf, 0) = 0x1c8;
        WBUFW(buf, 2) = idx + 2;
        WBUFW(buf, 4) = sd->status.inventory[idx].nameid;
        WBUFL(buf, 6) = uint32(sd->id);
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

    sint32 fd = sd->fd;
    WFIFOW(fd, 0) = 0xe5;
    strcpy(sign_cast<char *>(WFIFOP(fd, 2)), name);
    WFIFOSET(fd, packet_len_table[0xe5]);
}

/*==========================================
 * 取り引き要求応答
 *------------------------------------------
 */
void clif_tradestart(MapSessionData *sd, sint32 type)
{
    nullpo_retv(sd);

    sint32 fd = sd->fd;
    WFIFOW(fd, 0) = 0xe7;
    WFIFOB(fd, 2) = type;
    WFIFOSET(fd, packet_len_table[0xe7]);
}

/*==========================================
 * 相手方からのアイテム追加
 *------------------------------------------
 */
void clif_tradeadditem(MapSessionData *sd,
                       MapSessionData *tsd, sint32 idx, sint32 amount)
{
    nullpo_retv(sd);
    nullpo_retv(tsd);

    sint32 fd = tsd->fd;
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
        WFIFOB(fd, 8) = 0; //identify;
        WFIFOB(fd, 9) = 0; //broken or attribute;
        WFIFOB(fd, 10) = 0; //refine;
        WFIFOW(fd, 11) = 0; //card[0];
        WFIFOW(fd, 13) = 0; //card[1];
        WFIFOW(fd, 15) = 0; //card[2];
        WFIFOW(fd, 17) = 0; //card[3];
    }
    WFIFOSET(fd, packet_len_table[0xe9]);
}

/*==========================================
 * アイテム追加成功/失敗
 *------------------------------------------
 */
void clif_tradeitemok(MapSessionData *sd, sint32 idx, sint32 amount, sint32 fail)
{
    nullpo_retv(sd);

    sint32 fd = sd->fd;
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
void clif_tradedeal_lock(MapSessionData *sd, sint32 fail)
{
    nullpo_retv(sd);

    sint32 fd = sd->fd;
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

    sint32 fd = sd->fd;
    WFIFOW(fd, 0) = 0xee;
    WFIFOSET(fd, packet_len_table[0xee]);
}

/*==========================================
 * 取り引き完了
 *------------------------------------------
 */
void clif_tradecompleted(MapSessionData *sd, sint32 fail)
{
    nullpo_retv(sd);

    sint32 fd = sd->fd;
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

    sint32 fd = sd->fd;
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
                           sint32 idx, sint32 amount)
{
    nullpo_retv(sd);
    nullpo_retv(stor);

    sint32 fd = sd->fd;
    WFIFOW(fd, 0) = 0xf4;      // Storage item added
    WFIFOW(fd, 2) = idx + 1; // index
    WFIFOL(fd, 4) = amount;    // amount
    WFIFOW(fd, 8) = stor->storage_[idx].nameid;
    WFIFOB(fd, 10) = 0; //identify;
    WFIFOB(fd, 11) = 0; //broken or attribute;
    WFIFOB(fd, 12) = 0; //refine;
    WFIFOW(fd, 13) = 0; //card[0];
    WFIFOW(fd, 15) = 0; //card[1];
    WFIFOW(fd, 17) = 0; //card[2];
    WFIFOW(fd, 19) = 0; //card[3];
    WFIFOSET(fd, packet_len_table[0xf4]);
}

/*==========================================
 * カプラ倉庫からアイテムを取り去る
 *------------------------------------------
 */
void clif_storageitemremoved(MapSessionData *sd, sint32 idx, sint32 amount)
{
    nullpo_retv(sd);

    sint32 fd = sd->fd;
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

    sint32 fd = sd->fd;
    WFIFOW(fd, 0) = 0xf8;      // Storage Closed
    WFIFOSET(fd, packet_len_table[0xf8]);
}

void clif_changelook_accessories(BlockList *bl, MapSessionData *dest)
{

    for (LOOK i : { LOOK::SHOES, LOOK::GLOVES, LOOK::CAPE, LOOK::MISC1, LOOK::MISC2})
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
    sint32 len;

    if (dstsd->status.option & OPTION::INVISIBILITY)
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
    sint32 len;

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

    uint8 buf[256];
    uint16 len = clif_mob_move(md, buf);
    clif_send(buf, len, md, Whom::AREA);
}

/*==========================================
 * モンスターの位置修正
 *------------------------------------------
 */
void clif_fixmobpos(struct mob_data *md)
{
    nullpo_retv(md);

    if (md->state.state == MS::WALK)
    {
        uint8 buf[256];
        sint32 len = clif_mob_move(md, buf);
        clif_send(buf, len, md, Whom::AREA);
    }
    else
    {
        uint8 buf[256];
        sint32 len = clif_mob_appear(md, buf);
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
        uint8 buf[256];
        sint32 len = clif_player_move(sd, buf);
        clif_send(buf, len, sd, Whom::AREA);
    }
    else
    {
        uint8 buf[256];
        sint32 len = clif_player_update(sd, buf);
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
                 tick_t tick, interval_t sdelay, interval_t ddelay, sint32 damage,
                 sint32 div_, sint32 type, sint32 damage2)
{
    nullpo_retv(src);
    nullpo_retv(dst);

    uint8 buf[256];
    WBUFW(buf, 0) = 0x8a;
    WBUFL(buf, 2) = uint32(src->id);
    WBUFL(buf, 6) = uint32(dst->id);
    WBUFL(buf, 10) = std::chrono::duration_cast<std::chrono::milliseconds>(tick.time_since_epoch()).count();
    WBUFL(buf, 14) = std::chrono::duration_cast<std::chrono::milliseconds>(sdelay).count();
    WBUFL(buf, 18) = std::chrono::duration_cast<std::chrono::milliseconds>(ddelay).count();
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
    sint32 len;
    nullpo_retv(sd);
    nullpo_retv(md);

    if (md->state.state == MS::WALK)
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
    sint32 fd;

    nullpo_retv(sd);
    nullpo_retv(fitem);

    fd = sd->fd;
    //009d <ID>.l <item ID>.w <identify flag>.B <X>.w <Y>.w <amount>.w <subX>.B <subY>.B
    WFIFOW(fd, 0) = 0x9d;
    WFIFOL(fd, 2) = uint32(fitem->id);
    WFIFOW(fd, 6) = fitem->item_data.nameid;
    WFIFOB(fd, 8) = 0; //identify;
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

    sint32 fd = sd->fd;
    WFIFOW(fd, 0) = 0x10f;
    sint32 len = 4;
    for (sint32 i = 0, c = 0; i < MAX_SKILL; i++)
    {
        sint32 id = sd->status.skill[i].id;
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
void clif_skillup(MapSessionData *sd, sint32 skill_num)
{
    nullpo_retv(sd);

    sint32 fd = sd->fd;
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
void clif_status_change(BlockList *bl, sint32 type, sint32 flag)
{
    nullpo_retv(bl);

    uint8 buf[16];
    WBUFW(buf, 0) = 0x0196;
    WBUFW(buf, 2) = type;
    WBUFL(buf, 4) = uint32(bl->id);
    WBUFB(buf, 8) = flag;
    clif_send(buf, packet_len_table[0x196], bl, Whom::AREA);
}

/*==========================================
 * Send message(modified by [Yor])
 *------------------------------------------
 */
void clif_displaymessage(sint32 fd, const char *mes)
{
    sint32 len_mes = strlen(mes);

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
void clif_GMmessage(BlockList *bl, const char *mes, size_t len, sint32 flag)
{
    uint8 buf[len + 16];
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
void clif_resurrection(BlockList *bl, sint32 type)
{
    nullpo_retv(bl);

    uint8 buf[16];
    WBUFW(buf, 0) = 0x148;
    WBUFL(buf, 2) = uint32(bl->id);
    WBUFW(buf, 6) = type;

    clif_send(buf, packet_len_table[0x148], bl, type == 1 ? Whom::AREA : Whom::AREA_WOS);
}

/*==========================================
 * whisper is transmitted to the destination player
 *------------------------------------------
 */
void clif_whisper_message(sint32 fd, const char *nick, const char *mes, sint32 mes_len)   // R 0097 <len>.w <nick>.24B <message>.?B
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
void clif_whisper_end(sint32 fd, sint32 flag) // R 0098 <type>.B: 0: success to send whisper, 1: target character is not loged in?, 2: ignored by target
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
void clif_party_created(MapSessionData *sd, sint32 flag)
{
    nullpo_retv(sd);

    sint32 fd = sd->fd;
    WFIFOW(fd, 0) = 0xfa;
    WFIFOB(fd, 2) = flag;
    WFIFOSET(fd, packet_len_table[0xfa]);
}

/*==========================================
 * パーティ情報送信
 *------------------------------------------
 */
void clif_party_info(struct party *p, sint32 fd)
{
    nullpo_retv(p);

    uint8 buf[1024];
    WBUFW(buf, 0) = 0xfb;
    STRZCPY2(sign_cast<char *>(WBUFP(buf, 4)), p->name);

    MapSessionData *sd = NULL;
    sint32 c = 0;
    for (sint32 i = 0; i < MAX_PARTY; i++)
    {
        struct party_member *m = &p->member[i];
        if (m->account_id)
        {
            if (sd == NULL)
                sd = m->sd;
            WBUFL(buf, 28 + c * 46) = uint32(m->account_id);
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

    sint32 fd = tsd->fd;

    struct party *p = party_search(sd->status.party_id);
    if (!p)
        return;

    WFIFOW(fd, 0) = 0xfe;
    WFIFOL(fd, 2) = uint32(sd->status.account_id);
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
void clif_party_inviteack(MapSessionData *sd, char *nick, sint32 flag)
{
    nullpo_retv(sd);

    sint32 fd = sd->fd;
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
void clif_party_option(struct party *p, MapSessionData *sd, sint32 flag)
{
    nullpo_retv(p);

    if (!sd && !flag)
    {
        for (sint32 i = 0; i < MAX_PARTY; i++)
        {
            sd = map_id2sd(p->member[i].account_id);
            if (sd)
                break;
        }
    }
    if (!sd)
        return;

    uint8 buf[16];
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
                     account_t account_id, const char *name, sint32 flag)
{
    nullpo_retv(p);

    uint8 buf[64];
    WBUFW(buf, 0) = 0x105;
    WBUFL(buf, 2) = uint32(account_id);
    strzcpy(sign_cast<char *>(WBUFP(buf, 6)), name, 24);
    WBUFB(buf, 30) = flag & 0x0f;

    if ((flag & 0xf0) == 0)
    {
        if (!sd)
            for (sint32 i = 0; i < MAX_PARTY; i++)
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
void clif_party_message(struct party *p, account_t account_id, const char *mes, sint32 len)
{
    nullpo_retv(p);

    MapSessionData *sd = NULL;
    for (sint32 i = 0; i < MAX_PARTY; i++)
    {
        sd = p->member[i].sd;
        if (sd)
            break;
    }
    if (sd)
    {
        uint8 buf[1024];
        WBUFW(buf, 0) = 0x109;
        WBUFW(buf, 2) = len + 8;
        WBUFL(buf, 4) = uint32(account_id);
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

    uint8 buf[16];
    WBUFW(buf, 0) = 0x107;
    WBUFL(buf, 2) = uint32(sd->status.account_id);
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

    uint8 buf[16];
    WBUFW(buf, 0) = 0x106;
    WBUFL(buf, 2) = uint32(sd->status.account_id);
    WBUFW(buf, 6) = (sd->status.hp > 0x7fff) ? 0x7fff : sd->status.hp;
    WBUFW(buf, 8) = (sd->status.max_hp > 0x7fff) ? 0x7fff : sd->status.max_hp;
    clif_send(buf, packet_len_table[0x106], sd, Whom::PARTY_AREA_WOS);
}

/*==========================================
 * パーティ場所移動（未使用）
 *------------------------------------------
 */
void clif_party_move(struct party *p, MapSessionData *sd, bool online)
{
    uint8 buf[128];

    nullpo_retv(sd);
    nullpo_retv(p);

    WBUFW(buf, 0) = 0x104;
    WBUFL(buf, 2) = uint32(sd->status.account_id);
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
    sint32 fd;

    nullpo_retv(sd);
    nullpo_retv(bl);

    fd = sd->fd;
    WFIFOW(fd, 0) = 0x139;
    WFIFOL(fd, 2) = uint32(bl->id);
    WFIFOW(fd, 6) = bl->x;
    WFIFOW(fd, 8) = bl->y;
    WFIFOW(fd, 10) = sd->x;
    WFIFOW(fd, 12) = sd->y;
    WFIFOW(fd, 14) = sd->attackrange;
    WFIFOSET(fd, packet_len_table[0x139]);
}

/*==========================================
 * エモーション
 *------------------------------------------
 */
void clif_emotion(BlockList *bl, sint32 type)
{
    uint8 buf[8];

    nullpo_retv(bl);

    WBUFW(buf, 0) = 0xc0;
    WBUFL(buf, 2) = uint32(bl->id);
    WBUFB(buf, 6) = type;
    clif_send(buf, packet_len_table[0xc0], bl, Whom::AREA);
}

/*==========================================
 * 座る
 *------------------------------------------
 */
void clif_sitting(sint32, MapSessionData *sd)
{
    nullpo_retv(sd);

    uint8 buf[64];
    WBUFW(buf, 0) = 0x8a;
    WBUFL(buf, 2) = uint32(sd->id);
    WBUFB(buf, 26) = 2;
    clif_send(buf, packet_len_table[0x8a], sd, Whom::AREA);
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_disp_onlyself(MapSessionData *sd, char *mes, sint32 len)
{
    nullpo_retv(sd);

    uint8 buf[len + 32];
    WBUFW(buf, 0) = 0x17f;
    WBUFW(buf, 2) = len + 8;
    memcpy(WBUFP(buf, 4), mes, len + 4);

    clif_send(buf, WBUFW(buf, 2), sd, Whom::SELF);
}

/*==========================================
 *
 *------------------------------------------
 */

void clif_GM_kickack(MapSessionData *sd, account_t id)
{
    nullpo_retv(sd);

    sint32 fd = sd->fd;
    WFIFOW(fd, 0) = 0xcd;
    WFIFOL(fd, 2) = uint32(id);
    WFIFOSET(fd, packet_len_table[0xcd]);
}

static void clif_parse_QuitGame(sint32 fd, MapSessionData *sd);

void clif_GM_kick(MapSessionData *sd, MapSessionData *tsd,
                  sint32 type)
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
                       const char *name, sint32 type)
{
    sint32 fd;

    nullpo_retv(sd);
    nullpo_retv(bl);

    fd = sd->fd;
    WFIFOW(fd, 0) = 0x1d3;
    memcpy(WFIFOP(fd, 2), name, 24);
    WFIFOB(fd, 26) = type;
    WFIFOL(fd, 27) = 0;
    WFIFOL(fd, 31) = uint32(bl->id);
    WFIFOSET(fd, packet_len_table[0x1d3]);

    return;
}

// displaying special effects (npcs, weather, etc) [Valaris]
void clif_specialeffect(BlockList *bl, sint32 type, sint32 flag)
{
    uint8 buf[24];

    nullpo_retv(bl);

    memset(buf, 0, packet_len_table[0x19b]);

    WBUFW(buf, 0) = 0x19b;
    WBUFL(buf, 2) = uint32(bl->id);
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
static void clif_parse_WantToConnection(sint32 fd, MapSessionData *sd)
{
    if (sd)
    {
        map_log("%s: invalid request?\n", __func__);
        return;
    }

    account_t account_id = account_t(RFIFOL(fd, 2));

    WFIFOL(fd, 0) = uint32(account_id);
    WFIFOSET(fd, 4);

    // TODO: apply this at the char-server level instead
    // if same account already connected, we disconnect the 2 sessions
    MapSessionData *old_sd;
    if ((old_sd = map_id2sd(account_id)) != NULL)
    {
        clif_authfail_fd(fd, 2);   // same id
        clif_authfail_fd(old_sd->fd, 2);   // same id
        PRINTF("%s: Double connection for account %d (sessions: #%d (new) and #%d (old)).\n",
               __func__, account_id, fd, old_sd->fd);
    }
    else
    {
        sd = new MapSessionData(account_id);
        session[fd]->session_data = sd;
        sd->fd = fd;

        charid_t charid = charid_t(RFIFOL(fd, 6));
        uint32 login1 = RFIFOL(fd, 10);
        // uint32 client_tick = RFIFOL(fd, 14);
        uint8 sex = RFIFOB(fd, 18);
        pc_setnewpc(sd, /*account_id,*/ charid, login1, sex);

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
static void clif_parse_LoadEndAck(sint32, MapSessionData *sd)
{
//  struct item_data* item;
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
    clif_updatestatus(sd, SP::NEXTBASEEXP);
    clif_updatestatus(sd, SP::NEXTJOBEXP);
    // skill point
    clif_updatestatus(sd, SP::SKILLPOINT);
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
        pc_setinvincibletimer(sd, std::chrono::milliseconds(battle_config.pc_invincible_time));
    }

    map_addblock(sd);     // ブロック登録
    clif_spawnpc(sd);          // spawn

    // weight max , now
    clif_updatestatus(sd, SP::MAXWEIGHT);
    clif_updatestatus(sd, SP::WEIGHT);

    // pvp
    if (sd->pvp_timer && !battle_config.pk_mode)
        delete_timer(sd->pvp_timer);
    if (maps[sd->m].flag.pvp)
    {
        if (!battle_config.pk_mode)
        {                       // remove pvp stuff for pk_mode [Valaris]
            sd->pvp_timer =
                add_timer(gettick() + std::chrono::milliseconds(200), pc_calc_pvprank_timer, sd->id);
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
    clif_changelook(sd, LOOK::WEAPON, 0);

    // option
    clif_changeoption(sd);

//        clif_changelook_accessories(sd, NULL);

    map_foreachinarea(clif_getareachar, sd->m, sd->x - AREA_SIZE,
                      sd->y - AREA_SIZE, sd->x + AREA_SIZE,
                      sd->y + AREA_SIZE, BL_NUL, sd);
}

/*==========================================
 *
 *------------------------------------------
 */
static void clif_parse_WalkToXY(sint32 fd, MapSessionData *sd)
{
    sint32 x, y;

    nullpo_retv(sd);

    if (pc_isdead(sd))
    {
        clif_being_remove(sd, BeingRemoveType::DEAD);
        return;
    }

    if (sd->npc_id || sd->state.storage_flag)
        return;

    if (sd->canmove_tick > gettick())
        return;

    if (sd->opt1 > 0 && sd->opt1 != 6)
        return;
    if (sd->status.option & OPTION::HIDE2)
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
void clif_parse_QuitGame(sint32 fd, MapSessionData *sd)
{
    tick_t tick = gettick();
    nullpo_retv(sd);

    WFIFOW(fd, 0) = 0x18b;
    if ((!pc_isdead(sd) && (sd->opt1 || sd->opt2)) || tick < sd->canact_tick)
    {
        WFIFOW(fd, 2) = 1;
        WFIFOSET(fd, packet_len_table[0x18b]);
        return;
    }

    /*  Rovert's prevent logout option fixed [Valaris]  */
    if ((battle_config.prevent_logout
         && tick >= sd->canlog_tick + std::chrono::seconds(10))
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
static void clif_parse_GetCharNameRequest(sint32 fd, MapSessionData *sd)
{
    account_t account_id = account_t(RFIFOL(fd, 2));
    BlockList *bl = map_id2bl(account_id);
    if (bl == NULL)
        return;

    WFIFOW(fd, 0) = 0x95;
    WFIFOL(fd, 2) = uint32(account_id);

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

            if (ssd->status.party_id && (p = party_search(ssd->status.party_id)) != NULL)
            {
                party_name = p->name;
                will_send = 1;
            }

            if (will_send)
            {
                WFIFOW(fd, 0) = 0x195;
                WFIFOL(fd, 2) = uint32(account_id);
                memcpy(WFIFOP(fd, 6), party_name, 24);
                memset(WFIFOP(fd, 30), 0, 24);
                memset(WFIFOP(fd, 54), 0, 24);
                memset(WFIFOP(fd, 78), 0, 24); // We send this value twice because the client expects it
                WFIFOSET(fd, packet_len_table[0x195]);

            }

            if (pc_isGM(sd) >= gm_level_t(battle_config.hack_info_GM_level))
            {
                in_addr_t ip = session[ssd->fd]->client_addr.to_n();
                WFIFOW(fd, 0) = 0x20C;

                // Mask the IP using the char-server password
                if (battle_config.mask_ip_gms)
                    ip = MD5_ip(chrif_getpasswd(), ip);

                WFIFOL(fd, 2) = uint32(account_id);
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
            MAP_LOG("%s: bad type %d(%d)\n",
                    __func__, bl->type, account_id);
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
static void clif_parse_GlobalMessage(sint32 fd, MapSessionData *sd)
{
    sint32 msg_len = RFIFOW(fd, 2) - 4; /* Header (2) + length (2). */
    size_t message_len = 0;
    uint8 *buf = NULL;
    char *message = NULL;   /* The message text only. */

    nullpo_retv(sd);

    if (!(buf = clif_validate_chat(sd, 2, &message, &message_len)))
    {
        clif_displaymessage(fd, "Your message could not be sent.");
        return;
    }

    if (is_atcommand(fd, sd, message, DEFAULT))
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
        WBUFL(buf, 4) = uint32(sd->id);

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
    uint16 msg_len = strlen(msg) + 1;
    uint8 buf[512];

    if (msg_len + 16 > 512)
        return;

    nullpo_retv(bl);

    WBUFW(buf, 0) = 0x8d;
    WBUFW(buf, 2) = msg_len + 8;
    WBUFL(buf, 4) = uint32(bl->id);
    memcpy(WBUFP(buf, 8), msg, msg_len);

    clif_send(buf, WBUFW(buf, 2), bl, Whom::AREA);
}

/*==========================================
 *
 *------------------------------------------
 */
static void clif_parse_ChangeDir(sint32 fd, MapSessionData *sd)
{
    uint8 buf[64];

    nullpo_retv(sd);

    Direction dir = static_cast<Direction>(RFIFOB(fd, 4));

    if (dir == sd->dir)
        return;

    pc_setdir(sd, dir);

    WBUFW(buf, 0) = 0x9c;
    WBUFL(buf, 2) = uint32(sd->id);
    WBUFW(buf, 6) = 0;
    WBUFB(buf, 8) = static_cast<sint32>(dir);
    clif_send(buf, packet_len_table[0x9c], sd, Whom::AREA_WOS);

}

/*==========================================
 *
 *------------------------------------------
 */
static void clif_parse_Emotion(sint32 fd, MapSessionData *sd)
{
    uint8 buf[64];

    nullpo_retv(sd);

    if (pc_checkskill(sd, NV_EMOTE) >= 1)
    {
        WBUFW(buf, 0) = 0xc0;
        WBUFL(buf, 2) = uint32(sd->id);
        WBUFB(buf, 6) = RFIFOB(fd, 2);
        clif_send(buf, packet_len_table[0xc0], sd, Whom::AREA);
    }
}

/*==========================================
 *
 *------------------------------------------
 */
static void clif_parse_HowManyConnections(sint32 fd, MapSessionData *)
{
    WFIFOW(fd, 0) = 0xc2;
    WFIFOL(fd, 2) = map_getusers();
    WFIFOSET(fd, packet_len_table[0xc2]);
}

/*==========================================
 *
 *------------------------------------------
 */
static void clif_parse_ActionRequest(sint32 fd, MapSessionData *sd)
{
    uint8 buf[64];
    sint32 action_type;

    nullpo_retv(sd);

    if (pc_isdead(sd))
    {
        clif_being_remove(sd, BeingRemoveType::DEAD);
        return;
    }
    if (sd->npc_id || sd->opt1 > 0 || (sd->status.option & OPTION::HIDE2) || sd->state.storage_flag)
        return;

    tick_t tick = gettick();

    pc_stop_walking(sd, 0);
    pc_stopattack(sd);

    BlockID target_id = BlockID(RFIFOL(fd, 2));
    action_type = RFIFOB(fd, 6);

    switch (action_type)
    {
        case 0x00:             // once attack
        case 0x07:             // continuous attack
            if (sd->status.option & OPTION::HIDE)
                return;
            if (!battle_config.sdelay_attack_enable)
            {
                if (tick < sd->canact_tick)
                    return;
            }
            if (sd->invincible_timer)
                pc_delinvincibletimer(sd);
            if (sd->attacktarget)   // [Valaris]
                sd->attacktarget = DEFAULT;
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
            WBUFL(buf, 2) = uint32(sd->id);
            WBUFB(buf, 26) = 3;
            clif_send(buf, packet_len_table[0x8a], sd, Whom::AREA);
            break;
    }
}

/*==========================================
 *
 *------------------------------------------
 */
static void clif_parse_Restart(sint32 fd, MapSessionData *sd)
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
                 && gettick() >= sd->canlog_tick + std::chrono::seconds(10))
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
static void clif_parse_Wis(sint32 fd, MapSessionData *sd)
{
    size_t message_len = 0;
    uint8 *buf = NULL;
    char *message = NULL;   /* The message text only. */
    MapSessionData *dstsd = NULL;

    nullpo_retv(sd);

    if (!(buf = clif_validate_chat(sd, 1, &message, &message_len)))
    {
        clif_displaymessage(fd, "Your message could not be sent.");
        return;
    }

    if (is_atcommand(fd, sd, message, DEFAULT))
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
static void clif_parse_TakeItem(sint32 fd, MapSessionData *sd)
{
    nullpo_retv(sd);

    BlockID map_object_id = BlockID(RFIFOL(fd, 2));
    struct flooritem_data *fitem = static_cast<struct flooritem_data *>(map_id2bl(map_object_id));

    if (pc_isdead(sd))
    {
        clif_being_remove(sd, BeingRemoveType::DEAD);
        return;
    }

    if (sd->npc_id || sd->opt1 > 0)
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
static void clif_parse_DropItem(sint32 fd, MapSessionData *sd)
{
    sint32 item_index, item_amount;

    nullpo_retv(sd);

    if (pc_isdead(sd))
    {
        clif_being_remove(sd, BeingRemoveType::DEAD);
        return;
    }
    if (sd->npc_id || sd->opt1 > 0 || maps[sd->m].flag.no_player_drops)
        return;

    item_index = RFIFOW(fd, 2) - 2;
    item_amount = RFIFOW(fd, 4);

    pc_dropitem(sd, item_index, item_amount);
}

/*==========================================
 *
 *------------------------------------------
 */
static void clif_parse_UseItem(sint32 fd, MapSessionData *sd)
{
    nullpo_retv(sd);

    if (pc_isdead(sd))
    {
        clif_being_remove(sd, BeingRemoveType::DEAD);
        return;
    }
    if (sd->npc_id || sd->opt1 > 0)
        return;

    if (sd->invincible_timer)
        pc_delinvincibletimer(sd);

    pc_useitem(sd, RFIFOW(fd, 2) - 2);
}

/*==========================================
 *
 *------------------------------------------
 */
static void clif_parse_EquipItem(sint32 fd, MapSessionData *sd)
{
    sint32 idx;

    nullpo_retv(sd);

    if (pc_isdead(sd))
    {
        clif_being_remove(sd, BeingRemoveType::DEAD);
        return;
    }
    idx = RFIFOW(fd, 2) - 2;
    if (sd->npc_id)
        return;

    //ペット用装備であるかないか
    if (sd->inventory_data[idx])
    {
//         uint16 what = RFIFOW(fd, 4);
//         if (sd->inventory_data[idx]->type == 10)
//             what = 0x8000;
        pc_equipitem(sd, idx);
    }
}

/*==========================================
 *
 *------------------------------------------
 */
static void clif_parse_UnequipItem(sint32 fd, MapSessionData *sd)
{
    nullpo_retv(sd);

    if (pc_isdead(sd))
    {
        clif_being_remove(sd, BeingRemoveType::DEAD);
        return;
    }
    sint32 idx = RFIFOW(fd, 2) - 2;

    if (sd->npc_id || sd->opt1 > 0)
        return;
    pc_unequipitem(sd, idx, CalcStatus::NOW);
}

/*==========================================
 *
 *------------------------------------------
 */
static void clif_parse_NpcClicked(sint32 fd, MapSessionData *sd)
{
    nullpo_retv(sd);

    account_t acc = account_t(RFIFOL(fd, 2));

    if (pc_isdead(sd))
    {
        clif_being_remove(sd, BeingRemoveType::DEAD);
        return;
    }
    if (sd->npc_id)
        return;
    npc_click(sd, acc);
}

/*==========================================
 *
 *------------------------------------------
 */
static void clif_parse_NpcBuySellSelected(sint32 fd, MapSessionData *sd)
{
    account_t acc = account_t(RFIFOL(fd, 2));
    BuySell bs = BuySell(RFIFOB(fd, 6));
    npc_buysellsel(sd, acc, bs);
}

/*==========================================
 *
 *------------------------------------------
 */
static void clif_parse_NpcBuyListSend(sint32 fd, MapSessionData *sd)
{
    sint32 fail = 0, n;
    const uint16 *item_list;

    n = (RFIFOW(fd, 2) - 4) / 4;
    item_list = reinterpret_cast<const uint16 *>(RFIFOP(fd, 4));

    fail = npc_buylist(sd, n, item_list);

    WFIFOW(fd, 0) = 0xca;
    WFIFOB(fd, 2) = fail;
    WFIFOSET(fd, packet_len_table[0xca]);
}

/*==========================================
 *
 *------------------------------------------
 */
static void clif_parse_NpcSellListSend(sint32 fd, MapSessionData *sd)
{
    sint32 fail = 0, n;
    const uint16 *item_list;

    n = (RFIFOW(fd, 2) - 4) / 4;
    item_list = reinterpret_cast<const uint16 *>(RFIFOP(fd, 4));

    fail = npc_selllist(sd, n, item_list);

    WFIFOW(fd, 0) = 0xcb;
    WFIFOB(fd, 2) = fail;
    WFIFOSET(fd, packet_len_table[0xcb]);
}

/*==========================================
 * 取引要請を相手に送る
 *------------------------------------------
 */
static void clif_parse_TradeRequest(sint32, MapSessionData *sd)
{
    nullpo_retv(sd);

    account_t acc = account_t(RFIFOL(sd->fd, 2));
    if (pc_checkskill(sd, NV_TRADE) >= 1)
    {
        trade_traderequest(sd, acc);
    }
}

/*==========================================
 * 取引要請
 *------------------------------------------
 */
static void clif_parse_TradeAck(sint32, MapSessionData *sd)
{
    nullpo_retv(sd);

    trade_tradeack(sd, RFIFOB(sd->fd, 2));
}

/*==========================================
 * アイテム追加
 *------------------------------------------
 */
static void clif_parse_TradeAddItem(sint32, MapSessionData *sd)
{
    nullpo_retv(sd);

    trade_tradeadditem(sd, RFIFOW(sd->fd, 2), RFIFOL(sd->fd, 4));
}

/*==========================================
 * アイテム追加完了(ok押し)
 *------------------------------------------
 */
static void clif_parse_TradeOk(sint32, MapSessionData *sd)
{
    trade_tradeok(sd);
}

/*==========================================
 * 取引キャンセル
 *------------------------------------------
 */
static void clif_parse_TradeCansel(sint32, MapSessionData *sd)
{
    trade_tradecancel(sd);
}

/*==========================================
 * 取引許諾(trade押し)
 *------------------------------------------
 */
static void clif_parse_TradeCommit(sint32, MapSessionData *sd)
{
    trade_tradecommit(sd);
}

/*==========================================
 * ステータスアップ
 *------------------------------------------
 */
static void clif_parse_StatusUp(sint32 fd, MapSessionData *sd)
{
    pc_statusup(sd, static_cast<SP>(RFIFOW(fd, 2)));
}

/*==========================================
 * スキルレベルアップ
 *------------------------------------------
 */
static void clif_parse_SkillUp(sint32 fd, MapSessionData *sd)
{
    pc_skillup(sd, RFIFOW(fd, 2));
}

/*==========================================
 *
 *------------------------------------------
 */
static void clif_parse_NpcSelectMenu(sint32 fd, MapSessionData *sd)
{
    nullpo_retv(sd);

    account_t acc = account_t(RFIFOL(fd, 2));
    sd->npc_menu = RFIFOB(fd, 6);
    map_scriptcont(sd, acc);
}

/*==========================================
 *
 *------------------------------------------
 */
static void clif_parse_NpcNextClicked(sint32 fd, MapSessionData *sd)
{
    account_t acc = account_t(RFIFOL(fd, 2));
    map_scriptcont(sd, acc);
}

/*==========================================
 *
 *------------------------------------------
 */
static void clif_parse_NpcAmountInput(sint32 fd, MapSessionData *sd)
{
    nullpo_retv(sd);

    account_t acc = account_t(RFIFOL(fd, 2));

    sd->npc_amount = RFIFOL(fd, 6);

    map_scriptcont(sd, acc);
}

/*==========================================
 * Process string-based input for an NPC.
 *
 * (S 01d5 <len>.w <npc_ID>.l <message>.?B)
 *------------------------------------------
 */
static void clif_parse_NpcStringInput(sint32 fd, MapSessionData *sd)
{
    sint32 len;
    nullpo_retv(sd);

    len = RFIFOW(fd, 2) - 8;
    account_t acc = account_t(RFIFOL(fd, 4));
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

    map_scriptcont(sd, acc);
}

/*==========================================
 *
 *------------------------------------------
 */
static void clif_parse_NpcCloseClicked(sint32 fd, MapSessionData *sd)
{
    account_t acc = account_t(RFIFOL(fd, 2));
    map_scriptcont(sd, acc);
}

/*==========================================
 * カプラ倉庫へ入れる
 *------------------------------------------
 */
static void clif_parse_MoveToStorage(sint32 fd, MapSessionData *sd)
{
    sint32 item_index, item_amount;

    nullpo_retv(sd);

    item_index = RFIFOW(fd, 2) - 2;
    item_amount = RFIFOL(fd, 4);

    if ((sd->npc_id && !sd->npc_flags.storage) || sd->trade_partner
        || !sd->state.storage_flag)
        return;

    if (sd->state.storage_flag == 1)
        storage_storageadd(sd, item_index, item_amount);
}

/*==========================================
 * カプラ倉庫から出す
 *------------------------------------------
 */
static void clif_parse_MoveFromStorage(sint32 fd, MapSessionData *sd)
{
    sint32 item_index, item_amount;

    nullpo_retv(sd);

    item_index = RFIFOW(fd, 2) - 1;
    item_amount = RFIFOL(fd, 4);

    if ((sd->npc_id && !sd->npc_flags.storage) || sd->trade_partner
        || !sd->state.storage_flag)
        return;

    if (sd->state.storage_flag == 1)
        storage_storageget(sd, item_index, item_amount);
}

/*==========================================
 * カプラ倉庫を閉じる
 *------------------------------------------
 */
static void clif_parse_CloseStorage(sint32, MapSessionData *sd)
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
static void clif_parse_CreateParty(sint32 fd, MapSessionData *sd)
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
static void clif_parse_PartyInvite(sint32 fd, MapSessionData *sd)
{
    account_t acc = account_t(RFIFOL(fd, 2));
    party_invite(sd, acc);
}

/*==========================================
 * パーティ勧誘返答
 * Process reply to party invitation.
 *
 * (S 00ff <account_ID>.l <flag>.l)
 *------------------------------------------
 */
static void clif_parse_ReplyPartyInvite(sint32 fd, MapSessionData *sd)
{
    account_t acc = account_t(RFIFOL(fd, 2));
    bool accept = pc_checkskill(sd, NV_PARTY) >= 1 && RFIFOL(fd, 6);
    party_reply_invite(sd, acc, accept);
}

/*==========================================
 * パーティ脱退要求
 *------------------------------------------
 */
static void clif_parse_LeaveParty(sint32, MapSessionData *sd)
{
    party_leave(sd);
}

/*==========================================
 * パーティ除名要求
 *------------------------------------------
 */
static void clif_parse_RemovePartyMember(sint32 fd, MapSessionData *sd)
{
    account_t acc = account_t(RFIFOL(fd, 2));
    //const char *name = sign_cast<const char *>(RFIFOP(fd, 6));
    party_removemember(sd, acc/*, name*/);
}

/*==========================================
 * パーティ設定変更要求
 *------------------------------------------
 */
static void clif_parse_PartyChangeOption(sint32 fd, MapSessionData *sd)
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
static void clif_parse_PartyMessage(sint32 fd, MapSessionData *sd)
{
    size_t message_len = 0;
    uint8 *buf = NULL;
    char *message = NULL;   /* The message text only. */

    nullpo_retv(sd);

    if (!(buf = clif_validate_chat(sd, 0, &message, &message_len)))
    {
        clif_displaymessage(fd, "Your message could not be sent.");
        return;
    }

    if (is_atcommand(fd, sd, message, DEFAULT))
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

constexpr std::chrono::milliseconds DEFAULT_RATE(100);
constexpr std::chrono::milliseconds INFINITE_RATE(0);
// functions list. Rate is how many milliseconds are required between
// calls. Packets exceeding this rate will be dropped. flood_rates in
// map.structs.hpp must be the same length as this table.
typedef struct func_table
{
    interval_t rate;
    void (*func)(sint32 fd, MapSessionData *sd);
} func_table;
func_table clif_parse_func_table[0x220] =
{
    { DEFAULT_RATE, NULL }, // 0
    { DEFAULT_RATE, NULL }, // 1
    { DEFAULT_RATE, NULL }, // 2
    { DEFAULT_RATE, NULL }, // 3
    { DEFAULT_RATE, NULL }, // 4
    { DEFAULT_RATE, NULL }, // 5
    { DEFAULT_RATE, NULL }, // 6
    { DEFAULT_RATE, NULL }, // 7
    { DEFAULT_RATE, NULL }, // 8
    { DEFAULT_RATE, NULL }, // 9
    { DEFAULT_RATE, NULL }, // a
    { DEFAULT_RATE, NULL }, // b
    { DEFAULT_RATE, NULL }, // c
    { DEFAULT_RATE, NULL }, // d
    { DEFAULT_RATE, NULL }, // e
    { DEFAULT_RATE, NULL }, // f
    { DEFAULT_RATE, NULL }, // 10
    { DEFAULT_RATE, NULL }, // 11
    { DEFAULT_RATE, NULL }, // 12
    { DEFAULT_RATE, NULL }, // 13
    { DEFAULT_RATE, NULL }, // 14
    { DEFAULT_RATE, NULL }, // 15
    { DEFAULT_RATE, NULL }, // 16
    { DEFAULT_RATE, NULL }, // 17
    { DEFAULT_RATE, NULL }, // 18
    { DEFAULT_RATE, NULL }, // 19
    { DEFAULT_RATE, NULL }, // 1a
    { DEFAULT_RATE, NULL }, // 1b
    { DEFAULT_RATE, NULL }, // 1c
    { DEFAULT_RATE, NULL }, // 1d
    { DEFAULT_RATE, NULL }, // 1e
    { DEFAULT_RATE, NULL }, // 1f
    { DEFAULT_RATE, NULL }, // 20
    { DEFAULT_RATE, NULL }, // 21
    { DEFAULT_RATE, NULL }, // 22
    { DEFAULT_RATE, NULL }, // 23
    { DEFAULT_RATE, NULL }, // 24
    { DEFAULT_RATE, NULL }, // 25
    { DEFAULT_RATE, NULL }, // 26
    { DEFAULT_RATE, NULL }, // 27
    { DEFAULT_RATE, NULL }, // 28
    { DEFAULT_RATE, NULL }, // 29
    { DEFAULT_RATE, NULL }, // 2a
    { DEFAULT_RATE, NULL }, // 2b
    { DEFAULT_RATE, NULL }, // 2c
    { DEFAULT_RATE, NULL }, // 2d
    { DEFAULT_RATE, NULL }, // 2e
    { DEFAULT_RATE, NULL }, // 2f
    { DEFAULT_RATE, NULL }, // 30
    { DEFAULT_RATE, NULL }, // 31
    { DEFAULT_RATE, NULL }, // 32
    { DEFAULT_RATE, NULL }, // 33
    { DEFAULT_RATE, NULL }, // 34
    { DEFAULT_RATE, NULL }, // 35
    { DEFAULT_RATE, NULL }, // 36
    { DEFAULT_RATE, NULL }, // 37
    { DEFAULT_RATE, NULL }, // 38
    { DEFAULT_RATE, NULL }, // 39
    { DEFAULT_RATE, NULL }, // 3a
    { DEFAULT_RATE, NULL }, // 3b
    { DEFAULT_RATE, NULL }, // 3c
    { DEFAULT_RATE, NULL }, // 3d
    { DEFAULT_RATE, NULL }, // 3e
    { DEFAULT_RATE, NULL }, // 3f
    { DEFAULT_RATE, NULL }, // 40
    { DEFAULT_RATE, NULL }, // 41
    { DEFAULT_RATE, NULL }, // 42
    { DEFAULT_RATE, NULL }, // 43
    { DEFAULT_RATE, NULL }, // 44
    { DEFAULT_RATE, NULL }, // 45
    { DEFAULT_RATE, NULL }, // 46
    { DEFAULT_RATE, NULL }, // 47
    { DEFAULT_RATE, NULL }, // 48
    { DEFAULT_RATE, NULL }, // 49
    { DEFAULT_RATE, NULL }, // 4a
    { DEFAULT_RATE, NULL }, // 4b
    { DEFAULT_RATE, NULL }, // 4c
    { DEFAULT_RATE, NULL }, // 4d
    { DEFAULT_RATE, NULL }, // 4e
    { DEFAULT_RATE, NULL }, // 4f
    { DEFAULT_RATE, NULL }, // 50
    { DEFAULT_RATE, NULL }, // 51
    { DEFAULT_RATE, NULL }, // 52
    { DEFAULT_RATE, NULL }, // 53
    { DEFAULT_RATE, NULL }, // 54
    { DEFAULT_RATE, NULL }, // 55
    { DEFAULT_RATE, NULL }, // 56
    { DEFAULT_RATE, NULL }, // 57
    { DEFAULT_RATE, NULL }, // 58
    { DEFAULT_RATE, NULL }, // 59
    { DEFAULT_RATE, NULL }, // 5a
    { DEFAULT_RATE, NULL }, // 5b
    { DEFAULT_RATE, NULL }, // 5c
    { DEFAULT_RATE, NULL }, // 5d
    { DEFAULT_RATE, NULL }, // 5e
    { DEFAULT_RATE, NULL }, // 5f
    { DEFAULT_RATE, NULL }, // 60
    { DEFAULT_RATE, NULL }, // 61
    { DEFAULT_RATE, NULL }, // 62
    { DEFAULT_RATE, NULL }, // 63
    { DEFAULT_RATE, NULL }, // 64
    { DEFAULT_RATE, NULL }, // 65
    { DEFAULT_RATE, NULL }, // 66
    { DEFAULT_RATE, NULL }, // 67
    { DEFAULT_RATE, NULL }, // 68
    { DEFAULT_RATE, NULL }, // 69
    { DEFAULT_RATE, NULL }, // 6a
    { DEFAULT_RATE, NULL }, // 6b
    { DEFAULT_RATE, NULL }, // 6c
    { DEFAULT_RATE, NULL }, // 6d
    { DEFAULT_RATE, NULL }, // 6e
    { DEFAULT_RATE, NULL }, // 6f
    { DEFAULT_RATE, NULL }, // 70
    { DEFAULT_RATE, NULL }, // 71
    { DEFAULT_RATE, clif_parse_WantToConnection }, // 72
    { DEFAULT_RATE, NULL }, // 73
    { DEFAULT_RATE, NULL }, // 74
    { DEFAULT_RATE, NULL }, // 75
    { DEFAULT_RATE, NULL }, // 76
    { DEFAULT_RATE, NULL }, // 77
    { DEFAULT_RATE, NULL }, // 78
    { DEFAULT_RATE, NULL }, // 79
    { DEFAULT_RATE, NULL }, // 7a
    { DEFAULT_RATE, NULL }, // 7b
    { DEFAULT_RATE, NULL }, // 7c
    { INFINITE_RATE, clif_parse_LoadEndAck }, // 7d
    { DEFAULT_RATE, NULL }, // 7e
    { DEFAULT_RATE, NULL }, // 7f
    { DEFAULT_RATE, NULL }, // 80
    { DEFAULT_RATE, NULL }, // 81
    { DEFAULT_RATE, NULL }, // 82
    { DEFAULT_RATE, NULL }, // 83
    { DEFAULT_RATE, NULL }, // 84
    { INFINITE_RATE, clif_parse_WalkToXY }, // 85 Walk code limits this on it's own
    { DEFAULT_RATE, NULL }, // 86
    { DEFAULT_RATE, NULL }, // 87
    { DEFAULT_RATE, NULL }, // 88
    { std::chrono::milliseconds(1000), clif_parse_ActionRequest }, // 89 Special case - see below
    { DEFAULT_RATE, NULL }, // 8a
    { DEFAULT_RATE, NULL }, // 8b
    { std::chrono::milliseconds(300), clif_parse_GlobalMessage }, // 8c
    { DEFAULT_RATE, NULL }, // 8d
    { DEFAULT_RATE, NULL }, // 8e
    { DEFAULT_RATE, NULL }, // 8f
    { std::chrono::milliseconds(500), clif_parse_NpcClicked }, // 90
    { DEFAULT_RATE, NULL }, // 91
    { DEFAULT_RATE, NULL }, // 92
    { DEFAULT_RATE, NULL }, // 93
    { INFINITE_RATE, clif_parse_GetCharNameRequest }, // 94
    { DEFAULT_RATE, NULL }, // 95
    { std::chrono::milliseconds(300), clif_parse_Wis }, // 96
    { DEFAULT_RATE, NULL }, // 97
    { DEFAULT_RATE, NULL }, // 98
    { DEFAULT_RATE, NULL }, // 99
    { DEFAULT_RATE, NULL }, // 9a
    { INFINITE_RATE, clif_parse_ChangeDir }, // 9b
    { DEFAULT_RATE, NULL }, // 9c
    { DEFAULT_RATE, NULL }, // 9d
    { DEFAULT_RATE, NULL }, // 9e
    { std::chrono::milliseconds(400), clif_parse_TakeItem }, // 9f
    { DEFAULT_RATE, NULL }, // a0
    { DEFAULT_RATE, NULL }, // a1
    { std::chrono::milliseconds(50), clif_parse_DropItem }, // a2
    { DEFAULT_RATE, NULL }, // a3
    { DEFAULT_RATE, NULL }, // a4
    { DEFAULT_RATE, NULL }, // a5
    { DEFAULT_RATE, NULL }, // a6
    { DEFAULT_RATE, clif_parse_UseItem }, // a7
    { DEFAULT_RATE, NULL }, // a8
    { INFINITE_RATE, clif_parse_EquipItem }, // a9 Special case - outfit window (not implemented yet - needs to allow bursts)
    { DEFAULT_RATE, NULL }, // aa
    { INFINITE_RATE, clif_parse_UnequipItem }, // ab Special case - outfit window (not implemented yet - needs to allow bursts)
    { DEFAULT_RATE, NULL }, // ac
    { DEFAULT_RATE, NULL }, // ad
    { DEFAULT_RATE, NULL }, // ae
    { DEFAULT_RATE, NULL }, // af
    { DEFAULT_RATE, NULL }, // b0
    { DEFAULT_RATE, NULL }, // b1
    { DEFAULT_RATE, clif_parse_Restart }, // b2
    { DEFAULT_RATE, NULL }, // b3
    { DEFAULT_RATE, NULL }, // b4
    { DEFAULT_RATE, NULL }, // b5
    { DEFAULT_RATE, NULL }, // b6
    { DEFAULT_RATE, NULL }, // b7
    { DEFAULT_RATE, clif_parse_NpcSelectMenu }, // b8
    { INFINITE_RATE, clif_parse_NpcNextClicked }, // b9
    { DEFAULT_RATE, NULL }, // ba
    { INFINITE_RATE, clif_parse_StatusUp }, // bb People click this very quickly
    { DEFAULT_RATE, NULL }, // bc
    { DEFAULT_RATE, NULL }, // bd
    { DEFAULT_RATE, NULL }, // be
    { std::chrono::milliseconds(1000), clif_parse_Emotion }, // bf
    { DEFAULT_RATE, NULL }, // c0
    { DEFAULT_RATE, clif_parse_HowManyConnections }, // c1
    { DEFAULT_RATE, NULL }, // c2
    { DEFAULT_RATE, NULL }, // c3
    { DEFAULT_RATE, NULL }, // c4
    { DEFAULT_RATE, clif_parse_NpcBuySellSelected }, // c5
    { DEFAULT_RATE, NULL }, // c6
    { DEFAULT_RATE, NULL }, // c7
    { INFINITE_RATE, clif_parse_NpcBuyListSend }, // c8
    { INFINITE_RATE, clif_parse_NpcSellListSend }, // c9 Selling multiple 1-slot items
    { DEFAULT_RATE, NULL }, // ca
    { DEFAULT_RATE, NULL }, // cb
    { DEFAULT_RATE, NULL }, // cc
    { DEFAULT_RATE, NULL }, // cd
    { DEFAULT_RATE, NULL }, // ce
    { DEFAULT_RATE, NULL }, // cf
    { DEFAULT_RATE, NULL }, // d0
    { DEFAULT_RATE, NULL }, // d1
    { DEFAULT_RATE, NULL }, // d2
    { DEFAULT_RATE, NULL }, // d3
    { DEFAULT_RATE, NULL }, // d4
    { DEFAULT_RATE, NULL }, // d5
    { DEFAULT_RATE, NULL }, // d6
    { DEFAULT_RATE, NULL }, // d7
    { DEFAULT_RATE, NULL }, // d8
    { DEFAULT_RATE, NULL }, // d9
    { DEFAULT_RATE, NULL }, // da
    { DEFAULT_RATE, NULL }, // db
    { DEFAULT_RATE, NULL }, // dc
    { DEFAULT_RATE, NULL }, // dd
    { DEFAULT_RATE, NULL }, // de
    { DEFAULT_RATE, NULL }, // df
    { DEFAULT_RATE, NULL }, // e0
    { DEFAULT_RATE, NULL }, // e1
    { DEFAULT_RATE, NULL }, // e2
    { DEFAULT_RATE, NULL }, // e3
    { std::chrono::milliseconds(2000), clif_parse_TradeRequest }, // e4
    { DEFAULT_RATE, NULL }, // e5
    { DEFAULT_RATE, clif_parse_TradeAck }, // e6
    { DEFAULT_RATE, NULL }, // e7
    { DEFAULT_RATE, clif_parse_TradeAddItem }, // e8
    { DEFAULT_RATE, NULL }, // e9
    { DEFAULT_RATE, NULL }, // ea
    { DEFAULT_RATE, clif_parse_TradeOk }, // eb
    { DEFAULT_RATE, NULL }, // ec
    { DEFAULT_RATE, clif_parse_TradeCansel }, // ed
    { DEFAULT_RATE, NULL }, // ee
    { DEFAULT_RATE, clif_parse_TradeCommit }, // ef
    { DEFAULT_RATE, NULL }, // f0
    { DEFAULT_RATE, NULL }, // f1
    { DEFAULT_RATE, NULL }, // f2
    { INFINITE_RATE, clif_parse_MoveToStorage }, // f3
    { DEFAULT_RATE, NULL }, // f4
    { INFINITE_RATE, clif_parse_MoveFromStorage }, // f5
    { DEFAULT_RATE, NULL }, // f6
    { DEFAULT_RATE, clif_parse_CloseStorage }, // f7
    { DEFAULT_RATE, NULL }, // f8
    { std::chrono::milliseconds(2000), clif_parse_CreateParty }, // f9
    { DEFAULT_RATE, NULL }, // fa
    { DEFAULT_RATE, NULL }, // fb
    { std::chrono::milliseconds(2000), clif_parse_PartyInvite }, // fc
    { DEFAULT_RATE, NULL }, // fd
    { DEFAULT_RATE, NULL }, // fe
    { DEFAULT_RATE, clif_parse_ReplyPartyInvite }, // ff
    { DEFAULT_RATE, clif_parse_LeaveParty }, // 100
    { DEFAULT_RATE, NULL }, // 101
    { DEFAULT_RATE, clif_parse_PartyChangeOption }, // 102
    { DEFAULT_RATE, clif_parse_RemovePartyMember }, // 103
    { DEFAULT_RATE, NULL }, // 104
    { DEFAULT_RATE, NULL }, // 105
    { DEFAULT_RATE, NULL }, // 106
    { DEFAULT_RATE, NULL }, // 107
    { std::chrono::milliseconds(300), clif_parse_PartyMessage }, // 108
    { DEFAULT_RATE, NULL }, // 109
    { DEFAULT_RATE, NULL }, // 10a
    { DEFAULT_RATE, NULL }, // 10b
    { DEFAULT_RATE, NULL }, // 10c
    { DEFAULT_RATE, NULL }, // 10d
    { DEFAULT_RATE, NULL }, // 10e
    { DEFAULT_RATE, NULL }, // 10f
    { DEFAULT_RATE, NULL }, // 110
    { DEFAULT_RATE, NULL }, // 111
    { INFINITE_RATE, clif_parse_SkillUp }, // 112
    { DEFAULT_RATE, NULL }, // 113
    { DEFAULT_RATE, NULL }, // 114
    { DEFAULT_RATE, NULL }, // 115
    { DEFAULT_RATE, NULL }, // 116
    { DEFAULT_RATE, NULL }, // 117
    { DEFAULT_RATE, NULL }, // 118
    { DEFAULT_RATE, NULL }, // 119
    { DEFAULT_RATE, NULL }, // 11a
    { DEFAULT_RATE, NULL }, // 11b
    { DEFAULT_RATE, NULL }, // 11c
    { DEFAULT_RATE, NULL }, // 11d
    { DEFAULT_RATE, NULL }, // 11e
    { DEFAULT_RATE, NULL }, // 11f
    { DEFAULT_RATE, NULL }, // 120
    { DEFAULT_RATE, NULL }, // 121
    { DEFAULT_RATE, NULL }, // 122
    { DEFAULT_RATE, NULL }, // 123
    { DEFAULT_RATE, NULL }, // 124
    { DEFAULT_RATE, NULL }, // 125
    { DEFAULT_RATE, NULL }, // 126
    { DEFAULT_RATE, NULL }, // 127
    { DEFAULT_RATE, NULL }, // 128
    { DEFAULT_RATE, NULL }, // 129
    { DEFAULT_RATE, NULL }, // 12a
    { DEFAULT_RATE, NULL }, // 12b
    { DEFAULT_RATE, NULL }, // 12c
    { DEFAULT_RATE, NULL }, // 12d
    { DEFAULT_RATE, NULL }, // 12e
    { DEFAULT_RATE, NULL }, // 12f
    { DEFAULT_RATE, NULL }, // 130
    { DEFAULT_RATE, NULL }, // 131
    { DEFAULT_RATE, NULL }, // 132
    { DEFAULT_RATE, NULL }, // 133
    { DEFAULT_RATE, NULL }, // 134
    { DEFAULT_RATE, NULL }, // 135
    { DEFAULT_RATE, NULL }, // 136
    { DEFAULT_RATE, NULL }, // 137
    { DEFAULT_RATE, NULL }, // 138
    { DEFAULT_RATE, NULL }, // 139
    { DEFAULT_RATE, NULL }, // 13a
    { DEFAULT_RATE, NULL }, // 13b
    { DEFAULT_RATE, NULL }, // 13c
    { DEFAULT_RATE, NULL }, // 13d
    { DEFAULT_RATE, NULL }, // 13e
    { DEFAULT_RATE, NULL }, // 13f
    { DEFAULT_RATE, NULL }, // 140
    { DEFAULT_RATE, NULL }, // 141
    { DEFAULT_RATE, NULL }, // 142
    { std::chrono::milliseconds(300), clif_parse_NpcAmountInput }, // 143
    { DEFAULT_RATE, NULL }, // 144
    { DEFAULT_RATE, NULL }, // 145
    { std::chrono::milliseconds(300), clif_parse_NpcCloseClicked }, // 146
    { DEFAULT_RATE, NULL }, // 147
    { DEFAULT_RATE, NULL }, // 148
    { DEFAULT_RATE, NULL }, // 149
    { DEFAULT_RATE, NULL }, // 14a
    { DEFAULT_RATE, NULL }, // 14b
    { DEFAULT_RATE, NULL }, // 14c
    { DEFAULT_RATE, NULL }, // 14d
    { DEFAULT_RATE, NULL }, // 14e
    { DEFAULT_RATE, NULL }, // 14f
    { DEFAULT_RATE, NULL }, // 150
    { DEFAULT_RATE, NULL }, // 151
    { DEFAULT_RATE, NULL }, // 152
    { DEFAULT_RATE, NULL }, // 153
    { DEFAULT_RATE, NULL }, // 154
    { DEFAULT_RATE, NULL }, // 155
    { DEFAULT_RATE, NULL }, // 156
    { DEFAULT_RATE, NULL }, // 157
    { DEFAULT_RATE, NULL }, // 158
    { DEFAULT_RATE, NULL }, // 159
    { DEFAULT_RATE, NULL }, // 15a
    { DEFAULT_RATE, NULL }, // 15b
    { DEFAULT_RATE, NULL }, // 15c
    { DEFAULT_RATE, NULL }, // 15d
    { DEFAULT_RATE, NULL }, // 15e
    { DEFAULT_RATE, NULL }, // 15f
    { DEFAULT_RATE, NULL }, // 160
    { DEFAULT_RATE, NULL }, // 161
    { DEFAULT_RATE, NULL }, // 162
    { DEFAULT_RATE, NULL }, // 163
    { DEFAULT_RATE, NULL }, // 164
    { DEFAULT_RATE, NULL }, // 165
    { DEFAULT_RATE, NULL }, // 166
    { DEFAULT_RATE, NULL }, // 167
    { DEFAULT_RATE, NULL }, // 168
    { DEFAULT_RATE, NULL }, // 169
    { DEFAULT_RATE, NULL }, // 16a
    { DEFAULT_RATE, NULL }, // 16b
    { DEFAULT_RATE, NULL }, // 16c
    { DEFAULT_RATE, NULL }, // 16d
    { DEFAULT_RATE, NULL }, // 16e
    { DEFAULT_RATE, NULL }, // 16f
    { DEFAULT_RATE, NULL }, // 170
    { DEFAULT_RATE, NULL }, // 171
    { DEFAULT_RATE, NULL }, // 172
    { DEFAULT_RATE, NULL }, // 173
    { DEFAULT_RATE, NULL }, // 174
    { DEFAULT_RATE, NULL }, // 175
    { DEFAULT_RATE, NULL }, // 176
    { DEFAULT_RATE, NULL }, // 177
    { DEFAULT_RATE, NULL }, // 178
    { DEFAULT_RATE, NULL }, // 179
    { DEFAULT_RATE, NULL }, // 17a
    { DEFAULT_RATE, NULL }, // 17b
    { DEFAULT_RATE, NULL }, // 17c
    { DEFAULT_RATE, NULL }, // 17d
    { DEFAULT_RATE, NULL }, // 17e
    { DEFAULT_RATE, NULL }, // 17f
    { DEFAULT_RATE, NULL }, // 180
    { DEFAULT_RATE, NULL }, // 181
    { DEFAULT_RATE, NULL }, // 182
    { DEFAULT_RATE, NULL }, // 183
    { DEFAULT_RATE, NULL }, // 184
    { DEFAULT_RATE, NULL }, // 185
    { DEFAULT_RATE, NULL }, // 186
    { DEFAULT_RATE, NULL }, // 187
    { DEFAULT_RATE, NULL }, // 188
    { DEFAULT_RATE, NULL }, // 189
    { DEFAULT_RATE, clif_parse_QuitGame }, // 18a
    { DEFAULT_RATE, NULL }, // 18b
    { DEFAULT_RATE, NULL }, // 18c
    { DEFAULT_RATE, NULL }, // 18d
    { DEFAULT_RATE, NULL }, // 18e
    { DEFAULT_RATE, NULL }, // 18f
    { DEFAULT_RATE, NULL }, // 190
    { DEFAULT_RATE, NULL }, // 191
    { DEFAULT_RATE, NULL }, // 192
    { DEFAULT_RATE, NULL }, // 193
    { DEFAULT_RATE, NULL }, // 194
    { DEFAULT_RATE, NULL }, // 195
    { DEFAULT_RATE, NULL }, // 196
    { DEFAULT_RATE, NULL }, // 197
    { DEFAULT_RATE, NULL }, // 198
    { DEFAULT_RATE, NULL }, // 199
    { DEFAULT_RATE, NULL }, // 19a
    { DEFAULT_RATE, NULL }, // 19b
    { DEFAULT_RATE, NULL }, // 19c
    { DEFAULT_RATE, NULL }, // 19d
    { DEFAULT_RATE, NULL }, // 19e
    { DEFAULT_RATE, NULL }, // 19f
    { DEFAULT_RATE, NULL }, // 1a0
    { DEFAULT_RATE, NULL }, // 1a1
    { DEFAULT_RATE, NULL }, // 1a2
    { DEFAULT_RATE, NULL }, // 1a3
    { DEFAULT_RATE, NULL }, // 1a4
    { DEFAULT_RATE, NULL }, // 1a5
    { DEFAULT_RATE, NULL }, // 1a6
    { DEFAULT_RATE, NULL }, // 1a7
    { DEFAULT_RATE, NULL }, // 1a8
    { DEFAULT_RATE, NULL }, // 1a9
    { DEFAULT_RATE, NULL }, // 1aa
    { DEFAULT_RATE, NULL }, // 1ab
    { DEFAULT_RATE, NULL }, // 1ac
    { DEFAULT_RATE, NULL }, // 1ad
    { DEFAULT_RATE, NULL }, // 1ae
    { DEFAULT_RATE, NULL }, // 1af
    { DEFAULT_RATE, NULL }, // 1b0
    { DEFAULT_RATE, NULL }, // 1b1
    { DEFAULT_RATE, NULL }, // 1b2
    { DEFAULT_RATE, NULL }, // 1b3
    { DEFAULT_RATE, NULL }, // 1b4
    { DEFAULT_RATE, NULL }, // 1b5
    { DEFAULT_RATE, NULL }, // 1b6
    { DEFAULT_RATE, NULL }, // 1b7
    { DEFAULT_RATE, NULL }, // 1b8
    { DEFAULT_RATE, NULL }, // 1b9
    { DEFAULT_RATE, NULL }, // 1ba
    { DEFAULT_RATE, NULL }, // 1bb
    { DEFAULT_RATE, NULL }, // 1bc
    { DEFAULT_RATE, NULL }, // 1bd
    { DEFAULT_RATE, NULL }, // 1be
    { DEFAULT_RATE, NULL }, // 1bf
    { DEFAULT_RATE, NULL }, // 1c0
    { DEFAULT_RATE, NULL }, // 1c1
    { DEFAULT_RATE, NULL }, // 1c2
    { DEFAULT_RATE, NULL }, // 1c3
    { DEFAULT_RATE, NULL }, // 1c4
    { DEFAULT_RATE, NULL }, // 1c5
    { DEFAULT_RATE, NULL }, // 1c6
    { DEFAULT_RATE, NULL }, // 1c7
    { DEFAULT_RATE, NULL }, // 1c8
    { DEFAULT_RATE, NULL }, // 1c9
    { DEFAULT_RATE, NULL }, // 1ca
    { DEFAULT_RATE, NULL }, // 1cb
    { DEFAULT_RATE, NULL }, // 1cc
    { DEFAULT_RATE, NULL }, // 1cd
    { DEFAULT_RATE, NULL }, // 1ce
    { DEFAULT_RATE, NULL }, // 1cf
    { DEFAULT_RATE, NULL }, // 1d0
    { DEFAULT_RATE, NULL }, // 1d1
    { DEFAULT_RATE, NULL }, // 1d2
    { DEFAULT_RATE, NULL }, // 1d3
    { DEFAULT_RATE, NULL }, // 1d4
    { std::chrono::milliseconds(300), clif_parse_NpcStringInput }, // 1d5
    { DEFAULT_RATE, NULL }, // 1d6
    { DEFAULT_RATE, NULL }, // 1d7
    { DEFAULT_RATE, NULL }, // 1d8
    { DEFAULT_RATE, NULL }, // 1d9
    { DEFAULT_RATE, NULL }, // 1da
    { DEFAULT_RATE, NULL }, // 1db
    { DEFAULT_RATE, NULL }, // 1dc
    { DEFAULT_RATE, NULL }, // 1dd
    { DEFAULT_RATE, NULL }, // 1de
    { DEFAULT_RATE, NULL }, // 1df
    { DEFAULT_RATE, NULL }, // 1e0
    { DEFAULT_RATE, NULL }, // 1e1
    { DEFAULT_RATE, NULL }, // 1e2
    { DEFAULT_RATE, NULL }, // 1e3
    { DEFAULT_RATE, NULL }, // 1e4
    { DEFAULT_RATE, NULL }, // 1e5
    { DEFAULT_RATE, NULL }, // 1e6
    { DEFAULT_RATE, NULL }, // 1e7
    { DEFAULT_RATE, NULL }, // 1e8
    { DEFAULT_RATE, NULL }, // 1e9
    { DEFAULT_RATE, NULL }, // 1ea
    { DEFAULT_RATE, NULL }, // 1eb
    { DEFAULT_RATE, NULL }, // 1ec
    { DEFAULT_RATE, NULL }, // 1ed
    { DEFAULT_RATE, NULL }, // 1ee
    { DEFAULT_RATE, NULL }, // 1ef
    { DEFAULT_RATE, NULL }, // 1f0
    { DEFAULT_RATE, NULL }, // 1f1
    { DEFAULT_RATE, NULL }, // 1f2
    { DEFAULT_RATE, NULL }, // 1f3
    { DEFAULT_RATE, NULL }, // 1f4
    { DEFAULT_RATE, NULL }, // 1f5
    { DEFAULT_RATE, NULL }, // 1f6
    { DEFAULT_RATE, NULL }, // 1f7
    { DEFAULT_RATE, NULL }, // 1f8
    { DEFAULT_RATE, NULL }, // 1f9
    { DEFAULT_RATE, NULL }, // 1fa
    { DEFAULT_RATE, NULL }, // 1fb
    { DEFAULT_RATE, NULL }, // 1fc
    { DEFAULT_RATE, NULL }, // 1fd
    { DEFAULT_RATE, NULL }, // 1fe
    { DEFAULT_RATE, NULL }, // 1ff
    { DEFAULT_RATE, NULL }, // 200
    { DEFAULT_RATE, NULL }, // 201
    { DEFAULT_RATE, NULL }, // 202
    { DEFAULT_RATE, NULL }, // 203
    { DEFAULT_RATE, NULL }, // 204
    { DEFAULT_RATE, NULL }, // 205
    { DEFAULT_RATE, NULL }, // 206
    { DEFAULT_RATE, NULL }, // 207
    { DEFAULT_RATE, NULL }, // 208
    { DEFAULT_RATE, NULL }, // 209
    { DEFAULT_RATE, NULL }, // 20a
    { DEFAULT_RATE, NULL }, // 20b
    { DEFAULT_RATE, NULL }, // 20c
    { DEFAULT_RATE, NULL }, // 20d
    { DEFAULT_RATE, NULL }, // 20e
    { DEFAULT_RATE, NULL }, // 20f
    { DEFAULT_RATE, NULL }, // 210
    { DEFAULT_RATE, NULL }, // 211
    { DEFAULT_RATE, NULL }, // 212
    { DEFAULT_RATE, NULL }, // 213
    { DEFAULT_RATE, NULL }, // 214
    { DEFAULT_RATE, NULL }, // 215
    { DEFAULT_RATE, NULL }, // 216
    { DEFAULT_RATE, NULL }, // 217
    { DEFAULT_RATE, NULL }, // 218
    { DEFAULT_RATE, NULL }, // 219
    { DEFAULT_RATE, NULL }, // 21a
    { DEFAULT_RATE, NULL }, // 21b
    { DEFAULT_RATE, NULL }, // 21c
    { DEFAULT_RATE, NULL }, // 21d
    { DEFAULT_RATE, NULL }, // 21e
    { DEFAULT_RATE, NULL }, // 21f
};

// Checks for packet flooding
static bool clif_check_packet_flood(sint32 fd, sint32 cmd)
{
    MapSessionData *sd = static_cast<MapSessionData *>(session[fd]->session_data);
    tick_t tick = gettick();

    // sd will not be set if the client hasn't requested
    // WantToConnection yet. Do not apply flood logic to GMs
    // as approved bots (GMlvl1) should not have to work around
    // flood logic.
    if (!sd || pc_isGM(sd) || clif_parse_func_table[cmd].rate == INFINITE_RATE)
        return 0;

    // Timer has wrapped
    if (tick < sd->flood_rates[cmd])
    {
        sd->flood_rates[cmd] = tick;
        return 0;
    }

    interval_t rate = clif_parse_func_table[cmd].rate;

    // ActionRequest - attacks are allowed a faster rate than sit/stand
    if (cmd == 0x89)
    {
        uint8 action_type = RFIFOB(fd, 6);
        if (action_type == 0x00 || action_type == 0x07)
            rate = std::chrono::milliseconds(20);
        else
            rate = std::chrono::milliseconds(1000);
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
    PRINTF("clif_validate_chat(): %s (ID %d) sent a malformed"  \
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
static uint8 *clif_validate_chat(MapSessionData *sd, sint32 type,
                                 char **message, size_t *message_len)
{
    sint32 fd;
    uint32 buf_len;       /* Actual message length. */
    uint32 msg_len;       /* Reported message length. */
    uint32 min_len;       /* Minimum message length. */
    size_t name_len;            /* Sender's name length. */
    uint8 *buf = NULL;           /* Copy of actual message data. */

    *message = NULL;
    *message_len = 0;

    nullpo_retr(NULL, sd);
    /*
     * Don't send chat in the period between the ban and the connection's
     * closure.
     */
    if (type < 0 || type > 2 || sd->state.auto_ban_in_progress)
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

    CREATE(buf, uint8, buf_len);
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
static void clif_parse(sint32 fd)
{
    sint32 packet_len = 0, cmd = 0;
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
                PRINTF("Player with account [%d] has logged off your server.\n", sd->id);   // Player logout display [Yor]
        }
        else if (sd)
        {                       // not authentified! (refused by char-server or disconnect before to be authentified)
            PRINTF("Player with account [%d] has logged off your server (not auth account).\n", sd->id);    // Player logout display [Yor]
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
    if (packet_len == VAR)
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
                sint32 i;
                FILE *fp;
                char packet_txt[256] = "save/packet.txt";
                time_t now;
                printf("---- 00-01-02-03-04-05-06-07-08-09-0A-0B-0C-0D-0E-0F");
                for (i = 0; i < packet_len; i++)
                {
                    if ((i & 15) == 0)
                        printf("\n%04X ", i);
                    printf("%02X ", RFIFOB(fd, i));
                }
                if (sd && sd->state.auth)
                {
                    if (sd->status.name)
                        PRINTF("\nAccount ID %d, character ID %d, player name %s.\n",
                               sd->status.account_id, sd->status.char_id,
                               sd->status.name);
                    else
                        PRINTF("\nAccount ID %d.\n", sd->id);
                }
                else if (sd)    // not authentified! (refused by char-server or disconnect before to be authentified)
                    PRINTF("\nAccount ID %d.\n", sd->id);

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
                            FPRINTF(fp,
                                     "%sPlayer with account ID %d (character ID %d, player name %s) sent wrong packet:\n",
                                     asctime(gmtime(&now)),
                                     sd->status.account_id,
                                     sd->status.char_id, sd->status.name);
                        else
                            FPRINTF(fp,
                                     "%sPlayer with account ID %d sent wrong packet:\n",
                                     asctime(gmtime(&now)), sd->id);
                    }
                    else if (sd)    // not authentified! (refused by char-server or disconnect before to be authentified)
                        FPRINTF(fp,
                                 "%sPlayer with account ID %d sent wrong packet:\n",
                                 asctime(gmtime(&now)), sd->id);

                    FPRINTF(fp,
                            "\t---- 00-01-02-03-04-05-06-07-08-09-0A-0B-0C-0D-0E-0F");
                    for (i = 0; i < packet_len; i++)
                    {
                        if ((i & 15) == 0)
                            FPRINTF(fp, "\n\t%04X ", i);
                        FPRINTF(fp, "%02X ", RFIFOB(fd, i));
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
    for (sint32 i = 0; i < 10; i++)
    {
        if (make_listen_port(map_port))
            return;
        sleep(20);
    }
    map_log("can't bind port %hu\n", map_port);
    exit(1);
}
