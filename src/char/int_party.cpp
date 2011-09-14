#include "int_party.hpp"

#include "../common/db.hpp"
#include "../common/lock.hpp"
#include "../common/socket.hpp"
#include "../common/utils.hpp"

#include "inter.hpp"
#include "char.hpp"

char party_txt[1024] = "save/party.txt";

static struct dbt *party_db;
static party_t party_newid = 100;

static void mapif_party_broken(party_t party_id, uint8_t flag);
static bool party_check_empty(struct party *p);
static void mapif_parse_PartyLeave(int32_t fd, party_t party_id, account_t account_id);

/// Save party
static void inter_party_tofile(FILE *fp, struct party *p)
{
    fprintf(fp, "%u\t%s\t%d,%d\t", p->party_id, p->name, p->exp, p->item);
    for (int32_t i = 0; i < MAX_PARTY; i++)
    {
        struct party_member *m = &p->member[i];
        fprintf(fp, "%d,%d\t%s\t", m->account_id, m->leader, (m->account_id ? m->name : "-"));
    }
    fprintf(fp, "\n");
}

/// Read a party
static bool inter_party_fromstr(const char *str, struct party *p)
{
    memset(p, 0, sizeof(struct party));

    // can't scanf a bool
    int32_t exp, item;
    if (sscanf(str, "%d\t%23[^\t]\t%d,%d\t", &p->party_id, p->name, &exp, &item) != 4)
        return 1;
    p->exp = exp;
    p->item = item;

    // skip those tabs
    for (int32_t j = 0; j < 3 && str; j++)
        str = strchr(str + 1, '\t');

    for (int32_t i = 0; i < MAX_PARTY; i++)
    {
        struct party_member *m = &p->member[i];
        if (!str)
            return 1;

        if (sscanf(str + 1, "%d,%d\t%23[^\t]\t", &m->account_id, &m->leader, m->name) != 3)
            return 1;

        for (int32_t j = 0; j < 2 && str; j++)
            str = strchr(str + 1, '\t');
    }
    return 0;
}

/// Read all parties
bool inter_party_init(void)
{
    party_db = numdb_init();

    FILE *fp = fopen_(party_txt, "r");
    if (!fp)
        return 1;

    int32_t c = 0;
    char line[8192];
    while (fgets(line, sizeof(line), fp))
    {
        party_t i;
        int32_t j = 0;
        if (sscanf(line, "%d\t%%newid%%\n%n", &i, &j) == 1 && j > 0 && party_newid <= i)
        {
            party_newid = i;
            continue;
        }

        struct party *p;
        CREATE(p, struct party, 1);
        if (inter_party_fromstr(line, p) == 0 && p->party_id)
        {
            if (p->party_id >= party_newid)
                party_newid = p->party_id + 1;
            numdb_insert(party_db, static_cast<numdb_key_t>(p->party_id), static_cast<void *>(p));
            party_check_empty(p);
        }
        else
        {
            printf("int_party: broken data [%s] line %d\n", party_txt, c + 1);
            free(p);
        }
        c++;
    }
    fclose_(fp);
    return 0;
}

/// Save a party
static void inter_party_save_sub(db_key_t, db_val_t data, FILE *fp)
{
    inter_party_tofile(fp, static_cast<struct party *>(data.p));
}

/// Save all parties
bool inter_party_save(void)
{
    int32_t lock;
    FILE *fp = lock_fopen(party_txt, &lock);
    if (!fp)
    {
        char_log.error("int_party: can't write [%s] !!! data is lost !!!\n", party_txt);
        return 1;
    }
    numdb_foreach(party_db, inter_party_save_sub, fp);
    lock_fclose(fp, party_txt, &lock);
    return 0;
}

/// Check if a party has the name
static void search_partyname_sub(db_key_t, db_val_t data, const char *str, struct party **dst)
{
    struct party *p = static_cast<struct party *>(data.p);
    if (strcasecmp(p->name, str) == 0)
        *dst = p;
}

/// Find the party with the name
static struct party *search_partyname(const char *str)
{
    struct party *p = NULL;
    numdb_foreach(party_db, search_partyname_sub, str, &p);
    return p;
}

/// Check if party can share xp
// by checking whether the maximum difference between party members
// exceeds party_share_level
static bool party_check_exp_share(struct party *p)
{
    level_t maxlv = 0;
    level_t minlv = 0xff;

    for (int32_t i = 0; i < MAX_PARTY; i++)
    {
        level_t lv = p->member[i].lv;
        if (p->member[i].online)
        {
            if (lv < minlv)
                minlv = lv;
            if (maxlv < lv)
                maxlv = lv;
        }
    }
    return maxlv == 0 || maxlv - minlv <= party_share_level;
}

