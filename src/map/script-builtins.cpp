#include "script.hpp"

#include "../common/mt_rand.hpp"
#include "../common/timer.hpp"
#include "../common/utils.hpp"

#include "atcommand.hpp"
#include "battle.hpp"
#include "chrif.hpp"
#include "clif.hpp"
#include "itemdb.hpp"
#include "magic-base.hpp"
#include "map.hpp"
#include "mob.hpp"
#include "npc.hpp"
#include "party.hpp"
#include "pc.hpp"
#include "skill.hpp"
#include "storage.hpp"

// the + 1 is for the stupid Script::ARG thing
#define START_ARG               (st->start + 1)
#define ARG_LIMIT               (st->end - START_ARG)
#define ARG_COUNT               (ARG_LIMIT - 1)

#define RESOLVE(idx)            st->resolve(START_ARG + idx)
#define GET_ARG_STRING(idx)     st->to_string(START_ARG + idx)
#define GET_ARG_INT(idx)        st->to_int(START_ARG + idx)
#define HAS_ARG(idx)            (idx < ARG_LIMIT)
#define TYPE_ARG(idx)           st->type_at(START_ARG + idx)
#define GET_ARG_POS(idx)        st->get_as<Script::POS>(START_ARG + idx)
#define GET_ARG_NAME(idx)       st->get_as<Script::NAME>(START_ARG + idx)
#define PUSH_COPY(idx)          st->push_copy_of(START_ARG + idx)

/// Attached player
static MapSessionData *script_rid2sd(ScriptState *st)
{
    MapSessionData *sd = map_id2sd(st->rid);
    if (!sd)
        map_log("script_rid2sd: fatal error ! player not attached!\n");
    return sd;
}

/// Display a message in a dialog
static void builtin_mes(ScriptState *st)
{
    clif_scriptmes(script_rid2sd(st), st->oid, GET_ARG_STRING(1).c_str());
}

/// Jump to another point in execution
static void builtin_goto(ScriptState *st)
{
    if (TYPE_ARG(1) != Script::POS)
    {
        map_log("Panic: goto nonlabel!");
        abort();
        st->state = ScriptExecutionState::END;
        return;
    }

    st->pos = GET_ARG_POS(1);
    st->state = ScriptExecutionState::GOTO;
}

/// Call a function, by name
static void builtin_callfunc(ScriptState *st)
{
    std::string str = GET_ARG_STRING(1);
    const Script *scr = static_cast<const Script *>(strdb_search(script_get_userfunc_db(), str.c_str()).p);

    if (!scr)
    {
        map_log("Panic: callfunc: function not found! [%s]\n", str.c_str());
        abort();
        st->state = ScriptExecutionState::END;
    }
    // TODO: instead just adjust st->start += 3 or something
    int32_t j = 0;
    for (int32_t i = 2; i < ARG_LIMIT; i++, j++)
        PUSH_COPY(i);

    st->push<Script::INT>(j);
    st->push<Script::INT>(st->defsp);
    st->push<Script::INT>(st->pos);
    st->push<Script::RETINFO>(st->script);

    st->pos = 0;
    st->script = scr;
    // what is this doing? I should replace the whole mechanism ...
    st->defsp = st->start + 4 + j;
    st->state = ScriptExecutionState::GOTO;
}

/// call a label of the current script
static void builtin_callsub(ScriptState *st)
{
    int32_t pos = GET_ARG_POS(1);
    // TODO instead just adjust st->start += 3 or something
    int32_t j = 0;
    for (int32_t i = 2; i < ARG_LIMIT; i++, j++)
        PUSH_COPY(i);

    st->push<Script::INT>(j);
    st->push<Script::INT>(st->defsp);
    st->push<Script::INT>(st->pos);
    st->push<Script::RETINFO>(st->script);

    st->pos = pos;
    st->defsp = st->start + 4 + j;
    st->state = ScriptExecutionState::GOTO;
}

/// Get an argument, as passed to callfunc or callsub
static void builtin_getarg(ScriptState *st)
{
    int32_t num = GET_ARG_INT(1);
    if (st->defsp < 4 || st->type_at(st->defsp - 1) != Script::RETINFO)
    {
        map_log("script:getarg without callfunc or callsub!\n");
        st->state = ScriptExecutionState::END;
        return;
    }
    int32_t max = st->to_int(st->defsp - 4);
    int32_t stsp = st->defsp - max - 4;
    if (num >= max)
    {
        map_log("script:getarg arg1(%d) out of range(%d) !\n", num, max);
        st->state = ScriptExecutionState::END;
        return;
    }
    st->push_copy_of(stsp + num);
}

/// Return from a callfunc or callsub
static void builtin_return(ScriptState *st)
{
    if (HAS_ARG(1))
        PUSH_COPY(1);
    st->state = ScriptExecutionState::RETFUNC;
}

/// Wait for user input before continuing
static void builtin_next(ScriptState *st)
{
    st->state = ScriptExecutionState::STOP;
    clif_scriptnext(script_rid2sd(st), st->oid);
}

/// Display a close button and return immediately
static void builtin_close(ScriptState *st)
{
    st->state = ScriptExecutionState::END;
    clif_scriptclose(script_rid2sd(st), st->oid);
}

/// Display a close button and continue the script after it is pressed
static void builtin_close2(ScriptState *st)
{
    st->state = ScriptExecutionState::STOP;
    clif_scriptclose(script_rid2sd(st), st->oid);
}

/// Wait for the player to select on of many choices
static void builtin_menu(ScriptState *st)
{
    int32_t menu_choices = 0;

    MapSessionData *sd = script_rid2sd(st);

    // We don't need to do this iteration if the player cancels, strictly speaking.
    for (int32_t i = 1; i < ARG_LIMIT; i += 2)
    {
        std::string choice = GET_ARG_STRING(i);
        if (choice.empty())
            break;
        ++menu_choices;
    }

    if (!sd->state.menu_or_input)
    {
        st->state = ScriptExecutionState::RERUNLINE;
        sd->state.menu_or_input = true;

        std::vector<std::string> choices;
        for (int32_t i = 1; menu_choices > 0; i += 2, --menu_choices)
            choices.push_back(GET_ARG_STRING(i));
        clif_scriptmenu(script_rid2sd(st), st->oid, choices);
    }
    else // being rerun
    {
        if (sd->npc_menu == 0xff)
        {
            // the current client won't actually send this
            sd->state.menu_or_input = false;
            st->state = ScriptExecutionState::END;
        }
        sd->reg.set(add_str("@menu"), sd->npc_menu);
        sd->state.menu_or_input = false;
        if (sd->npc_menu > 0 && sd->npc_menu <= menu_choices)
        {
            if (TYPE_ARG(sd->npc_menu * 2) != Script::POS)
            {
                map_log("Fatal: menu target is not a label");
                abort();
                st->state = ScriptExecutionState::END;
                return;
            }
            st->pos = GET_ARG_POS(sd->npc_menu * 2);
            st->state = ScriptExecutionState::GOTO;
        }
    }
}

/// Generate a random number
static void builtin_rand(ScriptState *st)
{
    if (HAS_ARG(2))
    {
        int32_t min = GET_ARG_INT(1);
        int32_t max = GET_ARG_INT(2);
        if (max < min)
            std::swap(max, min);
        int32_t range = max - min + 1;
        st->push<Script::INT>(range <= 0 ? 0 : MPRAND(min, range));
    }
    else
    {
        int32_t range = GET_ARG_INT(1);
        st->push<Script::INT>(range <= 0 ? 0 : MRAND(range));
    }
}

/// Check whether the PC is at the specified location
static void builtin_isat(ScriptState *st)
{
    MapSessionData *sd = script_rid2sd(st);
    if (!sd)
        return;

    std::string str = GET_ARG_STRING(1);
    int32_t x = GET_ARG_INT(2);
    int32_t y = GET_ARG_INT(3);

    st->push<Script::INT>(x == sd->x && y == sd->y && str == &maps[sd->m].name);
}

/// Warp player to somewhere else
static void builtin_warp(ScriptState *st)
{
    MapSessionData *sd = script_rid2sd(st);

    std::string str = GET_ARG_STRING(1);
    int16_t x = GET_ARG_INT(2);
    int16_t y = GET_ARG_INT(3);
    if (str == "Random")
        pc_randomwarp(sd, BeingRemoveType::WARP);
    else if (str == "SavePoint")
    {
        if (maps[sd->m].flag.noreturn)
            return;

        pc_setpos(sd, sd->status.save_point, BeingRemoveType::WARP);
    }
    else if (str == "Save")
    {
        if (maps[sd->m].flag.noreturn)
            return;

        pc_setpos(sd, sd->status.save_point, BeingRemoveType::WARP);
    }
    else
    {
        fixed_string<16> fstr;
        fstr.copy_from(str.c_str());
        pc_setpos(sd, Point{fstr, x, y}, BeingRemoveType::ZERO);
    }
}

/// Warp a single player to a point
static void builtin_areawarp_sub(BlockList *bl, Point point)
{
    if (strcmp(&point.map, "Random") == 0)
        pc_randomwarp(static_cast<MapSessionData *>(bl), BeingRemoveType::WARP);
    else
        pc_setpos(static_cast<MapSessionData *>(bl), point, BeingRemoveType::ZERO);
}

static void builtin_areawarp(ScriptState *st)
{
    fixed_string<16> src_map;
    src_map.copy_from(GET_ARG_STRING(1).c_str());
    int32_t x_0 = GET_ARG_INT(2);
    int32_t y_0 = GET_ARG_INT(3);
    int32_t x_1 = GET_ARG_INT(4);
    int32_t y_1 = GET_ARG_INT(5);
    fixed_string<16> dst_map;
    dst_map.copy_from(GET_ARG_STRING(6).c_str());
    int16_t x = GET_ARG_INT(7);
    int16_t y = GET_ARG_INT(8);

    int32_t m = map_mapname2mapid(src_map);
    if (m < 0)
        return;

    map_foreachinarea(builtin_areawarp_sub,
                      m, x_0, y_0, x_1, y_1, BL_PC, Point{dst_map, x, y});
}

/// Recover HP and SP instantly
static void builtin_heal(ScriptState *st)
{
    int32_t hp = GET_ARG_INT(1);
    int32_t sp = GET_ARG_INT(2);
    pc_heal(script_rid2sd(st), hp, sp);
}

/// Recover HP and SP gradually, as by an item
static void builtin_itemheal(ScriptState *st)
{
    int32_t hp = GET_ARG_INT(1);
    int32_t sp = GET_ARG_INT(2);

    pc_itemheal(script_rid2sd(st), hp, sp);
}

