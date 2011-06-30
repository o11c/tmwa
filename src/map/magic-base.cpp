#include "magic-base.hpp"

#include "../common/timer.hpp"
#include "../common/utils.hpp"

#include "map.hpp"
#include "pc.hpp"

#include "magic.hpp"
#include "magic-expr.hpp"

static void set_int(val_t *v, int i)
{
    v->ty = TY::INT;
    v->v_int = i;
}

static void set_string (val_t *v, POD_string x)
{
    v->ty = TY::STRING;
    v->v_string = x;
}

static void set_entity(val_t *v, BlockList *e)
{
    v->ty = TY::ENTITY;
    v->v_int = e->id;
}

static void set_invocation(val_t *v, invocation_t *i)
{
    v->ty = TY::INVOCATION;
    v->v_int = i->id;
}

static void set_spell(val_t *v, spell_t *x)
{
    v->ty = TY::SPELL;
    v->v_spell = x;
}

// Global magic conf
namespace magic_conf
{
    std::vector<std::pair<POD_string, val_t>> vars;

    //int obscure_chance;
    //int min_casttime;

    std::map<POD_string, spell_t *> spells;

    std::map<POD_string, teleport_anchor_t *> anchors;
};

env_t magic_default_env = { NULL };

POD_string magic_find_invocation(POD_string spellname)
{
    for (auto& pair : magic_conf::spells)
        if (pair.second->name == spellname)
            return pair.first;

    return { NULL };
}

spell_t *magic_find_spell(POD_string invocation)
{
    return magic_conf::spells[invocation];
}

/* -------------------------------------------------------------------------------- */
/* Spell anchors */
/* -------------------------------------------------------------------------------- */

POD_string magic_find_anchor_invocation(POD_string anchor_name)
{
    for (auto& pair : magic_conf::anchors)
        if (pair.second->name == anchor_name)
            return pair.first;
    return { NULL };
}

teleport_anchor_t *magic_find_anchor(POD_string name)
{
    return magic_conf::anchors[name];
}

/* -------------------------------------------------------------------------------- */
/* Spell guard checks */
/* -------------------------------------------------------------------------------- */

static env_t *alloc_env()
{
    env_t *env;
    CREATE(env, env_t, 1);
    CREATE(env->vars, val_t, magic_conf::vars.size());
    return env;
}

static env_t *clone_env(env_t *src)
{
    env_t *retval = alloc_env();

    for (int i = 0; i < magic_conf::vars.size(); i++)
        magic_copy_var(&retval->vars[i], &src->vars[i]);

    return retval;
}

void magic_free_env(env_t *env)
{
    for (int i = 0; i < magic_conf::vars.size(); i++)
        magic_clear_var(&env->vars[i]);
    free(env);
}

env_t *spell_create_env(spell_t *spell,
                        MapSessionData *caster, int spellpower, POD_string param)
{
    env_t *env = alloc_env();

    switch (spell->spellarg_ty)
    {

        case SpellArgType::STRING:
            set_string      (&(env->vars[spell->arg]), param);
            break;

        case SpellArgType::PC:
        {
            MapSessionData *subject = map_nick2sd(param.c_str());
            if (!subject)
                subject = caster;
            set_entity(&(env->vars[spell->arg]), subject);
            param.free();
            break;
        }

        case SpellArgType::NONE:
            param.free();
            break;

        default:
            param.free();
            fprintf(stderr, "Unexpected spellarg type %d\n",
                    static_cast<int>(spell->spellarg_ty));
    }

    set_entity(&(env->vars[Var::CASTER]), caster);
    set_int(&(env->vars[Var::SPELLPOWER]), spellpower);
    set_spell(&(env->vars[Var::SPELL]), spell);

    return env;
}

static void free_components(component_t **component_holder)
{
    if (*component_holder == NULL)
        return;
    free_components(&(*component_holder)->next);
    free(*component_holder);
    *component_holder = NULL;
}

void magic_add_component(component_t **component_holder, int id, int count)
{
    if (count <= 0)
        return;

    if (*component_holder == NULL)
    {
        component_t *component;
        CREATE(component, component_t, 1);
        component->next = NULL;
        component->item_id = id;
        component->count = count;
        *component_holder = component;
    }
    else
    {
        component_t *component = *component_holder;
        if (component->item_id == id)
        {
            component->count += count;
            return;
        }
        else
            magic_add_component(&component->next, id, count);
        /* Tail-recurse; gcc can optimise this.  Not that it matters. */
    }
}