/// Check if party is empty, and clear it if it is
bool party_check_empty(struct party *p)
{
    for (int32_t i = 0; i < MAX_PARTY; i++)
        if (p->member[i].account_id)
            return 0;
    mapif_party_broken(p->party_id, 0);
    numdb_erase(party_db, static_cast<numdb_key_t>(p->party_id));
    free(p);
    return 1;
}

/// Checks if player is in the party, but only if it isn't the target party
static void party_check_conflict_sub(db_key_t, db_val_t data, party_t party_id, account_t account_id, const char *nick)
{
    struct party *p = static_cast<struct party *>(data.p);

    if (p->party_id == party_id)
        return;

    for (int32_t i = 0; i < MAX_PARTY; i++)
    {
        if (p->member[i].account_id == account_id
            && strcmp(p->member[i].name, nick) == 0)
        {
            printf("int_party: party conflict! account %u in %u and %u\n", account_id,
                    party_id, p->party_id);
            mapif_parse_PartyLeave(-1, p->party_id, account_id);
        }
    }
}

/// Make sure the character isn't already in any party
static void party_check_conflict(party_t party_id, account_t account_id, const char *nick)
{
    numdb_foreach(party_db, party_check_conflict_sub, party_id, account_id, nick);
}



/// inform the map server of whether the party was created
static void mapif_party_created(int32_t fd, account_t account_id, struct party *p)
{
    WFIFOW(fd, 0) = 0x3820;
    WFIFOL(fd, 2) = account_id;
    if (p)
    {
        WFIFOB(fd, 6) = 0;
        WFIFOL(fd, 7) = p->party_id;
        STRZCPY2(sign_cast<char *>(WFIFOP(fd, 11)), p->name);
        printf("int_party: created! %d %s\n", p->party_id, p->name);
    }
    else
    {
        WFIFOB(fd, 6) = 1;
        WFIFOL(fd, 7) = 0;
        strzcpy(sign_cast<char *>(WFIFOP(fd, 11)), "error", 24);
    }
    WFIFOSET(fd, 35);
}

/// found no party info
static void mapif_party_noinfo(int32_t fd, party_t party_id)
{
    WFIFOW(fd, 0) = 0x3821;
    WFIFOW(fd, 2) = 8;
    WFIFOL(fd, 4) = party_id;
    WFIFOSET(fd, 8);
    printf("int_party: info not found %u\n", party_id);
}

/// Return existing party info to given or all map servers
static void mapif_party_info(int32_t fd, struct party *p)
{
    uint8_t buf[4 + sizeof(struct party)];

    WBUFW(buf, 0) = 0x3821;
    memcpy(buf + 4, p, sizeof(struct party));
    WBUFW(buf, 2) = 4 + sizeof(struct party);
    if (fd < 0)
        mapif_sendall(buf, WBUFW(buf, 2));
    else
        mapif_send(fd, buf, WBUFW(buf, 2));
}

/// Inform the map server of whether member got added to the party
static void mapif_party_memberadded(int32_t fd, party_t party_id, account_t account_id, bool failed)
{
    WFIFOW(fd, 0) = 0x3822;
    WFIFOL(fd, 2) = party_id;
    WFIFOL(fd, 6) = account_id;
    WFIFOB(fd, 10) = failed;
    WFIFOSET(fd, 11);
}

/// Inform map server that a party option changed
// flag bits: 0x01 exp, 0x10 item
static void mapif_party_optionchanged(int32_t fd, struct party *p, account_t account_id,
                                uint8_t flag)
{
    uint8_t buf[15];

    WBUFW(buf, 0) = 0x3823;
    WBUFL(buf, 2) = p->party_id;
    WBUFL(buf, 6) = account_id;
    WBUFW(buf, 10) = p->exp;
    WBUFW(buf, 12) = p->item;
    WBUFB(buf, 14) = flag;
    if (flag == 0)
        mapif_sendall(buf, 15);
    else
        mapif_send(fd, buf, 15);
    printf("party %u by %u option %d %d %d\n", p->party_id,
            account_id, p->exp, p->item, flag);
}

/// Inform all map servers that someone left a party
static void mapif_party_left(party_t party_id, account_t account_id, const char *name)
{
    uint8_t buf[34];
    WBUFW(buf, 0) = 0x3824;
    WBUFL(buf, 2) = party_id;
    WBUFL(buf, 6) = account_id;
    strzcpy(sign_cast<char *>(WBUFP(buf, 10)), name, 24);
    mapif_sendall(buf, 34);
    printf("int_party: left party %u, did %u %s\n", party_id, account_id, name);
}