/// Recover HP and SP immediately, by percentage
static void builtin_percentheal(ScriptState *st)
{
    int32_t hp = GET_ARG_INT(1);
    int32_t sp = GET_ARG_INT(2);

    pc_percentheal(script_rid2sd(st), hp, sp);
}

/// Input an integer or string
static void builtin_input(ScriptState *st)
{
    int32_t num = GET_ARG_NAME(1);
    const std::string& name = str_data[num & 0x00ffffff].str;
    char postfix = name.back();

    MapSessionData *sd = script_rid2sd(st);
    if (sd->state.menu_or_input) // line is being run a second time
    {
        sd->state.menu_or_input = false;
        if (postfix == '$')
            set_reg_s(sd, num, name, static_cast<std::string>(sd->npc_str));
        else
            set_reg_i(sd, num, name, sd->npc_amount);
    }
    else
    {
        st->state = ScriptExecutionState::RERUNLINE;
        if (postfix == '$')
            clif_scriptinputstr(sd, st->oid);
        else
            clif_scriptinput(sd, st->oid);
        sd->state.menu_or_input = true;
    }
}

/// Maybe evaluate something
static void builtin_if(ScriptState *st)
{
    bool sel = GET_ARG_INT(1);
    if (!sel)
        return;

    // the command
    PUSH_COPY(2);
    st->push<Script::ARG>(0);
    // arguments
    for (int32_t i = 3; i < ARG_LIMIT; i++)
        PUSH_COPY(i);
    st->run_func();
}

/// Set a variable to a value
static void builtin_set(ScriptState *st)
{
    if (TYPE_ARG(1) != Script::NAME)
    {
        map_log("%s: not name\n", __func__);
        return;
    }

    int32_t num = GET_ARG_NAME(1);
    const std::string& name = str_data[num & 0x00ffffff].str;
    char prefix = name.front();
    char postfix = name.back();

    MapSessionData *sd = NULL;
    if (prefix != '$')
        sd = script_rid2sd(st);

    if (postfix == '$')
        set_reg_s(sd, num, name, GET_ARG_STRING(2));
    else
        set_reg_i(sd, num, name, GET_ARG_INT(2));
}

/// Set a variable to an array of values
static void builtin_setarray(ScriptState *st)
{
    int32_t num = GET_ARG_NAME(1);
    const std::string& name = str_data[num & 0x00ffffff].str;
    char prefix = name.front();
    char postfix = name.back();

    if (prefix != '$' && prefix != '@')
    {
        map_log("%s: illegal scope !\n", __func__);
        return;
    }
    MapSessionData *sd = NULL;
    if (prefix != '$')
        sd = script_rid2sd(st);

    for (int32_t j = 0, i = 2; i < ARG_LIMIT && j < 128; i++, j++)
    {
        if (postfix == '$')
            set_reg_s(sd, num + (j << 24), name, GET_ARG_STRING(i));
        else
            set_reg_i(sd, num + (j << 24), name, GET_ARG_INT(i));
    }
}

/// Fill an array with something
static void builtin_cleararray(ScriptState *st)
{
    int32_t num = GET_ARG_NAME(1);
    const std::string& name = str_data[num & 0x00ffffff].str;
    char prefix = name.front();
    char postfix = name.back();
    int32_t sz = GET_ARG_INT(3);

    if (prefix != '$' && prefix != '@')
    {
        map_log("%s: illegal scope !\n", __func__);
        return;
    }
    MapSessionData *sd = NULL;
    if (prefix != '$')
        sd = script_rid2sd(st);

    if (postfix == '$')
        for (int32_t i = 0; i < sz; i++)
            set_reg_s(sd, num + (i << 24), name, GET_ARG_STRING(2));
    else
        for (int32_t i = 0; i < sz; i++)
            set_reg_i(sd, num + (i << 24), name, GET_ARG_INT(2));
}

/// copy between arrays
static void builtin_copyarray(ScriptState *st)
{
    int32_t num = GET_ARG_NAME(1);
    const std::string& name = str_data[num & 0x00ffffff].str;
    char prefix = name.front();
    char postfix = name.back();
    int32_t num2 = GET_ARG_NAME(2);
    const std::string& name2 = str_data[num2 & 0x00ffffff].str;
    char prefix2 = name2.front();
    char postfix2 = name2.back();
    int32_t sz = GET_ARG_INT(3);

    if (prefix != '$' && prefix != '@' && prefix2 != '$' && prefix2 != '@')
    {
        map_log("%s: illegal scope!\n", __func__);
        return;
    }
    if ((postfix == '$') != (postfix2 == '$'))
    {
        map_log("%s: type mismatch!\n", __func__);
        return;
    }
    MapSessionData *sd = NULL;
    if (prefix != '$' || prefix2 != '$')
        sd = script_rid2sd(st);

    if (postfix == '$')
        for (int32_t i = 0; i < sz; i++)
            set_reg_s(sd, num + (i << 24), name, get_reg_s(sd, num2 + (i << 24), name2));
    else
        for (int32_t i = 0; i < sz; i++)
            set_reg_i(sd, num + (i << 24), name, get_reg_i(sd, num2 + (i << 24), name2));
}

/// Size of an array (to the last element that is not 0 or "")
static int32_t getarraysize(ScriptState *st, int32_t num, const std::string& name)
{
    char prefix = name.front();
    char postfix = name.back();
    MapSessionData *sd = NULL;
    if (prefix != '$')
        sd = script_rid2sd(st);

    uint8_t i = num >> 24, c = i;
    if (postfix == '$')
    {
        for (; i < 128; i++)
            if (!get_reg_s(sd, num + (i << 24), name).empty())
                c = i;
    }
    else
    {
        for (; i < 128; i++)
            if (get_reg_i(sd, num + (i << 24), name))
                c = i;
    }
    return c + 1;
}

static void builtin_getarraysize(ScriptState *st)
{
    int32_t num = GET_ARG_NAME(1);
    const std::string& name = str_data[num & 0x00ffffff].str;
    char prefix = name.front();
//     char postfix = name.back();

    if (prefix != '$' && prefix != '@')
    {
        map_log("%s: illegal scope !\n", __func__);
        return;
    }

    st->push<Script::INT>(getarraysize(st, num, name));
}

/// Delete elements from an array, shifting the remainder
static void builtin_deletearray(ScriptState *st)
{
    int32_t num = GET_ARG_NAME(1);
    const std::string& name = str_data[num & 0x00ffffff].str;
    char prefix = name.front();
    char postfix = name.back();
    int32_t count = 1;
    if (HAS_ARG(2))
        count = GET_ARG_INT(2);
    int32_t sz = getarraysize(st, num, name) - (num >> 24) - count + 1;


    if (prefix != '$' && prefix != '@')
    {
        map_log("%s: illegal scope !\n", __func__);
        return;
    }
    MapSessionData *sd = NULL;
    if (prefix != '$')
        sd = script_rid2sd(st);

    int32_t i;
    if (postfix == '$')
    {
        for (i = 0; i < sz; i++)
            set_reg_s(sd, num + (i << 24), name, get_reg_s(sd, num + ((i + count) << 24), name));
        for (; i < 128 - (num >> 24); i++)
            set_reg_s(sd, num + (i << 24), name, std::string());
    }
    else
    {
        for (i = 0; i < sz; i++)
            set_reg_i(sd, num + (i << 24), name, get_reg_i(sd, num + ((i + count) << 24), name));
        for (; i < 128 - (num >> 24); i++)
            set_reg_i(sd, num + (i << 24), name, 0);
    }
}

/// Return a reference to an offset from the array
static void builtin_getelementofarray(ScriptState *st)
{
    if (TYPE_ARG(1) != Script::NAME)
    {
        map_log("%s (operator[]): param1 not name !\n", __func__);
        st->push<Script::INT>(0);
    }
    int32_t i = GET_ARG_INT(2);
    if (i > 127 || i < 0)
    {
        map_log("%s (operator[]): param2 illegal number %d\n", __func__, i);
        st->push<Script::INT>(0);
    }
    else
        st->push<Script::NAME>((i << 24) + GET_ARG_NAME(1));
}

/// Change an aspect of the player's appearance
static void builtin_setlook(ScriptState *st)
{
    LOOK type = static_cast<LOOK>(GET_ARG_INT(1));
    int32_t val = GET_ARG_INT(2);

    pc_changelook(script_rid2sd(st), type, val);
}

/// Count how many of an item is in player's inventory
static void builtin_countitem(ScriptState *st)
{
    RESOLVE(1);
    int32_t nameid = 0;
    if (TYPE_ARG(1) == Script::STR)
    {
        std::string name = GET_ARG_STRING(1);
        struct item_data *item_data = itemdb_searchname(name.c_str());
        if (item_data)
            nameid = item_data->nameid;
    }
    else
        nameid = GET_ARG_INT(1);

    if (!nameid)
    {
        st->push<Script::INT>(0);
        return;
    }

    MapSessionData *sd = script_rid2sd(st);
    int32_t count = 0;
    for (int32_t i = 0; i < MAX_INVENTORY; i++)
    {
        if (sd->status.inventory[i].nameid == nameid)
            // we can't break; the item might be in a multiple slots
            count += sd->status.inventory[i].amount;
    }
    st->push<Script::INT>(count);
}

/// Check whether an item could be added without going over max weight
static void builtin_checkweight(ScriptState *st)
{
    RESOLVE(1);

    int32_t nameid = 0;
    if (TYPE_ARG(1) == Script::STR)
    {
        std::string name = GET_ARG_STRING(1);
        struct item_data *item_data = itemdb_searchname(name.c_str());
        if (item_data)
            nameid = item_data->nameid;
    }
    else
        nameid = GET_ARG_INT(1);

    int32_t amount = GET_ARG_INT(2);
    if (amount <= 0 || !nameid)
    {
        st->push<Script::INT>(0);
        return;
    }

    MapSessionData *sd = script_rid2sd(st);
    st->push<Script::INT>(itemdb_weight(nameid) * amount + sd->weight <= sd->max_weight);
}

