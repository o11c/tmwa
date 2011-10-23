#ifndef MAGIC_STMT_HPP
#define MAGIC_STMT_HPP

#include "magic.structs.hpp"
#include "magic-expr.structs.hpp"

void spell_free_invocation(invocation_t *invocation);

/**
 * Stops all magic bound to the specified character
 *
 */
void magic_stop_completely(MapSessionData *c);

/**
 * Removes the shroud from a character
 *
 * \param character The character to remove the shroud from
 */
void magic_unshroud(MapSessionData *character);

/**
 * Notifies a running spell that a status_change timer triggered by the spell has expired
 *
 * \param invocation The invocation to notify
 * \param bl_id ID of the PC for whom this happened
 * \param type sc_id ID of the status change entry that finished
 */
void spell_effect_report_termination(BlockID invocation, BlockID bl_id, sint32 sc_id);

/**
 * Execute a spell invocation and sets up timers to finish
 */
void spell_execute(invocation_t *invocation);

/**
 * Continue an NPC script embedded in a spell
 */
void spell_execute_script(invocation_t *invocation);

/**
 * Attacks with a magical spell charged to the character
 *
 * Returns 0 if there is no charged spell or the spell is depleted.
 */
bool spell_attack(BlockID caster, BlockID target);

/**
 * Retrieves an operation by name
 * @param name The name to look up
 * @return An operation of that name, or NULL
 */
const std::pair<const std::string, op_t> *magic_get_op(const char *name);

#endif // MAGIC_STMT_HPP