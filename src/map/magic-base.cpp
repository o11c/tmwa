#include "magic-base.hpp"

#include "../common/timer.hpp"
#include "../common/utils.hpp"

#include "map.hpp"
#include "pc.hpp"

#include "magic.hpp"
#include "magic-expr.hpp"

template class std::set<invocation_t *>;
template class std::map<POD_string, POD_string>;
template class std::map<POD_string, spell_t *>;
template class std::map<POD_string, area_t *>;
template class std::vector<std::pair<POD_string, val_t>>;

static void set_int(val_t& v, int32_t i)
{
    v.ty = TY::INT;
    v.v_int = i;
}

static void set_string (val_t& v, POD_string x)
{
    v.ty = TY::STRING;
    v.v_string = x;
}

static void set_entity(val_t& v, BlockList *e)
{
    v.ty = TY::ENTITY;
    v.v_int = e->id;
}

static void set_invocation(val_t& v, invocation_t *i)
{
    v.ty = TY::INVOCATION;
    v.v_int = i->id;
}

static void set_spell(val_t& v, spell_t *x)
{
    v.ty = TY::SPELL;
    v.v_spell = x;
}

// Global magic conf
namespace magic_conf
{
    std::vector<std::pair<POD_string, val_t>> vars;

    //int32_t obscure_chance;
    int32_t min_casttime;

    std::map<POD_string, POD_string> spell_names;
    std::map<POD_string, spell_t *> spells;

    std::map<POD_string, POD_string> anchor_names;
    std::map<POD_string, area_t *> anchors;
};

env_t magic_default_env = { NULL };

POD_string magic_find_invocation(POD_string spellname)
{
    auto it = magic_conf::spell_names.find(spellname);
    if (it != magic_conf::spell_names.end())
        return it->second;

    return NULL;
}

spell_t *magic_find_spell(POD_string invocation)
{
    auto it = magic_conf::spells.find(invocation);
    if (it != magic_conf::spells.end())
        return it->second;

    return NULL;
}

POD_string magic_find_anchor_invocation(POD_string anchor_name)
{
    auto it = magic_conf::anchor_names.find(anchor_name);
    if (it != magic_conf::anchor_names.end())
        return it->second;

    return NULL;
}

area_t *magic_find_anchor(POD_string name)
{
    auto it = magic_conf::anchors.find(name);
    if (it != magic_conf::anchors.end())
        return it->second;

    return NULL;
}


env_t::env_t() : vars(new val_t[magic_conf::vars.size()]) {}

env_t::env_t(const env_t& src) : vars(new val_t[magic_conf::vars.size()])
{
    for (int32_t i = 0; i < magic_conf::vars.size(); i++)
        magic_copy_var(&vars[i], &src.vars[i]);
}

env_t::~env_t()
{
    for (int32_t i = 0; i < magic_conf::vars.size(); i++)
        magic_clear_var(&vars[i]);
    delete[] vars;
}

env_t *spell_create_env(spell_t *spell, MapSessionData *caster, int32_t spellpower,
                        POD_string param)
{
    env_t *env = new env_t;

    switch (spell->spellarg_ty)
    {
    case SpellArgType::STRING:
        set_string(env->vars[spell->arg], param);
        break;
    case SpellArgType::PC:
    {
        MapSessionData *subject = map_nick2sd(param.c_str());
        if (!subject)
            subject = caster;
        set_entity(env->vars[spell->arg], subject);
        param.free();
        break;
    }
    case SpellArgType::NONE:
        param.free();
        break;
    default:
        fprintf(stderr, "Unexpected spellarg type %d\n",
                static_cast<int32_t>(spell->spellarg_ty));
        abort();
    }

    set_entity(env->vars[Var::CASTER], caster);
    set_int(env->vars[Var::SPELLPOWER], spellpower);
    set_spell(env->vars[Var::SPELL], spell);

    return env;
}

static void free_components(component_t *& component_holder)
{
    if (component_holder == NULL)
        return;
    free_components(component_holder->next);
    free(component_holder);
    component_holder = NULL;
}

void magic_add_component(component_t *& component_holder, int32_t id, int32_t count)
{
    if (count <= 0)
        return;

    if (component_holder == NULL)
    {
        component_t *component;
        CREATE(component, component_t, 1);
        component->next = NULL;
        component->item_id = id;
        component->count = count;
        component_holder = component;
    }
    else
    {
        component_t *component = component_holder;
        if (component->item_id == id)
        {
            component->count += count;
            return;
        }
        else
            magic_add_component(component->next, id, count);
        /* Tail-recurse; gcc can optimise this.  Not that it matters. */
    }
}

static void copy_components(component_t *& component_holder, component_t *component)
{
    if (component == NULL)
        return;

    magic_add_component(component_holder, component->item_id, component->count);
    copy_components(component_holder, component->next);
}

struct spellguard_check_t
{
    component_t *catalysts, *components;
    int32_t mana, casttime;
};

static bool check_prerequisites(MapSessionData *caster, component_t *component)
{
    for (; component; component = component->next)
    {
        if (component->count > pc_count_all_items(caster, component->item_id))
            return false;
    }

    return true;
}

static void consume_components(MapSessionData *caster, component_t *component)
{
    for (; component; component = component->next)
    {
        pc_remove_items(caster, component->item_id, component->count);
    }
}

