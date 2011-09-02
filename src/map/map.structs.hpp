#ifndef MAP_STRUCTS
#define MAP_STRUCTS

# include "battle.structs.hpp"
# include "itemdb.structs.hpp"
# include "path.structs.hpp"

# include "../lib/earray.hpp"

# include "../common/socket.structs.hpp"
# include "../common/mmo.hpp"

# include <cstring>
# include <cstdlib>

# include <vector>
# include <set>

# define MAX_PC_CLASS (1+6+6+1+6+1+1+1+1+4023)
# define PC_CLASS_BASE 0
# define PC_CLASS_BASE2 (PC_CLASS_BASE + 4001)
# define PC_CLASS_BASE3 (PC_CLASS_BASE2 + 22)
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
# define NATURAL_HEAL_INTERVAL 500
# define MAX_FLOORITEM 500000
# define MAX_LEVEL 255
# define MAX_DROP_PER_MAP 48

# define DEFAULT_AUTOSAVE_INTERVAL 60*1000

// [Fate] status.option properties.  These are persistent status changes.
// IDs that are not listed are not used in the code (to the best of my knowledge)
# define OPTION_HIDE2        0x0002  // apparently some weaker non-GM hide
# define OPTION_CLOAK        0x0004
# define OPTION_10           0x0010
# define OPTION_20           0x0020
# define OPTION_HIDE         0x0040  // [Fate] This is the GM `@hide' flag
# define OPTION_800          0x0800
# define OPTION_INVISIBILITY 0x1000  // [Fate] Complete invisibility to other clients
# define OPTION_SCRIBE       0x2000  // [Fate] Auto-logging of nearby comments
# define OPTION_CHASEWALK    0x4000

//  Below are special clif_changestatus() IDs reserved for option updates
# define CLIF_OPTION_SC_BASE         0x1000
# define CLIF_OPTION_SC_INVISIBILITY (CLIF_OPTION_SC_BASE)
# define CLIF_OPTION_SC_SCRIBE       (CLIF_OPTION_SC_BASE + 1)

enum BlockType
{ BL_NUL, BL_PC, BL_NPC, BL_MOB, BL_ITEM, BL_SPELL };
enum NPC_Subtype
{ WARP, SHOP, SCRIPT, MESSAGE };
class BlockList
{
public:
    BlockList *next, *prev;
    // different kind of ID depending on type: mob, pc, npc?
    uint32_t id;
    uint16_t m, x, y;
    const BlockType type;

    BlockList(BlockType t) : type(t) {}
    virtual ~BlockList() {}

    void *operator new(size_t sz)
    {
        // we used to calloc
        return memset(::operator new(sz), '\0', sz);
    }
};

// catch bugs
void free(BlockList *) = delete;

struct script_reg
{
    int index;
    int data;
};
struct script_regstr
{
    int index;
    char data[256];
};
struct status_change
{
    timer_id timer;
    int val1;
    int spell_invocation;      /* [Fate] If triggered by a spell, record here */
};

class invocation_t;

struct quick_regeneration
{                               // [Fate]
    int amount;                // Amount of HP/SP left to regenerate
    uint8_t speed;        // less is faster (number of half-second ticks to wait between updates)
    uint8_t tickdelay;    // number of ticks to next update
};

class MapSessionData : public SessionData, public BlockList
{
public:
    struct
    {
        bool auto_ban_in_progress:1;
        bool auth:1;
        bool change_walk_target:1;
        bool attack_continue:1;
        bool menu_or_input:1;
        unsigned dead_sit:2;
        bool waitingdisconnect:1;
        unsigned lr_flag:2;
        bool connect_new:1;
        bool arrow_atk:1;
        unsigned attack_type:3;
        bool produce_flag:1;
        bool make_arrow_flag:1;
        // TODO replace with a more generic storage system
        unsigned storage_flag:1;    //0: closed, 1: Normal Storage open
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
    int char_id, login_id1, login_id2, sex;
    uint8_t tmw_version;  // tmw client version
    struct mmo_charstatus status;
    struct item_data *inventory_data[MAX_INVENTORY];
    earray<short, EQUIP, EQUIP::COUNT> equip_index;
    int weight, max_weight;
    fixed_string<16> mapname;
    int fd, new_fd;
    short to_x, to_y;
    short speed, prev_speed;
    short opt1, opt2, opt3;
    Direction dir, head_dir;
    struct walkpath_data walkpath;
    timer_id walktimer;
    int npc_id, areanpc_id, npc_shopid;
    int npc_pos;
    int npc_menu;
    int npc_amount;
    int npc_stack, npc_stackmax;
    const char *npc_script, *npc_scriptroot;
    struct script_data *npc_stackbuf;
    char npc_str[256];
    struct
    {
        bool storage:1;
        bool divorce:1;
    } npc_flags;

