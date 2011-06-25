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
    int damage, damage2;
    int type, div_;
    int amotion, dmotion;
    int flag;
    AttackResult dmg_lv;
};

#endif //BATTLE_STRUCTS
