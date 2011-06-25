#include "inter.hpp"

#include "../common/db.hpp"
#include "../common/lock.hpp"
#include "../common/socket.hpp"
#include "../common/timer.hpp"
#include "../common/utils.hpp"

#include "char.hpp"
#include "int_party.hpp"
#include "int_storage.hpp"

// how long to hold whisper data, awaiting answers from map servers
#define WHISPER_DATA_TTL (60*1000)
// Number of elements of Whisp/page data deletion list
#define WHISPER_DELLIST_MAX 256

char accreg_txt[1024] = "save/accreg.txt";
static struct dbt *accreg_db = NULL;

struct accreg
{
    account_t account_id;
    int reg_num;
    struct global_reg reg[ACCOUNT_REG_NUM];
};

/// Max level difference to share xp in a party
int party_share_level = 10;

/// Lengths of packets sent
int inter_send_packet_length[] =
{
// 0x3800
    -1, -1, 27, -1,
    -1, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
// 0x3810
    -1, 7, 0, 0,
    0, 0, 0, 0,
    -1, 11, 0, 0,
    0, 0, 0, 0,
// 0x3820
    35, -1, 11, 15,
    34, 29, 7, -1,
    0, 0, 0, 0,
    0, 0, 0, 0,
// 0x3830
    10, -1, 15, 0,
    79, 19, 7, -1,
    0, -1, -1, -1,
    14, 67, 186, -1,
// 0x3840
    9, 9, -1, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
// 0x3850
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
// 0x3860
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
// 0x3870
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
// 0x3880
    11, -1, 7, 3,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
};

/// Lengths of the received packets
int inter_recv_packet_length[] =
{
// 0x3000
    -1, -1, 7, -1,
    -1, 6, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
// 0x3010
    6, -1, 0, 0,
    0, 0, 0, 0,
    10, -1, 0, 0,
    0, 0, 0, 0,
// 0x3020
    72, 6, 52, 14,
    10, 29, 6, -1,
    34, 0, 0, 0,
    0, 0, 0, 0,
// 0x3030
    -1, 6, -1, 0,
    55, 19, 6, -1,
    14, -1, -1, -1,
    14, 19, 186, -1,
// 0x3040
    5, 9, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
// 0x3050
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
// 0x3060
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
// 0x3070
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
// 0x3080
    48, 14, -1, 6,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
};

struct WhisperData
{
    int id, fd, count, len;
    tick_t tick;
    // speaker
    char src[24];
    // target
    char dst[24];
    char msg[1024];
};
static struct dbt *whis_db = NULL;

// TODO remove these as soon as the good db iterators are implemented
static int whis_dellist[WHISPER_DELLIST_MAX] __attribute__((deprecated));
static int whis_delnum __attribute__((deprecated));


/// Save variables
static void inter_accreg_tofile(FILE *fp, struct accreg *reg)
{
    if (!reg->reg_num)
        return;

    fprintf(fp, "%u\t", reg->account_id);
    for (int j = 0; j < reg->reg_num; j++)
        fprintf(fp, "%s,%d ", reg->reg[j].str, reg->reg[j].value);
}

/// Load variables
static int inter_accreg_fromstr(const char *p, struct accreg *reg)
{
    int n;
    if (sscanf(p, "%d\t%n", &reg->account_id, &n) != 1 || !reg->account_id)
        return 1;
    p += n;
    int j;
    for (j = 0; j < ACCOUNT_REG_NUM; j++, p += n)
    {
        char buf[128];
        int v;
        if (sscanf(p, "%[^,],%d %n", buf, &v, &n) != 2)
            break;
        STRZCPY(reg->reg[j].str, buf);
        reg->reg[j].value = v;
    }
    reg->reg_num = j;
    return 0;
}

/// Read the account variables
static void inter_accreg_init(void)
{
    accreg_db = numdb_init();

    FILE *fp = fopen_(accreg_txt, "r");
    if (!fp)
        return;

    int c = 0;
    char line[8192];
    while (fgets(line, sizeof(line), fp))
    {
        struct accreg *reg;
        CREATE(reg, struct accreg, 1);
        if (!inter_accreg_fromstr(line, reg) && reg->account_id)
        {
            numdb_insert(accreg_db, static_cast<numdb_key_t>(reg->account_id), static_cast<void *>(reg));
        }
        else
        {
            printf("inter: accreg: broken data [%s] line %d\n", accreg_txt, c);
            free(reg);
        }
        c++;
    }
    fclose_(fp);
}