static void copy_components(component_t **component_holder, component_t *component)
{
    if (component == NULL)
        return;

    magic_add_component(component_holder, component->item_id,
                         component->count);
    copy_components(component_holder, component->next);
}

typedef struct spellguard_check
{
    component_t *catalysts, *components;
    int mana, casttime;
} spellguard_check_t;

static int check_prerequisites(MapSessionData *caster, component_t *component)
{
    while (component)
    {
        if (pc_count_all_items(caster, component->item_id)
            < component->count)
            return 0;           /* insufficient */

        component = component->next;
    }

    return 1;
}

static void consume_components(MapSessionData *caster, component_t *component)
{
    while (component)
    {
        pc_remove_items(caster, component->item_id, component->count);
        component = component->next;
    }
}

static int spellguard_can_satisfy(spellguard_check_t *check, MapSessionData *caster,
                        env_t *env, int *near_miss)
{
    unsigned int tick = gettick();

    int retval = check_prerequisites(caster, check->catalysts);

/*
        fprintf(stderr, "MC(%d/%s)? %d%d%d%d (%u <= %u)\n",
                caster->id, caster->status.name,
                retval,
                caster->cast_tick <= tick,
                check->mana <= caster->status.sp,
                check_prerequisites(caster, check->components),
                caster->cast_tick, tick);
*/

    if (retval && near_miss)
        *near_miss = 1;         // close enough!

    retval = retval && caster->cast_tick <= tick    /* Hasn't cast a spell too recently */
        && check->mana <= caster->status.sp
        && check_prerequisites(caster, check->components);

    if (retval)
    {
        unsigned int casttime = check->casttime;

        if (VAR(Var::MIN_CASTTIME).ty == TY::INT)
            casttime = MAX(casttime, VAR(Var::MIN_CASTTIME).v_int);

        caster->cast_tick = tick + casttime;    /* Make sure not to cast too frequently */

        consume_components(caster, check->components);
        pc_heal(caster, 0, -check->mana);
    }

    return retval;
}

static effect_set_t *spellguard_check_sub(spellguard_check_t *check,
                                           spellguard_t *guard,
                                           MapSessionData *caster, env_t *env,
                                           int *near_miss)
{
    if (guard == NULL)
        return NULL;

    switch (guard->ty)
    {
        case SpellGuardType::CONDITION:
            if (!magic_eval_int(env, guard->s_condition))
                return NULL;
            break;

        case SpellGuardType::COMPONENTS:
            copy_components(&check->components, guard->s_components);
            break;

        case SpellGuardType::CATALYSTS:
            copy_components(&check->catalysts, guard->s_catalysts);
            break;

        case SpellGuardType::CHOICE:
        {
            spellguard_check_t altcheck = *check;
            effect_set_t *retval;

            altcheck.components = NULL;
            altcheck.catalysts = NULL;

            copy_components(&altcheck.catalysts, check->catalysts);
            copy_components(&altcheck.components, check->components);

            retval =
                spellguard_check_sub(&altcheck, guard->next, caster, env,
                                      near_miss);
            free_components(&altcheck.catalysts);
            free_components(&altcheck.components);
            if (retval)
                return retval;
            else
                return spellguard_check_sub(check, guard->s_alt, caster,
                                             env, near_miss);
        }

        case SpellGuardType::MANA:
            check->mana += magic_eval_int(env, guard->s_mana);
            break;

        case SpellGuardType::CASTTIME:
            check->casttime += magic_eval_int(env, guard->s_mana);
            break;

        case SpellGuardType::EFFECT:
            if (spellguard_can_satisfy(check, caster, env, near_miss))
                return &guard->s_effect;
            else
                return NULL;

        default:
            fprintf(stderr, "Unexpected spellguard type %d\n", static_cast<int>(guard->ty));
            return NULL;
    }

    return spellguard_check_sub(check, guard->next, caster, env, near_miss);
}

static effect_set_t *check_spellguard(spellguard_t *guard,
                                       MapSessionData *caster, env_t *env,
                                       int *near_miss)
{
    spellguard_check_t check;
    effect_set_t *retval;
    check.catalysts = NULL;
    check.components = NULL;
    check.mana = check.casttime = 0;

    retval = spellguard_check_sub(&check, guard, caster, env, near_miss);

    free_components(&check.catalysts);
    free_components(&check.components);

    return retval;
}

