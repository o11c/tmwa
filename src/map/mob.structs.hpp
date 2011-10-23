#ifndef MOB_STRUCTS
#define MOB_STRUCTS

# include "../common/mmo.hpp"
# include "../common/timer.hpp"

# include "../lib/enum.hpp"
# include "../lib/ints.hpp"

BIT_ENUM(MobMode, uint16_t)
{
    NONE                        = 0x0000,

    CAN_MOVE                    = 0x0001,
    LOOTER                      = 0x0002,
    AGGRESSIVE                  = 0x0004,
    ASSIST                      = 0x0008,

    BOSS                        = 0x0020,
    // sometimes also called "robust"
    PLANT                       = 0x0040,
    CAN_ATTACK                  = 0x0080,

    SUMMONED                    = 0x1000,
    TURNS_AGAINST_BAD_MASTER    = 0x2000,

    SENSIBLE_MASK               = 0xF000, // fate: mob mode flags that I actually understand
    ALL                         = 0xFFFF,
};

struct mob_db
{
    char name[24], jname[24];
    level_t lv;
    sint32 max_hp, max_sp;
    sint32 base_exp, job_exp;
    sint32 atk1, atk2;
    sint32 def, mdef;
    sint32 str, agi, vit, int_, dex, luk;
    sint32 range, range2, range3;
    sint32 size, race, element;
    MobMode mode;
    interval_t speed, adelay, amotion, dmotion;
    sint32 mutations_nr, mutation_power;
    struct
    {
        sint32 nameid, p;
    } dropitem[8];
    sint32 equip;                 // [Valaris]
};

#endif //MOB_STRUCTS
