#ifndef INT_PARTY_HPP
#define INT_PARTY_HPP

# include "../common/mmo.hpp"

bool inter_party_init(void);
bool inter_party_save(void);

bool inter_party_parse_frommap(sint32 fd);

void inter_party_leave(party_t party_id, account_t account_id);

extern char party_txt[1024];

#endif // INT_PARTY_HPP