/// Add item to player's inventory
static void builtin_getitem(ScriptState *st)
{
    RESOLVE(1);

    int32_t nameid = 0;
    if (TYPE_ARG(1) == Script::STR)
    {
        std::string name = GET_ARG_STRING(1);
        struct item_data *item_data = itemdb_searchname(name.c_str());
        if (item_data)
            nameid = item_data->nameid;
    }
    else
        nameid = GET_ARG_INT(1);

    int32_t amount = GET_ARG_INT(2);
    if (amount <= 0)
        return;

    if (!nameid)
        return;

    struct item item_tmp = {};
    item_tmp.nameid = nameid;

    MapSessionData *sd;
    if (HAS_ARG(4))
        sd = map_id2sd(GET_ARG_INT(4));
    else
        sd = script_rid2sd(st);
    if (!sd)
        return;
    PickupFail flag = pc_additem(sd, &item_tmp, amount);
    if (flag == PickupFail::OKAY)
        return;

    clif_additem(sd, 0, 0, flag);
    map_addflooritem(&item_tmp, amount, sd->m, sd->x, sd->y, NULL, NULL, NULL);
}

/// Drop an item on the floor
static void builtin_makeitem(ScriptState *st)
{
    RESOLVE(1);

    int32_t nameid = 0;
    if (TYPE_ARG(1) == Script::STR)
    {
        std::string name = GET_ARG_STRING(1);
        struct item_data *item_data = itemdb_searchname(name.c_str());
        if (item_data)
            nameid = item_data->nameid;
    }
    else
        nameid = GET_ARG_INT(1);

    int32_t amount = GET_ARG_INT(2);
    fixed_string<16> mapname;
    mapname.copy_from(GET_ARG_STRING(3).c_str());
    int32_t x = GET_ARG_INT(4);
    int32_t y = GET_ARG_INT(5);

    MapSessionData *sd = script_rid2sd(st);

    int32_t m;
    if (sd && strcmp(&mapname, "this") == 0)
        m = sd->m;
    else
        m = map_mapname2mapid(mapname);

    if (!nameid)
        return;
    struct item item_tmp = {};
    item_tmp.nameid = nameid;

    map_addflooritem(&item_tmp, amount, m, x, y, NULL, NULL, NULL);
}

/// Steal an item from the hardworking player
static void builtin_delitem(ScriptState *st)
{
    RESOLVE(1);

    int32_t nameid = 0;
    if (TYPE_ARG(1) == Script::STR)
    {
        std::string name = GET_ARG_STRING(1);
        struct item_data *item_data = itemdb_searchname(name.c_str());
        if (item_data)
            nameid = item_data->nameid;
    }
    else
        nameid = GET_ARG_INT(1);

    int32_t amount = GET_ARG_INT(2);

    if (!nameid || amount <= 0)
        return;
    MapSessionData *sd = script_rid2sd(st);

    for (int32_t i = 0; i < MAX_INVENTORY; i++)
    {
        if (sd->status.inventory[i].nameid != nameid)
            continue;
        if (sd->status.inventory[i].amount >= amount)
        {
            pc_delitem(sd, i, amount, 0);
            return;
        }
        // decrease the number to be deleted
        amount -= sd->status.inventory[i].amount;
        // delete how many there were
        pc_delitem(sd, i, sd->status.inventory[i].amount, 0);
        // continue with items in other slots
    }
}

static void builtin_readparam(ScriptState *st)
{
    SP type = static_cast<SP>(GET_ARG_INT(1));
    MapSessionData *sd = NULL;
    if (HAS_ARG(2))
        sd = map_nick2sd(GET_ARG_STRING(2).c_str());
    else
        sd = script_rid2sd(st);

    if (!sd)
    {
        st->push<Script::INT>(-1);
        return;
    }

    st->push<Script::INT>(pc_readparam(sd, type));
}

/// Get one of the character's IDs
static void builtin_getcharid(ScriptState *st)
{
    MapSessionData *sd;
    if (HAS_ARG(2))
        sd = map_nick2sd(GET_ARG_STRING(2).c_str());
    else
        sd = script_rid2sd(st);
    if (!sd)
    {
        st->push<Script::INT>(-1);
        return;
    }

    switch(GET_ARG_INT(1))
    {
    case 0:
        st->push<Script::INT>(sd->status.char_id);
        break;
    case 1:
        st->push<Script::INT>(sd->status.party_id);
        break;
    case 3:
        st->push<Script::INT>(sd->status.account_id);
        break;
    case 2: // guild_id
    default:
        st->push<Script::INT>(0);
        break;
    }
}

/// Actually get party name, or the empty string
static std::string getpartyname(int32_t party_id)
{
    struct party *p = party_search(party_id);
    if (!p)
        return std::string();

    return p->name;
}

static void builtin_getpartyname(ScriptState *st)
{
    int32_t party_id = GET_ARG_INT(1);
    st->push<Script::STR>(getpartyname(party_id));
}

/// fill in an array with party members
static void builtin_getpartymember(ScriptState *st)
{
    struct party *p = party_search(GET_ARG_INT(1));

    if (!p)
    {
        mapreg_setreg(add_str("$@partymembercount"), 0);
        return;
    }
    int32_t j = 0;
    for (int32_t i = 0; i < MAX_PARTY; i++)
    {
        if (p->member[i].account_id)
        {
            mapreg_setregstr(add_str("$@partymembername$") + (i << 24),
                             static_cast<std::string>(p->member[i].name));
            j++;
        }
    }
    mapreg_setreg(add_str("$@partymembercount"), j);
}

/// String information about a character
static void builtin_strcharinfo(ScriptState *st)
{
    MapSessionData *sd = script_rid2sd(st);
    switch(GET_ARG_INT(1))
    {
    case 0:
        st->push<Script::STR>(sd->status.name);
        return;
    case 1:
        st->push<Script::STR>(getpartyname(sd->status.party_id));
        return;
    case 2: // was: guild name
    default:
        st->push<Script::STR>(std::string());
        return;
    }
}

// scripts use a different set of enum constants (ugh)
// TODO change this to use the main enum class EQUIP
enum class EQ_SCR
{
    none,

    head, shield, hand2, hand1, gloves,
    shoes, misc1, misc2, torso, legs,

    count
};

constexpr earray<EPOS, EQ_SCR, EQ_SCR::count> equip =
{
    EPOS::NONE,

    EPOS::HELMET, EPOS::MISC1, EPOS::SHIELD, EPOS::WEAPON, EPOS::GLOVES,
    EPOS::SHOES, EPOS::CAPE, EPOS::MISC2, EPOS::CHEST, EPOS::LEGS,
};

constexpr earray<const char *, EQ_SCR, EQ_SCR::count> epos =
{
    "Not Equipped",

    "Head", "Shield", "Left hand", "Right hand", "Gloves",
    "Shoes", "Accessory 1", "Accessory 2", "Torso", "Legs"
};

/// Get an equipment from an equip_ slot
static void builtin_getequipid(ScriptState *st)
{
    MapSessionData *sd = script_rid2sd(st);
    EQ_SCR num = static_cast<EQ_SCR>(GET_ARG_INT(1));

    int32_t i = pc_checkequip(sd, equip[num]);
    if (i < 0)
    {
        st->push<Script::INT>(-1);
        return;
    }
    struct item_data *item = sd->inventory_data[i];
    if (item)
    {
        st->push<Script::INT>(item->nameid);
        return;
    }

    map_log("%s: I'm almost panicking here!", __func__);
    st->push<Script::INT>(0);
}

/// Get the name of equipment in a slot
static void builtin_getequipname(ScriptState *st)
{
    MapSessionData *sd = script_rid2sd(st);
    EQ_SCR num = static_cast<EQ_SCR>(GET_ARG_INT(1));
    int32_t i = pc_checkequip(sd, equip[num]);
    if (i < 0)
    {
        st->push<Script::STR>(std::string());
        return;
    }
    std::string out = epos[num];
    out += "-[";
    struct item_data *item = sd->inventory_data[i];
    if (item)
        out += item->jname;
    else
        out += epos[EQ_SCR::none];
    out += ']';
    st->push<Script::STR>(std::move(out));
}

/// Check if there is an item in the slot
static void builtin_getequipisequiped(ScriptState *st)
{
    EQ_SCR num = static_cast<EQ_SCR>(GET_ARG_INT(1));
    MapSessionData *sd = script_rid2sd(st);
    int32_t i = pc_checkequip(sd, equip[num]);
    st->push<Script::INT>(i >= 0);
}

/// Increase one of the player's stats by 1
static void builtin_statusup(ScriptState *st)
{
    SP type = static_cast<SP>(GET_ARG_INT(1));
    MapSessionData *sd = script_rid2sd(st);
    pc_statusup(sd, type);
}

/// Increase one of the player's stats by a value
static void builtin_statusup2(ScriptState *st)
{
    SP type = static_cast<SP>(GET_ARG_INT(2));
    int32_t val = GET_ARG_INT(2);
    MapSessionData *sd = script_rid2sd(st);
    pc_statusup2(sd, type, val);
}

/// Add a bonus to one of the player's stats
static void builtin_bonus(ScriptState *st)
{
    SP type = static_cast<SP>(GET_ARG_INT(1));
    int32_t val = GET_ARG_INT(2);
    MapSessionData *sd = script_rid2sd(st);
    pc_bonus(sd, type, val);
}

/// Do some sort of skill (temporary or permanent, depending on flag)
// this function should probably be avoided
static void builtin_skill(ScriptState *st)
{
    int32_t id = GET_ARG_INT(1);
    int32_t level = GET_ARG_INT(2);
    int32_t flag = 1;
    if (HAS_ARG(3))
        flag = GET_ARG_INT(3);
    MapSessionData *sd = script_rid2sd(st);
    pc_skill(sd, id, level, flag);
    clif_skillinfoblock(sd);
}

/// Grant player a skill, permanently
static void builtin_setskill(ScriptState *st)
{
    int32_t id = GET_ARG_INT(1);
    int32_t level = GET_ARG_INT(2);
    MapSessionData *sd = script_rid2sd(st);

    sd->status.skill[id].id = level ? id : 0;
    sd->status.skill[id].lv = level;
    clif_skillinfoblock(sd);
}

/// Return the level of a skill
static void builtin_getskilllv(ScriptState *st)
{
    int32_t id = GET_ARG_INT(1);
    st->push<Script::INT>(pc_checkskill(script_rid2sd(st), id));
}

/// Get GM level of a player
static void builtin_getgmlevel(ScriptState *st)
{
    st->push<Script::INT>(pc_isGM(script_rid2sd(st)));
}

/// Stop executing the script, immediately
static void builtin_end(ScriptState *st)
{
    st->state = ScriptExecutionState::END;
}

