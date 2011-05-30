#ifndef PC_H
#define PC_H

#include "map.hpp"

#define OPTION_MASK 0xd7b8

#define pc_setdead(sd) ((sd)->state.dead_sit = 1)
#define pc_setsit(sd) ((sd)->state.dead_sit = 2)
//#define pc_setstand(sd) ((sd)->state.dead_sit = 0)
#define pc_isdead(sd) ((sd)->state.dead_sit == 1)
#define pc_issit(sd) ((sd)->state.dead_sit == 2)
#define pc_setdir(sd,b) ((sd)->dir = (b))
#define pc_setchatid(sd,n) ((sd)->chatID = n)
#define pc_ishiding(sd) ((sd)->status.option&0x4006)
#define pc_isinvisible(sd) ((sd)->status.option&0x0040)
#define pc_is50overweight(sd) (sd->weight*2 >= sd->max_weight)
#define pc_is90overweight(sd) (sd->weight*10 >= sd->max_weight*9)

void pc_touch_all_relevant_npcs(struct map_session_data *sd);  /* Checks all npcs/warps at the same location to see whether they
                                                                 ** should do something with the specified player. */

int pc_isGM(struct map_session_data *sd);
int pc_iskiller(struct map_session_data *src, struct map_session_data *target);   // [MouseJstr]

void pc_invisibility(struct map_session_data *sd, int enabled);    // [Fate]
int pc_counttargeted(struct map_session_data *sd, struct block_list *src,
                     AttackResult target_lv);
int pc_setrestartvalue(struct map_session_data *sd, int type);
int pc_makesavestatus(struct map_session_data *);
int pc_setnewpc(struct map_session_data *, int, int, int, int, int, int);
int pc_authok(int, int, time_t, short tmw_version, const struct mmo_charstatus *);
int pc_authfail(int);

int pc_equippoint(struct map_session_data *sd, int n);

int pc_breakweapon(struct map_session_data *sd);  // weapon breaking [Valaris]
int pc_breakarmor(struct map_session_data *sd);   // armor breaking [Valaris]

int pc_checkskill(struct map_session_data *sd, int skill_id);
int pc_checkequip(struct map_session_data *sd, int pos);

int pc_walktoxy(struct map_session_data *, int, int);
int pc_stop_walking(struct map_session_data *, int);
int pc_setpos(struct map_session_data *, const char *, int, int, int);
int pc_setsavepoint(struct map_session_data *, const char *, int, int);
int pc_randomwarp(struct map_session_data *sd, int type);

int pc_checkadditem(struct map_session_data *, int, int);
int pc_inventoryblank(struct map_session_data *);
int pc_search_inventory(struct map_session_data *sd, int item_id);
int pc_payzeny(struct map_session_data *, int);
int pc_additem(struct map_session_data *, struct item *, int);
int pc_getzeny(struct map_session_data *, int);
int pc_delitem(struct map_session_data *, int, int, int);
int pc_checkitem(struct map_session_data *);
int pc_count_all_items(struct map_session_data *player, int item_id);
int pc_remove_items(struct map_session_data *player, int item_id,
                     int count);

int pc_takeitem(struct map_session_data *, struct flooritem_data *);
int pc_dropitem(struct map_session_data *, int, int);

int pc_calcstatus(struct map_session_data *, int);
int pc_bonus(struct map_session_data *, int, int);
int pc_skill(struct map_session_data *, int, int, int);

int pc_attack(struct map_session_data *, int, int);
int pc_stopattack(struct map_session_data *);

int pc_gainexp(struct map_session_data *, int, int);

#define PC_GAINEXP_REASON_KILLING 0
#define PC_GAINEXP_REASON_HEALING 1
#define PC_GAINEXP_REASON_SCRIPT  2
int pc_gainexp_reason(struct map_session_data *, int, int, int reason);
int pc_extract_healer_exp(struct map_session_data *, int max);    // [Fate] Used by healers: extract healer-xp from the target, return result (up to max)

int pc_nextbaseexp(struct map_session_data *);
int pc_nextjobexp(struct map_session_data *);
int pc_need_status_point(struct map_session_data *, int);
int pc_statusup(struct map_session_data *, int);
int pc_statusup2(struct map_session_data *, int, int);
int pc_skillup(struct map_session_data *, int);
int pc_resetlvl(struct map_session_data *, int type);
int pc_resetstate(struct map_session_data *);
int pc_resetskill(struct map_session_data *);
int pc_equipitem(struct map_session_data *, int, int);
int pc_unequipitem(struct map_session_data *, int, int);
int pc_unequipinvyitem(struct map_session_data *, int, int);
int pc_useitem(struct map_session_data *, int);

int pc_damage(struct block_list *, struct map_session_data *, int);
int pc_heal(struct map_session_data *, int, int);
int pc_itemheal(struct map_session_data *sd, int hp, int sp);
int pc_percentheal(struct map_session_data *sd, int, int);
int pc_setoption(struct map_session_data *, int);
int pc_changelook(struct map_session_data *, int, int);

int pc_readparam(struct map_session_data *, int);
int pc_setparam(struct map_session_data *, int, int);
int pc_readreg(struct map_session_data *, int);
int pc_setreg(struct map_session_data *, int, int);
char *pc_readregstr(struct map_session_data *sd, int reg);
int pc_setregstr(struct map_session_data *sd, int reg, const char *str);
int pc_readglobalreg(struct map_session_data *, const char *);
int pc_setglobalreg(struct map_session_data *, const char *, int);
int pc_readaccountreg(struct map_session_data *, const char *);
int pc_setaccountreg(struct map_session_data *, const char *, int);
int pc_readaccountreg2(struct map_session_data *, const char *);
int pc_setaccountreg2(struct map_session_data *, const char *, int);
int pc_percentrefinery(struct map_session_data *sd, struct item *item);

int pc_addeventtimer(struct map_session_data *sd, int tick,
                      const char *name);
int pc_deleventtimer(struct map_session_data *sd, const char *name);
int pc_cleareventtimer(struct map_session_data *sd);
int pc_addeventtimercount(struct map_session_data *sd, const char *name,
                           int tick);

void pc_calc_pvprank_timer(timer_id, tick_t, custom_id_t, custom_data_t);

int pc_marriage(struct map_session_data *sd,
                 struct map_session_data *dstsd);
int pc_divorce(struct map_session_data *sd);
struct map_session_data *pc_get_partner(struct map_session_data *sd);
int pc_set_gm_level(int account_id, int level);
void pc_setstand(struct map_session_data *sd);

int pc_read_gm_account(int fd);
int pc_setinvincibletimer(struct map_session_data *sd, int);
int pc_delinvincibletimer(struct map_session_data *sd);
int pc_logout(struct map_session_data *sd);   // [fate] Player logs out

int do_init_pc(void);

enum
{ ADDITEM_EXIST, ADDITEM_NEW, ADDITEM_OVERAMOUNT };

#endif // PC_H
