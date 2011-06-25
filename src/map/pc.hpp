#ifndef PC_HPP
#define PC_HPP

# include "pc.structs.hpp"

# include "map.structs.hpp"
# include "clif.structs.hpp"

# define OPTION_MASK 0xd7b8

# define pc_setdead(sd) ((sd)->state.dead_sit = 1)
# define pc_setsit(sd) ((sd)->state.dead_sit = 2)
//# define pc_setstand(sd) ((sd)->state.dead_sit = 0)
# define pc_isdead(sd) ((sd)->state.dead_sit == 1)
# define pc_issit(sd) ((sd)->state.dead_sit == 2)
# define pc_setdir(sd,b) ((sd)->dir = (b))
# define pc_setchatid(sd,n) ((sd)->chatID = n)
# define pc_ishiding(sd) ((sd)->status.option&0x4006)
# define pc_isinvisible(sd) ((sd)->status.option&0x0040)
# define pc_is50overweight(sd) (sd->weight*2 >= sd->max_weight)
# define pc_is90overweight(sd) (sd->weight*10 >= sd->max_weight*9)

void pc_touch_all_relevant_npcs(MapSessionData *sd);  /* Checks all npcs/warps at the same location to see whether they
                                                                 ** should do something with the specified player. */

int pc_isGM(MapSessionData *sd);
int pc_iskiller(MapSessionData *src, MapSessionData *target);   // [MouseJstr]

void pc_invisibility(MapSessionData *sd, int enabled);    // [Fate]
int pc_counttargeted(MapSessionData *sd, BlockList *src,
                     AttackResult target_lv);
int pc_setrestartvalue(MapSessionData *sd, int type);
int pc_makesavestatus(MapSessionData *);
int pc_setnewpc(MapSessionData *, account_t, charid_t, uint32_t, uint8_t);
int pc_authok(int, int, time_t, short tmw_version, const struct mmo_charstatus *);
int pc_authfail(int);

int pc_equippoint(MapSessionData *sd, int n);

int pc_breakweapon(MapSessionData *sd);  // weapon breaking [Valaris]
int pc_breakarmor(MapSessionData *sd);   // armor breaking [Valaris]

int pc_checkskill(MapSessionData *sd, int skill_id);
int pc_checkequip(MapSessionData *sd, int pos);

int pc_walktoxy(MapSessionData *, int, int);
int pc_stop_walking(MapSessionData *, int);
int pc_setpos(MapSessionData *, const Point&, BeingRemoveType);
int pc_setsavepoint(MapSessionData *, const Point&);
int pc_randomwarp(MapSessionData *sd, BeingRemoveType type);

int pc_checkadditem(MapSessionData *, int, int);
int pc_inventoryblank(MapSessionData *);
int pc_search_inventory(MapSessionData *sd, int item_id);
int pc_payzeny(MapSessionData *, int);
PickupFail pc_additem(MapSessionData *, struct item *, int);
int pc_getzeny(MapSessionData *, int);
int pc_delitem(MapSessionData *, int, int, int);
int pc_checkitem(MapSessionData *);
int pc_count_all_items(MapSessionData *player, int item_id);
int pc_remove_items(MapSessionData *player, int item_id,
                     int count);

int pc_takeitem(MapSessionData *, struct flooritem_data *);
int pc_dropitem(MapSessionData *, int, int);

int pc_calcstatus(MapSessionData *, int);
int pc_bonus(MapSessionData *, int, int);
int pc_skill(MapSessionData *, int, int, int);

int pc_attack(MapSessionData *, int, int);
int pc_stopattack(MapSessionData *);

int pc_gainexp(MapSessionData *, int, int);

# define PC_GAINEXP_REASON_KILLING 0
# define PC_GAINEXP_REASON_HEALING 1
# define PC_GAINEXP_REASON_SCRIPT  2
int pc_gainexp_reason(MapSessionData *, int, int, int reason);
int pc_extract_healer_exp(MapSessionData *, int max);    // [Fate] Used by healers: extract healer-xp from the target, return result (up to max)

int pc_nextbaseexp(MapSessionData *);
int pc_nextjobexp(MapSessionData *);
int pc_need_status_point(MapSessionData *, int);
int pc_statusup(MapSessionData *, int);
int pc_statusup2(MapSessionData *, int, int);
int pc_skillup(MapSessionData *, int);
int pc_resetlvl(MapSessionData *, int type);
int pc_resetstate(MapSessionData *);
int pc_resetskill(MapSessionData *);
int pc_equipitem(MapSessionData *, int, int);
int pc_unequipitem(MapSessionData *, int, int);
int pc_unequipinvyitem(MapSessionData *, int, int);
int pc_useitem(MapSessionData *, int);

int pc_damage(BlockList *, MapSessionData *, int);
int pc_heal(MapSessionData *, int, int);
int pc_itemheal(MapSessionData *sd, int hp, int sp);
int pc_percentheal(MapSessionData *sd, int, int);
int pc_setoption(MapSessionData *, int);
int pc_changelook(MapSessionData *, int, int);

int pc_readparam(MapSessionData *, int);
int pc_setparam(MapSessionData *, int, int);
int pc_readreg(MapSessionData *, int);
int pc_setreg(MapSessionData *, int, int);
char *pc_readregstr(MapSessionData *sd, int reg);
int pc_setregstr(MapSessionData *sd, int reg, const char *str);
int pc_readglobalreg(MapSessionData *, const char *);
int pc_setglobalreg(MapSessionData *, const char *, int);
int pc_readaccountreg(MapSessionData *, const char *);
int pc_setaccountreg(MapSessionData *, const char *, int);
int pc_readaccountreg2(MapSessionData *, const char *);
int pc_setaccountreg2(MapSessionData *, const char *, int);
int pc_percentrefinery(MapSessionData *sd, struct item *item);

int pc_addeventtimer(MapSessionData *sd, int tick,
                      const char *name);
int pc_deleventtimer(MapSessionData *sd, const char *name);
int pc_cleareventtimer(MapSessionData *sd);

void pc_calc_pvprank_timer(timer_id, tick_t, uint32_t);

int pc_marriage(MapSessionData *sd,
                 MapSessionData *dstsd);
int pc_divorce(MapSessionData *sd);
MapSessionData *pc_get_partner(MapSessionData *sd);
int pc_set_gm_level(int account_id, int level);
void pc_setstand(MapSessionData *sd);

int pc_read_gm_account(int fd);
int pc_setinvincibletimer(MapSessionData *sd, int);
int pc_delinvincibletimer(MapSessionData *sd);
int pc_logout(MapSessionData *sd);   // [fate] Player logs out

int do_init_pc(void);

#endif // PC_HPP