/// Return opt2
static void builtin_getopt2(ScriptState *st)
{
    MapSessionData *sd = script_rid2sd(st);
    st->push<Script::INT>(sd->opt2);
}

/// Set opt2
static void builtin_setopt2(ScriptState *st)
{
    int32_t new_opt2 = GET_ARG_INT(1);
    MapSessionData *sd = script_rid2sd(st);
    if (new_opt2 == sd->opt2)
        return;
    sd->opt2 = new_opt2;
    clif_changeoption(sd);
    pc_calcstatus(sd, 0);
}

/// Check whether the player's "option" has any of the specified bits
static void builtin_checkoption(ScriptState *st)
{
    int32_t type = GET_ARG_INT(1);
    MapSessionData *sd = script_rid2sd(st);
    st->push<Script::INT>(sd->status.option & type);
}

/// Set the player's "option"
static void builtin_setoption(ScriptState *st)
{
    int32_t type = GET_ARG_INT(1);
    MapSessionData *sd = script_rid2sd(st);
    pc_setoption(sd, type);
}

/// Set the player's respawn point
static void builtin_savepoint(ScriptState *st)
{
    fixed_string<16> str;
    str.copy_from(GET_ARG_STRING(1).c_str());
    int16_t x = GET_ARG_INT(2);
    int16_t y = GET_ARG_INT(3);
    pc_setsavepoint(script_rid2sd(st), Point{str, x, y});
}

/// gettimetick(type)
// 0: system tick (milliseconds)
// 1: seconds since midnight
// 2: seconds since epoch
static void builtin_gettimetick(ScriptState *st)
{
    switch (GET_ARG_INT(1))
    {
    // System tick (uint32_t, and yes, it will wrap)
    case 0:
        st->push<Script::INT>(gettick());
        break;
    // Seconds since midnight
    case 1:
    {
        time_t timer = time(NULL);
        struct tm *t = gmtime(&timer);
        st->push<Script::INT>(t->tm_hour * 3600 + t->tm_min * 60 + t->tm_sec);
        break;
    }
    // Seconds since Unix epoch.
    case 2:
        st->push<Script::INT>(time(NULL));
        break;
    default:
        st->push<Script::INT>(0);
    }
}

/// Get a component of the time
// 1: Seconds
// 2: Minutes
// 3: Hour
// 4: WeekDay (0-6)
// 5: MonthDay
// 6: Month
// 7: Year
static void builtin_gettime(ScriptState *st)
{
    time_t timer = time(NULL);
    struct tm *t = gmtime(&timer);

    switch (GET_ARG_INT(1))
    {
    case 1:
        st->push<Script::INT>(t->tm_sec);
        break;
    case 2:
        st->push<Script::INT>(t->tm_min);
        break;
    case 3:
        st->push<Script::INT>(t->tm_hour);
        break;
    case 4:
        st->push<Script::INT>(t->tm_wday);
        break;
    case 5:
        st->push<Script::INT>(t->tm_mday);
        break;
    case 6:
        st->push<Script::INT>(t->tm_mon + 1);
        break;
    case 7:
        st->push<Script::INT>(t->tm_year + 1900);
        break;
    default:
        st->push<Script::INT>(-1);
        break;
    }
}

/// interface for strftime
// it's annoying how this requires a length ...
static void builtin_gettimestr(ScriptState *st) __attribute__((deprecated));
static void builtin_gettimestr(ScriptState *st)
{
    time_t now = time(NULL);

    std::string fmtstr = GET_ARG_STRING(1);
    int32_t maxlen = GET_ARG_INT(2);

    char tmpstr[maxlen + 1];
    strftime(tmpstr, maxlen, fmtstr.c_str(), gmtime(&now));
    tmpstr[maxlen] = '\0';

    st->push<Script::STR>(tmpstr);
}

/// Open the player's storage
static void builtin_openstorage(ScriptState *st)
{
    MapSessionData *sd = script_rid2sd(st);

    st->state = ScriptExecutionState::STOP;
    sd->npc_flags.storage = 1;

    storage_storageopen(sd);
}

/// Gain experience of the 2 types
static void builtin_getexp(ScriptState *st)
{
    int32_t base = GET_ARG_INT(1);
    int32_t job = GET_ARG_INT(2);

    if (base < 0 || job < 0)
        return;
    MapSessionData *sd = script_rid2sd(st);
    if (sd)
        pc_gainexp_reason(sd, base, job, PC_GAINEXP_REASON::SCRIPT);
}

/// Spawn monster(s) at a point
static void builtin_monster(ScriptState *st)
{
    fixed_string<16> map;
    map.copy_from(GET_ARG_STRING(1).c_str());
    uint16_t x = GET_ARG_INT(2);
    uint16_t y = GET_ARG_INT(3);
    std::string str = GET_ARG_STRING(4);
    int32_t mob_class = GET_ARG_INT(5);
    int32_t amount = GET_ARG_INT(6);
    std::string event;
    if (HAS_ARG(7))
        event = GET_ARG_STRING(7);

    mob_once_spawn(map_id2sd(st->rid), {map, x, y}, str.c_str(), mob_class, amount, event.c_str());
}

/// Spawn monster(s) in an area
static void builtin_areamonster(ScriptState *st)
{
    fixed_string<16> map;
    map.copy_from(GET_ARG_STRING(1).c_str());
    int32_t x_0 = GET_ARG_INT(2);
    int32_t y_0 = GET_ARG_INT(3);
    int32_t x_1 = GET_ARG_INT(4);
    int32_t y_1 = GET_ARG_INT(5);
    std::string str = GET_ARG_STRING(6);
    int32_t mob_class = GET_ARG_INT(7);
    int32_t amount = GET_ARG_INT(8);
    std::string event;
    if (HAS_ARG(9))
        event = GET_ARG_STRING(9);

    mob_once_spawn_area(map_id2sd(st->rid), map, x_0, y_0, x_1, y_1,
                        str.c_str(), mob_class, amount, event.c_str());
}

/// Callback to maybe kill a monster
static void builtin_killmonster_sub(BlockList *bl, const char *event, bool allflag)
{
    struct mob_data *md = static_cast<struct mob_data *>(bl);
    if (allflag
            ? (md->spawndelay_1 == -1 && md->spawndelay2 == -1)
            : (strcmp(event, md->npc_event) == 0))
        mob_delete(md);
}

/// Kill monsters with a given event
static void builtin_killmonster(ScriptState *st)
{
    fixed_string<16> mapname;
    mapname.copy_from(GET_ARG_STRING(1).c_str());
    std::string event = GET_ARG_STRING(2);
    bool allflag = event == "All";

    int32_t m = map_mapname2mapid(mapname);
    if (m < 0)
        return;
    map_foreachinarea(builtin_killmonster_sub, m, 0, 0, maps[m].xs, maps[m].ys,
                      BL_MOB, event.c_str(), allflag);
}

/// Callback to definitely kill a monster
static void builtin_killmonsterall_sub(BlockList *bl)
{
    mob_delete(static_cast<struct mob_data *>(bl));
}

/// Kill monsters unconditionally
// this is probably a bad idea
static void builtin_killmonsterall(ScriptState *st)
{
    fixed_string<16> mapname;
    mapname.copy_from(GET_ARG_STRING(1).c_str());

    int32_t m = map_mapname2mapid(mapname);
    if (m < 0)
        return;
    map_foreachinarea(builtin_killmonsterall_sub,
                      m, 0, 0, maps[m].xs, maps[m].ys, BL_MOB);
}

/// Manually invoke an event
// the event is called with the attached player
// but not the way a callfunc happens
// (i.e. it has a separate stack)
static void builtin_doevent(ScriptState *st)
{
    std::string event = GET_ARG_STRING(1);
    npc_event(map_id2sd(st->rid), event.c_str(), 0);
}

/// Invoke an NPC event (without an attached player)
static void builtin_donpcevent(ScriptState *st)
{
    std::string event = GET_ARG_STRING(1);
    npc_event_do(event.c_str());
}

/// Invoke an event later
static void builtin_addtimer(ScriptState *st)
{
    int32_t tick = GET_ARG_INT(1);
    std::string event = GET_ARG_STRING(2);
    pc_addeventtimer(script_rid2sd(st), tick, event.c_str());
}

/// Don't invoke an event later
static void builtin_deltimer(ScriptState *st)
{
    std::string event = GET_ARG_STRING(1);
    pc_deleventtimer(script_rid2sd(st), event.c_str());
}

/// Set npc timer to tick 0 and start it
static void builtin_initnpctimer(ScriptState *st)
{
    struct npc_data_script *nd = static_cast<struct npc_data_script *>(map_id2bl(st->oid));

    npc_settimerevent_tick(nd, 0);
    npc_timerevent_start(nd);
}

/// Resume an NPC timer
// usually you want initnpctimer instead
static void builtin_startnpctimer(ScriptState *st)
{
    struct npc_data_script *nd = static_cast<struct npc_data_script *>(map_id2bl(st->oid));

    npc_timerevent_start(nd);
}

/// Stop an NPC timer
// does not reset the tick, that is the job of initnpctimer
static void builtin_stopnpctimer(ScriptState *st)
{
    struct npc_data_script *nd = static_cast<struct npc_data_script *>(map_id2bl(st->oid));

    npc_timerevent_stop(nd);
}

/// Get some form of a tick of the npc timer
static void builtin_getnpctimer(ScriptState *st)
{
    struct npc_data_script *nd = static_cast<struct npc_data_script *>(map_id2bl(st->oid));

    switch (GET_ARG_INT(1))
    {
    case 0:
        st->push<Script::INT>(npc_gettimerevent_tick(nd));
        break;
    case 1:
        st->push<Script::INT>((nd->scr.nexttimer >= 0));
        break;
    case 2:
        st->push<Script::INT>(nd->scr.timeramount);
        break;
    default:
        st->push<Script::INT>(0);
    }
}

/// Set the tick of an NPC timer
static void builtin_setnpctimer(ScriptState *st)
{
    int32_t tick = GET_ARG_INT(1);
    struct npc_data_script *nd = static_cast<struct npc_data_script *>(map_id2bl(st->oid));

    npc_settimerevent_tick(nd, tick);
}

/// Do an announcement (with flags)
static void builtin_announce(ScriptState *st)
{
    std::string str = GET_ARG_STRING(1);
    int32_t flag = GET_ARG_INT(2);

    if (flag & 0x0f)
    {
        BlockList *bl = (flag & 0x08)
                ? map_id2bl(st->oid)
                : script_rid2sd(st);
        clif_GMmessage(bl, str.c_str(), str.size() + 1, flag);
    }
    else
        intif_GMmessage(str.c_str(), str.size() + 1);
}

