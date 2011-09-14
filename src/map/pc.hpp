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

int32_t pc_isGM(MapSessionData *sd) __attribute__((pure));
int32_t pc_iskiller(MapSessionData *src, MapSessionData *target);   // [MouseJstr]

void pc_invisibility(MapSessionData *sd, int32_t enabled);    // [Fate]
int32_t pc_counttargeted(MapSessionData *sd, BlockList *src,
                     AttackResult target_lv);
int32_t pc_setrestartvalue(MapSessionData *sd, int32_t type);
int32_t pc_makesavestatus(MapSessionData *);
int32_t pc_setnewpc(MapSessionData *, account_t, charid_t, uint32_t, uint8_t);
int32_t pc_authok(int32_t, int32_t, time_t, int16_t tmw_version, const struct mmo_charstatus *);
int32_t pc_authfail(int32_t);

EPOS pc_equippoint(MapSessionData *sd, int32_t n);

int32_t pc_checkskill(MapSessionData *sd, int32_t skill_id) __attribute__((pure));
int32_t pc_checkequip(MapSessionData *sd, EPOS pos);

int32_t pc_walktoxy(MapSessionData *, int32_t, int32_t);
int32_t pc_stop_walking(MapSessionData *, int32_t);
int32_t pc_setpos(MapSessionData *, const Point&, BeingRemoveType);
int32_t pc_setsavepoint(MapSessionData *, const Point&);
int32_t pc_randomwarp(MapSessionData *sd, BeingRemoveType type);

int32_t pc_checkadditem(MapSessionData *, int32_t, int32_t);
int32_t pc_inventoryblank(MapSessionData *);
int32_t pc_search_inventory(MapSessionData *sd, int32_t item_id);
int32_t pc_payzeny(MapSessionData *, int32_t);
PickupFail pc_additem(MapSessionData *, struct item *, int32_t);
int32_t pc_getzeny(MapSessionData *, int32_t);
int32_t pc_delitem(MapSessionData *, int32_t, int32_t, int32_t);
int32_t pc_checkitem(MapSessionData *);
int32_t pc_count_all_items(MapSessionData *player, int32_t item_id);
int32_t pc_remove_items(MapSessionData *player, int32_t item_id,
                     int32_t count);

int32_t pc_takeitem(MapSessionData *, struct flooritem_data *);
int32_t pc_dropitem(MapSessionData *, int32_t, int32_t);

int32_t pc_calcstatus(MapSessionData *, int32_t);
int32_t pc_bonus(MapSessionData *, SP, int32_t);
int32_t pc_skill(MapSessionData *, int32_t, int32_t, int32_t);

int32_t pc_attack(MapSessionData *, int32_t, int32_t);
int32_t pc_stopattack(MapSessionData *);

int32_t pc_gainexp(MapSessionData *, int32_t, int32_t);

enum class PC_GAINEXP_REASON
{
    KILLING,
    HEALING,
    SCRIPT
};
int32_t pc_gainexp_reason(MapSessionData *, int32_t, int32_t, PC_GAINEXP_REASON reason);
int32_t pc_extract_healer_exp(MapSessionData *, int32_t max);    // [Fate] Used by healers: extract healer-xp from the target, return result (up to max)

int32_t pc_nextbaseexp(MapSessionData *);
int32_t pc_nextjobexp(MapSessionData *) __attribute__((pure));
int32_t pc_need_status_point(MapSessionData *, SP);
int32_t pc_statusup(MapSessionData *, SP);
int32_t pc_statusup2(MapSessionData *, SP, int32_t);
int32_t pc_skillup(MapSessionData *, int32_t);
int32_t pc_resetlvl(MapSessionData *, int32_t type);
int32_t pc_resetstate(MapSessionData *);
int32_t pc_resetskill(MapSessionData *);
int32_t pc_equipitem(MapSessionData *, int32_t);
int32_t pc_unequipitem(MapSessionData *, int32_t, bool);
int32_t pc_unequipinvyitem(MapSessionData *, int32_t, bool);
int32_t pc_useitem(MapSessionData *, int32_t);

int32_t pc_damage(BlockList *, MapSessionData *, int32_t);
int32_t pc_heal(MapSessionData *, int32_t, int32_t);
int32_t pc_itemheal(MapSessionData *sd, int32_t hp, int32_t sp);
int32_t pc_percentheal(MapSessionData *sd, int32_t, int32_t);
int32_t pc_setoption(MapSessionData *, int32_t);
int32_t pc_changelook(MapSessionData *, LOOK, int32_t);

int32_t pc_readparam(MapSessionData *, SP);
int32_t pc_setparam(MapSessionData *, SP, int32_t);
int32_t pc_readreg(MapSessionData *, int32_t) __attribute__((pure));
int32_t pc_setreg(MapSessionData *, int32_t, int32_t);
char *pc_readregstr(MapSessionData *sd, int32_t reg);
int32_t pc_setregstr(MapSessionData *sd, int32_t reg, const char *str);
int32_t pc_readglobalreg(MapSessionData *, const char *) __attribute__((pure));
int32_t pc_setglobalreg(MapSessionData *, const char *, int32_t);
int32_t pc_readaccountreg(MapSessionData *, const char *) __attribute__((pure));
int32_t pc_setaccountreg(MapSessionData *, const char *, int32_t);
int32_t pc_readaccountreg2(MapSessionData *, const char *) __attribute__((pure));
int32_t pc_setaccountreg2(MapSessionData *, const char *, int32_t);

int32_t pc_addeventtimer(MapSessionData *sd, int32_t tick, const char *name);
int32_t pc_deleventtimer(MapSessionData *sd, const char *name);
int32_t pc_cleareventtimer(MapSessionData *sd);

void pc_calc_pvprank_timer(timer_id, tick_t, uint32_t);

int32_t pc_marriage(MapSessionData *sd, MapSessionData *dstsd);
int32_t pc_divorce(MapSessionData *sd);
MapSessionData *pc_get_partner(MapSessionData *sd) __attribute__((pure));
int32_t pc_set_gm_level(int32_t account_id, int32_t level);
void pc_setstand(MapSessionData *sd);

int32_t pc_read_gm_account(int32_t fd);
int32_t pc_setinvincibletimer(MapSessionData *sd, int32_t);
int32_t pc_delinvincibletimer(MapSessionData *sd);
int32_t pc_logout(MapSessionData *sd);   // [fate] Player logs out

int32_t do_init_pc(void);

#endif // PC_HPP
