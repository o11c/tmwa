#include "magic-expr.hpp"

#include <cmath>
#include <climits>

#include "../lib/int.hpp"
#include "../lib/lfsr.hpp"

#include "../common/mt_rand.hpp"
#include "../common/utils.hpp"

#include "battle.hpp"
#include "itemdb.hpp"
#include "magic-base.hpp"
#include "map.hpp"
#include "npc.hpp"
#include "pc.hpp"


# define RESULT_INT         (*(result.ty = TY::INT,        &result.v_int))
# define RESULT_DIR         (*(result.ty = TY::DIR,        &result.v_dir))
# define RESULT_STR         (*(result.ty = TY::STRING,     &result.v_string))
# define RESULT_ENTITY      (*(result.ty = TY::ENTITY,     &result.v_entity))
# define RESULT_LOCATION    (*(result.ty = TY::LOCATION,   &result.v_location))
# define RESULT_AREA        (*(result.ty = TY::AREA,       &result.v_area))
# define RESULT_SPELL       (*(result.ty = TY::SPELL,      &result.v_spell))
# define RESULT_INVOCATION  (*(result.ty = TY::INVOCATION, &result.v_invocation))


static const int heading_x[8] = { 0, -1, -1, -1, 0, 1, 1, 1 };
static const int heading_y[8] = { 1, 1, 0, -1, -1, -1, 0, 1 };

int map_is_solid(int m, int x, int y)
{
    return map_getcell(m, x, y) == 1;
}

area_t::~area_t()
{
    if (ty == AreaType::UNION)
    {
        delete a_union[0];
        delete a_union[1];
    }
}

area_t::area_t(const area_t& area) : size(area.size), ty(area.ty)
{
    switch(ty)
    {
    case AreaType::LOCATION:
        a_loc = area.a_loc;
        return;
    case AreaType::UNION:
        a_union[0] = new area_t(*area.a_union[0]);
        a_union[1] = new area_t(*area.a_union[1]);
        return;
    case AreaType::RECT:
        a_rect = area.a_rect;
        return;
    }
    abort();
}

area_t::area_t(const location_t& loc) :
    a_loc(loc),
    size(1),
    ty(AreaType::LOCATION)
{}

area_t::area_t(area_t *area, area_t *other_area) :
    a_union({area, other_area}),
    size(area->size + other_area->size),
    ty(AreaType::UNION)
{}

area_t::area_t(const location_t& loc, int width, int height) :
    a_rect({loc, width, height}),
    size(width * height),
    ty(AreaType::RECT)
{}

void magic_copy_var(val_t *dest, val_t *src)
{
    *dest = *src;

    switch (dest->ty)
    {
    case TY::STRING:
        dest->v_string = dest->v_string.clone();
        return;
    case TY::AREA:
        dest->v_area = new area_t(*dest->v_area);
        return;
    }
}

void magic_clear_var(val_t *v)
{
    switch (v->ty)
    {
    case TY::STRING:
        v->v_string.free();
        return;
    case TY::AREA:
        delete v->v_area;
        return;
    }
}

static const char *show_entity(BlockList *entity)
{
    switch (entity->type)
    {
    case BL_PC:
        return static_cast<MapSessionData *>(entity)->status.name;
    case BL_NPC:
        return static_cast<struct npc_data *>(entity)->name;
    case BL_MOB:
        return static_cast<struct mob_data *>(entity)->name;
    case BL_ITEM:
        /// You should have seen this *before*
        return itemdb_search(static_cast<struct flooritem_data *>(entity)->item_data.nameid)->name;
    case BL_SPELL:
        return "%invocation(ERROR:this-should-not-be-an-entity)";
    default:
        return "%unknown-entity";
    }
}

static void stringify(val_t *v, int within_op __attribute__((deprecated)))
{
    static const char *dirs[8] =
    {
        "south", "south-west", "west", "north-west",
        "north", "north-east", "east", "south-east"
    };
    POD_string buf = NULL;

    switch (v->ty)
    {
    case TY::UNDEF:
        buf.assign("UNDEF");
        break;

    case TY::INT:
        buf.resize(31);
        sprintf(&buf[0], "%d", v->v_int);
        break;

    case TY::STRING:
        return;

    case TY::DIR:
        buf.assign(dirs[static_cast<int>(v->v_dir)]);
        break;

    case TY::ENTITY:
        buf.assign(show_entity(v->v_entity));
        break;

    case TY::LOCATION:
        buf.resize(127);
        sprintf(&buf[0], "<\"%s\", %d, %d>", &maps[v->v_location.m].name,
                v->v_location.x, v->v_location.y);
        break;

    case TY::AREA:
        buf.assign("%area");
        delete v->v_area;
        break;

    case TY::SPELL:
        buf = v->v_spell->name.clone();
        break;

    case TY::INVOCATION:
    {
        invocation_t *invocation =
            within_op
            ? v->v_invocation
            : static_cast<invocation_t *>(map_id2bl(v->v_int));
        buf = invocation->spell->name.clone();
    }
        break;

    default:
        fprintf(stderr, "[magic] INTERNAL ERROR: Cannot stringify %d\n",
                static_cast<int>(v->ty));
        abort();
    }

    v->v_string = buf;
    v->ty = TY::STRING;
}

static void intify(val_t *v)
{
    if (v->ty == TY::INT)
        return;

    magic_clear_var(v);
    v->ty = TY::INT;
    v->v_int = 1;
}