/// Inform all map servers that party member has changed map
static void mapif_party_membermoved(struct party *p, int32_t idx)
{
    uint8_t buf[29];
    WBUFW(buf, 0) = 0x3825;
    WBUFL(buf, 2) = p->party_id;
    WBUFL(buf, 6) = p->member[idx].account_id;
    STRZCPY2(sign_cast<char *>(WBUFP(buf, 10)), p->member[idx].map);
    WBUFB(buf, 26) = p->member[idx].online;
    WBUFW(buf, 27) = p->member[idx].lv;
    mapif_sendall(buf, 29);
}

/// Inform all map servers that a party dissolved
void mapif_party_broken(party_t party_id, uint8_t flag)
{
    uint8_t buf[7];
    WBUFW(buf, 0) = 0x3826;
    WBUFL(buf, 2) = party_id;
    WBUFB(buf, 6) = flag;
    mapif_sendall(buf, 7);
    printf("int_party: broken %u\n", party_id);
}

/// Forward party chat
static void mapif_party_message(party_t party_id, account_t account_id, const char *mes, int32_t len)
{
    uint8_t buf[len + 12];
    WBUFW(buf, 0) = 0x3827;
    WBUFW(buf, 2) = len + 12;
    WBUFL(buf, 4) = party_id;
    WBUFL(buf, 8) = account_id;
    memcpy(WBUFP(buf, 12), mes, len);
    mapif_sendall(buf, len + 12);
}



/// Create a party
static void mapif_parse_CreateParty(int32_t fd, account_t account_id, const char *name,
                              const char *nick, const char *map, level_t lv)
{
    for (int32_t i = 0; i < 24 && name[i]; i++)
    {
        // no control characters
        if (!(name[i] & 0xe0) || name[i] == 0x7f)
        {
            printf("int_party: illegal party name [%s]\n", name);
            mapif_party_created(fd, account_id, NULL);
            return;
        }
    }

    struct party *p = search_partyname(name);
    if (p)
    {
        printf("int_party: same name party exists [%s]\n", name);
        mapif_party_created(fd, account_id, NULL);
        return;
    }
    CREATE(p, struct party, 1);
    p->party_id = party_newid++;
    STRZCPY(p->name, name);
    p->exp = 0;
    p->item = 0;
    p->member[0].account_id = account_id;
    STRZCPY(p->member[0].name, nick);
    STRZCPY(p->member[0].map, map);
    p->member[0].leader = 1;
    p->member[0].online = 1;
    p->member[0].lv = lv;

    numdb_insert(party_db, static_cast<numdb_key_t>(p->party_id), static_cast<void *>(p));

    mapif_party_created(fd, account_id, p);
    mapif_party_info(fd, p);
}

/// Request for party info
static void mapif_parse_PartyInfo(int32_t fd, party_t party_id)
{
    struct party *p = reinterpret_cast<struct party *>(numdb_search(party_db, static_cast<numdb_key_t>(party_id)).p);
    if (p)
        mapif_party_info(fd, p);
    else
        mapif_party_noinfo(fd, party_id);
}

/// Add member to party
static void mapif_parse_PartyAddMember(int32_t fd, party_t party_id, account_t account_id,
                                 const char *nick, const char *map, level_t lv)
{
    struct party *p = reinterpret_cast<struct party *>(numdb_search(party_db, static_cast<numdb_key_t>(party_id)).p);
    if (!p)
    {
        mapif_party_memberadded(fd, party_id, account_id, 1);
        return;
    }

    for (int32_t i = 0; i < MAX_PARTY; i++)
    {
        if (p->member[i].account_id)
            continue;
        bool flag = 0;

        p->member[i].account_id = account_id;
        STRZCPY(p->member[i].name, nick);
        STRZCPY(p->member[i].map, map);
        p->member[i].leader = 0;
        p->member[i].online = 1;
        p->member[i].lv = lv;
        mapif_party_memberadded(fd, party_id, account_id, 0);
        mapif_party_info(-1, p);

        if (p->exp && !party_check_exp_share(p))
        {
            p->exp = 0;
            flag = 0x01;
        }
        if (flag)
            mapif_party_optionchanged(fd, p, 0, 0);
        return;
    }
    // failed
    mapif_party_memberadded(fd, party_id, account_id, 1);
}

/// Request to change a party option
static void mapif_parse_PartyChangeOption(int32_t fd, party_t party_id, account_t account_id,
                                    bool exp, bool item)
{
    struct party *p = reinterpret_cast<struct party *>(numdb_search(party_db, static_cast<numdb_key_t>(party_id)).p);
    if (!p)
        return;

    p->exp = exp;
    uint8_t flag = 0;
    if (exp && !party_check_exp_share(p))
    {
        flag |= 0x01;
        p->exp = 0;
    }

    p->item = item;

    mapif_party_optionchanged(fd, p, account_id, flag);
}

