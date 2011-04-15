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
