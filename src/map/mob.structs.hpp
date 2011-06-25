#ifndef MOB_STRUCTS
#define MOB_STRUCTS

struct mob_db
{
    char name[24], jname[24];
    int lv;
    int max_hp, max_sp;
    int base_exp, job_exp;
    int atk1, atk2;
    int def, mdef;
    int str, agi, vit, int_, dex, luk;
    int range, range2, range3;
    int size, race, element, mode;
    int speed, adelay, amotion, dmotion;
    int mutations_nr, mutation_power;
    struct
    {
        int nameid, p;
    } dropitem[8];
    int equip;                 // [Valaris]
};

#endif //MOB_STRUCTS