    timer_id attacktimer;
    int attacktarget;
    AttackResult attacktarget_lv;
    unsigned int attackabletime;

    /// Used with the GM commands to iterate over players
    int followtarget;

    unsigned int cast_tick;     // [Fate] Next tick at which spellcasting is allowed
    std::set<invocation_t *> active_spells;
    int attack_spell_override; // [Fate] When an attack spell is active for this player, they trigger it
    // like a weapon.  Check pc_attack_timer() for details.
    short attack_spell_icon_override;   // Weapon equipment slot (slot 4) item override
    short attack_spell_look_override;   // Weapon `look' (attack animation) override
    short attack_spell_charges; // [Fate] Remaining number of charges for the attack spell
    short attack_spell_delay;   // [Fate] ms delay after spell attack
    short attack_spell_range;   // [Fate] spell range
    short spellpower_bonus_target, spellpower_bonus_current;    // [Fate] Spellpower boni.  _current is the active one.
    //_current slowly approximates _target, and _target is determined by equipment.

    short attackrange, attackrange_;

    // [Fate] Used for gradual healing; amount of enqueued regeneration
    struct quick_regeneration quick_regeneration_hp, quick_regeneration_sp;
    // [Fate] XP that can be extracted from this player by healing
    int heal_xp;               // i.e., OTHER players (healers) can partake in this player's XP

    timer_id invincible_timer;
    unsigned int canact_tick;
    unsigned int canmove_tick;
    unsigned int canlog_tick;
    int hp_sub, sp_sub;
    int inchealhptick, inchealsptick, inchealspirithptick,
    inchealspiritsptick;
    // -- moonsoul (new tick for berserk self-damage)
    int berserkdamagetick;
    int fame;

    short weapontype1, weapontype2;
    earray<int, ATTR, ATTR::COUNT> paramb, paramc, parame;
    int hit, flee, flee2, aspd, amotion, dmotion;
    int watk, watk2;
    int def, def2, mdef, mdef2, critical, matk1, matk2;
    int atk_ele, def_ele, star;
    int castrate, hprate, sprate, dsprate;
    int watk_, watk_2;    //二刀流のために追加
    int atk_ele_, star_;  //二刀流のために追加
    int base_atk, atk_rate;
    int arrow_atk, arrow_ele, arrow_cri, arrow_hit, arrow_range;
    int nhealhp, nhealsp, nshealhp, nshealsp, nsshealhp, nsshealsp;
    int aspd_rate, speed_rate, hprecov_rate, sprecov_rate, critical_def,
    double_rate;
    int near_attack_def_rate, long_attack_def_rate, magic_def_rate,
    misc_def_rate;
    int matk_rate, ignore_def_ele, ignore_def_race, ignore_def_ele_,
    ignore_def_race_;
    int ignore_mdef_ele, ignore_mdef_race;
    int perfect_hit, get_zeny_num;
    int critical_rate, hit_rate, flee_rate, flee2_rate, def_rate, def2_rate,
    mdef_rate, mdef2_rate;
    int def_ratio_atk_ele, def_ratio_atk_ele_, def_ratio_atk_race,
    def_ratio_atk_race_;

    int double_add_rate, speed_add_rate, aspd_add_rate, perfect_hit_add,
    get_zeny_add_num;
    short splash_range, splash_add_range;
    int short_weapon_damage_return, long_weapon_damage_return;
    short break_weapon_rate, break_armor_rate;
    short add_steal_rate;

    int magic_damage_return;   // AppleGirl Was Here
    int random_attack_increase_add, random_attack_increase_per;    // [Valaris]
    int perfect_hiding;        // [Valaris]

    int die_counter;
    short doridori_counter;

    int reg_num;
    struct script_reg *reg;
    int regstr_num;
    struct script_regstr *regstr;

    struct status_change sc_data[MAX_STATUSCHANGE];
    short sc_count;

    int trade_partner;
    int deal_item_index[10];
    int deal_item_amount[10];
    int deal_zeny;
    short deal_locked;

    int party_sended, party_invite, party_invite_account;
    int party_hp, party_x, party_y;

    char message[80];

    int catch_target_class;

    int pvp_point, pvp_rank, pvp_lastusers;
    timer_id pvp_timer;

    char eventqueue[MAX_EVENTQUEUE][50];
    struct
    {
        timer_id tid;
        char *name;
    } eventtimer[MAX_EVENTTIMER];

    time_t chat_reset_due;
    time_t chat_repeat_reset_due;
    int chat_lines_in;
    int chat_total_repeats;
    char chat_lastmsg[513];

    unsigned int flood_rates[0x220];
    time_t packet_flood_reset_due;
    int packet_flood_in;

