#ifndef ITEMDB_STRUCTS
#define ITEMDB_STRUCTS

struct item_data
{
    int nameid;
    char name[24], jname[24];
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