/// saving the variables of an account
static void inter_accreg_save_sub(db_key_t, db_val_t data, FILE *fp)
{
    struct accreg *reg = static_cast<struct accreg *>(data.p);
    inter_accreg_tofile(fp, reg);
}

/// Save variables of all accounts
static void inter_accreg_save(void)
{
    int lock;
    FILE *fp = lock_fopen(accreg_txt, &lock);

    if (!fp)
    {
        printf("int_accreg: cant write [%s] !!! data is lost !!!\n",
                accreg_txt);
        return;
    }
    numdb_foreach(accreg_db, inter_accreg_save_sub, fp);
    lock_fclose(fp, accreg_txt, &lock);
}

/// Read inter server config file
static void inter_config_read(const char *cfgName)
{
    FILE *fp = fopen_(cfgName, "r");
    if (!fp)
    {
        printf("file not found: %s\n", cfgName);
        return;
    }

    char line[1024];
    while (fgets(line, sizeof(line), fp))
    {
        if (line[0] == '/' && line[1] == '/')
            continue;
        char w1[1024], w2[1024];
        if (sscanf(line, "%[^:]: %[^\r\n]", w1, w2) != 2)
            continue;

        if (strcasecmp(w1, "storage_txt") == 0)
        {
            STRZCPY(storage_txt, w2);
            continue;
        }
        if (strcasecmp(w1, "party_txt") == 0)
        {
            STRZCPY(party_txt, w2);
            continue;
        }
        if (strcasecmp(w1, "accreg_txt") == 0)
        {
            STRZCPY(accreg_txt, w2);
            continue;
        }
        if (strcasecmp(w1, "party_share_level") == 0)
        {
            party_share_level = atoi(w2);
            if (party_share_level < 0)
                party_share_level = 0;
            continue;
        }
        if (strcasecmp(w1, "import") == 0)
        {
            inter_config_read(w2);
            continue;
        }
    }
    fclose_(fp);
}

/// Save everything
void inter_save(void)
{
    inter_party_save();
    inter_storage_save();
    inter_accreg_save();
}

/// Init everything from config file
void inter_init(const char *file)
{
    inter_config_read(file);

    whis_db = numdb_init();

    inter_party_init();
    inter_storage_init();
    inter_accreg_init();
}

/// Send a message to all GMs
// length of mes is actually only len-4 - it includes the header
static void mapif_GMmessage(const char *mes, int len)
{
    uint8_t buf[len];

    WBUFW(buf, 0) = 0x3800;
    WBUFW(buf, 2) = len;
    memcpy(WBUFP(buf, 4), mes, len - 4);
    mapif_sendall(buf, len);
}

extern int server_fd[];
/// Transmit a whisper to all map servers
static void mapif_whis_message(struct WhisperData *wd)
{
    uint8_t buf[56 + wd->len];

    WBUFW(buf, 0) = 0x3801;
    WBUFW(buf, 2) = 56 + wd->len;
    WBUFL(buf, 4) = wd->id;
    STRZCPY2(sign_cast<char *>(WBUFP(buf, 8)), wd->src);
    STRZCPY2(sign_cast<char *>(WBUFP(buf, 32)), wd->dst);
    strzcpy(sign_cast<char *>(WBUFP(buf, 56)), wd->msg, wd->len);
    // This was the only case where the return value of mapif_sendall was used
    mapif_sendall(buf, WBUFW(buf, 2));
    wd->count = 0;
    // This feels hackish but it eliminates the return value check
    for (int i = 0; i < MAX_MAP_SERVERS; i++)
        if (server_fd[i] >= 0)
            wd->count++;
}

/// Transmit the result of a whisper back to the map-server that requested it
static void mapif_whis_end(struct WhisperData *wd, uint8_t flag)
{
    uint8_t buf[27];

    WBUFW(buf, 0) = 0x3802;
    STRZCPY2(sign_cast<char *>(WBUFP(buf, 2)), wd->src);
    // flag: 0: success, 1: target not logged in, 2: ignored
    WBUFB(buf, 26) = flag;
    mapif_send(wd->fd, buf, 27);
}

/// Send all variables
static void mapif_account_reg(int fd)
{
    session[fd]->rfifo_change_packet(0x3804);
    mapif_sendallwos(fd, RFIFOP(fd, 0), RFIFOW(fd, 2));
}