/**
 * Turns location into area, leaves other types untouched
 */
static void make_area(val_t *v)
{
    if (v->ty == TY::LOCATION)
    {
        v->v_area = new area_t(v->v_location);
        v->ty = TY::AREA;
    }
}

static void make_location(val_t *v)
{
    if (v->ty == TY::AREA && v->v_area->ty == AreaType::LOCATION)
    {
        location_t location = v->v_area->a_loc;
        delete v->v_area;
        v->ty = TY::LOCATION;
        v->v_location = location;
    }
}

static void make_spell(val_t *v)
{
    if (v->ty == TY::INVOCATION)
    {
        invocation_t *invoc = v->v_invocation;
        if (!invoc)
            v->ty = TY::FAIL;
        else
        {
            v->ty = TY::SPELL;
            v->v_spell = invoc->spell;
        }
    }
}

// Functions
// int fun_FOO(result, argv[])

static bool fun_add(val_t& result, val_t args[])
{
    if (args[0].ty == TY::INT && args[1].ty == TY::INT)
    {
        /* Integer addition */
        RESULT_INT = ARG_INT(0) + ARG_INT(1);
        result.ty = TY::INT;
    }
    else if (ARG_MAY_BE_AREA(0) && ARG_MAY_BE_AREA(1))
    {
        /* AreaType union */
        make_area(&args[0]);
        make_area(&args[1]);
        RESULT_AREA = new area_t(ARG_AREA(0), ARG_AREA(1));
        ARG_AREA(0) = NULL;
        ARG_AREA(1) = NULL;
        result.ty = TY::AREA;
    }
    else
    {
        /* Anything else -> string concatenation */
        stringify(&args[0], 1);
        stringify(&args[1], 1);
        /* Yes, we could speed this up. */
        RESULT_STR = ARG_STR(0).clone();
        RESULT_STR.reserve(ARG_STR(0).size() + ARG_STR(1).size());
        RESULT_STR.append(ARG_STR(1));
        result.ty = TY::STRING;
    }
    return 0;
}

static bool fun_sub(val_t& result, val_t args[])
{
    RESULT_INT = ARG_INT(0) - ARG_INT(1);
    return 0;
}

static bool fun_mul(val_t& result, val_t args[])
{
    RESULT_INT = ARG_INT(0) * ARG_INT(1);
    return 0;
}

static bool fun_div(val_t& result, val_t args[])
{
    if (!ARG_INT(1))
        return 1;               /* division by zero */
    RESULT_INT = ARG_INT(0) / ARG_INT(1);
    return 0;
}

static bool fun_mod(val_t& result, val_t args[])
{
    if (!ARG_INT(1))
        return 1;               /* division by zero */
    RESULT_INT = ARG_INT(0) % ARG_INT(1);
    return 0;
}

static bool fun_or(val_t& result, val_t args[])
{
    RESULT_INT = ARG_INT(0) || ARG_INT(1);
    return 0;
}

static bool fun_and(val_t& result, val_t args[])
{
    RESULT_INT = ARG_INT(0) && ARG_INT(1);
    return 0;
}

static bool fun_not(val_t& result, val_t args[])
{
    RESULT_INT = !ARG_INT(0);
    return 0;
}

static bool fun_neg(val_t& result, val_t args[])
{
    RESULT_INT = ~ARG_INT(0);
    return 0;
}

static bool fun_gte(val_t& result, val_t args[])
{
    if (args[0].ty == TY::STRING || args[1].ty == TY::STRING)
    {
        stringify(&args[0], 1);
        stringify(&args[1], 1);
        RESULT_INT = ARG_STR(0) >= ARG_STR(1);
    }
    else
    {
        intify(&args[0]);
        intify(&args[1]);
        RESULT_INT = ARG_INT(0) >= ARG_INT(1);
    }
    return 0;
}

static bool fun_gt(val_t& result, val_t args[])
{
    if (args[0].ty == TY::STRING || args[1].ty == TY::STRING)
    {
        stringify(&args[0], 1);
        stringify(&args[1], 1);
        RESULT_INT = ARG_STR(0) > ARG_STR(1);
    }
    else
    {
        intify(&args[0]);
        intify(&args[1]);
        RESULT_INT = ARG_INT(0) > ARG_INT(1);
    }
    return 0;
}

static bool fun_eq(val_t& result, val_t args[])
{
    if (args[0].ty == TY::STRING || args[1].ty == TY::STRING)
    {
        stringify(&args[0], 1);
        stringify(&args[1], 1);
        RESULT_INT = ARG_STR(0) == ARG_STR(1);
    }
    else if (args[0].ty == TY::DIR && args[1].ty == TY::DIR)
        RESULT_INT = ARG_DIR(0) == ARG_DIR(1);
    else if (args[0].ty == TY::ENTITY && args[1].ty == TY::ENTITY)
        RESULT_INT = ARG_ENTITY(0) == ARG_ENTITY(1);
    else if (args[0].ty == TY::LOCATION && args[1].ty == TY::LOCATION)
        RESULT_INT = (ARG_LOCATION(0).x == ARG_LOCATION(1).x
                     && ARG_LOCATION(0).y == ARG_LOCATION(1).y
                     && ARG_LOCATION(0).m == ARG_LOCATION(1).m);
    else if (args[0].ty == TY::AREA && args[1].ty == TY::AREA)
        RESULT_INT = ARG_AREA(0) == ARG_AREA(1); /* Probably not that great an idea... */
    else if (args[0].ty == TY::SPELL && args[1].ty == TY::SPELL)
        RESULT_INT = ARG_SPELL(0) == ARG_SPELL(1);
    else if (args[0].ty == TY::INVOCATION && args[1].ty == TY::INVOCATION)
        RESULT_INT = ARG_INVOCATION(0) == ARG_INVOCATION(1);
    else
    {
        intify(&args[0]);
        intify(&args[1]);
        RESULT_INT = ARG_INT(0) == ARG_INT(1);
    }
    return 0;
}

