#include "magic-expr.hpp"

#include <cmath>
#include <climits>

#include "itemdb.hpp"

#include "../common/mt_rand.hpp"
#include "../common/utils.hpp"

#include "battle.hpp"
#include "magic-base.hpp"
#include "map.hpp"
#include "npc.hpp"
#include "pc.hpp"

#define CHECK_TYPE(v, t) ((v)->ty == t)

#define IS_SOLID(c) ((c) == 1 || (c) == 5)

static int heading_x[8] = { 0, -1, -1, -1, 0, 1, 1, 1 };
static int heading_y[8] = { 1, 1, 0, -1, -1, -1, 0, 1 };

static int magic_location_in_area(int m, int x, int y, area_t *area);

int map_is_solid(int m, int x, int y)
{
    return (IS_SOLID(map_getcell(m, x, y)));
}

#undef IS_SOLID

static void free_area(area_t *area)
{
    if (!area)
        return;

    switch (area->ty)
    {
        case AreaType::UNION:
            free_area(area->a_union[0]);
            free_area(area->a_union[1]);
            break;
        default:
            break;
    }

    free(area);
}

static area_t *dup_area(area_t *area)
{
    area_t *retval;
    CREATE(retval, area_t, 1);
    *retval = *area;

    switch (area->ty)
    {
        case AreaType::UNION:
            retval->a_union[0] = dup_area(retval->a_union[0]);
            retval->a_union[1] = dup_area(retval->a_union[1]);
            break;
        default:
            break;
    }

    return retval;
}

void magic_copy_var(val_t *dest, val_t *src)
{
    *dest = *src;

    switch (dest->ty)
    {
        case TY::STRING:
            dest->v_string = dest->v_string.clone();
            break;
        case TY::AREA:
            dest->v_area = dup_area(dest->v_area);
            break;
        default:
            break;
    }

}