    MapSessionData() : BlockList(BL_PC) {}
};

struct npc_timerevent_list
{
    int timer, pos;
};
struct npc_label_list
{
    char name[24];
    int pos;
};
struct npc_item_list
{
    int nameid, value;
};
struct npc_data : public BlockList
{
    const NPC_Subtype subtype;
    short n;
    short npc_class;
    Direction dir;
    short speed;
    char name[24];
    char exname[24];
    short opt1, opt2, opt3, option;
    short flag;
    // ここにメンバを追加してはならない(shop_itemが可変長の為)

    char eventqueue[MAX_EVENTQUEUE][50];
    struct
    {
        timer_id tid;
        char *name;
    } eventtimer[MAX_EVENTTIMER];

    short arenaflag;

protected:
    npc_data(NPC_Subtype sub) : BlockList(BL_NPC), subtype(sub) {}
    ~npc_data();
};

struct npc_data_script : npc_data
{
    struct
    {
        const char *script;
        short xs, ys;
        timer_id timerid;
        int timer, timeramount, nexttimer;
        unsigned int timertick;
        struct npc_timerevent_list *timer_event;
        int label_list_num;
        struct npc_label_list *label_list;
        int src_id;
    } scr;
    npc_data_script() : npc_data(SCRIPT) {}
    ~npc_data_script();
};
struct npc_data_shop : npc_data
{
    std::vector<struct npc_item_list> shop_item;

    npc_data_shop() : npc_data(SHOP) {}
};
struct npc_data_warp : npc_data
{
    struct
    {
        short xs, ys;
        Point dst;
    } warp;
    npc_data_warp() : npc_data(WARP) {}
    ~npc_data_warp() {}
};
struct npc_data_message : npc_data
{
    char *message;          // for MESSAGE: only send this message

    npc_data_message() : npc_data(MESSAGE) {}
    ~npc_data_message();
};

#define MOB_MODE_SUMMONED                 0x1000
#define MOB_MODE_TURNS_AGAINST_BAD_MASTER 0x2000

#define MOB_SENSIBLE_MASK 0xf000    // fate: mob mode flags that I actually understand

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

enum class MS : uint8_t
{
    IDLE,
    WALK,
    ATTACK,
    DEAD,
    DELAY
};

struct mob_data : public BlockList
{
    short n;
    short base_class, mob_class, mode;
    Direction dir;
    short m_0, x_0, y_0, xs, ys;
    char name[24];
    int spawndelay_1, spawndelay2;
    struct
    {
        MS state;
        bool target_attackable:1;
        bool steal_flag:1;
        bool steal_coin_flag:1;
        bool master_check:1;
        bool change_walk_target:1;
        bool walk_easy:1;
        unsigned special_mob_ai:3;
    } state;
    timer_id timer;
    short to_x, to_y;
    int hp;
    int target_id, attacked_id;
    AttackResult target_lv;
    struct walkpath_data walkpath;
    unsigned int next_walktime;
    unsigned int attackabletime;
    unsigned int last_deadtime, last_spawntime, last_thinktime;
    unsigned int canmove_tick;
    short move_fail_count;
    struct
    {
        int id;
        int dmg;
    } dmglog[DAMAGELOG_SIZE];
    struct item *lootitem;
    short lootitem_count;

    struct status_change sc_data[MAX_STATUSCHANGE];
    short sc_count;
    short opt1, opt2, opt3, option;
    short min_chase;
    timer_id deletetimer;

    int def_ele;
    int master_id, master_dist;
    int exclusion_src, exclusion_party;
    char npc_event[50];
    unsigned short stats[MOB_LAST]; // [Fate] mob-specific stats
    short size;

    mob_data() : BlockList(BL_MOB) {}
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
    uint8_t *gat;
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
    int *block_count, *block_mob_count;
    int m;
    short xs, ys;
    short bxs, bys;
    int npc_num;
    int users;
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
        int drop_id;
        int drop_type;
        int drop_per;
    } drop_list[MAX_DROP_PER_MAP];
};
#define read_gat(m,x,y) (maps[m].gat[(x)+(y)*maps[m].xs])
#define read_gatp(m,x,y) (m->gat[(x)+(y)*m->xs])

struct flooritem_data : public BlockList
{
    short subx, suby;
    timer_id cleartimer;
    int first_get_id, second_get_id, third_get_id;
    unsigned int first_get_tick, second_get_tick, third_get_tick;
    struct item item_data;

    flooritem_data() : BlockList(BL_ITEM) {}
};

enum class SP : uint16_t
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
    return SP(int(attr) + int(SP::STR));
}
constexpr SP ATTR_TO_SP_UP(ATTR attr)
{
    return SP(int(attr) + int(SP::USTR));
}
constexpr ATTR ATTR_FROM_SP_BASE(SP sp)
{
    return ATTR(int(sp) - int(SP::STR));
}
constexpr ATTR ATTR_FROM_SP_UP(SP sp)
{
    return ATTR(int(sp) - int(SP::USTR));
}

enum class LOOK : uint8_t
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

#endif //MAP_STRUCTS
