#ifndef CHAT_H
#define CHAT_H

#include "map.h"

int  chat_createchat (struct map_session_data *, int, int, char *, char *,
                      int);
int  chat_joinchat (struct map_session_data *, int, char *);
int  chat_leavechat (struct map_session_data *);
int  chat_changechatowner (struct map_session_data *, char *);
int  chat_changechatstatus (struct map_session_data *, int, int, char *,
                            char *, int);
int  chat_kickchat (struct map_session_data *, char *);

int  chat_createnpcchat (struct npc_data *nd, int limit, int pub, int trigger,
                         const char *title, int titlelen, const char *ev);
int  chat_deletenpcchat (struct npc_data *nd);
int  chat_enableevent (struct chat_data *cd);
int  chat_disableevent (struct chat_data *cd);
int  chat_npckickall (struct chat_data *cd);

int  do_final_chat (void);

#endif // CHAT_H
