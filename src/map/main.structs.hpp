#ifndef MAIN_STRUCTS
#define MAIN_STRUCTS

// TODO: improve this include location and contents
// for now I'm putting it here because it's useful
# include "precompiled.hpp"

# include "battle.structs.hpp"
# include "itemdb.structs.hpp"
# include "mob.structs.hpp"
# include "path.structs.hpp"
# include "script.structs.hpp"

# include "../lib/dmap.hpp"
# include "../lib/earray.hpp"

# include "../common/socket.structs.hpp"
# include "../common/mmo.hpp"

# define MAX_NPC_PER_MAP 512
# define BLOCK_SIZE 8
# define AREA_SIZE battle_config.area_size
# define LOCAL_REG_NUM 16
# define LIFETIME_FLOORITEM 60
# define DAMAGELOG_SIZE 30
# define LOOTITEM_SIZE 10
# define MAX_SKILL_LEVEL 100
# define MAX_STATUSCHANGE 200
# define MAX_EVENTQUEUE 2
# define MAX_EVENTTIMER 32
constexpr std::chrono::milliseconds NATURAL_HEAL_INTERVAL(500);
# define MAX_DROP_PER_MAP 48

//  Below are special clif_changestatus() IDs reserved for option updates
# define CLIF_OPTION_SC_BASE         0x1000
# define CLIF_OPTION_SC_INVISIBILITY (CLIF_OPTION_SC_BASE)
# define CLIF_OPTION_SC_SCRIBE       (CLIF_OPTION_SC_BASE + 1)

typedef account_t BlockID;

enum BlockType
{
    BL_NUL,
    BL_PC,
    BL_NPC,
    BL_MOB,
    BL_ITEM,
    BL_SPELL
};
enum NPC_Subtype
{
    WARP,
    SHOP,
    SCRIPT,
    MESSAGE
};

class BlockList
{
public:
    BlockList *next, *prev;
    // these IDs are unique across all types
    const BlockID id;
    // TODO change to map_data_local *
    uint16 m;
    uint16 x, y;
    const BlockType type;

    BlockList(BlockType t, BlockID id_, uint16 m_ = 0, uint16 x_ = 0, uint16 y_ = 0)
    : next()
    , prev()
    , id(id_)
    , m(m_)
    , x(x_)
    , y(y_)
    , type(t)
    {}
    BlockList(const BlockList&) = delete;
    virtual ~BlockList() {}

    void *operator new(size_t sz)
    {
        // we used to calloc
        return memset(::operator new(sz), '\0', sz);
    }
};

// catch bugs
void free(BlockList *) = delete;
// TODO: remove this after splitting the headers
BlockID map_addobject(BlockList *);

struct status_change
{
    timer_id timer;
    sint32 val1;
    BlockID spell_invocation;      /* [Fate] If triggered by a spell, record here */
};

class invocation_t;

struct quick_regeneration
{                               // [Fate]
    sint32 amount;                // Amount of HP/SP left to regenerate
    uint8 speed;        // less is faster (number of half-second ticks to wait between updates)
    uint8 tickdelay;    // number of ticks to next update
};

