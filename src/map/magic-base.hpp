#ifndef MAGIC_BASE_HPP
#define MAGIC_BASE_HPP

#include "magic.structs.hpp"

/**
 * Identifies the invocation used to trigger a spell
 *
 * Returns NULL if not found
 */
POD_string magic_find_invocation(POD_string spellname) __attribute__((pure));

/**
 * Identifies the invocation used to denote a teleport location
 *
 * Returns NULL if not found
 */
POD_string magic_find_anchor_invocation(POD_string teleport_location) __attribute__((pure));

/**
 * Adds a component selection to a component holder(which may initially be NULL)
 */
void magic_add_component(component_t *& component_holder, int id, int count);

expr_t *magic_find_anchor(POD_string name) __attribute__((pure));

/**
 * The parameter `param' must have been dynamically allocated; ownership is transferred to the resultant env_t.
 */
env_t *spell_create_env(spell_t *spell,
                        MapSessionData *caster, int spellpower, POD_string param);

effect_set_t *spell_trigger(spell_t *spell, MapSessionData *caster, env_t *env);

invocation_t *spell_instantiate(effect_set_t *effect, env_t *env);

/**
 * Bind a spell to a subject(this is a no-op for `local' spells).
 */
void spell_bind(MapSessionData *subject, invocation_t *invocation);

void spell_unbind(MapSessionData *subject, invocation_t *invocation);

/**
 * Clones a spell to run the at_effect field
 */
invocation_t *spell_clone_effect(invocation_t *source);

spell_t *magic_find_spell(POD_string invocation) __attribute__((pure));

void spell_update_location(invocation_t *invocation);

// The configuration
namespace magic_conf
{
    extern std::vector<std::pair<POD_string, val_t>> vars;

    //extern int obscure_chance;
    extern int min_casttime;

    extern std::map<POD_string, POD_string> spell_names;
    extern std::map<POD_string, spell_t *> spells;

    extern std::map<POD_string, POD_string> anchor_names;
    extern std::map<POD_string, expr_t *> anchors;
};

inline val_t& env_t::VAR(int i)
{
    if (!vars || vars[i].ty == TY::UNDEF)
        return magic_conf::vars[i].second;
    return vars[i];
}

#endif // MAGIC_BASE_HPP