/// Announce to all players on a map
static void builtin_mapannounce(ScriptState *st)
{
    fixed_string<16> mapname;
    mapname.copy_from(GET_ARG_STRING(1).c_str());
    std::string str = GET_ARG_STRING(2);
//     int32_t flag = GET_ARG_INT(3);

    int32_t m = map_mapname2mapid(mapname);
    if (m < 0)
        return;
    map_foreachinarea(clif_GMmessage,
                      m, 0, 0, maps[m].xs, maps[m].ys, BL_PC, str.c_str(),
                      str.size() + 1, 3);
}

/// Announce to players in part of a map
static void builtin_areaannounce(ScriptState *st)
{
    fixed_string<16> map;
    map.copy_from(GET_ARG_STRING(1).c_str());
    int32_t x_0 = GET_ARG_INT(2);
    int32_t y_0 = GET_ARG_INT(3);
    int32_t x_1 = GET_ARG_INT(4);
    int32_t y_1 = GET_ARG_INT(5);
    std::string str = GET_ARG_STRING(6);
//     int32_t flag = GET_ARG_INT(7);

    int32_t m = map_mapname2mapid(map);
    if (m < 0)
        return;

    map_foreachinarea(clif_GMmessage,
                      m, x_0, y_0, x_1, y_1, BL_PC, str.c_str(),
                      str.size() + 1, 3);
}

/// Count users - on server, or on map of NPC or player
static void builtin_getusers(ScriptState *st)
{
    int32_t flag = GET_ARG_INT(1);
    BlockList *bl = map_id2bl((flag & 0x08) ? st->oid : st->rid);
    int32_t val = (flag & 1) ? map_getusers() : maps[bl->m].users;
    st->push<Script::INT>(val);
}

/// Get users on a map
static void builtin_getmapusers(ScriptState *st)
{
    fixed_string<16> str;
    str.copy_from(GET_ARG_STRING(1).c_str());
    int32_t m = map_mapname2mapid(str);
    if (m < 0)
    {
        st->push<Script::INT>(-1);
        return;
    }
    st->push<Script::INT>(maps[m].users);
}

/// Helper to count the users in an area
static void builtin_getareausers_sub(BlockList *, int32_t *users)
{
    ++*users;
}

/// Count the number of users in an area
static void builtin_getareausers(ScriptState *st)
{
    fixed_string<16> str;
    str.copy_from(GET_ARG_STRING(1).c_str());
    int32_t x_0 = GET_ARG_INT(2);
    int32_t y_0 = GET_ARG_INT(3);
    int32_t x_1 = GET_ARG_INT(4);
    int32_t y_1 = GET_ARG_INT(5);
    int32_t m = map_mapname2mapid(str);
    if (m < 0)
    {
        st->push<Script::INT>(-1);
        return;
    }
    int32_t users;
    map_foreachinarea(builtin_getareausers_sub,
                      m, x_0, y_0, x_1, y_1, BL_PC, &users);
    st->push<Script::INT>(users);
}

/// Helper to count the dropped items of a type, and possibly delete them
template<bool del>
static void builtin_getareadropitem_sub(BlockList *bl, int32_t item, int32_t *amount)
{
    struct flooritem_data *drop = static_cast<struct flooritem_data *>(bl);

    if (drop->item_data.nameid == item)
    {
        (*amount) += drop->item_data.amount;
        if (del)
        {
            clif_clearflooritem(drop, -1);
            map_delobject(drop->id, drop->type);
        }
    }
}

/// Count dropped items of a type in an area, and possibly delete them
static void builtin_getareadropitem(ScriptState *st)
{
    fixed_string<16> str;
    str.copy_from(GET_ARG_STRING(1).c_str());
    int32_t x_0 = GET_ARG_INT(2);
    int32_t y_0 = GET_ARG_INT(3);
    int32_t x_1 = GET_ARG_INT(4);
    int32_t y_1 = GET_ARG_INT(5);

    int32_t item = 0;
    RESOLVE(6);
    if (TYPE_ARG(6) == Script::STR)
    {
        std::string name = GET_ARG_STRING(6);
        struct item_data *item_data = itemdb_searchname(name.c_str());
        if (item_data)
            item = item_data->nameid;
    }
    else
        item = GET_ARG_INT(6);

    int32_t delitems = 0;

    if (HAS_ARG(7))
        delitems = GET_ARG_INT(7);

    int32_t m = map_mapname2mapid(str);
    if (m < 0)
    {
        st->push<Script::INT>(-1);
        return;
    }
    int32_t amount = 0;
    if (delitems)
        map_foreachinarea(builtin_getareadropitem_sub<true>,
                          m, x_0, y_0, x_1, y_1, BL_ITEM, item, &amount);
    else
        map_foreachinarea(builtin_getareadropitem_sub<false>,
                          m, x_0, y_0, x_1, y_1, BL_ITEM, item, &amount);

    st->push<Script::INT>(amount);
}

/// Enable an NPC
static void builtin_enablenpc(ScriptState *st)
{
    std::string str = GET_ARG_STRING(1);
    npc_enable(str.c_str(), 1);
}

/// Disable an NPC
static void builtin_disablenpc(ScriptState *st)
{
    std::string str = GET_ARG_STRING(1);
    npc_enable(str.c_str(), 0);
}

/// Unhide an NPC
static void builtin_hideoffnpc(ScriptState *st)
{
    std::string str = GET_ARG_STRING(1);
    npc_enable(str.c_str(), 2);
}

/// Hide an NPC
static void builtin_hideonnpc(ScriptState *st)
{
    std::string str = GET_ARG_STRING(1);
    npc_enable(str.c_str(), 4);
}

/// Begin a status effect
static void builtin_sc_start(ScriptState *st)
{
    int32_t type = GET_ARG_INT(1);
    int32_t tick = GET_ARG_INT(2);
    int32_t val1 = GET_ARG_INT(3);
    BlockList *bl;
    if (HAS_ARG(4))
        bl = map_id2bl(GET_ARG_INT(4));
    else
        bl = script_rid2sd(st);
    skill_status_change_start(bl, type, val1, tick);
}

/// Maybe begin a status effect
static void builtin_sc_start2(ScriptState *st)
{
    int32_t type = GET_ARG_INT(1);
    int32_t tick = GET_ARG_INT(2);
    int32_t val1 = GET_ARG_INT(3);
    int32_t per = GET_ARG_INT(4);
    BlockList *bl;
    if (HAS_ARG(5))
        bl = map_id2bl(GET_ARG_INT(5));
    else
        bl = script_rid2sd(st);
    if (MRAND(10000) < per)
        skill_status_change_start(bl, type, val1, tick);
}

/// End a status effect
static void builtin_sc_end(ScriptState *st)
{
    int32_t type = GET_ARG_INT(1);
    BlockList *bl = script_rid2sd(st);
    skill_status_change_end(bl, type, NULL);
}

/// Check whether a status effect is active
static void builtin_sc_check(ScriptState *st)
{
    int32_t type = GET_ARG_INT(1);
    BlockList *bl = script_rid2sd(st);

    st->push<Script::INT>(skill_status_change_active(bl, type));
}

/// Print a debug message to stderr
static void builtin_debugmes(ScriptState *st)
{
    fprintf(stderr, "script debug : %d %d : %s\n", st->rid, st->oid,
            GET_ARG_STRING(1).c_str());
}

/// Resets all skills, and depending on the type, also:
/// 1: skill points, option, base/job level/exp, and stats
/// 2: skill points, base/job level/exp
/// 3: base level/exp
/// 4: job level/exp
static void builtin_resetlvl(ScriptState *st)
{
    int32_t type = GET_ARG_INT(1);

    MapSessionData *sd = script_rid2sd(st);
    pc_resetlvl(sd, type);
}

/// Reset your status points
static void builtin_resetstatus(ScriptState *st)
{
    MapSessionData *sd = script_rid2sd(st);
    pc_resetstate(sd);
}

/// Reset skills
static void builtin_resetskill(ScriptState *st)
{
    MapSessionData *sd;
    sd = script_rid2sd(st);
    pc_resetskill(sd);
}

/// Toggle the account's gender
static void builtin_changesex(ScriptState *st)
{
    MapSessionData *sd = script_rid2sd(st);

    if (sd->status.sex == 0)
    {
        sd->status.sex = 1;
        sd->sex = 1;
    }
    else if (sd->status.sex == 1)
    {
        sd->status.sex = 0;
        sd->sex = 0;
    }
    chrif_char_ask_name(-1, sd->status.name, CharOperation::CHANGE_SEX);
    chrif_save(sd);
}

/// Change the attached player
static void builtin_attachrid(ScriptState *st)
{
    st->rid = GET_ARG_INT(1);
    st->push<Script::INT>(map_id2sd(st->rid) != NULL);
}

/// Unattach the attached player
static void builtin_detachrid(ScriptState *st)
{
    st->rid = 0;
}

/// Check whether the player is logged in
static void builtin_isloggedin(ScriptState *st)
{
    st->push<Script::INT>(map_id2sd(GET_ARG_INT(1)) != NULL);
}

/// Note: These were changed to correspond with db/const.txt
enum class MapFlag
{
    NOMEMO = 0,
    NOTELEPORT = 1,
    NOSAVE = 2,
    NOBRANCH = 3,
    NOPENALTY = 4,
    PVP = 5,
    PVP_NOPARTY = 6,
    NOZENYPENALTY = 10,
    // not in const.txt but could be useful
    NOTRADE,
    NOWARP,
    NOPVP,
};

/// Disallow saving on the map, and set the respawn point
static void builtin_setmapflagnosave(ScriptState *st)
{
    fixed_string<16> str;
    str.copy_from(GET_ARG_STRING(1).c_str());
    fixed_string<16> str2;
    str2.copy_from(GET_ARG_STRING(2).c_str());
    int32_t x = GET_ARG_INT(3);
    int32_t y = GET_ARG_INT(4);
    int32_t m = map_mapname2mapid(str);
    if (m >= 0)
    {
        maps[m].flag.nosave = 1;
        maps[m].save.map = str2;
        maps[m].save.x = x;
        maps[m].save.y = y;
    }
}