/// Somebody leaves the party
void mapif_parse_PartyLeave(int32_t, party_t party_id, account_t account_id)
{
    struct party *p = reinterpret_cast<struct party *>(numdb_search(party_db, static_cast<numdb_key_t>(party_id)).p);
    if (!p)
        return;
    for (int32_t i = 0; i < MAX_PARTY; i++)
    {
        if (p->member[i].account_id != account_id)
            continue;
        mapif_party_left(party_id, account_id, p->member[i].name);

        memset(&p->member[i], 0, sizeof(struct party_member));
        if (!party_check_empty(p))
            // party not emptied, map servers still need to know
            mapif_party_info(-1, p);
        return;
    }
}

/// A party member changed map (possibly logging off?)
static void mapif_parse_PartyChangeMap(int32_t fd, party_t party_id, account_t account_id,
                                 const char *map, bool online, level_t lv)
{
    struct party *p = reinterpret_cast<struct party *>(numdb_search(party_db, static_cast<numdb_key_t>(party_id)).p);
    if (!p)
        return;

    for (int32_t i = 0; i < MAX_PARTY; i++)
    {
        if (p->member[i].account_id != account_id)
            continue;
        bool flag = 0;

        STRZCPY(p->member[i].map, map);
        p->member[i].online = online;
        p->member[i].lv = lv;
        mapif_party_membermoved(p, i);

        if (p->exp && !party_check_exp_share(p))
        {
            p->exp = 0;
            flag = 1;
        }
        if (flag)
            mapif_party_optionchanged(fd, p, 0, 0);
        return;
    }
}

/// Request to delete a party
static void mapif_parse_BreakParty(int32_t fd, party_t party_id)
{
    struct party *p = reinterpret_cast<struct party *>(numdb_search(party_db, static_cast<numdb_key_t>(party_id)).p);
    if (!p)
        return;
    numdb_erase(party_db, static_cast<numdb_key_t>(party_id));
    mapif_party_broken(fd, party_id);
}

/// Parse party packets for the inter subserver
// the caller has already made length checks and will RFIFOSKIP
// return 1 if we understood it, 0 if we didn't
bool inter_party_parse_frommap(int32_t fd)
{
    switch (RFIFOW(fd, 0))
    {
    case 0x3020:
        mapif_parse_CreateParty(fd, RFIFOL(fd, 2), sign_cast<const char *>(RFIFOP(fd, 6)),
                                sign_cast<const char *>(RFIFOP(fd, 30)),
                                sign_cast<const char *>(RFIFOP(fd, 54)), RFIFOW(fd, 70));
        return 1;
    case 0x3021:
        mapif_parse_PartyInfo(fd, RFIFOL(fd, 2));
        return 1;
    case 0x3022:
        mapif_parse_PartyAddMember(fd, RFIFOL(fd, 2), RFIFOL(fd, 6),
                                   sign_cast<const char *>(RFIFOP(fd, 10)),
                                   sign_cast<const char *>(RFIFOP(fd, 34)),
                                   RFIFOW(fd, 50));
        return 1;
    case 0x3023:
        mapif_parse_PartyChangeOption(fd, RFIFOL(fd, 2), RFIFOL(fd, 6),
                                       RFIFOW(fd, 10), RFIFOW(fd, 12));
        return 1;
    case 0x3024:
        mapif_parse_PartyLeave(fd, RFIFOL(fd, 2), RFIFOL(fd, 6));
        return 1;
    case 0x3025:
        mapif_parse_PartyChangeMap(fd, RFIFOL(fd, 2), RFIFOL(fd, 6),
                                   sign_cast<const char *>(RFIFOP(fd, 10)),
                                   RFIFOB(fd, 26), RFIFOW(fd, 27));
        return 1;
    case 0x3026:
        mapif_parse_BreakParty(fd, RFIFOL(fd, 2));
        return 1;
    case 0x3027:
        mapif_party_message(RFIFOL(fd, 4), RFIFOL(fd, 8),
                            sign_cast<const char *>(RFIFOP(fd, 12)), RFIFOW(fd, 2) - 12);
        return 1;
    case 0x3028:
        party_check_conflict(RFIFOL(fd, 2), RFIFOL(fd, 6), sign_cast<const char *>(RFIFOP(fd, 10)));
        return 1;
    default:
        return 0;
    }
}

/// Subfunction called for character deleted
void inter_party_leave(party_t party_id, account_t account_id)
{
    mapif_parse_PartyLeave(-1, party_id, account_id);
}
