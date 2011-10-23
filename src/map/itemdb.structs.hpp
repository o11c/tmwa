#ifndef ITEMDB_STRUCTS
#define ITEMDB_STRUCTS

# include "../lib/earray.hpp"

# include "../common/mmo.hpp"

# include "script.structs.hpp"

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
    sint32 nameid;
    char name[24], jname[24];
    sint32 value_buy;
    sint32 value_sell;
    sint32 type;
    sint32 sex;
    EPOS equip;
    sint32 weight;
    sint32 atk;
    sint32 def;
    sint32 range;
    sint32 magic_bonus;
    sint32 look;
    /// Base level require to equip this
    level_t elv;
    /// "Weapon level", used in damage calculations
    sint32 wlv;
    std::vector<Script> use_script;
    std::vector<Script> equip_script;
    struct
    {
        bool available:1;
        uint32 no_equip:3;
        bool no_drop:1;
        bool no_use:1;
    } flag;
};

#endif //ITEMDB_STRUCTS