static int32_t spellguard_can_satisfy(spellguard_check_t *check, MapSessionData *caster)
{
    tick_t tick = gettick();

    if (caster->cast_tick <= tick && check->mana <= caster->status.sp
        && check_prerequisites(caster, check->catalysts)
        && check_prerequisites(caster, check->components))
    {
        uint32_t casttime = max(check->casttime, magic_conf::min_casttime);

        caster->cast_tick = tick + casttime;

        consume_components(caster, check->components);
        pc_heal(caster, 0/*hp*/, -check->mana);
        return true;
    }

    return false;
}

static effect_set_t *spellguard_check_sub(spellguard_check_t *check,
                                          spellguard_t *guard,
                                          MapSessionData *caster, env_t *env)
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
        copy_components(check->components, guard->s_components);
        break;

    case SpellGuardType::CATALYSTS:
        copy_components(check->catalysts, guard->s_catalysts);
        break;

    case SpellGuardType::CHOICE:
    {
        spellguard_check_t altcheck = *check;
        effect_set_t *retval;

        altcheck.components = NULL;
        altcheck.catalysts = NULL;

        copy_components(altcheck.catalysts, check->catalysts);
        copy_components(altcheck.components, check->components);

        retval = spellguard_check_sub(&altcheck, guard->next, caster, env);
        free_components(altcheck.catalysts);
        free_components(altcheck.components);
        if (retval)
            return retval;
        else
            return spellguard_check_sub(check, guard->s_alt, caster, env);
    }

    case SpellGuardType::MANA:
        check->mana += magic_eval_int(env, guard->s_mana);
        break;

    case SpellGuardType::CASTTIME:
        check->casttime += magic_eval_int(env, guard->s_mana);
        break;

    case SpellGuardType::EFFECT:
        if (spellguard_can_satisfy(check, caster))
            return &guard->s_effect;
        else
            return NULL;

    default:
        fprintf(stderr, "Unexpected spellguard type %d\n", static_cast<int32_t>(guard->ty));
        return NULL;
    }

    return spellguard_check_sub(check, guard->next, caster, env);
}

static effect_set_t *check_spellguard(spellguard_t *guard,
                                      MapSessionData *caster, env_t *env)
{
    spellguard_check_t check;
    check.catalysts = NULL;
    check.components = NULL;
    check.mana = check.casttime = 0;

    effect_set_t *retval = spellguard_check_sub(&check, guard, caster, env);

    free_components(check.catalysts);
    free_components(check.components);

    return retval;
}



effect_set_t *spell_trigger(spell_t *spell, MapSessionData *caster, env_t *env)
{
    spellguard_t *guard = spell->spellguard;

    for (int32_t i = 0; i < spell->letdefs_nr; i++)
        env->vars[spell->letdefs[i].id] = env->magic_eval(spell->letdefs[i].expr);

    return check_spellguard(guard, caster, env);
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

    MapSessionData *owner = map_id2sd(invocation->subject);
    if (!owner)
        return;

    spell_set_location(invocation, owner);
}

invocation_t *spell_instantiate(effect_set_t *effect_set, env_t *env)
{
    invocation_t *retval = new invocation_t;

    retval->env = env;

    retval->caster = env->VAR(Var::CASTER).v_int;
    retval->spell = env->VAR(Var::SPELL).v_spell;
    retval->current_effect = effect_set->effect;
    retval->trigger_effect = effect_set->at_trigger;
    retval->end_effect = effect_set->at_end;

    BlockList *caster = map_id2bl(retval->caster);    // must still exist
    retval->id = map_addobject(retval);
    retval->m = caster->m;
    retval->x = caster->x;
    retval->y = caster->y;

    map_addblock(retval);
    set_invocation  (env->vars[Var::INVOCATION], retval);

    return retval;
}

invocation_t::invocation_t(invocation_t* rhs) : BlockList(*rhs),
    flags(),
    env(new env_t(*rhs->env)),
    spell(rhs->spell),
    caster(rhs->caster),
    subject(0),
    timer(0),
    stack(),
    script_pos(0),
    current_effect(rhs->trigger_effect),
    trigger_effect(rhs->trigger_effect),
    end_effect(NULL),
    status_change_refs()
{
    // unset some parent fields first
    // (alternatively, we could have used the other constructor and set m, x, y)
    id = 0;
    prev = NULL;
    next = NULL;

    id = map_addobject(this);
    set_invocation(env->vars[Var::INVOCATION], this);
}

void spell_bind(MapSessionData *subject, invocation_t *invocation)
{
    /// Only bind nonlocal spells
    if (!(invocation->spell->flags & SpellFlag::LOCAL))
    {
        if (invocation->subject)
        {
            fprintf(stderr,
                    "%s: Attempt to re-bind spell invocation `%s'\n",
                    __func__, invocation->spell->name.c_str());
            abort();
        }

        subject->active_spells.insert(invocation);
        invocation->subject = subject->id;
    }

    spell_set_location(invocation, subject);
}

void spell_unbind(MapSessionData *subject, invocation_t *invocation)
{
    invocation->subject = 0;

    if (!subject->active_spells.erase(invocation))
        abort();
}
