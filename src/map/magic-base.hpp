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
void magic_add_component(component_t **component_holder, int id, int count);

teleport_anchor_t *magic_find_anchor(POD_string name);

/**
 * The parameter `param' must have been dynamically allocated; ownership is transferred to the resultant env_t.
 */
env_t *spell_create_env(spell_t *spell,
                        MapSessionData *caster, int spellpower, POD_string param);

void magic_free_env(env_t *env);

/**
 * near_miss is set to nonzero iff the spell only failed due to ephemereal issues(spell delay in effect, out of mana, out of components)
 */
effect_set_t *spell_trigger(spell_t *spell, MapSessionData *caster,
                            env_t *env, int *near_miss);

invocation_t *spell_instantiate(effect_set_t *effect, env_t *env);

/**
 * Bind a spell to a subject(this is a no-op for `local' spells).
 */
void spell_bind(MapSessionData *subject, invocation_t *invocation);

// 1 on failure
int spell_unbind(MapSessionData *subject, invocation_t *invocation);

/**
 * Clones a spell to run the at_effect field
 */
invocation_t *spell_clone_effect(invocation_t *source);

spell_t *magic_find_spell(POD_string invocation);

void spell_update_location(invocation_t *invocation);

#endif // MAGIC_BASE_HPP
