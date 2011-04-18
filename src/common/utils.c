#include "utils.h"
static const char hex[] = "0123456789abcdef";

void hexdump (FILE *fp, uint8_t *data, size_t len)
{
    if (len > 0x10000)
        len = 0x10000;
    fputs ("----  ?0 ?1 ?2 ?3  ?4 ?5 ?6 ?7  ?8 ?9 ?A ?B  ?C ?D ?E ?F\n", fp);
    for (uint16_t i = 0; i < len/16; i++, data += 16)
    {
        fputc (hex[(i>>8)%16], fp);
        fputc (hex[(i>>4)%16], fp);
        fputc (hex[i%16], fp);
        fputc ('?', fp);
        for (uint8_t j=0; j<16; j++)
        {
            if (j%4 == 0)
                fputc (' ', fp);
            fputc (' ', fp);
            fputc (hex[(i>>4)%16], fp);
            fputc (hex[i%16], fp);
        }
    }
    if (len%16)
    {
        fputc (hex[((len/16)>>8)%16], fp);
        fputc (hex[((len/16)>>4)%16], fp);
        fputc (hex[(len/16)%16], fp);
        fputc ('?', fp);
    }
    for (uint8_t j=0; j< len%16; j++)
    {
        if (j%4 == 0)
            fputc (' ', fp);
        fputc (' ', fp);
        fputc (hex[(j>>4)%16], fp);
        fputc (hex[j%16], fp);
    }
}

/// Make a string safe by replacing control characters with _
void remove_control_chars (char *str)
{
    for (int i = 0; str[i]; i++)
        if ((unsigned char)str[i] < 32)
            str[i] = '_';
}

/// Check if there are any control chars
bool has_control_chars (char *str)
{
    for (int i = 0; str[i]; i++)
        if ((unsigned char)str[i] < 32)
            return true;
    return false;
}

/// Check whether it looks like a valid email
bool e_mail_check (const char *email)
{
    if (email[0] == '.' || email[0] == '@')
        return 0;

    size_t len = strlen(email);
    if (len < 3 || len > 39)
        return 0;

    if (email[len - 1] == '@' || email[len - 1] == '.')
        return 0;

    const char *at = strchr (email, '@');
    if (!at)
        return 0;
    if (at[1] == '.' || at[-1] == '.')
        return 0;
    if (strchr (at + 1, '@'))
        return 0;

    if (strstr (email, ".."))
        return 0;

    do
    {
        // Note: this doesn't support quoted local parts - nobody uses them.
        // Note: this doesn't support UTF-8 addresses - it's not allowed yet.
        // Note: this doesn't support user@[ip.add.r.ess].
        if (*email < 0x20 || *email >= 0x7f || strchr(" \",:;<>[\\]", *email))
            return 0;
    } while (*++email);

    return 1;
}


/// Convert string to number
// Parses booleans: on/off and yes/no in english, français, deutsch, español
// Then falls back to atoi (which means non-integers are parsed as 0)
// TODO replace by config_parse_bool and config_parse_int?
int config_switch (const char *str)
{
    if (strcasecmp (str, "on") == 0 || strcasecmp (str, "yes") == 0
        || strcasecmp (str, "oui") == 0 || strcasecmp (str, "ja") == 0
        || strcasecmp (str, "si") == 0)
        return 1;
    if (strcasecmp (str, "off") == 0 || strcasecmp (str, "no") == 0
        || strcasecmp (str, "non") == 0 || strcasecmp (str, "nein") == 0)
        return 0;

    return atoi (str);
}
