#include "magic.hpp"

#include "magic.structs.hpp"
#include "magic-stmt.hpp"
#include "magic-base.hpp"
#include "map.hpp"
#include "pc.hpp"

static char *magic_preprocess_message(MapSessionData *character, char *start, char *end)
{
    if (character->state.shroud_active && character->state.shroud_disappears_on_talk)
        magic_unshroud(character);

    if (character->state.shroud_active && character->state.shroud_hides_name_talking)
    {
        size_t len = strlen(end);
        strcpy(start, "? ");
        memmove(start + 2, end, len + 1);
        return start + 4;
    }
    else
        // step past blank
        return end + 2;
}

// Returns a dynamically allocated copy of src, with the first word NUL-terminated
// parameter will be set to the beginning of the second word, or NULL
static char *magic_tokenise(char *src, char *& parameter)
{
    char *retval = strdup(src);
    char *seeker = retval;

    // skip leading blanks
    while (*seeker && *seeker != ' ')
        ++seeker;

    if (!*seeker)
    {
        parameter = NULL;
        return retval;
    }

    // Terminate invocation (first word)
    *seeker = 0;
    ++seeker;

    // seek beginning of second word
    while (*seeker == ' ')
        ++seeker;

    parameter = seeker;

    return retval;
}

int32_t magic_message(MapSessionData *caster, char *spell_, size_t)
{
    if (pc_isdead(caster))
        return 0;

    int32_t power = caster->matk1;
    char *invocation_base = spell_;
    char *source_invocation = 1 + invocation_base + strlen(caster->status.name);

    // Pre-message filter in case some spell alters output
    source_invocation = magic_preprocess_message(caster, invocation_base, source_invocation);

    spell_t *spell;
    POD_string parameter = NULL;
    {
        POD_string spell_invocation;
        char *parm;
        spell_invocation.take_ownership(magic_tokenise(source_invocation, parm));
        parameter.assign(parm);

        spell = magic_find_spell(spell_invocation);
        spell_invocation.free();
    }

    if (!spell)
    {
        parameter.free();
        return 0;
    }

    env_t *env = spell_create_env(spell, caster, power, parameter);
    effect_set_t *effects;

    if ((spell->flags & SpellFlag::NONMAGIC) || (power >= 1))
        effects = spell_trigger(spell, caster, env);
    else
        effects = NULL;

    if (caster->status.option & OPTION_HIDE)
        // No spellcasting while hidden
        return 0;

    MAP_LOG_PC(caster, "CAST %s %s", spell->name.c_str(), effects ? "SUCCESS" : "FAILURE");

    if (!effects)
    {
        delete env;
        return 1;
    }

    invocation_t *invocation = spell_instantiate(effects, env);

    spell_bind(caster, invocation);
    spell_execute(invocation);

    return (spell->flags & SpellFlag::SILENT) ? -1 : 1;
}

void do_init_magic(void)
{
    magic_init(MAGIC_CONFIG_FILE);
}
