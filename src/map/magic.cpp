#include "magic.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

#include "magic.structs.hpp"
#include "magic-stmt.hpp"
#include "magic-base.hpp"
#include "map.hpp"
#include "pc.hpp"

static char *magic_preprocess_message(MapSessionData *character, char *start,
                                       char *end)
{
    if (character->state.shroud_active
        && character->state.shroud_disappears_on_talk)
        magic_unshroud(character);

    if (character->state.shroud_active
        && character->state.shroud_hides_name_talking)
    {
        int len = strlen(end);
        strcpy(start, "? ");
        memmove(start + 2, end, len + 1);
        return start + 4;
    }
    else
        return end + 2;         /* step past blank */
}

#define ISBLANK(c) ((c) == ' ')

/* Returns a dynamically allocated copy of `src'.
 * `*parameter' may point within that copy or be NULL. */
static char *magic_tokenise(char *src, char **parameter)
{
    char *retval = strdup(src);
    char *seeker = retval;

    while (*seeker && !ISBLANK(*seeker))
        ++seeker;

    if (!*seeker)
        *parameter = NULL;
    else
    {
        *seeker = 0;            /* Terminate invocation */
        ++seeker;

        while (ISBLANK(*seeker))
            ++seeker;

        *parameter = seeker;
    }

    return retval;
}

int magic_message(MapSessionData *caster, char *spell_, size_t)
{
    if (pc_isdead(caster))
        return 0;

    int power = caster->matk1;
    char *invocation_base = spell_;
    char *source_invocation = 1 + invocation_base + strlen(caster->status.name);

    /* Pre-message filter in case some spell alters output */
    source_invocation = magic_preprocess_message(caster, invocation_base, source_invocation);

    spell_t *spell;
    POD_string parameter;
    parameter.init();
    {
        POD_string spell_invocation;
        char *parm;
        spell_invocation.take_ownership(magic_tokenise(source_invocation, &parm));
        parameter.assign(parm);

        spell = magic_find_spell(spell_invocation);
        spell_invocation.free();
    }
    if (spell)
    {
        int near_miss;
        env_t *env = spell_create_env(spell, caster, power, parameter);
        effect_set_t *effects;

        if ((spell->flags & SpellFlag::NONMAGIC) || (power >= 1))
            effects = spell_trigger(spell, caster, env, &near_miss);
        else
            effects = NULL;

        if (caster->status.option & OPTION_HIDE)
            return 0;           // No spellcasting while hidden

        MAP_LOG_PC(caster, "CAST %s %s",
                    spell->name.c_str(), effects ? "SUCCESS" : "FAILURE");

        if (effects)
        {
            invocation_t *invocation = spell_instantiate(effects, env);

            spell_bind(caster, invocation);
            spell_execute(invocation);

            return (spell->flags & SpellFlag::SILENT) ? -1 : 1;
        }
        else
            magic_free_env(env);

        return 1;
    }
    else
        parameter.free();

    return 0;                   /* Not a spell */
}

void do_init_magic(void)
{
    magic_init(MAGIC_CONFIG_FILE);
}
