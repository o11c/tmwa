#ifndef ITEMDB_STRUCTS
#define ITEMDB_STRUCTS

struct item_data
{
    int nameid;
    char name[24], jname[24];
    char prefix[24], suffix[24];
    char cardillustname[64];
    int value_buy;
    int value_sell;
    int type;
    int sex;
    int equip;
    int weight;
    int atk;
    int def;
    int range;
    int magic_bonus;
    int slot;
    int look;
    int elv;
    int wlv;
    int refine;
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