/// Account variable reply
static void mapif_account_reg_reply(int fd, account_t account_id)
{
    struct accreg *reg = reinterpret_cast<struct accreg *>(numdb_search(accreg_db, static_cast<numdb_key_t>(account_id)).p);

    WFIFOW(fd, 0) = 0x3804;
    WFIFOL(fd, 4) = account_id;
    if (!reg)
    {
        WFIFOW(fd, 2) = 8;
    }
    else
    {
        int p = 8;
        for (int j = 0; j < reg->reg_num; j++)
        {
            STRZCPY2(sign_cast<char *>(WFIFOP(fd, p)), reg->reg[j].str);
            p += 32;
            WFIFOL(fd, p) = reg->reg[j].value;
            p += 4;
        }
        WFIFOW(fd, 2) = p;
    }
    WFIFOSET(fd, WFIFOW(fd, 2));
}


/// Check whisper data to time out
static void check_ttl_whisdata_sub(db_key_t, db_val_t data, tick_t tick)
{
    struct WhisperData *wd = static_cast<struct WhisperData *>(data.p);

    if (DIFF_TICK(tick, wd->tick) > WHISPER_DATA_TTL
            && whis_delnum < WHISPER_DELLIST_MAX)
    {
        // TODO We really need to implement "delete while traversing"
        // hm, it we use those new methods I made, instead of the foreach
        // the node pointers of every valid node do not change, only the valid ones
        // so, save the "next" pointer, deleted the node
        // and operate on it - it's that simple
        whis_dellist[whis_delnum++] = wd->id;
    }
}

/// Check all whisper data to time out
static void check_ttl_whisdata(void)
{
    tick_t tick = gettick();
    do
    {
        whis_delnum = 0;
        numdb_foreach(whis_db, check_ttl_whisdata_sub, tick);
        for (int i = 0; i < whis_delnum; i++)
        {
            struct WhisperData *wd = reinterpret_cast<struct WhisperData *>(numdb_search(whis_db, whis_dellist[i]).p);
            printf("inter: whis data id=%d time out : from %s to %s\n",
                    wd->id, wd->src, wd->dst);
            numdb_erase(whis_db, wd->id);
            free(wd);
        }
    }
    // the _sub ran out of space. We're just hoping this doesn't happen too much
    while (whis_delnum == WHISPER_DELLIST_MAX);
}

/// received packets from map-server

// GM messaging
static void mapif_parse_GMmessage(int fd)
{
    mapif_GMmessage(sign_cast<const char *>(RFIFOP(fd, 4)), RFIFOW(fd, 2));
}

/// Send whisper
static void mapif_parse_WhisRequest(int fd)
{
    struct WhisperData *wd;
    static int whisid = 0;

    if (RFIFOW(fd, 2) - 52 >= sizeof(wd->msg))
    {
        printf("inter: Whis message size too long.\n");
        return;
    }

    // search if character exists before to ask all map-servers
    struct mmo_charstatus *character = character_by_name(sign_cast<const char *>(RFIFOP(fd, 28)));
    if (!character)
    {
        uint8_t buf[27];
        WBUFW(buf, 0) = 0x3802;
        memcpy(WBUFP(buf, 2), RFIFOP(fd, 4), 24);
        // flag: 0: success, 1: target not logged in, 2: ignored
        WBUFB(buf, 26) = 1;
        mapif_send(fd, buf, 27);
        return;
    }
    char character_name[24];
    // Character exists. So, ask all map-servers
    // to be sure of the correct name, rewrite it
    STRZCPY2(character_name, character->name);
    // if source is destination, don't ask other servers.
    if (strcmp(sign_cast<const char *>(RFIFOP(fd, 4)), character_name) == 0)
    {
        uint8_t buf[27];
        WBUFW(buf, 0) = 0x3802;
        strzcpy(sign_cast<char *>(WBUFP(buf, 2)), sign_cast<const char *>(RFIFOP(fd, 4)), 24);
        // flag: 0: success, 1: target not logged in, 2: ignored
        WBUFB(buf, 26) = 1;
        mapif_send(fd, buf, 27);
        return;
    }

    // we're really sending it

    /// Timeout old messages
    check_ttl_whisdata();

    CREATE(wd, struct WhisperData, 1);
    wd->id = ++whisid;
    wd->fd = fd;
    wd->len = RFIFOW(fd, 2) - 52;
    STRZCPY(wd->src, sign_cast<const char *>(RFIFOP(fd, 4)));
    STRZCPY(wd->dst, character_name);
    strzcpy(wd->msg, sign_cast<const char *>(RFIFOP(fd, 52)), wd->len);
    wd->tick = gettick();
    numdb_insert(whis_db, wd->id, static_cast<void *>(wd));
    mapif_whis_message(wd);
}