class MapSessionData : public BlockList, public SessionData
{
public:
    struct
    {
        bool auto_ban_in_progress:1;
        bool auth:1;
        bool change_walk_target:1;
        bool attack_continue:1;
        bool menu_or_input:1;
        uint32 dead_sit:2;
        bool waitingdisconnect:1;
        uint32 lr_flag:2;
        bool connect_new:1;
        bool arrow_atk:1;
        bool attack_type_is_weapon:1;
        bool produce_flag:1;
        bool make_arrow_flag:1;
        // TODO replace with a more generic storage system
        uint32 storage_flag:1;    //0: closed, 1: Normal Storage open
        bool shroud_active:1;
        bool shroud_hides_name_talking:1;
        bool shroud_disappears_on_pickup:1;
        bool shroud_disappears_on_talk:1;
    } state;
    struct
    {
        bool killer:1;
        bool killable:1;
        bool restart_full_recover:1;
        bool no_castcancel:1;
        bool no_castcancel2:1;
        bool no_magic_damage:1;
        bool no_weapon_damage:1;
        bool no_gemstone:1;
    } special_state;
    charid_t char_id;
    sint32 login_id1, login_id2, sex;
    uint8 tmw_version;  // tmw client version
    struct mmo_charstatus status;
    struct item_data *inventory_data[MAX_INVENTORY];
    earray<sint16, EQUIP, EQUIP::COUNT> equip_index;
    sint32 weight, max_weight;
    fixed_string<16> mapname;
    sint32 fd, new_fd;
    sint16 to_x, to_y;
    interval_t speed;
    sint16 opt1, opt2, opt3;
    Direction dir, head_dir;
    struct walkpath_data walkpath;
    timer_id walktimer;
    BlockID npc_id, areanpc_id, npc_shopid;
    sint32 npc_pos;
    sint32 npc_menu;
    sint32 npc_amount;
    const Script *npc_script, *npc_scriptroot;
    std::vector<script_data> npc_stackbuf;
    char npc_str[256];
    struct
    {
        bool storage:1;
        bool divorce:1;
    } npc_flags;

    timer_id attacktimer;
    BlockID attacktarget;
    AttackResult attacktarget_lv;
    tick_t attackabletime;

    /// Used with the GM commands to iterate over players
    account_t followtarget;

    tick_t cast_tick;     // [Fate] Next tick at which spellcasting is allowed
    std::set<invocation_t *> active_spells;
    BlockID attack_spell_override; // [Fate] When an attack spell is active for this player, they trigger it
    // like a weapon.  Check pc_attack_timer() for details.
    sint16 attack_spell_icon_override;   // Weapon equipment slot (slot 4) item override
    sint16 attack_spell_look_override;   // Weapon `look' (attack animation) override
    sint16 attack_spell_charges; // [Fate] Remaining number of charges for the attack spell
    interval_t attack_spell_delay;   // [Fate] ms delay after spell attack
    sint16 attack_spell_range;   // [Fate] spell range
    sint16 spellpower_bonus_target, spellpower_bonus_current;    // [Fate] Spellpower boni.  _current is the active one.
    //_current slowly approximates _target, and _target is determined by equipment.

    sint16 attackrange, attackrange_;

    // [Fate] Used for gradual healing; amount of enqueued regeneration
    struct quick_regeneration quick_regeneration_hp, quick_regeneration_sp;
    // [Fate] XP that can be extracted from this player by healing
    sint32 heal_xp;               // i.e., OTHER players (healers) can partake in this player's XP

    timer_id invincible_timer;
    tick_t canact_tick;
    tick_t canmove_tick;
    tick_t canlog_tick;
    interval_t hp_sub, sp_sub;
    interval_t inchealhptick, inchealsptick;
    // -- moonsoul (new tick for berserk self-damage)
    sint32 fame;

    sint16 weapontype1, weapontype2;
    earray<sint32, ATTR, ATTR::COUNT> paramb, paramc, parame;
    sint32 hit, flee, flee2;
    interval_t aspd, amotion, dmotion;
    sint32 watk, watk2;
    sint32 def, def2, mdef, mdef2, critical, matk1, matk2;
    sint32 atk_ele, def_ele, star;
    sint32 castrate, hprate, sprate, dsprate;
    sint32 watk_, watk_2;    //二刀流のために追加
    sint32 atk_ele_, star_;  //二刀流のために追加
    sint32 base_atk, atk_rate;
    sint32 arrow_atk, arrow_ele, arrow_cri, arrow_hit, arrow_range;
    sint32 nhealhp, nhealsp, nshealhp, nshealsp, nsshealhp, nsshealsp;
    sint32 aspd_rate, speed_rate, hprecov_rate, sprecov_rate, critical_def,
    double_rate;
    sint32 near_attack_def_rate, long_attack_def_rate, magic_def_rate,
    misc_def_rate;
    sint32 matk_rate, ignore_def_ele, ignore_def_race, ignore_def_ele_,
    ignore_def_race_;
    sint32 ignore_mdef_ele, ignore_mdef_race;
    sint32 perfect_hit, get_zeny_num;
    sint32 critical_rate, hit_rate, flee_rate, flee2_rate, def_rate, def2_rate,
    mdef_rate, mdef2_rate;
    sint32 def_ratio_atk_ele, def_ratio_atk_ele_, def_ratio_atk_race,
    def_ratio_atk_race_;

