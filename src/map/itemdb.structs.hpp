#ifndef ITEMDB_STRUCTS
#define ITEMDB_STRUCTS

# include "../lib/earray.hpp"

# include "../common/mmo.hpp"

enum class EQUIP
{
    NONE    = -1,

    MISC2   = 0,
    CAPE    = 1,
    SHOES   = 2,
    GLOVES  = 3,
    LEGS    = 4,
    CHEST   = 5,
    HELMET  = 6,
    MISC1   = 7,
    // SHIELD is also used for dual-wielding and two-handed weapons
    SHIELD  = 8,
    WEAPON  = 9,
    ARROW   = 10,

    COUNT = 11
};
constexpr EQUIP EQUIPs[11] =
{
    EQUIP::MISC2, EQUIP::CAPE, EQUIP::SHOES, EQUIP::GLOVES, EQUIP::LEGS,
    EQUIP::CHEST, EQUIP::HELMET, EQUIP::MISC1, EQUIP::SHIELD, EQUIP::WEAPON,
    EQUIP::ARROW,
};

constexpr EQUIP EQUIPs_no_arrow[10] =
{
    EQUIP::MISC2, EQUIP::CAPE, EQUIP::SHOES, EQUIP::GLOVES, EQUIP::LEGS,
    EQUIP::CHEST, EQUIP::HELMET, EQUIP::MISC1, EQUIP::SHIELD, EQUIP::WEAPON,
};

constexpr earray<EPOS, EQUIP, EQUIP::COUNT> equip_pos =
{
    EPOS::MISC2, EPOS::CAPE, EPOS::SHOES, EPOS::GLOVES, EPOS::LEGS,
    EPOS::CHEST, EPOS::HELMET, EPOS::MISC1, EPOS::SHIELD, EPOS::WEAPON,
    EPOS::ARROW
};
// It should be possible to directly assign to this using the variadic
// template constructor, but there's a but in GCC

struct item_data
{
    int nameid;
    char name[24], jname[24];
    int value_buy;
    int value_sell;
    int type;
    int sex;
    EPOS equip;
    int weight;
    int atk;
    int def;
    int range;
    int magic_bonus;
    int look;
    /// Base level require to equip this
    int elv;
    /// "Weapon level", used in damage calculations
    int wlv;
    const char *use_script;
    const char *equip_script;
    struct
    {
        bool available:1;
        unsigned no_equip:3;
        bool no_drop:1;
        bool no_use:1;
    } flag;
};

#endif //ITEMDB_STRUCTS
