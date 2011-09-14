#include "utils.hpp"

#include <sys/time.h>

#include <cstring>

static const char hex[] = "0123456789abcdef";

void hexdump(Log& log, const uint8_t *data, size_t len)
{
    if (len > 0x10000)
        len = 0x10000;
    //         0         1         2         3         4         5         6
    //         012345678901234567890123456789012345678901234567890123456789
    log.debug("----  ?0 ?1 ?2 ?3  ?4 ?5 ?6 ?7  ?8 ?9 ?A ?B  ?C ?D ?E ?F\n");
    for (uint16_t i = 0; i < len/16; i++, data += 16)
    {
        char buf[56+1];
        buf[0] = hex[(i>>8)%16];
        buf[1] = hex[(i>>4)%16];
        buf[2] = hex[i%16];
        buf[3] = '?';

        for (uint8_t j=0; j<16; j++)
        {
            if (j%4 == 0)
                buf[4 + (j/4) * 13] = ' ';
            buf[5 + j*13/4] = ' ';
            buf[6 + j*13/4] = hex[(data[j]>>4)%16];
            buf[7 + j*13/4] = hex[data[j]%16];
        }
        buf[56] = '\0';
        log.debug("%s", buf);
    }
    if (len%16)
    {
        char buf[56+1];
        buf[0] = hex[((len/16)>>8)%16];
        buf[1] = hex[((len/16)>>4)%16];
        buf[2] = hex[(len/16)%16];
        buf[3] = '?';

        for (uint8_t j=0; j < len%16; j++)
        {
            if (j%4 == 0)
                buf[4 + (j/4) * 13] = ' ';
            buf[5 + j*13/4] = ' ';
            buf[6 + j*13/4] = hex[(data[j]>>4)%16];
            buf[7 + j*13/4] = hex[data[j]%16];
        }
        buf[8 + (len%16 - 1) * 13/4] = '\0';
        log.debug("%s\n", buf);
    }
}

bool strzcpy(char *dst, const char *src, size_t n)
{
    if (!n) abort();
    strncpy(dst, src, n);
    dst[n-1] = '\0';
    return strnlen(src, n) != n;
}

/// Make a string safe by replacing control characters with _
void remove_control_chars(char *str)
{
    for (int32_t i = 0; str[i]; i++)
        if (!(str[i] & 0xE0))
            str[i] = '_';
}

/// Check if there are any control chars
bool has_control_chars(char *str)
{
    for (int32_t i = 0; str[i]; i++)
        if (!(str[i] & 0xE0))
            return true;
    return false;
}

/// Check whether it looks like a valid email
bool e_mail_check(const char *email)
{
    if (email[0] == '.' || email[0] == '@')
        return 0;

    size_t len = strlen(email);
    if (len < 3 || len > 39)
        return 0;

    if (email[len - 1] == '@' || email[len - 1] == '.')
        return 0;

    const char *at = strchr(email, '@');
    if (!at)
        return 0;
    if (at[1] == '.' || at[-1] == '.')
        return 0;
    if (strchr(at + 1, '@'))
        return 0;

    if (strstr(email, ".."))
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
int32_t config_switch(const char *str)
{
    if (strcasecmp(str, "on") == 0 || strcasecmp(str, "yes") == 0
        || strcasecmp(str, "oui") == 0 || strcasecmp(str, "ja") == 0
        || strcasecmp(str, "si") == 0)
        return 1;
    if (strcasecmp(str, "off") == 0 || strcasecmp(str, "no") == 0
        || strcasecmp(str, "non") == 0 || strcasecmp(str, "nein") == 0)
        return 0;

    return atoi(str);
}

#define DATE_FORMAT "%Y-%m-%d %H:%M:%S"
#define DATE_FORMAT_MAX 20
const char *stamp_now(bool millis)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    static char tmpstr[DATE_FORMAT_MAX + 4];
    strftime(tmpstr, DATE_FORMAT_MAX, DATE_FORMAT, gmtime(&tv.tv_sec));
    if (millis)
        sprintf(tmpstr + DATE_FORMAT_MAX - 1, ".%03u", static_cast<uint32_t>(tv.tv_usec / 1000));
    return tmpstr;
}

const char *stamp_time(time_t when, const char *def)
{
    if (def && !when)
        return def;
    static char tmpstr[DATE_FORMAT_MAX];
    strftime(tmpstr, DATE_FORMAT_MAX, DATE_FORMAT, gmtime(&when));
    return tmpstr;
}