/// Set a flag on a map
static void builtin_setmapflag(ScriptState *st)
{
    fixed_string<16> str;
    str.copy_from(GET_ARG_STRING(1).c_str());
    MapFlag i = static_cast<MapFlag>(GET_ARG_INT(2));
    int32_t m = map_mapname2mapid(str);
    if (m >= 0)
    {
        switch (i)
        {
        case MapFlag::NOMEMO:
            maps[m].flag.nomemo = 1;
            break;
        case MapFlag::NOTELEPORT:
            maps[m].flag.noteleport = 1;
            break;
        case MapFlag::NOBRANCH:
            maps[m].flag.nobranch = 1;
            break;
        case MapFlag::NOPENALTY:
            maps[m].flag.nopenalty = 1;
            break;
        case MapFlag::PVP_NOPARTY:
            maps[m].flag.pvp_noparty = 1;
            break;
        case MapFlag::NOZENYPENALTY:
            maps[m].flag.nozenypenalty = 1;
            break;
        case MapFlag::NOTRADE:
            maps[m].flag.notrade = 1;
            break;
        case MapFlag::NOWARP:
            maps[m].flag.nowarp = 1;
            break;
        case MapFlag::NOPVP:
            maps[m].flag.nopvp = 1;
            break;
        }
    }
}

/// Unset a flag on a map
static void builtin_removemapflag(ScriptState *st)
{
    fixed_string<16> str;
    str.copy_from(GET_ARG_STRING(1).c_str());
    MapFlag i = static_cast<MapFlag>(GET_ARG_INT(2));
    int32_t m = map_mapname2mapid(str);
    if (m >= 0)
    {
        switch (i)
        {
        case MapFlag::NOMEMO:
            maps[m].flag.nomemo = 0;
            break;
        case MapFlag::NOTELEPORT:
            maps[m].flag.noteleport = 0;
            break;
        case MapFlag::NOSAVE:
            maps[m].flag.nosave = 0;
            break;
        case MapFlag::NOBRANCH:
            maps[m].flag.nobranch = 0;
            break;
        case MapFlag::NOPENALTY:
            maps[m].flag.nopenalty = 0;
            break;
        case MapFlag::PVP_NOPARTY:
            maps[m].flag.pvp_noparty = 0;
            break;
        case MapFlag::NOZENYPENALTY:
            maps[m].flag.nozenypenalty = 0;
            break;
        case MapFlag::NOWARP:
            maps[m].flag.nowarp = 0;
            break;
        case MapFlag::NOPVP:
            maps[m].flag.nopvp = 0;
            break;
        }
    }
}

/// Enable PvP on a map
static void builtin_pvpon(ScriptState *st)
{
    fixed_string<16> str;
    str.copy_from(GET_ARG_STRING(1).c_str());
    int32_t m = map_mapname2mapid(str);
    if (m < 0)
        return;
    if (maps[m].flag.pvp)
        return;
    if (maps[m].flag.nopvp)
        return;
    maps[m].flag.pvp = 1;
    if (battle_config.pk_mode)
        return;

    for (MapSessionData *pl_sd : auth_sessions)
    {
        if (m != pl_sd->m)
            continue;
        if (!pl_sd->pvp_timer)
            continue;
        pl_sd->pvp_timer = add_timer(gettick() + 200, pc_calc_pvprank_timer, pl_sd->id);
        pl_sd->pvp_rank = 0;
        pl_sd->pvp_lastusers = 0;
        pl_sd->pvp_point = 5;
    }
}

/// Disable PvP on a map
static void builtin_pvpoff(ScriptState *st)
{
    fixed_string<16> str;
    str.copy_from(GET_ARG_STRING(1).c_str());
    int32_t m = map_mapname2mapid(str);
    if (m < 0)
        return;
    if (!maps[m].flag.pvp)
        return;
    // this used to be here, and was seriously broken
//     if (!maps[m].flag.nopvp)
//         return;
    maps[m].flag.pvp = 0;
    if (battle_config.pk_mode)
        return;

    for (MapSessionData *pl_sd : auth_sessions)
    {
        if (m != pl_sd->m)
            continue;
        if (!pl_sd->pvp_timer)
            continue;
        delete_timer(pl_sd->pvp_timer);
        pl_sd->pvp_timer = NULL;
    }
}

/// Display an emotion above the OID
static void builtin_emotion(ScriptState *st)
{
    clif_emotion(map_id2bl(st->oid), GET_ARG_INT(1));
}

/// Warp all players from a map
static void builtin_mapwarp(ScriptState *st)
{
    fixed_string<16> src_map;
    src_map.copy_from(GET_ARG_STRING(1).c_str());
    int32_t m = map_mapname2mapid(src_map);
    if (m < 0)
        return;

    int32_t x_0 = 0;
    int32_t y_0 = 0;
    int32_t x_1 = maps[m].xs;
    int32_t y_1 = maps[m].ys;
    fixed_string<16> dst_map;
    dst_map.copy_from(GET_ARG_STRING(2).c_str());
    int16_t x = GET_ARG_INT(3);
    int16_t y = GET_ARG_INT(4);

    map_foreachinarea(builtin_areawarp_sub,
                      m, x_0, y_0, x_1, y_1, BL_PC, Point{dst_map, x, y});
}

/// Invoke a "OnCommand*" Event on another NPC
static void builtin_cmdothernpc(ScriptState *st)
{
    std::string npc = GET_ARG_STRING(1);
    std::string command = GET_ARG_STRING(2);

    npc_command(map_id2sd(st->rid), npc.c_str(), command.c_str());
}

/// Helper to count mobs
static void builtin_mobcount_sub(BlockList *bl, const char *event, int32_t *c)
{
    if (strcmp(event, static_cast<struct mob_data *>(bl)->npc_event) == 0)
        ++*c;
}

/// Count mobs (- 1)
static void builtin_mobcount(ScriptState *st)
{
    fixed_string<16> mapname;
    mapname.copy_from(GET_ARG_STRING(1).c_str());
    std::string event = GET_ARG_STRING(2);
    int32_t m = map_mapname2mapid(mapname);
    if (m < 0)
    {
        st->push<Script::INT>(-1);
        return;
    }
    int32_t c = 0;
    map_foreachinarea(builtin_mobcount_sub, m, 0, 0, maps[m].xs, maps[m].ys,
                      BL_MOB, event.c_str(), &c);

    st->push<Script::INT>(c - 1);
}

/// (Try to) marry another player
static void builtin_marriage(ScriptState *st)
{
    std::string partner = GET_ARG_STRING(1);
    MapSessionData *sd = script_rid2sd(st);
    MapSessionData *p_sd = map_nick2sd(partner.c_str());

    st->push<Script::INT>(pc_marriage(sd, p_sd));
}

/// (Try to) divorce
static void builtin_divorce(ScriptState *st)
{
    MapSessionData *sd = script_rid2sd(st);

    // rely on pc_divorce to restart
    st->state = ScriptExecutionState::STOP;

    sd->npc_flags.divorce = 1;

    st->push<Script::INT>(pc_divorce(sd));
}

/// Mob information (not necessarily strings)
static void builtin_strmobinfo(ScriptState *st)
{
    int32_t num = GET_ARG_INT(1);
    int32_t mob_class = GET_ARG_INT(2);

    if (num <= 0 || num >= 8 || (mob_class >= 0 && mob_class <= 1000) || mob_class > 2000)
        abort();

    switch(num)
    {
    case 1:
        st->push<Script::STR>(mob_db[mob_class].name);
        return;
    case 2:
        st->push<Script::STR>(mob_db[mob_class].jname);
        return;
    case 3:
        st->push<Script::INT>(mob_db[mob_class].lv);
        return;
    case 4:
        st->push<Script::INT>(mob_db[mob_class].max_hp);
        return;
    case 5:
        st->push<Script::INT>(mob_db[mob_class].max_sp);
        return;
    case 6:
        st->push<Script::INT>(mob_db[mob_class].base_exp);
        return;
    case 7:
        st->push<Script::INT>(mob_db[mob_class].job_exp);
        return;
    }
}

/// Item name from ID or name
// The latter is not senseless: the argument is the programmer's name,
// but the result is the friendly name
static void builtin_getitemname(ScriptState *st)
{
    RESOLVE(1);

    struct item_data *i_data;
    if (TYPE_ARG(1) == Script::STR)
        i_data = itemdb_searchname(GET_ARG_STRING(1).c_str());
    else
        i_data = itemdb_search(GET_ARG_INT(1));

    std::string item_name;
    if (i_data)
        item_name = i_data->jname;

    st->push<Script::STR>(item_name);
}

/// Convert a logical spell name to its visible #invocation
static void builtin_getspellinvocation(ScriptState *st)
{
    POD_string name = NULL;
    name.assign(GET_ARG_STRING(1));

    POD_string invocation = magic_find_invocation(name);
    if (!invocation)
        invocation.assign("...");

    st->push<Script::STR>(invocation.c_str());
    name.free();
    invocation.free();
}

/// Get the name of a teleport anchor (is this used?)
static void builtin_getanchorinvocation(ScriptState *st)
{
    POD_string name = NULL;
    name.assign(GET_ARG_STRING(1));

    POD_string invocation = magic_find_anchor_invocation(name);
    if (!invocation)
        invocation.assign("...");

    st->push<Script::STR>(strdup(invocation.c_str()));
    name.free();
    invocation.free();
}

/// Married to whom?
// (why is this 2?)
static void builtin_getpartnerid2(ScriptState *st)
{
    MapSessionData *sd = script_rid2sd(st);
    st->push<Script::INT>(sd->status.partner_id);
}

/// Fill in some arrays of inventory stuff
static void builtin_getinventorylist(ScriptState *st)
{
    MapSessionData *sd = script_rid2sd(st);

    int32_t j = 0;
    for (int32_t i = 0; i < MAX_INVENTORY; i++)
    {
        if (!sd->status.inventory[i].nameid
                || !sd->status.inventory[i].amount)
            continue;
        sd->reg.set(add_str("@inventorylist_id") + (j << 24), sd->status.inventory[i].nameid);
        sd->reg.set(add_str("@inventorylist_amount") + (j << 24), sd->status.inventory[i].amount);
        sd->reg.set(add_str("@inventorylist_equip") + (j << 24), static_cast<uint16_t>(sd->status.inventory[i].equip));
        j++;
    }
    sd->reg.set(add_str("@inventorylist_count"), j);
}

/// helper to fill in some arrays of skill stuff
static void add_to_skill_list(MapSessionData *sd, int32_t skill_id, int32_t& count)
{
    sd->reg.set(add_str("@skilllist_id") + (count << 24), sd->status.skill[skill_id].id);
    sd->reg.set(add_str("@skilllist_lv") + (count << 24), sd->status.skill[skill_id].lv);
    sd->reg.set(add_str("@skilllist_flag") + (count << 24), sd->status.skill[skill_id].flags);
    sd->regstr.set(add_str("@skilllist_name$") + (count << 24), skill_name(skill_id));
    ++count;
}