static bool fun_bitand(val_t& result, val_t args[])
{
    RESULT_INT = ARG_INT(0) & ARG_INT(1);
    return 0;
}

static bool fun_bitor(val_t& result, val_t args[])
{
    RESULT_INT = ARG_INT(0) | ARG_INT(1);
    return 0;
}

static bool fun_bitxor(val_t& result, val_t args[])
{
    RESULT_INT = ARG_INT(0) ^ ARG_INT(1);
    return 0;
}

static bool fun_bitshl(val_t& result, val_t args[])
{
    RESULT_INT = ARG_INT(0) << ARG_INT(1);
    return 0;
}

static bool fun_bitshr(val_t& result, val_t args[])
{
    RESULT_INT = ARG_INT(0) >> ARG_INT(1);
    return 0;
}

static bool fun_max(val_t& result, val_t args[])
{
    RESULT_INT = std::max(ARG_INT(0), ARG_INT(1));
    return 0;
}

static bool fun_min(val_t& result, val_t args[])
{
    RESULT_INT = std::min(ARG_INT(0), ARG_INT(1));
    return 0;
}

static bool fun_if_then_else(val_t& result, val_t args[])
{
    if (ARG_INT(0))
        magic_copy_var(&result, &args[1]);
    else
        magic_copy_var(&result, &args[2]);
    return 0;
}

location_t area_t::rect(unsigned int& width, unsigned int& height)
{
    switch (ty)
    {
    case AreaType::UNION:
    default:
        abort();

    case AreaType::LOCATION:
        width = 1;
        height = 1;
        return a_loc;

    case AreaType::RECT:
        width = a_rect.width;
        height = a_rect.height;
        return a_rect.loc;
    }
}

bool area_t::contains(location_t loc)
{
    switch (ty)
    {
    case AreaType::UNION:
        return a_union[0]->contains(loc)
            || a_union[1]->contains(loc);
    case AreaType::LOCATION:
        return a_loc.m == loc.m && a_loc.x == loc.x && a_loc.y == loc.y;
    case AreaType::RECT:
    {
        location_t aloc = a_rect.loc;
        unsigned awidth = a_rect.width;
        unsigned aheight = a_rect.height;
        return aloc.m == loc.m
                && (loc.x >= aloc.x) && (loc.y >= aloc.y)
                && (loc.x < aloc.x + awidth) && (loc.y < aloc.y + aheight);
    }
    default:
        fprintf(stderr, "INTERNAL ERROR: Invalid area\n");
        abort();
    }
}

static bool fun_is_in(val_t& result, val_t args[])
{
    RESULT_INT = ARG_AREA(1)->contains(ARG_LOCATION(0));
    return 0;
}

static bool fun_skill(val_t& result, val_t args[])
{
    if (ARG_ENTITY(0)->type != BL_PC
        || ARG_INT(1) < 0
        || ARG_INT(1) >= MAX_SKILL
        || ARG_PC(0)->status.skill[ARG_INT(1)].id != ARG_INT(1))
        RESULT_INT = 0;
    else
        RESULT_INT = ARG_PC(0)->status.skill[ARG_INT(1)].lv;
    return 0;
}

static bool fun_has_shroud(val_t& result, val_t args[])
{
    RESULT_INT = (ARG_ENTITY(0)->type == BL_PC && ARG_PC(0)->state.shroud_active);
    return 0;
}