void magic_clear_var(val_t *v)
{
    switch (v->ty)
    {
        case TY::STRING:
            v->v_string.free();
            break;
        case TY::AREA:
            free_area(v->v_area);
            break;
        default:
            break;
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

static void stringify(val_t *v, int within_op)
{
    static const char *dirs[8] =
    {
        "south", "south-west", "west", "north-west",
        "north", "north-east", "east", "south-east"
    };
    POD_string buf;
    buf.init();

    switch (v->ty)
    {
        case TY::UNDEF:
            buf.assign("UNDEF");
            break;

        case TY::INT:
            buf.resize(31);
            sprintf(&buf[0], "%i", v->v_int);
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
            free_area(v->v_area);
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
            return;
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

static area_t *area_new(AreaType ty)
{
    area_t *retval;
    CREATE(retval, area_t, 1);
    retval->ty = ty;
    return retval;
}

static area_t *area_union(area_t *area, area_t *other_area)
{
    area_t *retval = area_new(AreaType::UNION);
    retval->a_union[0] = area;
    retval->a_union[1] = other_area;
    retval->size = area->size + other_area->size;   /* Assume no overlap */
    return retval;
}

/**
 * Turns location into area, leaves other types untouched
 */
static void make_area(val_t *v)
{
    if (v->ty == TY::LOCATION)
    {
        area_t *a;
        CREATE(a, area_t, 1);
        v->ty = TY::AREA;
        a->ty = AreaType::LOCATION;
        a->a_loc = v->v_location;
        v->v_area = a;
    }
}

static void make_location(val_t *v)
{
    if (v->ty == TY::AREA && v->v_area->ty == AreaType::LOCATION)
    {
        location_t location = v->v_area->a_loc;
        free_area(v->v_area);
        v->ty = TY::LOCATION;
        v->v_location = location;
    }
}

static void make_spell(val_t *v)
{
    if (v->ty == TY::INVOCATION)
    {
        invocation_t *invoc = v->v_invocation;    //(invocation_t *) map_id2bl(v->v_int);
        if (!invoc)
            v->ty = TY::FAIL;
        else
        {
            v->ty = TY::SPELL;
            v->v_spell = invoc->spell;
        }
    }
}

static int fun_add(env_t *, int, val_t *result, val_t *args)
{
    if (TY(0) == TY::INT && TY(1) == TY::INT)
    {
        /* Integer addition */
        RESULTINT = ARGINT(0) + ARGINT(1);
        result->ty = TY::INT;
    }
    else if (ARG_MAY_BE_AREA(0) && ARG_MAY_BE_AREA(1))
    {
        /* AreaType union */
        make_area(&args[0]);
        make_area(&args[1]);
        RESULTAREA = area_union(ARGAREA(0), ARGAREA(1));
        ARGAREA(0) = NULL;
        ARGAREA(1) = NULL;
        result->ty = TY::AREA;
    }
    else
    {
        /* Anything else -> string concatenation */
        stringify(&args[0], 1);
        stringify(&args[1], 1);
        /* Yes, we could speed this up. */
        RESULTSTR.init();
        RESULTSTR = ARGSTR(0).clone();
        RESULTSTR.reserve(ARGSTR(0).size() + ARGSTR(1).size());
        RESULTSTR.append(ARGSTR(1));
        result->ty = TY::STRING;
    }
    return 0;
}

static int fun_sub(env_t *, int, val_t *result, val_t *args)
{
    RESULTINT = ARGINT(0) - ARGINT(1);
    return 0;
}

static int fun_mul(env_t *, int, val_t *result, val_t *args)
{
    RESULTINT = ARGINT(0) * ARGINT(1);
    return 0;
}

static int fun_div(env_t *, int, val_t *result, val_t *args)
{
    if (!ARGINT(1))
        return 1;               /* division by zero */
    RESULTINT = ARGINT(0) / ARGINT(1);
    return 0;
}

static int fun_mod(env_t *, int, val_t *result, val_t *args)
{
    if (!ARGINT(1))
        return 1;               /* division by zero */
    RESULTINT = ARGINT(0) % ARGINT(1);
    return 0;
}

static int fun_or(env_t *, int, val_t *result, val_t *args)
{
    RESULTINT = ARGINT(0) || ARGINT(1);
    return 0;
}

static int fun_and(env_t *, int, val_t *result, val_t *args)
{
    RESULTINT = ARGINT(0) && ARGINT(1);
    return 0;
}

static int fun_not(env_t *, int, val_t *result, val_t *args)
{
    RESULTINT = !ARGINT(0);
    return 0;
}

static int fun_neg(env_t *, int, val_t *result, val_t *args)
{
    RESULTINT = ~ARGINT(0);
    return 0;
}

static int fun_gte(env_t *, int, val_t *result, val_t *args)
{
    if (TY(0) == TY::STRING || TY(1) == TY::STRING)
    {
        stringify(&args[0], 1);
        stringify(&args[1], 1);
        RESULTINT = ARGSTR(0) >= ARGSTR(1);
    }
    else
    {
        intify(&args[0]);
        intify(&args[1]);
        RESULTINT = ARGINT(0) >= ARGINT(1);
    }
    return 0;
}

static int fun_gt(env_t *, int, val_t *result, val_t *args)
{
    if (TY(0) == TY::STRING || TY(1) == TY::STRING)
    {
        stringify(&args[0], 1);
        stringify(&args[1], 1);
        RESULTINT = ARGSTR(0) > ARGSTR(1);
    }
    else
    {
        intify(&args[0]);
        intify(&args[1]);
        RESULTINT = ARGINT(0) > ARGINT(1);
    }
    return 0;
}

static int fun_eq(env_t *, int, val_t *result, val_t *args)
{
    if (TY(0) == TY::STRING || TY(1) == TY::STRING)
    {
        stringify(&args[0], 1);
        stringify(&args[1], 1);
        RESULTINT = ARGSTR(0) == ARGSTR(1);
    }
    else if (TY(0) == TY::DIR && TY(1) == TY::DIR)
        RESULTINT = ARGDIR(0) == ARGDIR(1);
    else if (TY(0) == TY::ENTITY && TY(1) == TY::ENTITY)
        RESULTINT = ARGENTITY(0) == ARGENTITY(1);
    else if (TY(0) == TY::LOCATION && TY(1) == TY::LOCATION)
        RESULTINT = (ARGLOCATION(0).x == ARGLOCATION(1).x
                     && ARGLOCATION(0).y == ARGLOCATION(1).y
                     && ARGLOCATION(0).m == ARGLOCATION(1).m);
    else if (TY(0) == TY::AREA && TY(1) == TY::AREA)
        RESULTINT = ARGAREA(0) == ARGAREA(1); /* Probably not that great an idea... */
    else if (TY(0) == TY::SPELL && TY(1) == TY::SPELL)
        RESULTINT = ARGSPELL(0) == ARGSPELL(1);
    else if (TY(0) == TY::INVOCATION && TY(1) == TY::INVOCATION)
        RESULTINT = ARGINVOCATION(0) == ARGINVOCATION(1);
    else
    {
        intify(&args[0]);
        intify(&args[1]);
        RESULTINT = ARGINT(0) == ARGINT(1);
    }
    return 0;
}

static int fun_bitand(env_t *, int, val_t *result, val_t *args)
{
    RESULTINT = ARGINT(0) & ARGINT(1);
    return 0;
}

static int fun_bitor(env_t *, int, val_t *result, val_t *args)
{
    RESULTINT = ARGINT(0) | ARGINT(1);
    return 0;
}

static int fun_bitxor(env_t *, int, val_t *result, val_t *args)
{
    RESULTINT = ARGINT(0) ^ ARGINT(1);
    return 0;
}

static int fun_bitshl(env_t *, int, val_t *result, val_t *args)
{
    RESULTINT = ARGINT(0) << ARGINT(1);
    return 0;
}

static int fun_bitshr(env_t *, int, val_t *result, val_t *args)
{
    RESULTINT = ARGINT(0) >> ARGINT(1);
    return 0;
}

static int fun_max(env_t *, int, val_t *result, val_t *args)
{
    RESULTINT = MAX(ARGINT(0), ARGINT(1));
    return 0;
}

static int fun_min(env_t *, int, val_t *result, val_t *args)
{
    RESULTINT = MIN(ARGINT(0), ARGINT(1));
    return 0;
}

static int fun_if_then_else(env_t *, int, val_t *result, val_t *args)
{
    if (ARGINT(0))
        magic_copy_var(result, &args[1]);
    else
        magic_copy_var(result, &args[2]);
    return 0;
}

void magic_area_rect(int *m, int *x, int *y, int *width, int *height,
                 area_t *area)
{
    switch (area->ty)
    {
        case AreaType::UNION:
            break;

        case AreaType::LOCATION:
            *m = area->a_loc.m;
            *x = area->a_loc.x;
            *y = area->a_loc.y;
            *width = 1;
            *height = 1;
            break;

        case AreaType::RECT:
            *m = area->a_rect.loc.m;
            *x = area->a_rect.loc.x;
            *y = area->a_rect.loc.y;
            *width = area->a_rect.width;
            *height = area->a_rect.height;
            break;

        case AreaType::BAR:
        {
            int tx = area->a_bar.loc.x;
            int ty = area->a_bar.loc.y;
            int twidth = area->a_bar.width;
            int tdepth = area->a_bar.width;
            *m = area->a_bar.loc.m;

            switch (area->a_bar.dir)
            {
                case Direction::S:
                    *x = tx - twidth;
                    *y = ty;
                    *width = twidth * 2 + 1;
                    *height = tdepth;
                    break;

                case Direction::W:
                    *x = tx - tdepth;
                    *y = ty - twidth;
                    *width = tdepth;
                    *height = twidth * 2 + 1;
                    break;

                case Direction::N:
                    *x = tx - twidth;
                    *y = ty - tdepth;
                    *width = twidth * 2 + 1;
                    *height = tdepth;
                    break;

                case Direction::E:
                    *x = tx;
                    *y = ty - twidth;
                    *width = tdepth;
                    *height = twidth * 2 + 1;
                    break;

                default:
                    fprintf(stderr,
                             "Error: Trying to compute area of NE/SE/NW/SW-facing bar");
                    *x = tx;
                    *y = ty;
                    *width = *height = 1;
            }
            break;
        }
    }
}

static int magic_location_in_area(int m, int x, int y, area_t *area)
{
    switch (area->ty)
    {
        case AreaType::UNION:
            return magic_location_in_area(m, x, y, area->a_union[0])
                || magic_location_in_area(m, x, y, area->a_union[1]);
        case AreaType::LOCATION:
        case AreaType::RECT:
        case AreaType::BAR:
        {
            int am;
            int ax, ay, awidth, aheight;
            magic_area_rect(&am, &ax, &ay, &awidth, &aheight, area);
            return (am == m
                    && (x >= ax) && (y >= ay)
                    && (x < ax + awidth) && (y < ay + aheight));
        }
        default:
            fprintf(stderr, "INTERNAL ERROR: Invalid area\n");
            return 0;
    }
}

static int fun_is_in(env_t *, int, val_t *result, val_t *args)
{
    RESULTINT = magic_location_in_area(ARGLOCATION(0).m,
                                        ARGLOCATION(0).x,
                                        ARGLOCATION(0).y, ARGAREA(1));
    return 0;
}

static int fun_skill(env_t *, int, val_t *result, val_t *args)
{
    if (ETY(0) != BL_PC
        || ARGINT(1) < 0
        || ARGINT(1) >= MAX_SKILL
        || ARGPC(0)->status.skill[ARGINT(1)].id != ARGINT(1))
        RESULTINT = 0;
    else
        RESULTINT = ARGPC(0)->status.skill[ARGINT(1)].lv;
    return 0;
}

static int fun_has_shroud(env_t *, int, val_t *result, val_t *args)
{
    RESULTINT = (ETY(0) == BL_PC && ARGPC(0)->state.shroud_active);
    return 0;
}

#define BATTLE_GETTER(name) \
static int fun_get_##name(env_t *, int, val_t *result, val_t *args) \
{ \
    RESULTINT = static_cast<int>(battle_get_##name(ARGENTITY(0))); \
    return 0; \
}

BATTLE_GETTER(str);     // battle_get_str
BATTLE_GETTER(agi);     // battle_get_agi
BATTLE_GETTER(vit);     // battle_get_vit
BATTLE_GETTER(dex);     // battle_get_dex
BATTLE_GETTER(luk);     // battle_get_luk
BATTLE_GETTER(int);     // battle_get_int
BATTLE_GETTER(lv);      // battle_get_lv
BATTLE_GETTER(hp);      // battle_get_hp
BATTLE_GETTER(mdef);    // battle_get_mdef
BATTLE_GETTER(def);     // battle_get_def
BATTLE_GETTER(max_hp);  // battle_get_max_hp
BATTLE_GETTER(dir);     // battle_get_dir

#define MMO_GETTER(name) \
static int fun_get_##name(env_t *, int, val_t *result, val_t *args) \
{ \
    if (ETY(0) == BL_PC) \
        RESULTINT = ARGPC(0)->status.name; \
    else \
        RESULTINT = 0; \
    return 0; \
}

MMO_GETTER(sp);
MMO_GETTER(max_sp);

static int fun_name_of(env_t *, int, val_t *result, val_t *args)
{
    if (TY(0) == TY::ENTITY)
    {
        RESULTSTR.assign(show_entity(ARGENTITY(0)));
        return 0;
    }
    else if (TY(0) == TY::SPELL)
    {
        RESULTSTR = ARGSPELL(0)->name.clone();
        return 0;
    }
    else if (TY(0) == TY::INVOCATION)
    {
        RESULTSTR = ARGINVOCATION(0)->spell->name.clone();
        return 0;
    }
    return 1;
}

/* [Freeyorp] I'm putting this one in as name_of seems to have issues with summoned or spawned mobs. */
static int fun_mob_id(env_t *, int, val_t *result, val_t *args)
{
    if (ETY(0) != BL_MOB) return 1;
    RESULTINT = ARGMOB(0)->mob_class;
    return 0;
}

#define COPY_LOCATION(dest, src) (dest).x = (src).x; (dest).y = (src).y; (dest).m = (src).m;

static int fun_location(env_t *, int, val_t *result, val_t *args)
{
    COPY_LOCATION(RESULTLOCATION, *(ARGENTITY(0)));
    return 0;
}

static int fun_random(env_t *, int, val_t *result, val_t *args)
{
    int delta = ARGINT(0);
    if (delta < 0)
        delta = -delta;
    if (delta == 0)
    {
        RESULTINT = 0;
        return 0;
    }
    RESULTINT = MRAND(delta);

    if (ARGINT(0) < 0)
        RESULTINT = -RESULTINT;
    return 0;
}

static int fun_random_dir(env_t *, int, val_t *result, val_t *args)
{
    if (ARGINT(0))
        RESULTDIR = static_cast<Direction>(MRAND(8));
    else
        RESULTDIR = static_cast<Direction>(MRAND(4) * 2);
    return 0;
}

static int fun_hash_entity(env_t *, int, val_t *result, val_t *args)
{
    RESULTINT = ARGENTITY(0)->id;
    return 0;
}

// ret -1: not a string, ret 1: no such item, ret 0: OK
int magic_find_item(val_t *args, int idx, struct item *item, int *stackable)
{
    struct item_data *item_data;
    int must_add_sequentially;

    if (TY(idx) == TY::INT)
        item_data = itemdb_exists(ARGINT(idx));
    else if (TY(idx) == TY::STRING)
        item_data = itemdb_searchname(ARGSTR(idx).c_str());
    else
        return -1;

    if (!item_data)
        return 1;

    must_add_sequentially = (item_data->type == 4 || item_data->type == 5 || item_data->type == 7 || item_data->type == 8); /* Very elegant. */

    if (stackable)
        *stackable = !must_add_sequentially;

    memset(item, 0, sizeof(struct item));
    item->nameid = item_data->nameid;
    item->identify = 1;

    return 0;
}

static int fun_count_item(env_t *, int, val_t *result, val_t *args)
{
    MapSessionData *chr = (ETY(0) == BL_PC) ? ARGPC(0) : NULL;
    int stackable;
    struct item item;

    GET_ARG_ITEM(1, item, stackable);

    if (!chr)
        return 1;

    RESULTINT = pc_count_all_items(chr, item.nameid);
    return 0;
}

static int fun_is_equipped(env_t *, int, val_t *result, val_t *args)
{
    MapSessionData *chr = (ETY(0) == BL_PC) ? ARGPC(0) : NULL;
    int stackable;
    struct item item;
    int i;
    int retval = 0;

    GET_ARG_ITEM(1, item, stackable);

    if (!chr)
        return 1;

    for (i = 0; i < 11; i++)
        if (chr->equip_index[i] >= 0
            && chr->status.inventory[chr->equip_index[i]].nameid ==
            item.nameid)
        {
            retval = i + 1;
            break;
        }

    RESULTINT = retval;
    return 0;
}

static int fun_is_married(env_t *, int, val_t *result, val_t *args)
{
    RESULTINT = (ETY(0) == BL_PC && ARGPC(0)->status.partner_id);
    return 0;
}

static int fun_is_dead(env_t *, int, val_t *result, val_t *args)
{
    RESULTINT = (ETY(0) == BL_PC && pc_isdead(ARGPC(0)));
    return 0;
}

static int fun_is_pc(env_t *, int, val_t *result, val_t *args)
{
    RESULTINT = (ETY(0) == BL_PC);
    return 0;
}

static int fun_partner(env_t *, int, val_t *result, val_t *args)
{
    if (ETY(0) == BL_PC && ARGPC(0)->status.partner_id)
    {
        RESULTENTITY = map_nick2sd(map_charid2nick(ARGPC(0)->status.partner_id));
        return 0;
    }
    else
        return 1;
}

static int fun_awayfrom(env_t *, int, val_t *result, val_t *args)
{
    location_t *loc = &ARGLOCATION(0);
    int dx = heading_x[static_cast<int>(ARGDIR(1))];
    int dy = heading_y[static_cast<int>(ARGDIR(1))];
    int distance = ARGINT(2);
    while (distance-- && !map_is_solid(loc->m, loc->x + dx, loc->y + dy))
    {
        loc->x += dx;
        loc->y += dy;
    }

    RESULTLOCATION = *loc;
    return 0;
}

static int fun_failed(env_t *, int, val_t *result, val_t *args)
{
    RESULTINT = TY(0) == TY::FAIL;
    return 0;
}

static int fun_npc(env_t *, int, val_t *result, val_t *args)
{
    RESULTENTITY = npc_name2id(ARGSTR(0).c_str());
    return RESULTENTITY == NULL;
}

static int fun_pc(env_t *, int, val_t *result, val_t *args)
{
    RESULTENTITY = map_nick2sd(ARGSTR(0).c_str());
    return RESULTENTITY == NULL;
}

static int fun_distance(env_t *, int, val_t *result, val_t *args)
{
    if (ARGLOCATION(0).m != ARGLOCATION(1).m)
        RESULTINT = INT_MAX;
    else
        RESULTINT = MAX(abs(ARGLOCATION(0).x - ARGLOCATION(1).x),
                         abs(ARGLOCATION(0).y - ARGLOCATION(1).y));
    return 0;
}

static int fun_rdistance(env_t *, int, val_t *result, val_t *args)
{
    if (ARGLOCATION(0).m != ARGLOCATION(1).m)
        RESULTINT = INT_MAX;
    else
    {
        int dx = ARGLOCATION(0).x - ARGLOCATION(1).x;
        int dy = ARGLOCATION(0).y - ARGLOCATION(1).y;
        RESULTINT = sqrt((dx * dx) + (dy * dy));
    }
    return 0;
}

static int fun_anchor(env_t *env, int, val_t *result, val_t *args)
{
    teleport_anchor_t *anchor = magic_find_anchor(ARGSTR(0));

    if (!anchor)
        return 1;

    magic_eval(env, result, anchor->location);

    make_area(result);
    if (result->ty != TY::AREA)
    {
        magic_clear_var(result);
        return 1;
    }

    return 0;
}

static int fun_line_of_sight(env_t *, int, val_t *result, val_t *args)
{
    BlockList e1(BL_NUL), e2(BL_NUL);

    COPY_LOCATION(e1, ARGLOCATION(0));
    COPY_LOCATION(e2, ARGLOCATION(1));

    RESULTINT = battle_check_range(&e1, &e2, 0);

    return 0;
}

void magic_random_location(location_t *dest, area_t *area)
{
    switch (area->ty)
    {
        case AreaType::UNION:
        {
            int rv = MRAND(area->size);
            if (rv < area->a_union[0]->size)
                magic_random_location(dest, area->a_union[0]);
            else
                magic_random_location(dest, area->a_union[1]);
            break;
        }

        case AreaType::LOCATION:
        case AreaType::RECT:
        case AreaType::BAR:
        {
            int m, x, y, w, h;
            magic_area_rect(&m, &x, &y, &w, &h, area);

            if (w <= 1)
                w = 1;

            if (h <= 1)
                h = 1;

            x += MRAND(w);
            y += MRAND(h);

            if (!map_is_solid(m, x, y))
            {
                int start_x = x;
                int start_y = y;
                int i;
                int initial_dir = mt_random() & 0x7;
                int dir = initial_dir;

                /* try all directions, up to a distance to 10, for a free slot */
                do
                {
                    x = start_x;
                    y = start_y;

                    for (i = 0; i < 10 && map_is_solid(m, x, y); i++)
                    {
                        x += heading_x[dir];
                        y += heading_y[dir];
                    }

                    dir = (dir + 1) & 0x7;
                }
                while (map_is_solid(m, x, y) && dir != initial_dir);

            }
            /* We've tried our best.  If the map is still solid, the engine will automatically randomise the target location if we try to warp. */

            dest->m = m;
            dest->x = x;
            dest->y = y;
            break;
        }

        default:
            fprintf(stderr, "Unknown area type %d\n", static_cast<int>(area->ty));
    }
}

static int fun_pick_location(env_t *, int, val_t *result, val_t *args)
{
    magic_random_location(&result->v_location, ARGAREA(0));
    return 0;
}

static int fun_read_script_int(env_t *, int, val_t *result, val_t *args)
{
    BlockList *subject_p = ARGENTITY(0);

    if (subject_p->type != BL_PC)
        return 1;

    RESULTINT = pc_readglobalreg(static_cast<MapSessionData *>(subject_p), ARGSTR(1).c_str());
    return 0;
}

static int fun_rbox(env_t *, int, val_t *result, val_t *args)
{
    location_t loc = ARGLOCATION(0);
    int radius = ARGINT(1);

    RESULTAREA = area_new(AreaType::RECT);
    RESULTAREA->a_rect.loc.m = loc.m;
    RESULTAREA->a_rect.loc.x = loc.x - radius;
    RESULTAREA->a_rect.loc.y = loc.y - radius;
    RESULTAREA->a_rect.width = radius * 2 + 1;
    RESULTAREA->a_rect.height = radius * 2 + 1;

    return 0;
}

static int fun_running_status_update(env_t *, int, val_t *result, val_t *args)
{
    if (ETY(0) != BL_PC && ETY(0) != BL_MOB)
        return 1;

    RESULTINT = battle_get_sc_data(ARGENTITY(0))[ARGINT(1)].timer != NULL;
    return 0;
}

static int fun_status_option(env_t *, int, val_t *result, val_t *args)
{
    RESULTINT = (ARGPC(0)->status.option & ARGINT(1)) != 0;
    return 0;
}

static int fun_element(env_t *, int, val_t *result, val_t *args)
{
    RESULTINT = battle_get_element(ARGENTITY(0)) % 10;
    return 0;
}

static int fun_element_level(env_t *, int, val_t *result, val_t *args)
{
    RESULTINT = battle_get_element(ARGENTITY(0)) / 10;
    return 0;
}

static int fun_index(env_t *, int, val_t *result, val_t *args)
{
    RESULTINT = ARGSPELL(0)->idx;
    return 0;
}

static int fun_is_exterior(env_t *, int, val_t *result, val_t *args)
{
    RESULTINT = maps[ARGLOCATION(0).m].name[4] == '1';
    return 0;
}

static int fun_contains_string(env_t *, int, val_t *result, val_t *args)
{
    RESULTINT = NULL != strstr(ARGSTR(0).c_str(), ARGSTR(1).c_str());
    return 0;
}

static int fun_strstr(env_t *, int, val_t *result, val_t *args)
{
    const char *offset = strstr(ARGSTR(0).c_str(), ARGSTR(1).c_str());
    RESULTINT = offset - ARGSTR(0).c_str();
    return offset == NULL;
}

static int fun_strlen(env_t *, int, val_t *result, val_t *args)
{
    RESULTINT = ARGSTR(0).size();
    return 0;
}

static int fun_substr(env_t *, int, val_t *result, val_t *args)
{
    const char *src = ARGSTR(0).c_str();
    const int slen = ARGSTR(0).size();
    int offset = ARGINT(1);
    int len = ARGINT(2);

    if (len < 0)
        len = 0;
    if (offset < 0)
        offset = 0;

    if (offset > slen)
        offset = slen;

    if (offset + len > slen)
        len = slen - offset;

    RESULTSTR.init();
    RESULTSTR.assign(src + offset, len);

    return 0;
}

static int fun_sqrt(env_t *, int, val_t *result, val_t *args)
{
    RESULTINT = sqrt(ARGINT(0));
    return 0;
}

static int fun_map_level(env_t *, int, val_t *result, val_t *args)
{
    RESULTINT = maps[ARGLOCATION(0).m].name[4] - '0';
    return 0;
}

static int fun_map_nr(env_t *, int, val_t *result, val_t *args)
{
    const fixed_string<16>& mapname = maps[ARGLOCATION(0).m].name;

    RESULTINT = ((mapname[0] - '0') * 100)
        + ((mapname[1] - '0') * 10) + ((mapname[2] - '0'));
    return 0;
}

static int fun_dir_towards(env_t *, int, val_t *result, val_t *args)
{
    int dx;
    int dy;

    if (ARGLOCATION(0).m != ARGLOCATION(1).m)
        return 1;

    dx = ARGLOCATION(1).x - ARGLOCATION(0).x;
    dy = ARGLOCATION(1).y - ARGLOCATION(0).y;

    if (ARGINT(1))
    {
        /* 8-direction mode */
        if (abs(dx) > abs(dy) * 2)
        {                       /* east or west */
            if (dx < 0)
                RESULTINT = 2 /* west */ ;
            else
                RESULTINT = 6 /* east */ ;
        }
        else if (abs(dy) > abs(dx) * 2)
        {                       /* north or south */
            if (dy > 0)
                RESULTINT = 0 /* south */ ;
            else
                RESULTINT = 4 /* north */ ;
        }
        else if (dx < 0)
        {                       /* north-west or south-west */
            if (dy < 0)
                RESULTINT = 3 /* north-west */ ;
            else
                RESULTINT = 1 /* south-west */ ;
        }
        else
        {                       /* north-east or south-east */
            if (dy < 0)
                RESULTINT = 5 /* north-east */ ;
            else
                RESULTINT = 7 /* south-east */ ;
        }
    }
    else
    {
        /* 4-direction mode */
        if (abs(dx) > abs(dy))
        {                       /* east or west */
            if (dx < 0)
                RESULTINT = 2 /* west */ ;
            else
                RESULTINT = 6 /* east */ ;
        }
        else
        {                       /* north or south */
            if (dy > 0)
                RESULTINT = 0 /* south */ ;
            else
                RESULTINT = 4 /* north */ ;
        }
    }

    return 0;
}

static int fun_extract_healer_xp(env_t *, int, val_t *result, val_t *args)
{
    MapSessionData *sd = (ETY(0) == BL_PC) ? ARGPC(0) : NULL;

    if (!sd)
        RESULTINT = 0;
    else
        RESULTINT = pc_extract_healer_exp(sd, ARGINT(1));
    return 0;
}

#define BATTLE_RECORD2(sname, name) { sname, "e", 'i', fun_get_##name }
#define BATTLE_RECORD(name) BATTLE_RECORD2(#name, name)
static fun_t functions[] = {
    {"+", "..", '.', fun_add},
    {"-", "ii", 'i', fun_sub},
    {"*", "ii", 'i', fun_mul},
    {"/", "ii", 'i', fun_div},
    {"%", "ii", 'i', fun_mod},
    {"||", "ii", 'i', fun_or},
    {"&&", "ii", 'i', fun_and},
    {">", "..", 'i', fun_gt},
    {">=", "..", 'i', fun_gte},
    {"=", "..", 'i', fun_eq},
    {"|", "..", 'i', fun_bitor},
    {"&", "ii", 'i', fun_bitand},
    {"^", "ii", 'i', fun_bitxor},
    {"<<", "ii", 'i', fun_bitshl},
    {">>", "ii", 'i', fun_bitshr},
    {"not", "i", 'i', fun_not},
    {"neg", "i", 'i', fun_neg},
    {"max", "ii", 'i', fun_max},
    {"min", "ii", 'i', fun_min},
    {"is_in", "la", 'i', fun_is_in},
    {"if_then_else", "i__", '_', fun_if_then_else},
    {"skill", "ei", 'i', fun_skill},
    BATTLE_RECORD(str),
    BATTLE_RECORD(agi),
    BATTLE_RECORD(vit),
    BATTLE_RECORD(dex),
    BATTLE_RECORD(luk),
    BATTLE_RECORD(int),
    BATTLE_RECORD2("level", lv),
    BATTLE_RECORD(mdef),
    BATTLE_RECORD(def),
    BATTLE_RECORD(hp),
    BATTLE_RECORD(max_hp),
    BATTLE_RECORD(sp),
    BATTLE_RECORD(max_sp),
    {"dir", "e", 'd', fun_get_dir},
    {"name_of", ".", 's', fun_name_of},
    {"mob_id", "e", 'i', fun_mob_id},
    {"location", "e", 'l', fun_location},
    {"random", "i", 'i', fun_random},
    {"random_dir", "i", 'd', fun_random_dir},
    {"hash_entity", "e", 'i', fun_hash_entity},
    {"is_married", "e", 'i', fun_is_married},
    {"partner", "e", 'e', fun_partner},
    {"awayfrom", "ldi", 'l', fun_awayfrom},
    {"failed", "_", 'i', fun_failed},
    {"pc", "s", 'e', fun_pc},
    {"npc", "s", 'e', fun_npc},
    {"distance", "ll", 'i', fun_distance},
    {"rdistance", "ll", 'i', fun_rdistance},
    {"anchor", "s", 'a', fun_anchor},
    {"random_location", "a", 'l', fun_pick_location},
    {"script_int", "es", 'i', fun_read_script_int},
    {"rbox", "li", 'a', fun_rbox},
    {"count_item", "e.", 'i', fun_count_item},
    {"line_of_sight", "ll", 'i', fun_line_of_sight},
    {"running_status_update", "ei", 'i', fun_running_status_update},
    {"status_option", "ei", 'i', fun_status_option},
    {"element", "e", 'i', fun_element},
    {"element_level", "e", 'i', fun_element_level},
    {"has_shroud", "e", 'i', fun_has_shroud},
    {"is_equipped", "e.", 'i', fun_is_equipped},
    {"spell_index", "S", 'i', fun_index},
    {"is_exterior", "l", 'i', fun_is_exterior},
    {"contains_string", "ss", 'i', fun_contains_string},
    {"strstr", "ss", 'i', fun_strstr},
    {"strlen", "s", 'i', fun_strlen},
    {"substr", "sii", 's', fun_substr},
    {"sqrt", "i", 'i', fun_sqrt},
    {"map_level", "l", 'i', fun_map_level},
    {"map_nr", "l", 'i', fun_map_nr},
    {"dir_towards", "lli", 'd', fun_dir_towards},
    {"is_dead", "e", 'i', fun_is_dead},
    {"is_pc", "e", 'i', fun_is_pc},
    {"extract_healer_experience", "ei", 'i', fun_extract_healer_xp},
    {NULL, NULL, '.', NULL}
};

static int functions_are_sorted = 0;

static int compare_fun(const void *lhs, const void *rhs)
{
    return strcmp(static_cast<const fun_t *>(lhs)->name,
                  static_cast<const fun_t *>(rhs)->name);
}

const fun_t *magic_get_fun(const char *name, int *idx)
{
    static int functions_nr;
    fun_t *result;
    fun_t key;

    if (!functions_are_sorted)
    {
        fun_t *it = functions;

        while (it->name)
            ++it;
        functions_nr = it - functions;

        qsort(functions, functions_nr, sizeof(fun_t), compare_fun);
        functions_are_sorted = 1;
    }

    key.name = name;
    result = static_cast<fun_t *>(
            bsearch(&key, functions, functions_nr, sizeof(fun_t), compare_fun));

    if (result && idx)
        *idx = result - functions;

    return result;
}

// 1 on failure
static int eval_location(env_t *env, location_t *dest, e_location_t *expr)
{
    val_t m, x, y;
    magic_eval(env, &m, expr->m);
    magic_eval(env, &x, expr->x);
    magic_eval(env, &y, expr->y);

    if (CHECK_TYPE(&m, TY::STRING)
        && CHECK_TYPE(&x, TY::INT) && CHECK_TYPE(&y, TY::INT))
    {
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
    else
    {
        magic_clear_var(&m);
        magic_clear_var(&x);
        magic_clear_var(&y);
        return 1;
    }
}

static area_t *eval_area(env_t *env, e_area_t *expr)
{
    area_t *area;
    CREATE(area, area_t, 1);
    area->ty = expr->ty;

    switch (expr->ty)
    {
        case AreaType::LOCATION:
            area->size = 1;
            if (eval_location(env, &area->a_loc, &expr->a_loc))
            {
                free(area);
                return NULL;
            }
            else
                return area;

        case AreaType::UNION:
        {
            int i, fail = 0;
            for (i = 0; i < 2; i++)
            {
                area->a_union[i] = eval_area(env, expr->a_union[i]);
                if (!area->a_union[i])
                    fail = 1;
            }

            if (fail)
            {
                for (i = 0; i < 2; i++)
                {
                    if (area->a_union[i])
                        free_area(area->a_union[i]);
                }
                free(area);
                return NULL;
            }
            area->size = area->a_union[0]->size + area->a_union[1]->size;
            return area;
        }

        case AreaType::RECT:
        {
            val_t width, height;
            magic_eval(env, &width, expr->a_rect.width);
            magic_eval(env, &height, expr->a_rect.height);

            area->a_rect.width = width.v_int;
            area->a_rect.height = height.v_int;

            if (CHECK_TYPE(&width, TY::INT)
                && CHECK_TYPE(&height, TY::INT)
                && !eval_location(env, &(area->a_rect.loc),
                                   &expr->a_rect.loc))
            {
                area->size = area->a_rect.width * area->a_rect.height;
                magic_clear_var(&width);
                magic_clear_var(&height);
                return area;
            }
            else
            {
                free(area);
                magic_clear_var(&width);
                magic_clear_var(&height);
                return NULL;
            }
        }

        case AreaType::BAR:
        {
            val_t width, depth, dir;
            magic_eval(env, &width, expr->a_bar.width);
            magic_eval(env, &depth, expr->a_bar.depth);
            magic_eval(env, &dir, expr->a_bar.dir);

            area->a_bar.width = width.v_int;
            area->a_bar.depth = depth.v_int;
            area->a_bar.dir = static_cast<Direction>(dir.v_int);

            if (CHECK_TYPE(&width, TY::INT)
                && CHECK_TYPE(&depth, TY::INT)
                && CHECK_TYPE(&dir, TY::DIR)
                && !eval_location(env, &area->a_bar.loc,
                                   &expr->a_bar.loc))
            {
                area->size =
                    (area->a_bar.width * 2 + 1) * area->a_bar.depth;
                magic_clear_var(&width);
                magic_clear_var(&depth);
                magic_clear_var(&dir);
                return area;
            }
            else
            {
                free(area);
                magic_clear_var(&width);
                magic_clear_var(&depth);
                magic_clear_var(&dir);
                return NULL;
            }
        }

        default:
            fprintf(stderr, "INTERNAL ERROR: Unknown area type %d\n",
                    static_cast<int>(area->ty));
            free(area);
            return NULL;
    }
}

static TY type_key(char ty_key)
{
    switch (ty_key)
    {
        case 'i':
            return TY::INT;
        case 'd':
            return TY::DIR;
        case 's':
            return TY::STRING;
        case 'e':
            return TY::ENTITY;
        case 'l':
            return TY::LOCATION;
        case 'a':
            return TY::AREA;
        case 'S':
            return TY::SPELL;
        case 'I':
            return TY::INVOCATION;
        default:
            return TY::FAIL;
    }
}

int magic_signature_check(const char *opname, const char *funname, const char *signature,
                          int args_nr, val_t *args, int line, int column)
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

void magic_eval(env_t *env, val_t *dest, expr_t *expr)
{
    switch (expr->ty)
    {
        case ExprType::VAL:
            magic_copy_var(dest, &expr->e_val);
            break;

        case ExprType::LOCATION:
            if (eval_location(env, &dest->v_location, &expr->e_location))
                dest->ty = TY::FAIL;
            else
                dest->ty = TY::LOCATION;
            break;

        case ExprType::AREA:
            if ((dest->v_area = eval_area(env, &expr->e_area)))
                dest->ty = TY::AREA;
            else
                dest->ty = TY::FAIL;
            break;

        case ExprType::FUNAPP:
        {
            val_t arguments[MAX_ARGS];
            int args_nr = expr->e_funapp.args_nr;
            int i;
            fun_t *f = functions + expr->e_funapp.id;

            for (i = 0; i < args_nr; ++i)
                magic_eval(env, &arguments[i], expr->e_funapp.args[i]);
            if (magic_signature_check
                ("function", f->name, f->signature, args_nr, arguments,
                 expr->e_funapp.line_nr, expr->e_funapp.column)
                || f->fun(env, args_nr, dest, arguments))
                dest->ty = TY::FAIL;
            else
            {
                TY dest_ty = type_key(f->ret_ty);
                if (dest_ty != TY::FAIL)
                    dest->ty = dest_ty;

                /* translate entity back into persistent int */
                if (dest->ty == TY::ENTITY)
                {
                    if (dest->v_entity)
                        dest->v_int = dest->v_entity->id;
                    else
                        dest->ty = TY::FAIL;
                }
            }

            for (i = 0; i < args_nr; ++i)
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
            val_t v;
            int id = expr->e_field.id;
            magic_eval(env, &v, expr->e_field.expr);

            if (v.ty == TY::INVOCATION)
            {
                invocation_t *t = static_cast<invocation_t *>(map_id2bl(v.v_int));

                if (!t)
                    dest->ty = TY::UNDEF;
                else
                {
// This is why macros should capture nonargument variables
#  define env t->env
                    val_t val2 = VAR(id);
#  undef env
                    magic_copy_var(dest, &val2);
                }
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
            break;
    }
}

int magic_eval_int(env_t *env, expr_t *expr)
{
    val_t result;
    magic_eval(env, &result, expr);

    if (result.ty == TY::FAIL || result.ty == TY::UNDEF)
        return 0;

    intify(&result);

    return result.v_int;
}

POD_string magic_eval_str(env_t *env, expr_t *expr)
{
    val_t result;
    magic_eval(env, &result, expr);

    if (result.ty == TY::FAIL || result.ty == TY::UNDEF)
    {
        POD_string out;
        out.init();
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