    sint32 double_add_rate, speed_add_rate, aspd_add_rate, perfect_hit_add,
    get_zeny_add_num;
    sint16 splash_range, splash_add_range;
    sint32 short_weapon_damage_return, long_weapon_damage_return;
    sint16 break_weapon_rate, break_armor_rate;
    sint16 add_steal_rate;

    sint32 magic_damage_return;   // AppleGirl Was Here
    sint32 random_attack_increase_add, random_attack_increase_per;    // [Valaris]
    sint32 perfect_hiding;        // [Valaris]

    sint32 die_counter;
    sint16 doridori_counter;

    DMap<sint32, sint32> reg;
    DMap<sint32, std::string> regstr;

    struct status_change sc_data[MAX_STATUSCHANGE];
    sint16 sc_count;

    account_t trade_partner;
    sint32 deal_item_index[10];
    sint32 deal_item_amount[10];
    sint32 deal_zeny;
    sint16 deal_locked;

    bool party_sent;
    party_t party_invite;
    account_t party_invite_account;
    sint32 party_hp, party_x, party_y;

    char message[80];

    sint32 catch_target_class;

    sint32 pvp_point, pvp_rank, pvp_lastusers;
    timer_id pvp_timer;

    char eventqueue[MAX_EVENTQUEUE][50];
    struct
    {
        timer_id tid;
        char *name;
    } eventtimer[MAX_EVENTTIMER];

    time_t chat_reset_due;
    time_t chat_repeat_reset_due;
    sint32 chat_lines_in;
    sint32 chat_total_repeats;
    char chat_lastmsg[513];

    tick_t flood_rates[0x220];
    time_t packet_flood_reset_due;
    sint32 packet_flood_in;

    MapSessionData(BlockID id_, uint16 m_ = 0, uint16 x_ = 0, uint16 y_ = 0)
    : BlockList(BL_PC, id_, m_, x_, y_)
    {}
};

struct npc_timerevent_list
{
    interval_t timer;
    sint32 pos;
};
struct npc_label_list
{
    char name[24];
    sint32 pos;
};
struct npc_item_list
{
    sint32 nameid, value;
};
struct npc_data : public BlockList
{
    const NPC_Subtype subtype;
    sint16 n;
    sint16 npc_class;
    Direction dir;
    interval_t speed;
    char name[24];
    char exname[24];
    sint16 opt1, opt2, opt3;
    OPTION option;
    sint16 flag;
    // ここにメンバを追加してはならない(shop_itemが可変長の為)

    char eventqueue[MAX_EVENTQUEUE][50];
    struct
    {
        timer_id tid;
        char *name;
    } eventtimer[MAX_EVENTTIMER];

    sint16 arenaflag;

protected:
    npc_data(NPC_Subtype sub, BlockID id_, uint16 m_ = 0, uint16 x_ = 0, uint16 y_ = 0)
    : BlockList(BL_NPC, id_, m_, x_, y_)
    , subtype(sub)
    {}
    virtual ~npc_data();
};

struct npc_data_script : npc_data
{
    struct
    {
        const std::vector<Script> script;
        sint16 xs, ys;
        timer_id timerid;
        interval_t timer;
        sint32 timeramount;
        sint32 nexttimer;
        tick_t timertick;
        struct npc_timerevent_list *timer_event;
        sint32 label_list_num;
        struct npc_label_list *label_list;
        sint32 src_id;
    } scr;
    npc_data_script(std::vector<Script>&& s, BlockID id_, uint16 m_ = 0, uint16 x_ = 0, uint16 y_ = 0)
    : npc_data(SCRIPT, id_, m_, x_, y_)
    , scr({script : s})
    {}
    ~npc_data_script();
};
struct npc_data_shop : npc_data
{
    std::vector<struct npc_item_list> shop_item;