#define BATTLE_GETTER(name) \
static bool fun_##name(val_t& result, val_t args[]) \
{ \
    RESULT_INT = static_cast<int>(battle_get_##name(ARG_ENTITY(0))); \
    return 0; \
}

BATTLE_GETTER(str);     // battle_get_str
BATTLE_GETTER(agi);     // battle_get_agi
BATTLE_GETTER(vit);     // battle_get_vit
BATTLE_GETTER(dex);     // battle_get_dex
BATTLE_GETTER(luk);     // battle_get_luk
BATTLE_GETTER(int);     // battle_get_int
BATTLE_GETTER(level);   // battle_get_level
BATTLE_GETTER(hp);      // battle_get_hp
BATTLE_GETTER(mdef);    // battle_get_mdef
BATTLE_GETTER(def);     // battle_get_def
BATTLE_GETTER(max_hp);  // battle_get_max_hp
BATTLE_GETTER(dir);     // battle_get_dir

#define MMO_GETTER(name) \
static bool fun_##name(val_t& result, val_t args[]) \
{ \
    if (ARG_ENTITY(0)->type == BL_PC) \
        RESULT_INT = ARG_PC(0)->status.name; \
    else \
        RESULT_INT = 0; \
    return 0; \
}

MMO_GETTER(sp);
MMO_GETTER(max_sp);

static bool fun_name_of(val_t& result, val_t args[])
{
    if (args[0].ty == TY::ENTITY)
    {
        RESULT_STR.assign(show_entity(ARG_ENTITY(0)));
        return 0;
    }
    else if (args[0].ty == TY::SPELL)
    {
        RESULT_STR = ARG_SPELL(0)->name.clone();
        return 0;
    }
    else if (args[0].ty == TY::INVOCATION)
    {
        RESULT_STR = ARG_INVOCATION(0)->spell->name.clone();
        return 0;
    }
    return 1;
}

/* [Freeyorp] I'm putting this one in as name_of seems to have issues with summoned or spawned mobs. */
static bool fun_mob_id(val_t& result, val_t args[])
{
    if (ARG_ENTITY(0)->type != BL_MOB) return 1;
    RESULT_INT = ARG_MOB(0)->mob_class;
    return 0;
}

#define COPY_LOCATION(dest, src) (dest).x = (src).x; (dest).y = (src).y; (dest).m = (src).m;

static bool fun_location(val_t& result, val_t args[])
{
    COPY_LOCATION(RESULT_LOCATION, *(ARG_ENTITY(0)));
    return 0;
}

static bool fun_random(val_t& result, val_t args[])
{
    int delta = ARG_INT(0);
    if (delta < 0)
        delta = -delta;
    if (delta == 0)
    {
        RESULT_INT = 0;
        return 0;
    }
    RESULT_INT = MRAND(delta);

    if (ARG_INT(0) < 0)
        result.v_int = -result.v_int;
    return 0;
}

static bool fun_random_dir(val_t& result, val_t args[])
{
    if (ARG_INT(0))
        RESULT_DIR = static_cast<Direction>(MRAND(8));
    else
        RESULT_DIR = static_cast<Direction>(MRAND(4) * 2);
    return 0;
}

static bool fun_hash_entity(val_t& result, val_t args[])
{
    RESULT_INT = ARG_ENTITY(0)->id;
    return 0;
}

bool magic_find_item(val_t args[], int idx, struct item *item, bool *stackable)
{
    struct item_data *item_data;

    if (args[idx].ty == TY::INT)
        item_data = itemdb_exists(ARG_INT(idx));
    else if (args[idx].ty == TY::STRING)
        item_data = itemdb_searchname(ARG_STR(idx).c_str());
    else
        return -1;

    if (!item_data)
        return 1;

    bool must_add_sequentially = (
        item_data->type == 4
        || item_data->type == 5
        || item_data->type == 7
        || item_data->type == 8); /* Very elegant. */

    if (stackable)
        *stackable = !must_add_sequentially;

    memset(item, 0, sizeof(struct item));
    item->nameid = item_data->nameid;

    return 0;
}

static bool fun_count_item(val_t& result, val_t args[])
{
    if (ARG_ENTITY(0)->type != BL_PC)
        return 1;

    struct item item;
    bool stackable;
    GET_ARG_ITEM(1, item, stackable);

    RESULT_INT = pc_count_all_items(ARG_PC(0), item.nameid);
    return 0;
}

static bool fun_is_equipped(val_t& result, val_t args[])
{
    if (ARG_ENTITY(0)->type != BL_PC)
        return 1;

    struct item item;
    bool stackable;
    GET_ARG_ITEM(1, item, stackable);

    MapSessionData *chr = ARG_PC(0);
    int retval = 0;
    for (int i = 0; i < 11; i++)
        if (chr->equip_index[i] >= 0
            && chr->status.inventory[chr->equip_index[i]].nameid == item.nameid)
        {
            retval = i + 1;
            break;
        }

    RESULT_INT = retval;
    return 0;
}

static bool fun_is_married(val_t& result, val_t args[])
{
    RESULT_INT = (ARG_ENTITY(0)->type == BL_PC && ARG_PC(0)->status.partner_id);
    return 0;
}

static bool fun_is_dead(val_t& result, val_t args[])
{
    RESULT_INT = (ARG_ENTITY(0)->type == BL_PC && pc_isdead(ARG_PC(0)));
    return 0;
}

static bool fun_is_pc(val_t& result, val_t args[])
{
    RESULT_INT = (ARG_ENTITY(0)->type == BL_PC);
    return 0;
}

static bool fun_partner(val_t& result, val_t args[])
{
    if (ARG_ENTITY(0)->type == BL_PC && ARG_PC(0)->status.partner_id)
    {
        RESULT_ENTITY = map_id2sd(ARG_PC(0)->status.partner_id);
        return 0;
    }
    return 1;
}

static bool fun_awayfrom(val_t& result, val_t args[])
{
    location_t& loc = ARG_LOCATION(0);
    int dx = heading_x[static_cast<int>(ARG_DIR(1))];
    int dy = heading_y[static_cast<int>(ARG_DIR(1))];
    int distance = ARG_INT(2);
    while (distance-- && !map_is_solid(loc.m, loc.x + dx, loc.y + dy))
    {
        loc.x += dx;
        loc.y += dy;
    }

    RESULT_LOCATION = loc;
    return 0;
}

static bool fun_failed(val_t& result, val_t args[])
{
    RESULT_INT = args[0].ty == TY::FAIL;
    return 0;
}

static bool fun_npc(val_t& result, val_t args[])
{
    RESULT_ENTITY = npc_name2id(ARG_STR(0).c_str());
    return RESULT_ENTITY == NULL;
}

static bool fun_pc(val_t& result, val_t args[])
{
    RESULT_ENTITY = map_nick2sd(ARG_STR(0).c_str());
    return RESULT_ENTITY == NULL;
}

static bool fun_distance(val_t& result, val_t args[])
{
    if (ARG_LOCATION(0).m != ARG_LOCATION(1).m)
        RESULT_INT = INT_MAX;
    else
        RESULT_INT = MAX(abs(ARG_LOCATION(0).x - ARG_LOCATION(1).x),
                         abs(ARG_LOCATION(0).y - ARG_LOCATION(1).y));
    return 0;
}

static bool fun_rdistance(val_t& result, val_t args[])
{
    if (ARG_LOCATION(0).m != ARG_LOCATION(1).m)
        RESULT_INT = INT_MAX;
    else
    {
        int dx = ARG_LOCATION(0).x - ARG_LOCATION(1).x;
        int dy = ARG_LOCATION(0).y - ARG_LOCATION(1).y;
        RESULT_INT = sqrt((dx * dx) + (dy * dy));
    }
    return 0;
}

static bool fun_anchor(val_t& result, val_t args[])
{
    RESULT_AREA = magic_find_anchor(ARG_STR(0));

    return 0;
}

static bool fun_line_of_sight(val_t& result, val_t args[])
{
    BlockList e1(BL_NUL), e2(BL_NUL);

    COPY_LOCATION(e1, ARG_LOCATION(0));
    COPY_LOCATION(e2, ARG_LOCATION(1));

    RESULT_INT = battle_check_range(&e1, &e2, 0);

    return 0;
}

location_t area_t::random_location()
{
    switch (ty)
    {
    case AreaType::UNION:
    {
        // remember: size = au0->size + au1->size
        int rv = MRAND(size);
        // we are operating under the assumption that each component
        // has at least one walkable cell
        if (rv < a_union[0]->size)
            return a_union[0]->random_location();
        else
            return a_union[1]->random_location();
    }

    case AreaType::LOCATION:
        return a_loc;
    case AreaType::RECT:
    {
        unsigned int w = a_rect.width, h = a_rect.height;
        const location_t loc = a_rect.loc;

        if (w < 1 || h < 1)
            abort();

        const unsigned init_pos = MRAND(w*h);
        // LFSR has 1-based bit indexing, Also need to add one if w*h is 2^n-1
        const unsigned bits = 1 + highest_bit(w*h + 1);

        unsigned pos = init_pos;
        if (map_is_solid(loc.m, loc.x + pos % w, loc.y + pos / w))
        {
            do
            {
                do
                    pos = lfsr_next(pos, bits, true);
                while (pos >= w*h);
                if (map_is_solid(loc.m, loc.x + pos % w, loc.y + pos / w))
                    break;
                // LFSR is a cycle, just skip the ones out of range
            } while (pos != init_pos);
        }

        return {loc.m, static_cast<uint16_t>(loc.x + pos % w), static_cast<uint16_t>(loc.y + pos / w)};
    }

    default:
        fprintf(stderr, "Unknown area type %d\n", static_cast<int>(ty));
        abort();
    }
}

static bool fun_random_location(val_t& result, val_t args[])
{
    RESULT_LOCATION = ARG_AREA(0)->random_location();
    return 0;
}

static bool fun_script_int(val_t& result, val_t args[])
{
    if (ARG_ENTITY(0)->type != BL_PC)
        return 1;

    RESULT_INT = pc_readglobalreg(ARG_PC(0), ARG_STR(1).c_str());
    return 0;
}

static bool fun_rbox(val_t& result, val_t args[])
{
    location_t loc = ARG_LOCATION(0);
    int radius = ARG_INT(1);

    RESULT_AREA = new area_t({loc.m, static_cast<uint16_t>(loc.x - radius), static_cast<uint16_t>(loc.y - radius)},
                             radius * 2 + 1, radius * 2 + 1);

    return 0;
}

static bool fun_running_status_update(val_t& result, val_t args[])
{
    if (ARG_ENTITY(0)->type != BL_PC && ARG_ENTITY(0)->type != BL_MOB)
        return 1;

    RESULT_INT = battle_get_sc_data(ARG_ENTITY(0))[ARG_INT(1)].timer != NULL;
    return 0;
}

static bool fun_status_option(val_t& result, val_t args[])
{
    RESULT_INT = (ARG_PC(0)->status.option & ARG_INT(1)) != 0;
    return 0;
}

static bool fun_element(val_t& result, val_t args[])
{
    RESULT_INT = battle_get_element(ARG_ENTITY(0)) % 10;
    return 0;
}

static bool fun_element_level(val_t& result, val_t args[])
{
    RESULT_INT = battle_get_element(ARG_ENTITY(0)) / 10;
    return 0;
}

static bool fun_spell_index(val_t& result, val_t args[])
{
    RESULT_INT = ARG_SPELL(0)->idx;
    return 0;
}

static bool fun_is_exterior(val_t& result, val_t args[])
{
    // ouch
    RESULT_INT = maps[ARG_LOCATION(0).m].name[4] == '1';
    return 0;
}

static bool fun_contains_string(val_t& result, val_t args[])
{
    RESULT_INT = NULL != strstr(ARG_STR(0).c_str(), ARG_STR(1).c_str());
    return 0;
}

static bool fun_strstr(val_t& result, val_t args[])
{
    const char *offset = strstr(ARG_STR(0).c_str(), ARG_STR(1).c_str());
    // this may fail, but subtraction won't kill anybody
    RESULT_INT = offset - ARG_STR(0).c_str();
    return offset == NULL;
}

static bool fun_strlen(val_t& result, val_t args[])
{
    RESULT_INT = ARG_STR(0).size();
    return 0;
}

static bool fun_substr(val_t& result, val_t args[])
{
    const char *src = ARG_STR(0).c_str();
    const int slen = ARG_STR(0).size();
    int offset = ARG_INT(1);
    int len = ARG_INT(2);

    if (len < 0)
        len = 0;
    if (offset < 0)
        offset = 0;

    if (offset > slen)
        offset = slen;

    if (offset + len > slen)
        len = slen - offset;

    RESULT_STR = NULL;
    RESULT_STR.assign(src + offset, len);

    return 0;
}

static bool fun_sqrt(val_t& result, val_t args[])
{
    RESULT_INT = sqrt(ARG_INT(0));
    return 0;
}

static bool fun_map_level(val_t& result, val_t args[])
{
    // ouch
    RESULT_INT = maps[ARG_LOCATION(0).m].name[4] - '0';
    return 0;
}

static bool fun_map_nr(val_t& result, val_t args[])
{
    const fixed_string<16>& mapname = maps[ARG_LOCATION(0).m].name;

    // really ouch
    RESULT_INT = ((mapname[0] - '0') * 100)
        + ((mapname[1] - '0') * 10) + ((mapname[2] - '0'));
    return 0;
}

static bool fun_dir_towards(val_t& result, val_t args[])
{
    if (ARG_LOCATION(0).m != ARG_LOCATION(1).m)
        return 1;

    int dx = ARG_LOCATION(1).x - ARG_LOCATION(0).x;
    int dy = ARG_LOCATION(1).y - ARG_LOCATION(0).y;

    if (ARG_INT(1))
    {
        /* 8-direction mode */
        if (abs(dx) > abs(dy) * 2)
            RESULT_DIR = dx < 0 ? Direction::W : Direction::E;
        else if (abs(dy) > abs(dx) * 2)
            RESULT_DIR = dy > 0 ? Direction::S : Direction::N;
        else if (dx < 0)
            RESULT_DIR = dy < 0 ? Direction::NW : Direction::SW;
        else
            RESULT_DIR = dy < 0 ? Direction::NE : Direction::SE;
    }
    else
    {
        /* 4-direction mode */
        if (abs(dx) > abs(dy))
            RESULT_DIR = dx < 0 ? Direction::W : Direction::E;
        else
            RESULT_DIR = dy > 0 ? Direction::S : Direction::N;
    }

    return 0;
}

static bool fun_extract_healer_experience(val_t& result, val_t args[])
{
    MapSessionData *sd = (ARG_ENTITY(0)->type == BL_PC) ? ARG_PC(0) : NULL;

    if (!sd)
        RESULT_INT = 0;
    else
        RESULT_INT = pc_extract_healer_exp(sd, ARG_INT(1));
    return 0;
}

#define FUNC_SIG(name, args, ret) {#name, {args, ret, fun_##name}}
#define BATTLE_SIG(name) FUNC_SIG(name, "e", 'i')

const static std::map<std::string, fun_t> functions =
{
    FUNC_SIG(add, "..", '.'),
    FUNC_SIG(sub, "ii", 'i'),
    FUNC_SIG(mul, "ii", 'i'),
    FUNC_SIG(div, "ii", 'i'),
    FUNC_SIG(mod, "ii", 'i'),
    FUNC_SIG(or, "ii", 'i'),
    FUNC_SIG(and, "ii", 'i'),
    FUNC_SIG(gt, "..", 'i'),
    FUNC_SIG(gte, "..", 'i'),
    FUNC_SIG(eq, "..", 'i'),
    FUNC_SIG(bitor, "..", 'i'),
    FUNC_SIG(bitand, "ii", 'i'),
    FUNC_SIG(bitxor, "ii", 'i'),
    FUNC_SIG(bitshl, "ii", 'i'),
    FUNC_SIG(bitshr, "ii", 'i'),
    FUNC_SIG(not, "i", 'i'),
    FUNC_SIG(neg, "i", 'i'),
    FUNC_SIG(max, "ii", 'i'),
    FUNC_SIG(min, "ii", 'i'),
    FUNC_SIG(is_in, "la", 'i'),
    FUNC_SIG(if_then_else, "i__", '_'),
    FUNC_SIG(skill, "ei", 'i'),
    BATTLE_SIG(str),
    BATTLE_SIG(agi),
    BATTLE_SIG(vit),
    BATTLE_SIG(dex),
    BATTLE_SIG(luk),
    BATTLE_SIG(int),
    BATTLE_SIG(level),
    BATTLE_SIG(mdef),
    BATTLE_SIG(def),
    BATTLE_SIG(hp),
    BATTLE_SIG(max_hp),
    BATTLE_SIG(sp),
    BATTLE_SIG(max_sp),
    FUNC_SIG(dir, "e", 'd'),
    FUNC_SIG(name_of, ".", 's'),
    FUNC_SIG(mob_id, "e", 'i'),
    FUNC_SIG(location, "e", 'l'),
    FUNC_SIG(random, "i", 'i'),
    FUNC_SIG(random_dir, "i", 'd'),
    FUNC_SIG(hash_entity, "e", 'i'),
    FUNC_SIG(is_married, "e", 'i'),
    FUNC_SIG(partner, "e", 'e'),
    FUNC_SIG(awayfrom, "ldi", 'l'),
    FUNC_SIG(failed, "_", 'i'),
    FUNC_SIG(pc, "s", 'e'),
    FUNC_SIG(npc, "s", 'e'),
    FUNC_SIG(distance, "ll", 'i'),
    FUNC_SIG(rdistance, "ll", 'i'),
    FUNC_SIG(anchor, "s", 'a'),
    FUNC_SIG(random_location, "a", 'l'),
    FUNC_SIG(script_int, "es", 'i'),
    FUNC_SIG(rbox, "li", 'a'),
    FUNC_SIG(count_item, "e.", 'i'),
    FUNC_SIG(line_of_sight, "ll", 'i'),
    FUNC_SIG(running_status_update, "ei", 'i'),
    FUNC_SIG(status_option, "ei", 'i'),
    FUNC_SIG(element, "e", 'i'),
    FUNC_SIG(element_level, "e", 'i'),
    FUNC_SIG(has_shroud, "e", 'i'),
    FUNC_SIG(is_equipped, "e.", 'i'),
    FUNC_SIG(spell_index, "S", 'i'),
    FUNC_SIG(is_exterior, "l", 'i'),
    FUNC_SIG(contains_string, "ss", 'i'),
    FUNC_SIG(strstr, "ss", 'i'),
    FUNC_SIG(strlen, "s", 'i'),
    FUNC_SIG(substr, "sii", 's'),
    FUNC_SIG(sqrt, "i", 'i'),
    FUNC_SIG(map_level, "l", 'i'),
    FUNC_SIG(map_nr, "l", 'i'),
    FUNC_SIG(dir_towards, "lli", 'd'),
    FUNC_SIG(is_dead, "e", 'i'),
    FUNC_SIG(is_pc, "e", 'i'),
    FUNC_SIG(extract_healer_experience, "ei", 'i'),
};

const std::pair<const std::string, fun_t> *magic_get_fun(const char *name)
{
    auto it = functions.find(name);
    if (it != functions.end())
        // It's a const map so iterators will remain valid
        return &*it;

    return NULL;
}

static bool eval_location(env_t *env, location_t *dest, e_location_t *expr)
{
    val_t m = env->magic_eval(expr->m);
    val_t x = env->magic_eval(expr->x);
    val_t y = env->magic_eval(expr->y);

    if (m.ty != TY::STRING || x.ty != TY::INT || y.ty != TY::INT)
    {
        magic_clear_var(&m);
        magic_clear_var(&x);
        magic_clear_var(&y);
        return 1;
    }

    fixed_string<16> mapname;
    mapname.copy_from(m.v_string.c_str());
    int map_id = map_mapname2mapid(mapname);
    magic_clear_var(&m);
    if (map_id < 0)
        return 1;
    dest->m = map_id;
    dest->x = x.v_int;
    dest->y = y.v_int;
    return 0;
}

static area_t *eval_area(env_t *env, e_area_t *expr)
{
    switch (expr->ty)
    {
    case AreaType::LOCATION:
    {
        location_t loc;
        if (eval_location(env, &loc, &expr->a_loc))
            return NULL;
        return new area_t(loc);
    }
    case AreaType::UNION:
    {
        area_t *au0 = eval_area(env, expr->a_union[0]);
        if (!au0)
            return NULL;

        area_t *au1 = eval_area(env, expr->a_union[1]);
        if (!au1)
        {
            delete au0;
            return NULL;
        }

        return new area_t(au0, au1);
    }

    case AreaType::RECT:
    {
        val_t width = env->magic_eval(expr->a_rect.width);
        val_t height = env->magic_eval(expr->a_rect.height);
        int w = width.ty == TY::INT ? width.v_int : 0;
        int h = height.ty == TY::INT ? height.v_int : 0;
        magic_clear_var(&width);
        magic_clear_var(&height);
        if (!w || !h)
            return NULL;

        location_t loc;
        if (eval_location(env, &loc, &expr->a_rect.loc))
            return NULL;
        return new area_t(loc, w, h);
    }

    default:
        fprintf(stderr, "INTERNAL ERROR: Unknown area type %d\n",
                static_cast<int>(expr->ty));
        abort();
    }
}

static TY type_key(char ty_key)
{
    switch (ty_key)
    {
    case 'i': return TY::INT;
    case 'd': return TY::DIR;
    case 's': return TY::STRING;
    case 'e': return TY::ENTITY;
    case 'l': return TY::LOCATION;
    case 'a': return TY::AREA;
    case 'S': return TY::SPELL;
    case 'I': return TY::INVOCATION;
    default:  return TY::FAIL;
    }
}

int magic_signature_check(const char *opname, const char *funname, const char *signature,
                          int args_nr, val_t args[], int line, int column)
{
    for (int i = 0; i < args_nr; i++)
    {
        val_t *arg = &args[i];
        char ty_key = signature[i];
        TY ty = arg->ty;
        TY desired_ty = type_key(ty_key);

        if (ty == TY::ENTITY)
        {
            /* Dereference entities in preparation for calling function */
            arg->v_entity = map_id2bl(arg->v_int);
            if (!arg->v_entity)
                ty = arg->ty = TY::FAIL;
        }
        else if (ty == TY::INVOCATION)
        {
            arg->v_invocation = static_cast<invocation_t *>(map_id2bl(arg->v_int));
            if (!arg->v_entity)
                ty = arg->ty = TY::FAIL;
        }

        if (!ty_key)
        {
            fprintf(stderr,
                    "[magic-eval]:  L%d:%d: Too many arguments (%d) to %s `%s'\n",
                    line, column, args_nr, opname, funname);
            return 1;
        }

        if (ty == TY::FAIL && ty_key != '_')
            return 1;           /* Fail `in a sane way':  This is a perfectly permissible error */

        if (ty == desired_ty || desired_ty == TY::FAIL /* `dontcare' */ )
            continue;

        if (ty == TY::UNDEF)
        {
            fprintf(stderr,
                    "[magic-eval]:  L%d:%d: Argument #%d to %s `%s' undefined\n",
                    line, column, i + 1, opname, funname);
            return 1;
        }

        /* If we are here, we have a type mismatch but no failure _yet_.  Try to coerce. */
        switch (desired_ty)
        {
        case TY::INT:
            intify(arg);
            break;          /* 100% success rate */
        case TY::STRING:
            stringify(arg, 1);
            break;          /* 100% success rate */
        case TY::AREA:
            make_area(arg);
            break;          /* Only works for locations */
        case TY::LOCATION:
            make_location(arg);
            break;          /* Only works for some areas */
        case TY::SPELL:
            make_spell(arg);
            break;          /* Only works for still-active invocatoins */
        default:
            break;          /* We'll fail right below */
        }

        ty = arg->ty;
        if (ty != desired_ty)
        {                       /* Coercion failed? */
            if (ty != TY::FAIL)
                fprintf(stderr,
                        "[magic-eval]:  L%d:%d: Argument #%d to %s `%s' of incorrect type (%d)\n",
                        line, column, i + 1, opname, funname, static_cast<int>(ty));
            return 1;
        }
    }

    return 0;
}

val_t env_t::magic_eval(expr_t *expr)
{
    val_t out;
    val_t *dest = &out;
    switch (expr->ty)
    {
    case ExprType::VAL:
        magic_copy_var(dest, &expr->e_val);
        break;

    case ExprType::LOCATION:
        if (eval_location(this, &dest->v_location, &expr->e_location))
            dest->ty = TY::FAIL;
        else
            dest->ty = TY::LOCATION;
        break;

    case ExprType::AREA:
        if ((dest->v_area = eval_area(this, &expr->e_area)))
            dest->ty = TY::AREA;
        else
            dest->ty = TY::FAIL;
        break;

    case ExprType::FUNAPP:
    {
        val_t arguments[MAX_ARGS];
        int args_nr = expr->e_funapp.args_nr;
        const std::pair<const std::string, fun_t> *f = expr->e_funapp.fun;

        for (int i = 0; i < args_nr; ++i)
            arguments[i] = magic_eval(expr->e_funapp.args[i]);
        if (magic_signature_check("function", f->first.c_str(), f->second.signature, args_nr, arguments,
                expr->e_funapp.line_nr, expr->e_funapp.column)
            || f->second.fun(*dest, arguments))
            dest->ty = TY::FAIL;
        else
        {
            TY dest_ty = type_key(f->second.ret_ty);
            if (dest_ty != TY::FAIL)
                dest->ty = dest_ty;

            /* translate entity back into persistent int */
            // is this really necessary?
            if (dest->ty == TY::ENTITY)
            {
                if (dest->v_entity)
                    dest->v_int = dest->v_entity->id;
                else
                    dest->ty = TY::FAIL;
            }
        }

        for (int i = 0; i < args_nr; ++i)
            magic_clear_var(&arguments[i]);
        break;
    }

    case ExprType::ID:
    {
        val_t v = VAR(expr->e_id);
        magic_copy_var(dest, &v);
        break;
    }

    case ExprType::SPELLFIELD:
    {
        int id = expr->e_field.id;
        val_t v = magic_eval(expr->e_field.expr);

        if (v.ty == TY::INVOCATION)
        {
            invocation_t *t = static_cast<invocation_t *>(map_id2bl(v.v_int));

            if (!t)
                dest->ty = TY::UNDEF;
            else
                magic_copy_var(dest, &t->env->VAR(id));
        }
        else
        {
            fprintf(stderr,
                    "[magic] Attempt to access field %s on non-spell\n",
                    magic_conf::vars[id].first.c_str());
            dest->ty = TY::FAIL;
        }
        break;
    }

    default:
        fprintf(stderr, "[magic] INTERNAL ERROR: Unknown expression type %d\n",
                static_cast<int>(expr->ty));
        abort();
    }
    return out;
}

int magic_eval_int(env_t *env, expr_t *expr)
{
    val_t result = env->magic_eval(expr);

    if (result.ty == TY::FAIL || result.ty == TY::UNDEF)
        return 0;

    intify(&result);

    return result.v_int;
}

POD_string magic_eval_str(env_t *env, expr_t *expr)
{
    val_t result = env->magic_eval(expr);

    if (result.ty == TY::FAIL || result.ty == TY::UNDEF)
    {
        POD_string out = NULL;
        out.assign("?");
        return out;
    }

    stringify(&result, 0);

    return result.v_string;
}

expr_t *magic_new_expr(ExprType ty)
{
    expr_t *expr;
    CREATE(expr, expr_t, 1);
    expr->ty = ty;
    return expr;
}
