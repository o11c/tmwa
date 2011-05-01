#include "intif.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <stdio.h>
#include <stdlib.h>

#include <sys/time.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <signal.h>
#include <fcntl.h>
#include <string.h>

#include "../common/nullpo.h"
#include "../common/socket.h"
#include "../common/timer.h"

#include "battle.h"
#include "chrif.h"
#include "clif.h"
#include "map.h"
#include "party.h"
#include "pc.h"
#include "storage.h"

static const int packet_len_table[] = {
    -1, -1, 27, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    -1, 7, 0, 0, 0, 0, 0, 0, -1, 11, 0, 0, 0, 0, 0, 0,
    35, -1, 11, 15, 34, 29, 7, -1, 0, 0, 0, 0, 0, 0, 0, 0,
    10, -1, 15, 0, 79, 19, 7, -1, 0, -1, -1, -1, 14, 67, 186, -1,
    9, 9, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    11, -1, 7, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

extern int char_fd;             // inter serverのfdはchar_fdを使う
#define inter_fd (char_fd)      // エイリアス

//-----------------------------------------------------------------
// inter serverへの送信

// Message for all GMs on all map servers
int intif_GMmessage (char *mes, int len, int flag)
{
    int  lp = (flag & 0x10) ? 8 : 4;
    WFIFOW (inter_fd, 0) = 0x3000;
    WFIFOW (inter_fd, 2) = lp + len;
    WFIFOL (inter_fd, 4) = 0x65756c62;
    memcpy (WFIFOP (inter_fd, lp), mes, len);
    WFIFOSET (inter_fd, WFIFOW (inter_fd, 2));

    return 0;
}

// The transmission of Wisp/Page to inter-server (player not found on this server)
int intif_wis_message (struct map_session_data *sd, char *nick, char *mes,
                       int mes_len)
{
    nullpo_retr (0, sd);

    WFIFOW (inter_fd, 0) = 0x3001;
    WFIFOW (inter_fd, 2) = mes_len + 52;
    memcpy (WFIFOP (inter_fd, 4), sd->status.name, 24);
    memcpy (WFIFOP (inter_fd, 28), nick, 24);
    memcpy (WFIFOP (inter_fd, 52), mes, mes_len);
    WFIFOSET (inter_fd, WFIFOW (inter_fd, 2));

    if (battle_config.etc_log)
        printf ("intif_wis_message from %s to %s (message: '%s')\n",
                sd->status.name, nick, mes);

    return 0;
}

// The reply of Wisp/page
int intif_wis_replay (int id, int flag)
{
    WFIFOW (inter_fd, 0) = 0x3002;
    WFIFOL (inter_fd, 2) = id;
    WFIFOB (inter_fd, 6) = flag;    // flag: 0: success to send wisper, 1: target character is not loged in?, 2: ignored by target
    WFIFOSET (inter_fd, 7);

    if (battle_config.etc_log)
        printf ("intif_wis_replay: id: %d, flag:%d\n", id, flag);

    return 0;
}

// The transmission of GM only Wisp/Page from server to inter-server
int intif_wis_message_to_gm (char *Wisp_name, int min_gm_level, char *mes,
                             int mes_len)
{
    WFIFOW (inter_fd, 0) = 0x3003;
    WFIFOW (inter_fd, 2) = mes_len + 30;
    memcpy (WFIFOP (inter_fd, 4), Wisp_name, 24);
    WFIFOW (inter_fd, 28) = (short) min_gm_level;
    memcpy (WFIFOP (inter_fd, 30), mes, mes_len);
    WFIFOSET (inter_fd, WFIFOW (inter_fd, 2));

    if (battle_config.etc_log)
        printf
            ("intif_wis_message_to_gm: from: '%s', min level: %d, message: '%s'.\n",
             Wisp_name, min_gm_level, mes);

    return 0;
}

// アカウント変数送信
int intif_saveaccountreg (struct map_session_data *sd)
{
    int  j, p;

    nullpo_retr (0, sd);

    WFIFOW (inter_fd, 0) = 0x3004;
    WFIFOL (inter_fd, 4) = sd->bl.id;
    for (j = 0, p = 8; j < sd->status.account_reg_num; j++, p += 36)
    {
        memcpy (WFIFOP (inter_fd, p), sd->status.account_reg[j].str, 32);
        WFIFOL (inter_fd, p + 32) = sd->status.account_reg[j].value;
    }
    WFIFOW (inter_fd, 2) = p;
    WFIFOSET (inter_fd, p);
    return 0;
}

// アカウント変数要求
int intif_request_accountreg (struct map_session_data *sd)
{
    nullpo_retr (0, sd);

    WFIFOW (inter_fd, 0) = 0x3005;
    WFIFOL (inter_fd, 2) = sd->bl.id;
    WFIFOSET (inter_fd, 6);
    return 0;
}

// 倉庫データ要求
int intif_request_storage (int account_id)
{
    WFIFOW (inter_fd, 0) = 0x3010;
    WFIFOL (inter_fd, 2) = account_id;
    WFIFOSET (inter_fd, 6);
    return 0;
}

// 倉庫データ送信
int intif_send_storage (struct storage *stor)
{
    nullpo_retr (0, stor);
    WFIFOW (inter_fd, 0) = 0x3011;
    WFIFOW (inter_fd, 2) = sizeof (struct storage) + 8;
    WFIFOL (inter_fd, 4) = stor->account_id;
    memcpy (WFIFOP (inter_fd, 8), stor, sizeof (struct storage));
    WFIFOSET (inter_fd, WFIFOW (inter_fd, 2));
    return 0;
}

// パーティ作成要求
int intif_create_party (struct map_session_data *sd, char *name)
{
    nullpo_retr (0, sd);

    WFIFOW (inter_fd, 0) = 0x3020;
    WFIFOL (inter_fd, 2) = sd->status.account_id;
    memcpy (WFIFOP (inter_fd, 6), name, 24);
    memcpy (WFIFOP (inter_fd, 30), sd->status.name, 24);
    memcpy (WFIFOP (inter_fd, 54), map[sd->bl.m].name, 16);
    WFIFOW (inter_fd, 70) = sd->status.base_level;
    WFIFOSET (inter_fd, 72);
//  if(battle_config.etc_log)
//      printf("intif: create party\n");
    return 0;
}

// パーティ情報要求
int intif_request_partyinfo (int party_id)
{
    WFIFOW (inter_fd, 0) = 0x3021;
    WFIFOL (inter_fd, 2) = party_id;
    WFIFOSET (inter_fd, 6);
//  if(battle_config.etc_log)
//      printf("intif: request party info\n");
    return 0;
}

// パーティ追加要求
int intif_party_addmember (int party_id, int account_id)
{
    struct map_session_data *sd;
    sd = map_id2sd (account_id);
//  if(battle_config.etc_log)
//      printf("intif: party add member %d %d\n",party_id,account_id);
    if (sd != NULL)
    {
        WFIFOW (inter_fd, 0) = 0x3022;
        WFIFOL (inter_fd, 2) = party_id;
        WFIFOL (inter_fd, 6) = account_id;
        memcpy (WFIFOP (inter_fd, 10), sd->status.name, 24);
        memcpy (WFIFOP (inter_fd, 34), map[sd->bl.m].name, 16);
        WFIFOW (inter_fd, 50) = sd->status.base_level;
        WFIFOSET (inter_fd, 52);
    }
    return 0;
}

// パーティ設定変更
int intif_party_changeoption (int party_id, int account_id, int exp, int item)
{
    WFIFOW (inter_fd, 0) = 0x3023;
    WFIFOL (inter_fd, 2) = party_id;
    WFIFOL (inter_fd, 6) = account_id;
    WFIFOW (inter_fd, 10) = exp;
    WFIFOW (inter_fd, 12) = item;
    WFIFOSET (inter_fd, 14);
    return 0;
}

// パーティ脱退要求
int intif_party_leave (int party_id, int account_id)
{
//  if(battle_config.etc_log)
//      printf("intif: party leave %d %d\n",party_id,account_id);
    WFIFOW (inter_fd, 0) = 0x3024;
    WFIFOL (inter_fd, 2) = party_id;
    WFIFOL (inter_fd, 6) = account_id;
    WFIFOSET (inter_fd, 10);
    return 0;
}

// パーティ移動要求
int intif_party_changemap (struct map_session_data *sd, int online)
{
    if (sd != NULL)
    {
        WFIFOW (inter_fd, 0) = 0x3025;
        WFIFOL (inter_fd, 2) = sd->status.party_id;
        WFIFOL (inter_fd, 6) = sd->status.account_id;
        memcpy (WFIFOP (inter_fd, 10), map[sd->bl.m].name, 16);
        WFIFOB (inter_fd, 26) = online;
        WFIFOW (inter_fd, 27) = sd->status.base_level;
        WFIFOSET (inter_fd, 29);
    }
//  if(battle_config.etc_log)
//      printf("party: change map\n");
    return 0;
}

// パーティー解散要求
int intif_break_party (int party_id)
{
    WFIFOW (inter_fd, 0) = 0x3026;
    WFIFOL (inter_fd, 2) = party_id;
    WFIFOSET (inter_fd, 6);
    return 0;
}

// パーティ会話送信
int intif_party_message (int party_id, int account_id, char *mes, int len)
{
//  if(battle_config.etc_log)
//      printf("intif_party_message: %s\n",mes);
    WFIFOW (inter_fd, 0) = 0x3027;
    WFIFOW (inter_fd, 2) = len + 12;
    WFIFOL (inter_fd, 4) = party_id;
    WFIFOL (inter_fd, 8) = account_id;
    memcpy (WFIFOP (inter_fd, 12), mes, len);
    WFIFOSET (inter_fd, len + 12);
    return 0;
}

// パーティ競合チェック要求
int intif_party_checkconflict (int party_id, int account_id, char *nick)
{
    WFIFOW (inter_fd, 0) = 0x3028;
    WFIFOL (inter_fd, 2) = party_id;
    WFIFOL (inter_fd, 6) = account_id;
    memcpy (WFIFOP (inter_fd, 10), nick, 24);
    WFIFOSET (inter_fd, 34);
    return 0;
}

//-----------------------------------------------------------------
// Packets receive from inter server

// Wisp/Page reception
int intif_parse_WisMessage (int fd)
{                               // rewritten by [Yor]
    struct map_session_data *sd;
    int  i;
    char *wisp_source;

    if (battle_config.etc_log)
        printf
            ("intif_parse_wismessage: id: %d, from: %s, to: %s, message: '%s'\n",
             RFIFOL (fd, 4), RFIFOP (fd, 8), RFIFOP (fd, 32), RFIFOP (fd,
                                                                      56));
    sd = map_nick2sd (RFIFOP (fd, 32)); // Searching destination player
    if (sd != NULL && strcmp (sd->status.name, RFIFOP (fd, 32)) == 0)
    {                           // exactly same name (inter-server have checked the name before)
        // if player ignore all
        if (sd->ignoreAll == 1)
            intif_wis_replay (RFIFOL (fd, 4), 2);   // flag: 0: success to send wisper, 1: target character is not loged in?, 2: ignored by target
        else
        {
            wisp_source = RFIFOP (fd, 8);   // speed up
            // if player ignore the source character
            for (i = 0; i < (sizeof (sd->ignore) / sizeof (sd->ignore[0]));
                 i++)
                if (strcmp (sd->ignore[i].name, wisp_source) == 0)
                {
                    intif_wis_replay (RFIFOL (fd, 4), 2);   // flag: 0: success to send wisper, 1: target character is not loged in?, 2: ignored by target
                    break;
                }
            // if source player not found in ignore list
            if (i == (sizeof (sd->ignore) / sizeof (sd->ignore[0])))
            {
                clif_wis_message (sd->fd, RFIFOP (fd, 8), RFIFOP (fd, 56),
                                  RFIFOW (fd, 2) - 56);
                intif_wis_replay (RFIFOL (fd, 4), 0);   // flag: 0: success to send wisper, 1: target character is not loged in?, 2: ignored by target
            }
        }
    }
    else
        intif_wis_replay (RFIFOL (fd, 4), 1);   // flag: 0: success to send wisper, 1: target character is not loged in?, 2: ignored by target
    return 0;
}

// Wisp/page transmission result reception
int intif_parse_WisEnd (int fd)
{
    struct map_session_data *sd;

    if (battle_config.etc_log)
        printf ("intif_parse_wisend: player: %s, flag: %d\n", RFIFOP (fd, 2), RFIFOB (fd, 26)); // flag: 0: success to send wisper, 1: target character is not loged in?, 2: ignored by target
    sd = map_nick2sd (RFIFOP (fd, 2));
    if (sd != NULL)
        clif_wis_end (sd->fd, RFIFOB (fd, 26));

    return 0;
}

// Received wisp message from map-server via char-server for ALL gm
int mapif_parse_WisToGM (int fd)
{                               // 0x3003/0x3803 <packet_len>.w <wispname>.24B <min_gm_level>.w <message>.?B
    int  i, min_gm_level, len;
    struct map_session_data *pl_sd;
    char Wisp_name[24];
    char mbuf[255];

    if (RFIFOW (fd, 2) - 30 <= 0)
        return 0;

    len = RFIFOW (fd, 2) - 30;
    char *message = ((len) >= 255) ? (char *) malloc (len) : mbuf;

    min_gm_level = (int) RFIFOW (fd, 28);
    memcpy (Wisp_name, RFIFOP (fd, 4), 24);
    Wisp_name[23] = '\0';
    memcpy (message, RFIFOP (fd, 30), len);
    message[len - 1] = '\0';
    // information is sended to all online GM
    for (i = 0; i < fd_max; i++)
        if (session[i] && (pl_sd = (struct map_session_data *)session[i]->session_data)
            && pl_sd->state.auth)
            if (pc_isGM (pl_sd) >= min_gm_level)
                clif_wis_message (i, Wisp_name, message,
                                  strlen (message) + 1);

    if (message != mbuf)
        free (message);

    return 0;
}

// アカウント変数通知
int intif_parse_AccountReg (int fd)
{
    int  j, p;
    struct map_session_data *sd;

    if ((sd = map_id2sd (RFIFOL (fd, 4))) == NULL)
        return 1;
    for (p = 8, j = 0; p < RFIFOW (fd, 2) && j < ACCOUNT_REG_NUM;
         p += 36, j++)
    {
        memcpy (sd->status.account_reg[j].str, RFIFOP (fd, p), 32);
        sd->status.account_reg[j].value = RFIFOL (fd, p + 32);
    }
    sd->status.account_reg_num = j;
//  printf("intif: accountreg\n");

    return 0;
}

// 倉庫データ受信
int intif_parse_LoadStorage (int fd)
{
    struct storage *stor;
    struct map_session_data *sd;

    sd = map_id2sd (RFIFOL (fd, 4));
    if (sd == NULL)
    {
        if (battle_config.error_log)
            printf ("intif_parse_LoadStorage: user not found %d\n",
                    RFIFOL (fd, 4));
        return 1;
    }
    stor = account2storage (RFIFOL (fd, 4));
    if (stor->storage_status == 1)
    {                           // Already open.. lets ignore this update
        if (battle_config.error_log)
            printf
                ("intif_parse_LoadStorage: storage received for a client already open (User %d:%d)\n",
                 sd->status.account_id, sd->status.char_id);
        return 1;
    }
    if (stor->dirty)
    {                           // Already have storage, and it has been modified and not saved yet! Exploit! [Skotlex]
        if (battle_config.error_log)
            printf
                ("intif_parse_LoadStorage: received storage for an already modified non-saved storage! (User %d:%d)\n",
                 sd->status.account_id, sd->status.char_id);
        return 1;
    }

    if (RFIFOW (fd, 2) - 8 != sizeof (struct storage))
    {
        if (battle_config.error_log)
            printf ("intif_parse_LoadStorage: data size error %d %d\n",
                    RFIFOW (fd, 2) - 8, sizeof (struct storage));
        return 1;
    }
    if (battle_config.save_log)
        printf ("intif_openstorage: %d\n", RFIFOL (fd, 4));
    memcpy (stor, RFIFOP (fd, 8), sizeof (struct storage));
    stor->dirty = 0;
    stor->storage_status = 1;
    sd->state.storage_flag = 1;
    clif_storageitemlist (sd, stor);
    clif_storageequiplist (sd, stor);
    clif_updatestorageamount (sd, stor);

    return 0;
}

// 倉庫データ送信成功
int intif_parse_SaveStorage (int fd)
{
    if (battle_config.save_log)
        printf ("intif_savestorage: done %d %d\n", RFIFOL (fd, 2),
                RFIFOB (fd, 6));
    storage_storage_saved (RFIFOL (fd, 2));
    return 0;
}
// パーティ作成可否
int intif_parse_PartyCreated (int fd)
{
    if (battle_config.etc_log)
        printf ("intif: party created\n");
    party_created (RFIFOL (fd, 2), RFIFOB (fd, 6), RFIFOL (fd, 7),
                   RFIFOP (fd, 11));
    return 0;
}

// パーティ情報
int intif_parse_PartyInfo (int fd)
{
    if (RFIFOW (fd, 2) == 8)
    {
        if (battle_config.error_log)
            printf ("intif: party noinfo %d\n", RFIFOL (fd, 4));
        party_recv_noinfo (RFIFOL (fd, 4));
        return 0;
    }

//  printf("intif: party info %d\n",RFIFOL(fd,4));
    if (RFIFOW (fd, 2) != sizeof (struct party) + 4)
    {
        if (battle_config.error_log)
            printf ("intif: party info : data size error %d %d %d\n",
                    RFIFOL (fd, 4), RFIFOW (fd, 2),
                    sizeof (struct party) + 4);
    }
    party_recv_info ((struct party *) RFIFOP (fd, 4));
    return 0;
}

// パーティ追加通知
int intif_parse_PartyMemberAdded (int fd)
{
    if (battle_config.etc_log)
        printf ("intif: party member added %d %d %d\n", RFIFOL (fd, 2),
                RFIFOL (fd, 6), RFIFOB (fd, 10));
    party_member_added (RFIFOL (fd, 2), RFIFOL (fd, 6), RFIFOB (fd, 10));
    return 0;
}

// パーティ設定変更通知
int intif_parse_PartyOptionChanged (int fd)
{
    party_optionchanged (RFIFOL (fd, 2), RFIFOL (fd, 6), RFIFOW (fd, 10),
                         RFIFOW (fd, 12), RFIFOB (fd, 14));
    return 0;
}

// パーティ脱退通知
int intif_parse_PartyMemberLeaved (int fd)
{
    if (battle_config.etc_log)
        printf ("intif: party member leaved %d %d %s\n", RFIFOL (fd, 2),
                RFIFOL (fd, 6), RFIFOP (fd, 10));
    party_member_leaved (RFIFOL (fd, 2), RFIFOL (fd, 6), RFIFOP (fd, 10));
    return 0;
}

// パーティ解散通知
int intif_parse_PartyBroken (int fd)
{
    party_broken (RFIFOL (fd, 2));
    return 0;
}

// パーティ移動通知
int intif_parse_PartyMove (int fd)
{
//  if(battle_config.etc_log)
//      printf("intif: party move %d %d %s %d %d\n",RFIFOL(fd,2),RFIFOL(fd,6),RFIFOP(fd,10),RFIFOB(fd,26),RFIFOW(fd,27));
    party_recv_movemap (RFIFOL (fd, 2), RFIFOL (fd, 6), RFIFOP (fd, 10),
                        RFIFOB (fd, 26), RFIFOW (fd, 27));
    return 0;
}

// パーティメッセージ
int intif_parse_PartyMessage (int fd)
{
//  if(battle_config.etc_log)
//      printf("intif_parse_PartyMessage: %s\n",RFIFOP(fd,12));
    party_recv_message (RFIFOL (fd, 4), RFIFOL (fd, 8), RFIFOP (fd, 12),
                        RFIFOW (fd, 2) - 12);
    return 0;
}

//-----------------------------------------------------------------
// inter serverからの通信
// エラーがあれば0(false)を返すこと
// パケットが処理できれば1,パケット長が足りなければ2を返すこと
int intif_parse (int fd)
{
    int  packet_len;
    int  cmd = RFIFOW (fd, 0);
    // パケットのID確認
    if (cmd < 0x3800
        || cmd >=
        0x3800 + (sizeof (packet_len_table) / sizeof (packet_len_table[0]))
        || packet_len_table[cmd - 0x3800] == 0)
    {
        return 0;
    }
    // パケットの長さ確認
    packet_len = packet_len_table[cmd - 0x3800];
    if (packet_len == -1)
    {
        if (RFIFOREST (fd) < 4)
            return 2;
        packet_len = RFIFOW (fd, 2);
    }
//  if(battle_config.etc_log)
//      printf("intif_parse %d %x %d %d\n",fd,cmd,packet_len,RFIFOREST(fd));
    if (RFIFOREST (fd) < packet_len)
    {
        return 2;
    }
    // 処理分岐
    switch (cmd)
    {
        case 0x3800:
            clif_GMmessage (NULL, RFIFOP (fd, 4), packet_len - 4, 0);
            break;
        case 0x3801:
            intif_parse_WisMessage (fd);
            break;
        case 0x3802:
            intif_parse_WisEnd (fd);
            break;
        case 0x3803:
            mapif_parse_WisToGM (fd);
            break;
        case 0x3804:
            intif_parse_AccountReg (fd);
            break;
        case 0x3810:
            intif_parse_LoadStorage (fd);
            break;
        case 0x3811:
            intif_parse_SaveStorage (fd);
            break;
        case 0x3820:
            intif_parse_PartyCreated (fd);
            break;
        case 0x3821:
            intif_parse_PartyInfo (fd);
            break;
        case 0x3822:
            intif_parse_PartyMemberAdded (fd);
            break;
        case 0x3823:
            intif_parse_PartyOptionChanged (fd);
            break;
        case 0x3824:
            intif_parse_PartyMemberLeaved (fd);
            break;
        case 0x3825:
            intif_parse_PartyMove (fd);
            break;
        case 0x3826:
            intif_parse_PartyBroken (fd);
            break;
        case 0x3827:
            intif_parse_PartyMessage (fd);
            break;
            //case 0x3880:  intif_parse_CreateP.et(fd); break;
            //case 0x3881:  intif_parse_RecvP.etData(fd); break;
            //case 0x3882:  intif_parse_SaveP.etOk(fd); break;
            //case 0x3883:  intif_parse_DeleteP.etOk(fd); break;
        default:
            if (battle_config.error_log)
                printf ("intif_parse : unknown packet %d %x\n", fd,
                        RFIFOW (fd, 0));
            return 0;
    }
    // パケット読み飛ばし
    RFIFOSKIP (fd, packet_len);
    return 1;
}