    npc_data_shop(BlockID id_, uint16 m_ = 0, uint16 x_ = 0, uint16 y_ = 0)
    : npc_data(SHOP, id_, m_, x_, y_)
    {}
};
struct npc_data_warp : npc_data
{
    struct
    {
        sint16 xs, ys;
        Point dst;
    } warp;
    npc_data_warp(BlockID id_, uint16 m_ = 0, uint16 x_ = 0, uint16 y_ = 0)
    : npc_data(WARP, id_, m_, x_, y_)
    {}
    ~npc_data_warp() {}
};
struct npc_data_message : npc_data
{
    char *message;          // for MESSAGE: only send this message

    npc_data_message(BlockID id_, uint16 m_ = 0, uint16 x_ = 0, uint16 y_ = 0)
    : npc_data(MESSAGE, id_, m_, x_, y_)
    {}
    ~npc_data_message();
};

enum mob_stat
{
    MOB_LV,
    MOB_MAX_HP,
    MOB_STR, MOB_AGI, MOB_VIT, MOB_INT, MOB_DEX, MOB_LUK,
    MOB_ATK1, MOB_ATK2,         // low and high attacks
    MOB_ADELAY,                 // attack delay
    MOB_DEF, MOB_MDEF,
    MOB_SPEED,
    // These must come last:
    MOB_XP_BONUS,               /* [Fate] Encoded as base to 1024: 1024 means 100% */
    MOB_LAST
};

#define MOB_XP_BONUS_BASE  1024
#define MOB_XP_BONUS_SHIFT 10

enum class MS : uint8
{
    IDLE,
    WALK,
    ATTACK,
    DEAD,
};

constexpr interval_t SPAWN_ONCE_DELAY = interval_t::zero();

struct mob_data : public BlockList
{
    sint16 n;
    sint16 base_class, mob_class;
    MobMode mode;
    Direction dir;
    sint16 m_0, x_0, y_0, xs, ys;
    char name[24];
    interval_t spawndelay_1, spawndelay2;
    struct
    {
        MS state;
        bool target_attackable:1;
        bool steal_flag:1;
        bool steal_coin_flag:1;
        bool master_check:1;
        bool change_walk_target:1;
        bool walk_easy:1;
        uint32 special_mob_ai:3;
    } state;
    timer_id timer;
    sint16 to_x, to_y;
    sint32 hp;
    BlockID target_id, attacked_id;
    AttackResult target_lv;
    struct walkpath_data walkpath;
    tick_t next_walktime;
    tick_t attackabletime;
    tick_t last_deadtime, last_spawntime, last_thinktime;
    tick_t canmove_tick;
    sint16 move_fail_count;
    struct
    {
        BlockID id;
        sint32 dmg;
    } dmglog[DAMAGELOG_SIZE];
    struct item *lootitem;
    sint16 lootitem_count;

    struct status_change sc_data[MAX_STATUSCHANGE];
    sint16 sc_count;
    sint16 opt1, opt2, opt3;
    OPTION option;
    sint16 min_chase;
    timer_id deletetimer;

    sint32 def_ele;
    BlockID master_id;
    sint32 master_dist;
    sint32 exclusion_src, exclusion_party;
    char npc_event[50];
    uint16 stats[MOB_LAST]; // [Fate] mob-specific stats
    sint16 size;

    mob_data(BlockID id_, uint16 m_ = 0, uint16 x_ = 0, uint16 y_ = 0)
    : BlockList(BL_MOB, id_, m_, x_, y_)
    // TODO initialize all that stuff
    {}
    ~mob_data()
    {
        delete lootitem;
    }
};

