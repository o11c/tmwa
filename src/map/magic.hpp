#ifndef MAGIC_HPP
#define MAGIC_HPP

# include "magic.structs.hpp"

# include "battle.structs.hpp"
# include "main.structs.hpp"

// actually in magic-parser.ypp
void magic_init(const char *conffile);   // must be called after itemdb initialisation

/**
 * Try to cast magic.
 *
 * As an intended side effect, the magic message may be distorted(text only).
 *
 * \param caster Player attempting to cast magic
 * \param spell The prospective incantation
 * \param spell_len Number of characters in the incantation
 * \return 1 or -1 if the input message was magic and was handled by this function, 0 otherwise.  -1 is returned when the
 *         message should not be repeated.
 */
sint32 magic_message(MapSessionData *caster, char *spell, size_t spell_len);

/**
 * Initialise all spells, read config data
 */
void do_init_magic(void);

#endif // MAGIC_HPP
