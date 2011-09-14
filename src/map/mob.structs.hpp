#ifndef MOB_STRUCTS
#define MOB_STRUCTS

struct mob_db
{
    char name[24], jname[24];
    int32_t lv;
    int32_t max_hp, max_sp;
    int32_t base_exp, job_exp;
    int32_t atk1, atk2;
    int32_t def, mdef;
    int32_t str, agi, vit, int_, dex, luk;
    int32_t range, range2, range3;
    int32_t size, race, element, mode;
    int32_t speed, adelay, amotion, dmotion;
    int32_t mutations_nr, mutation_power;
    struct
    {
        int32_t nameid, p;
    } dropitem[8];
    int32_t equip;                 // [Valaris]
};

#endif //MOB_STRUCTS