class map_data
{
public:
    fixed_string<16> name;
    // NULL for maps on other map servers
    MapCell *gat;
};
// it would be nice if at least size info was available for remote maps
class map_data_remote : public map_data
{
public:
    IP_Address ip;
    in_port_t port;
};
class map_data_local : public map_data
{
public:
    BlockList **block;
    BlockList **block_mob;
    sint32 *block_count, *block_mob_count;
    sint32 m;
    sint16 xs, ys;
    sint16 bxs, bys;
    sint32 npc_num;
    sint32 users;
    struct
    {
        bool alias:1;
        bool nomemo:1;
        bool noteleport:1;
        bool noreturn:1;
        bool monster_noteleport:1;
        bool nosave:1;
        bool nobranch:1;
        bool nopenalty:1;
        bool pvp:1;
        bool pvp_noparty:1;
        bool pvp_nightmaredrop:1;
        bool pvp_nocalcrank:1;
        bool nozenypenalty:1;
        bool notrade:1;
        bool nowarp:1;
        bool nowarpto:1;
        bool nopvp:1;       // [Valaris]
        bool no_player_drops:1; // [Jaxad0127]
        bool town:1;        // [remoitnane]
    } flag;
    Point save;
    struct npc_data *npc[MAX_NPC_PER_MAP];
    struct
    {
        sint32 drop_id;
        sint32 drop_type;
        sint32 drop_per;
    } drop_list[MAX_DROP_PER_MAP];
};
#define read_gat(m, x, y)   (maps[m].gat[(x) + (y) * maps[m].xs])
#define read_gatp(m, x, y)  (m->gat[(x) + (y) * m->xs])

struct flooritem_data : public BlockList
{
    sint16 subx, suby;
    timer_id cleartimer;
    BlockID first_get_id, second_get_id, third_get_id;
    tick_t first_get_tick, second_get_tick, third_get_tick;
    struct item item_data;

    flooritem_data(sint32 m_, uint16 x_, uint16 y_)
    : BlockList(BL_ITEM, map_addobject(this), m_, x_, y_)
    , subx()
    , suby()
    , cleartimer()
    , first_get_id()
    , second_get_id()
    , first_get_tick()
    , second_get_tick()
    , third_get_tick()
    , item_data()
    {}
};

enum class SP : uint16
{
    NONE                        = 0xffff,

    SPEED                       = 0,
    BASEEXP                     = 1,
    JOBEXP                      = 2,

    HP                          = 5,
    MAXHP                       = 6,
    SP                          = 7,
    MAXSP                       = 8,
    STATUSPOINT                 = 9,
    BASELEVEL                   = 11,
    SKILLPOINT                  = 12,

    STR                         = 13,
    AGI                         = 14,
    VIT                         = 15,
    INT                         = 16,
    DEX                         = 17,
    LUK                         = 18,

    ZENY                        = 20,
    SEX                         = 21,
    NEXTBASEEXP                 = 22,
    NEXTJOBEXP                  = 23,
    WEIGHT                      = 24,
    MAXWEIGHT                   = 25,

    USTR                        = 32,
    UAGI                        = 33,
    UVIT                        = 34,
    UINT                        = 35,
    UDEX                        = 36,
    ULUK                        = 37,

    ATK1                        = 41,
    ATK2                        = 42,
    MATK1                       = 43,
    MATK2                       = 44,
    DEF1                        = 45,
    DEF2                        = 46,
    MDEF1                       = 47,
    MDEF2                       = 48,
    HIT                         = 49,
    FLEE1                       = 50,
    FLEE2                       = 51,
    CRITICAL                    = 52,
    ASPD                        = 53,

    JOBLEVEL                    = 55,

    FAME                        = 59,
    UNBREAKABLE                 = 60,

    GM                          = 500,

    ATTACKRANGE                 = 1000,
    ATKELE                      = 1001,
    DEFELE                      = 1002,
    CASTRATE                    = 1003,
    MAXHPRATE                   = 1004,
    MAXSPRATE                   = 1005,
    SPRATE                      = 1006,

