#ifndef BATTLE_STRUCTS
#define BATTLE_STRUCTS

# include "../common/timer.structs.hpp"

# include "../lib/enum.hpp"

enum class Direction
{
    S,
    SW,
    W,
    NW,
    N,
    NE,
    E,
    SE,
};

enum class AttackResult
{
    // what is this first?
    ZERO,
    LUCKY,
    FLEE,
    DEF
};

/// flags for battle_calc_damage
BIT_ENUM(BattleFlags, uint8)
{
    NONE        = 0x00,
    WEAPON      = 0x01,
    MAGIC       = 0x02,
    MISC        = 0x04,
    SHORT       = 0x10,
    LONG        = 0x40,
    WEAPON_MASK = WEAPON | MAGIC | MISC,
    RANGE_MASK  = SHORT | LONG,
    ALL = WEAPON_MASK | RANGE_MASK
};

struct Damage
{
    sint32 damage, damage2;
    sint32 type, div_;
    interval_t amotion, dmotion;
    BattleFlags flag;
    AttackResult dmg_lv;
};

#endif //BATTLE_STRUCTS
