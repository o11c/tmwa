#ifndef BATTLE_STRUCTS
#define BATTLE_STRUCTS

# include "../common/timer.structs.hpp"

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

struct Damage
{
    int32_t damage, damage2;
    int32_t type, div_;
    int32_t amotion, dmotion;
    int32_t flag;
    AttackResult dmg_lv;
};

#endif //BATTLE_STRUCTS