    BASE_ATK                    = 1014,
    ASPD_RATE                   = 1015,
    HP_RECOV_RATE               = 1016,
    SP_RECOV_RATE               = 1017,
    SPEED_RATE                  = 1018,
    CRITICAL_DEF                = 1019,
    NEAR_ATK_DEF                = 1020,
    LONG_ATK_DEF                = 1021,
    DOUBLE_RATE                 = 1022,
    DOUBLE_ADD_RATE             = 1023,
    MATK                        = 1024,
    MATK_RATE                   = 1025,
    IGNORE_DEF_ELE              = 1026,
    IGNORE_DEF_RACE             = 1027,
    ATK_RATE                    = 1028,
    SPEED_ADDRATE               = 1029,
    ASPD_ADDRATE                = 1030,
    MAGIC_ATK_DEF               = 1031,
    MISC_ATK_DEF                = 1032,
    IGNORE_MDEF_ELE             = 1033,
    IGNORE_MDEF_RACE            = 1034,

    PERFECT_HIT_RATE            = 1038,
    PERFECT_HIT_ADD_RATE        = 1039,
    CRITICAL_RATE               = 1040,
    GET_ZENY_NUM                = 1041,
    ADD_GET_ZENY_NUM            = 1042,

    DEF_RATIO_ATK_ELE           = 1048,
    DEF_RATIO_ATK_RACE          = 1049,
    ADD_SPEED                   = 1050,
    HIT_RATE                    = 1051,
    FLEE_RATE                   = 1052,
    FLEE2_RATE                  = 1053,
    DEF_RATE                    = 1054,
    DEF2_RATE                   = 1055,
    MDEF_RATE                   = 1056,
    MDEF2_RATE                  = 1057,
    SPLASH_RANGE                = 1058,
    SPLASH_ADD_RANGE            = 1059,

    SHORT_WEAPON_DAMAGE_RETURN  = 1063,
    LONG_WEAPON_DAMAGE_RETURN   = 1064,

    MAGIC_DAMAGE_RETURN         = 1071,

    ALL_STATS                   = 1073,
    AGI_VIT                     = 1074,
    AGI_DEX_STR                 = 1075,
    PERFECT_HIDE                = 1076,

    RESTART_FULL_RECORVER       = 2000,
    NO_CASTCANCEL               = 2001,

    NO_MAGIC_DAMAGE             = 2003,
    NO_WEAPON_DAMAGE            = 2004,
    NO_GEMSTONE                 = 2005,
    NO_CASTCANCEL2              = 2006,
};

constexpr bool SP_IS_BASE_ATTR(SP type)
{
    return type >= SP::STR && type <= SP::LUK;
}

constexpr SP ATTR_TO_SP_BASE(ATTR attr)
{
    return SP(sint32(attr) + sint32(SP::STR));
}
constexpr SP ATTR_TO_SP_UP(ATTR attr)
{
    return SP(sint32(attr) + sint32(SP::USTR));
}
constexpr ATTR ATTR_FROM_SP_BASE(SP sp)
{
    return ATTR(sint32(sp) - sint32(SP::STR));
}
constexpr ATTR ATTR_FROM_SP_UP(SP sp)
{
    return ATTR(sint32(sp) - sint32(SP::USTR));
}

enum class LOOK : uint8
{
    BASE = 0,
    HAIR = 1,
    WEAPON = 2,
    LEGS = 3,
    HEAD = 4,
    CHEST = 5,
    HAIR_COLOR = 6,

    SHIELD = 8,
    SHOES = 9,
    GLOVES = 10,
    CAPE = 11,
    MISC1 = 12,
    MISC2 = 13,

    COUNT = 14
};

extern template class DMap<sint32, sint32>;
extern template class DMap<sint32, std::string>;
extern template class std::set<invocation_t *>;
extern template class std::vector<npc_item_list>;

#endif //MAIN_STRUCTS