/* -------------------------------------------------------------------------------- */
/* Public API */
/* -------------------------------------------------------------------------------- */

effect_set_t *spell_trigger(spell_t *spell, MapSessionData *caster,
                             env_t *env, int *near_miss)
{
    int i;
    spellguard_t *guard = spell->spellguard;

    if (near_miss)
        *near_miss = 0;

    for (i = 0; i < spell->letdefs_nr; i++)
        magic_eval(env,
                    &env->vars[spell->letdefs[i].id], spell->letdefs[i].expr);

    return check_spellguard(guard, caster, env, near_miss);
}

static void spell_set_location(invocation_t *invocation, BlockList *entity)
{
    magic_clear_var(&invocation->env->vars[Var::LOCATION]);
    invocation->env->vars[Var::LOCATION].ty = TY::LOCATION;
    invocation->env->vars[Var::LOCATION].v_location.m = entity->m;
    invocation->env->vars[Var::LOCATION].v_location.x = entity->x;
    invocation->env->vars[Var::LOCATION].v_location.y = entity->y;
}

void spell_update_location(invocation_t *invocation)
{
    if (invocation->spell->flags & SpellFlag::LOCAL)
        return;
    else
    {
        MapSessionData *owner = static_cast<MapSessionData *>(map_id2bl(invocation->subject));
        if (!owner)
            return;

        spell_set_location(invocation, static_cast<BlockList *>(owner));
    }
}

invocation_t *spell_instantiate(effect_set_t *effect_set, env_t *env)
{
    invocation_t *retval = new invocation_t;
    BlockList *caster;

    retval->env = env;

    retval->caster = VAR(Var::CASTER).v_int;
    retval->spell = VAR(Var::SPELL).v_spell;
    retval->stack_size = 0;
    retval->current_effect = effect_set->effect;
    retval->trigger_effect = effect_set->at_trigger;
    retval->end_effect = effect_set->at_end;

    caster = map_id2bl(retval->caster);    // must still exist
    retval->id = map_addobject(retval);
    retval->m = caster->m;
    retval->x = caster->x;
    retval->y = caster->y;

    map_addblock(retval);
    set_invocation  (&(env->vars[Var::INVOCATION]), retval);

    return retval;
}

invocation_t *spell_clone_effect(invocation_t *base)
{
    invocation_t *retval = static_cast<invocation_t *>(malloc(sizeof(invocation_t)));
    env_t *env;

    memcpy(retval, base, sizeof(invocation_t));

    retval->env = clone_env(retval->env);
    env = retval->env;
    retval->current_effect = retval->trigger_effect;
    retval->next_invocation = NULL;
    retval->end_effect = NULL;
    retval->script_pos = 0;
    retval->stack_size = 0;
    retval->timer = 0;
    retval->subject = 0;
    retval->status_change_refs_nr = 0;
    retval->status_change_refs = NULL;
    retval->flags = 0;

    retval->id = 0;
    retval->prev = NULL;
    retval->next = NULL;

    retval->id = map_addobject(retval);
    set_invocation(&(env->vars[Var::INVOCATION]), retval);

    return retval;
}

void spell_bind(MapSessionData *subject, invocation_t *invocation)
{
    /* Only bind nonlocal spells */

    if (!(invocation->spell->flags & SpellFlag::LOCAL))
    {
        if (invocation->flags & InvocationFlag::BOUND
            || invocation->subject || invocation->next_invocation)
        {
            int *i = NULL;
            fprintf(stderr,
                     "[magic] INTERNAL ERROR: Attempt to re-bind spell invocation `%s'\n",
                     invocation->spell->name.c_str());
            *i = 1;
            return;
        }

        invocation->next_invocation = subject->active_spells;
        subject->active_spells = invocation;
        invocation->flags |= InvocationFlag::BOUND;
        invocation->subject = subject->id;
    }

    spell_set_location(invocation, static_cast<BlockList *>(subject));
}

int spell_unbind(MapSessionData *subject, invocation_t *invocation)
{
    invocation_t **seeker = &subject->active_spells;

    while (*seeker)
    {
        if (*seeker == invocation)
        {
            *seeker = invocation->next_invocation;

            invocation->flags &= ~InvocationFlag::BOUND;
            invocation->next_invocation = NULL;
            invocation->subject = 0;

            return 0;
        }
        seeker = &((*seeker)->next_invocation);
    }

    return 1;
}