/// Fill in all skills the player has
static void builtin_getskilllist(ScriptState *st)
{
    MapSessionData *sd = script_rid2sd(st);

    int32_t j = 0;
    for (int32_t i = 0; i < MAX_SKILL; i++)
        if (sd->status.skill[i].id && sd->status.skill[i].lv)
            add_to_skill_list(sd, i, j);
    sd->reg.set(add_str("@skilllist_count"), j);
}

/// Fill in all the "activated" skills (usually only one)
static void builtin_getactivatedpoolskilllist(ScriptState *st)
{
    MapSessionData *sd = script_rid2sd(st);
    int32_t pool_skills[MAX_SKILL_POOL];
    int32_t pool_size = skill_pool(sd, pool_skills);

    int32_t count = 0;
    for (int32_t i = 0; i < pool_size; i++)
    {
        int32_t skill_id = pool_skills[i];
        if (sd->status.skill[skill_id].id == skill_id)
            add_to_skill_list(sd, skill_id, count);
    }
    sd->reg.set(add_str("@skilllist_count"), count);
}


static void builtin_getunactivatedpoolskilllist(ScriptState *st)
{
    MapSessionData *sd = script_rid2sd(st);

    int32_t count = 0;
    for (int32_t i = 0; i < skill_pool_skills_size; i++)
    {
        int32_t skill_id = skill_pool_skills[i];
        if (sd->status.skill[skill_id].id == skill_id
                && !(sd->status.skill[skill_id].flags & SKILL_POOL_ACTIVATED))
            add_to_skill_list(sd, skill_id, count);
    }
    sd->reg.set(add_str("@skilllist_count"), count);
}

/// Get status of all poolable skills
static void builtin_getpoolskilllist(ScriptState *st)
{
    MapSessionData *sd = script_rid2sd(st);

    int32_t count = 0;
    for (int32_t i = 0; i < skill_pool_skills_size; i++)
    {
        int32_t skill_id = skill_pool_skills[i];
        if (sd->status.skill[skill_id].id == skill_id)
            add_to_skill_list(sd, skill_id, count);
    }
    sd->reg.set(add_str("@skilllist_count"), count);
}

/// Add a skill to the pool
static void builtin_poolskill(ScriptState *st)
{
    MapSessionData *sd = script_rid2sd(st);
    int32_t skill_id = GET_ARG_INT(1);

    skill_pool_activate(sd, skill_id);
    clif_skillinfoblock(sd);
}

/// Remove a skill from the pool
static void builtin_unpoolskill(ScriptState *st)
{
    MapSessionData *sd = script_rid2sd(st);
    int32_t skill_id = GET_ARG_INT(1);

    skill_pool_deactivate(sd, skill_id);
    clif_skillinfoblock(sd);
}

/// Check whether a skill is in the pool
static void builtin_checkpoolskill(ScriptState *st)
{
    MapSessionData *sd = script_rid2sd(st);
    int32_t skill_id = GET_ARG_INT(1);

    st->push<Script::INT>(skill_pool_is_activated(sd, skill_id));
}

/// Remove all items from the player's inventory
static void builtin_clearitem(ScriptState *st)
{
    MapSessionData *sd = script_rid2sd(st);
    for (int32_t i = 0; i < MAX_INVENTORY; i++)
    {
        if (sd->status.inventory[i].amount)
            pc_delitem(sd, i, sd->status.inventory[i].amount, 0);
    }
}

/// Special effect
static void builtin_misceffect(ScriptState *st)
{
    int32_t type = GET_ARG_INT(1);
    BlockList *bl = NULL;

    if (HAS_ARG(2))
    {
        RESOLVE(2);

        if (TYPE_ARG(2) == Script::STR)
            bl = map_nick2sd(GET_ARG_STRING(2).c_str());
        else
            bl = map_id2bl(GET_ARG_INT(2));
    }

    if (!bl && st->oid)
        bl = map_id2bl(st->oid);
    if (!bl)
        bl = script_rid2sd(st);

    if (bl)
        clif_misceffect(bl, type);
}

/// Special effect on the NPC
static void builtin_specialeffect(ScriptState *st)
{
    BlockList *bl = map_id2bl(st->oid);

    if (!bl)
        return;

    clif_specialeffect(bl, GET_ARG_INT(1), 0);
}

/// Special effect on the player
static void builtin_specialeffect2(ScriptState *st)
{
    MapSessionData *sd = script_rid2sd(st);

    if (!sd)
        return;

    clif_specialeffect(sd, GET_ARG_INT(1), 0);
}

/// Unequip everything
static void builtin_nude(ScriptState *st)
{
    MapSessionData *sd = script_rid2sd(st);

    if (!sd)
        return;

    for (EQUIP i : EQUIPs)
        if (sd->equip_index[i] >= 0)
            pc_unequipitem(sd, sd->equip_index[i], CalcStatus::LATER);
    pc_calcstatus(sd, 0);
}

/// Unequip whatever's in the slot
static void builtin_unequipbyid(ScriptState *st)
{
    MapSessionData *sd = script_rid2sd(st);
    if (!sd)
        return;

    EQUIP slot_id = static_cast<EQUIP>(GET_ARG_INT(1));

    if (sd->equip_index[slot_id] >= 0)
        pc_unequipitem(sd, sd->equip_index[slot_id], CalcStatus::LATER);

    pc_calcstatus(sd, 0);
}

/// Invoke a GM command as if from the current player with level 99
static void builtin_gmcommand(ScriptState *st)
{
    MapSessionData *sd = script_rid2sd(st);
    std::string cmd = GET_ARG_STRING(1);
    is_atcommand(sd->fd, sd, cmd.c_str(), 99);
}

/// Move an NPC within a map
static void builtin_npcwarp(ScriptState *st)
{
    int32_t x = GET_ARG_INT(1);
    int32_t y = GET_ARG_INT(2);
    std::string npc = GET_ARG_STRING(3);
    struct npc_data *nd = npc_name2id(npc.c_str());

    if (!nd)
        return;

    int16_t m = nd->m;

    if (m < 0 || !nd->prev
        || x < 0 || x >= maps[m].xs
        || y < 0 || y >= maps[m].ys)
        return;

    npc_enable(npc.c_str(), 0);
    map_delblock(nd);
    nd->x = x;
    nd->y = y;
    map_addblock(nd);
    npc_enable(npc.c_str(), 1);
}

/// direct chat message
static void builtin_message(ScriptState *st)
{
    std::string player = GET_ARG_STRING(1);
    std::string msg = GET_ARG_STRING(2);
    MapSessionData *pl_sd = map_nick2sd(player.c_str());

    if (!pl_sd)
        return;
    clif_displaymessage(pl_sd->fd, msg.c_str());
}

/// NPC chat
static void builtin_npctalk(ScriptState *st)
{
    struct npc_data *nd = static_cast<struct npc_data *>(map_id2bl(st->oid));
    if (!nd)
        return;

    std::string message = GET_ARG_STRING(1);
    std::string prefix = nd->name;
    prefix += " : ";
    message.insert(0, prefix);
    prefix.erase();
    clif_message(nd, message.c_str());
}

/// Check if player has any items at all
static void builtin_hasitems(ScriptState *st)
{
    MapSessionData *sd = script_rid2sd(st);

    for (int32_t i = 0; i < MAX_INVENTORY; i++)
    {
        if (sd->status.inventory[i].amount)
        {
            st->push<Script::INT>(1);
            return;
        }
    }

    st->push<Script::INT>(0);
}

/// Get an aspect of the character's appearance
// use one of the LOOK_* constants in db/const.txt
// usually LOOK_HAIR_STYLE or LOOK_HAIR_COLOR
// LOOK_SHOES doesn't work :/
static void builtin_getlook(ScriptState *st)
{
    MapSessionData *sd = script_rid2sd(st);

    LOOK type = static_cast<LOOK>(GET_ARG_INT(1));
    int32_t val = -1;
    switch (type)
    {
    case LOOK::HAIR:
        val = sd->status.hair;
        break;
    case LOOK::WEAPON:
        val = sd->status.weapon;
        break;
    case LOOK::LEGS:
        val = sd->status.legs;
        break;
    case LOOK::HEAD:
        val = sd->status.head;
        break;
    case LOOK::CHEST:
        val = sd->status.chest;
        break;
    case LOOK::HAIR_COLOR:
        val = sd->status.hair_color;
        break;
    case LOOK::SHIELD:
        val = sd->status.shield;
        break;
    case LOOK::SHOES:
        // huh?
        break;
}

    st->push<Script::INT>(val);
}

/// Return (a component of) the player's respawn point
// 0: map name (string)
// 1: x
// 2: y
static void builtin_getsavepoint(ScriptState *st)
{
    MapSessionData *sd = script_rid2sd(st);

    switch (GET_ARG_INT(1))
    {
    case 0:
        st->push<Script::STR>(&sd->status.save_point.map);
        return;
    case 1:
        st->push<Script::INT>(sd->status.save_point.x);
        return;
    case 2:
        st->push<Script::INT>(sd->status.save_point.y);
        return;
    default:
        abort();
    }
}

/// Cause an event to happen in the future
// this is like addtimer for all
static void builtin_areatimer_sub(BlockList *bl, int32_t tick, const char *event)
{
    pc_addeventtimer(static_cast<MapSessionData *>(bl), tick, event);
}

/// Cause an event to happen in the future for all players
static void builtin_areatimer(ScriptState *st)
{
    fixed_string<16> mapname;
    mapname.copy_from(GET_ARG_STRING(1).c_str());
    int32_t x_0 = GET_ARG_INT(2);
    int32_t y_0 = GET_ARG_INT(3);
    int32_t x_1 = GET_ARG_INT(4);
    int32_t y_1 = GET_ARG_INT(5);
    int32_t tick = GET_ARG_INT(6);
    std::string event = GET_ARG_STRING(7);

    int32_t m = map_mapname2mapid(mapname);
    if (m < 0)
        return;

    map_foreachinarea(builtin_areatimer_sub,
                      m, x_0, y_0, x_1, y_1, BL_PC, tick, event.c_str());
}

