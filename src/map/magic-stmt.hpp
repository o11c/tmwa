#ifndef MAGIC_STMT_HPP
#define MAGIC_STMT_HPP

#include "magic.structs.hpp"

void spell_free_invocation(invocation_t *invocation);

/**
 * Stops all magic bound to the specified character
 *
 */
void magic_stop_completely(character_t * c);

/**
 * Removes the shroud from a character
 *
 * \param character The character to remove the shroud from
 */
void magic_unshroud(character_t * character);

/**
 * Notifies a running spell that a status_change timer triggered by the spell has expired
 *
 * \param invocation The invocation to notify
 * \param bl_id ID of the PC for whom this happened
 * \param type sc_id ID of the status change entry that finished
 * \param supplanted Whether the status_change finished normally(0) or was supplanted by a new status_change(1)
 */
void spell_effect_report_termination(int invocation, int bl_id, int sc_id,
                                     int supplanted);

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
int spell_attack(int caster, int target);


#endif // MAGIC_STMT_HPP