/// Whisper result
// note that we get this once per map server
static void mapif_parse_WhisReply(int fd)
{
    int id = RFIFOL(fd, 2);
    uint8_t flag = RFIFOB(fd, 6);
    struct WhisperData *wd = static_cast<struct WhisperData *>(numdb_search(whis_db, id).p);

    /// timeout or already delivered to another map-server
    if (!wd)
        return;

    // fails if sent to more servers (still awaiting reply)
    // and error is "target is not logged in"
    // this is to prevent spurious "failed to send whisper" messages
    wd->count--;
    if (wd->count <= 0 || flag != 1)
    {
        // flag: 0: success, 1: target not logged in, 2: ignored
        mapif_whis_end(wd, flag);
        numdb_erase(whis_db, id);
        free(wd);
    }
}

/// forward @wgm
// TODO handle immediately on the originating map server and use wos
static void mapif_parse_WhisToGM(int fd)
{
    session[fd]->rfifo_change_packet(0x3803);
    mapif_sendall(RFIFOP(fd, 0), RFIFOW(fd, 2));
}

/// Store account variables
static void mapif_parse_AccReg(int fd)
{
    account_t acc = RFIFOL(fd, 4);
    struct accreg *reg = reinterpret_cast<struct accreg*>(numdb_search(accreg_db, static_cast<numdb_key_t>(acc)).p);

    if (!reg)
    {
        CREATE(reg, struct accreg, 1);
        reg->account_id = acc;
        numdb_insert(accreg_db, static_cast<numdb_key_t>(acc), static_cast<void *>(reg));
    }

    int j;
    int p = 8;
    for (j = 0; j < ACCOUNT_REG_NUM && p < RFIFOW(fd, 2); j++)
    {
        STRZCPY(reg->reg[j].str, sign_cast<const char *>(RFIFOP(fd, p)));
        p += 32;
        reg->reg[j].value = RFIFOL(fd, p);
        p += 4;
    }
    reg->reg_num = j;

    /// let the other map servers know of the change
    mapif_account_reg(fd);
}

/// Account variable reply
static void mapif_parse_AccRegRequest(int fd)
{
    mapif_account_reg_reply(fd, RFIFOL(fd, 2));
}


/// parse_char failed
// try inter server packets instead
// return: 0 unknown, 1 ok, 2 too short
int inter_parse_frommap(int fd)
{
    uint16_t cmd = RFIFOW(fd, 0);

    if (cmd < 0x3000 || cmd >= 0x3000 + ARRAY_SIZEOF(inter_recv_packet_length))
        return 0;
    if (!inter_recv_packet_length[cmd - 0x3000])
        return 0;

    /// return length of packet, looking up variable-length packets, or 0 if not long enough
    int len = inter_check_length(fd, inter_recv_packet_length[cmd - 0x3000]);
    if (len == 0)
        return 2;

    switch (cmd)
    {
    case 0x3000:
        mapif_parse_GMmessage(fd);
        break;
    case 0x3001:
        mapif_parse_WhisRequest(fd);
        break;
    case 0x3002:
        mapif_parse_WhisReply(fd);
        break;
    case 0x3003:
        mapif_parse_WhisToGM(fd);
        break;
    case 0x3004:
        mapif_parse_AccReg(fd);
        break;
    case 0x3005:
        mapif_parse_AccRegRequest(fd);
        break;
    default:
        if (inter_party_parse_frommap(fd))
            break;
        if (inter_storage_parse_frommap(fd))
            break;
        return 0;
    }
    RFIFOSKIP(fd, len);
    return 1;
}

/// Return length if enough bytes remain,
/// return 0 if not enough in the fifo
int inter_check_length(int fd, int length)
{
    if (length == -1)
    {
        if (RFIFOREST(fd) < 4)
            return 0;
        length = RFIFOW(fd, 2);
    }
    if (RFIFOREST(fd) < length)
        return 0;
    return length;
}
