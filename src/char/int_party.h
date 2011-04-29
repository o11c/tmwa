#ifndef INT_PARTY_H
#define INT_PARTY_H
#include "../common/sanity.h"
#include "../common/mmo.h"

bool inter_party_init (void);
bool inter_party_save (void);

bool inter_party_parse_frommap (int fd);

void inter_party_leave (party_t party_id, account_t account_id);

extern char party_txt[1024];
#endif