/// Check whether the PC is in the specified rectangle
static void builtin_isin(ScriptState *st)
{
    MapSessionData *sd = script_rid2sd(st);

    std::string str = GET_ARG_STRING(1);
    int32_t x_0 = GET_ARG_INT(2);
    int32_t y_0 = GET_ARG_INT(3);
    int32_t x_1 = GET_ARG_INT(4);
    int32_t y_1 = GET_ARG_INT(5);

    if (!sd)
        return;

    // assuming that map is most likely to match, so do the rest first
    st->push<Script::INT>(
        (sd->x >= x_0 && sd->x <= x_1)
        && (sd->y >= y_0 && sd->y <= y_1)
        && str == &maps[sd->m].name);
}

/// Close and Trigger the shop on a (hopefully) nearby shop NPC
// if it's not nearby, the client will get confused
static void builtin_shop(ScriptState *st)
{
    MapSessionData *sd = script_rid2sd(st);
    if (!sd)
        return;

    struct npc_data *nd = npc_name2id(GET_ARG_STRING(1).c_str());
    if (!nd)
        return;

    builtin_close(st);
    clif_npcbuysell(sd, nd->id);
}

/// Check whether the PC is dead
static void builtin_isdead(ScriptState *st)
{
    MapSessionData *sd = script_rid2sd(st);
    st->push<Script::INT>(pc_isdead(sd));
}

/// Changes a NPC name, and sprite
// NOTE: this is not persistent
static void builtin_fakenpcname(ScriptState *st)
{
    std::string name = GET_ARG_STRING(1);
    struct npc_data *nd = npc_name2id(name.c_str());
    if (!nd)
        return;
    std::string newname = GET_ARG_STRING(2);
    nd->npc_class = GET_ARG_INT(3);
    STRZCPY(nd->name, newname.c_str());

    // Refresh this npc
    npc_enable(name.c_str(), 0);
    npc_enable(name.c_str(), 1);
}

/// Get the PC's x pos
static void builtin_getx(ScriptState *st)
{
    MapSessionData *sd = script_rid2sd(st);

    st->push<Script::INT>(sd->x);
}

/// Get the PC's y pos
static void builtin_gety(ScriptState *st)
{
    MapSessionData *sd = script_rid2sd(st);
    st->push<Script::INT>(sd->y);
}

builtin_function_t builtin_functions[] =
{
#define BUILTIN_ARGS(name, args) {builtin_##name, #name, args}
    BUILTIN_ARGS(mes, "s"),
    BUILTIN_ARGS(next, ""),
    BUILTIN_ARGS(close, ""),
    BUILTIN_ARGS(close2, ""),
    BUILTIN_ARGS(menu, "sL*"),
    BUILTIN_ARGS(goto, "L"),
    BUILTIN_ARGS(callsub, "L*"),
    BUILTIN_ARGS(callfunc, "F*"),
    BUILTIN_ARGS(return, "*"),
    BUILTIN_ARGS(getarg, "i"),
    BUILTIN_ARGS(input, "N"),
    BUILTIN_ARGS(warp, "Mxy"),
    BUILTIN_ARGS(isat, "Mxy"),
    BUILTIN_ARGS(areawarp, "MxyxyMxy"),
    BUILTIN_ARGS(setlook, "ii"),
    BUILTIN_ARGS(set, "Ne"),
    BUILTIN_ARGS(setarray, "Ne*"),
    BUILTIN_ARGS(cleararray, "Nei"),
    BUILTIN_ARGS(copyarray, "NNi"),
    BUILTIN_ARGS(getarraysize, "N"),
    BUILTIN_ARGS(deletearray, "N*"),
    BUILTIN_ARGS(getelementofarray, "Ni"),
    BUILTIN_ARGS(if, "iF*"),
    BUILTIN_ARGS(getitem, "Ii**"),
    BUILTIN_ARGS(makeitem, "IiMxy"),
    BUILTIN_ARGS(delitem, "Ii"),
    BUILTIN_ARGS(heal, "ii"),
    BUILTIN_ARGS(itemheal, "ii"),
    BUILTIN_ARGS(percentheal, "ii"),
    BUILTIN_ARGS(rand, "i*"),
    BUILTIN_ARGS(countitem, "I"),
    BUILTIN_ARGS(checkweight, "Ii"),
    BUILTIN_ARGS(readparam, "i*"),
    BUILTIN_ARGS(getcharid, "i*"),
    BUILTIN_ARGS(getpartyname, "i"),
    BUILTIN_ARGS(getpartymember, "i"),
    BUILTIN_ARGS(strcharinfo, "i"),
    BUILTIN_ARGS(getequipid, "i"),
    BUILTIN_ARGS(getequipname, "i"),
    BUILTIN_ARGS(getequipisequiped, "i"),
    BUILTIN_ARGS(statusup, "i"),
    BUILTIN_ARGS(statusup2, "ii"),
    BUILTIN_ARGS(bonus, "ii"),
    BUILTIN_ARGS(skill, "ii*"),
    BUILTIN_ARGS(setskill, "ii"),
    BUILTIN_ARGS(getskilllv, "i"),
    BUILTIN_ARGS(getgmlevel, ""),
    BUILTIN_ARGS(end, ""),
    BUILTIN_ARGS(getopt2, ""),
    BUILTIN_ARGS(setopt2, "i"),
    BUILTIN_ARGS(checkoption, "i"),
    BUILTIN_ARGS(setoption, "i"),
    BUILTIN_ARGS(savepoint, "Mxy"),
    BUILTIN_ARGS(gettimetick, "i"),
    BUILTIN_ARGS(gettime, "i"),
    BUILTIN_ARGS(gettimestr, "si"),
    BUILTIN_ARGS(openstorage, "*"),
    BUILTIN_ARGS(monster, "Mxysmi*"),
    BUILTIN_ARGS(areamonster, "Mxyxysmi*"),
    BUILTIN_ARGS(killmonster, "ME"),
    BUILTIN_ARGS(killmonsterall, "M"),
    BUILTIN_ARGS(doevent, "E"),
    BUILTIN_ARGS(donpcevent, "E"),
    BUILTIN_ARGS(addtimer, "tE"),
    BUILTIN_ARGS(deltimer, "E"),
    BUILTIN_ARGS(initnpctimer, ""),
    BUILTIN_ARGS(stopnpctimer, ""),
    BUILTIN_ARGS(startnpctimer, ""),
    BUILTIN_ARGS(setnpctimer, "i"),
    BUILTIN_ARGS(getnpctimer, "i"),
    BUILTIN_ARGS(announce, "si"),
    BUILTIN_ARGS(mapannounce, "Msi"),
    BUILTIN_ARGS(areaannounce, "Mxyxysi"),
    BUILTIN_ARGS(getusers, "i"),
    BUILTIN_ARGS(getmapusers, "M"),
    BUILTIN_ARGS(getareausers, "Mxyxy"),
    BUILTIN_ARGS(getareadropitem, "Mxyxyi*"),
    BUILTIN_ARGS(enablenpc, "s"),
    BUILTIN_ARGS(disablenpc, "s"),
    BUILTIN_ARGS(hideoffnpc, "s"),
    BUILTIN_ARGS(hideonnpc, "s"),
    BUILTIN_ARGS(sc_start, "iTi*"),
    BUILTIN_ARGS(sc_start2, "iTii*"),
    BUILTIN_ARGS(sc_end, "i"),
    BUILTIN_ARGS(sc_check, "i"),
    BUILTIN_ARGS(debugmes, "s"),
    BUILTIN_ARGS(resetlvl, "i"),
    BUILTIN_ARGS(resetstatus, ""),
    BUILTIN_ARGS(resetskill, ""),
    BUILTIN_ARGS(changesex, ""),
    BUILTIN_ARGS(attachrid, "i"),
    BUILTIN_ARGS(detachrid, ""),
    BUILTIN_ARGS(isloggedin, "i"),
    BUILTIN_ARGS(setmapflagnosave, "MMxy"),
    BUILTIN_ARGS(setmapflag, "Mi"),
    BUILTIN_ARGS(removemapflag, "Mi"),
    BUILTIN_ARGS(pvpon, "M"),
    BUILTIN_ARGS(pvpoff, "M"),
    BUILTIN_ARGS(emotion, "i"),
    BUILTIN_ARGS(marriage, "P"),
    BUILTIN_ARGS(divorce, ""),
    BUILTIN_ARGS(getitemname, "I"),
    BUILTIN_ARGS(getspellinvocation, "s"),
    BUILTIN_ARGS(getanchorinvocation, "s"),
    BUILTIN_ARGS(getpartnerid2, ""),
    BUILTIN_ARGS(getexp, "ii"),
    BUILTIN_ARGS(getinventorylist, ""),
    BUILTIN_ARGS(getskilllist, ""),
    BUILTIN_ARGS(getpoolskilllist, ""),
    BUILTIN_ARGS(getactivatedpoolskilllist, ""),
    BUILTIN_ARGS(getunactivatedpoolskilllist, ""),
    BUILTIN_ARGS(poolskill, "i"),
    BUILTIN_ARGS(unpoolskill, "i"),
    BUILTIN_ARGS(checkpoolskill, "i"),
    BUILTIN_ARGS(clearitem, ""),
    BUILTIN_ARGS(misceffect, "i*"),
    BUILTIN_ARGS(strmobinfo, "im"),
    BUILTIN_ARGS(specialeffect, "i"),
    BUILTIN_ARGS(specialeffect2, "i"),
    BUILTIN_ARGS(nude, ""),
    BUILTIN_ARGS(mapwarp, "MMxy"),
    BUILTIN_ARGS(cmdothernpc, "ss"),
    BUILTIN_ARGS(gmcommand, "s"),
    BUILTIN_ARGS(npcwarp, "xys"),
    BUILTIN_ARGS(message, "Ps"),
    BUILTIN_ARGS(npctalk, "s"),
    BUILTIN_ARGS(hasitems, ""),
    BUILTIN_ARGS(mobcount, "ME"),
    BUILTIN_ARGS(getlook, "i"),
    BUILTIN_ARGS(getsavepoint, "i"),
    BUILTIN_ARGS(areatimer, "MxyxytE"),
    BUILTIN_ARGS(isin, "Mxyxy"),
    BUILTIN_ARGS(shop, "s"),
    BUILTIN_ARGS(isdead, ""),
    BUILTIN_ARGS(fakenpcname, "ssi"),
    BUILTIN_ARGS(unequipbyid, "i"),
    BUILTIN_ARGS(getx, ""),
    BUILTIN_ARGS(gety, ""),
    {NULL, NULL, NULL},
};
