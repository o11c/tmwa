#ifndef PC_HPP
#define PC_HPP

# include "pc.structs.hpp"

# include "main.structs.hpp"
# include "clif.structs.hpp"

# define pc_setdead(sd) ((sd)->state.dead_sit = 1)
# define pc_setsit(sd) ((sd)->state.dead_sit = 2)
//# define pc_setstand(sd) ((sd)->state.dead_sit = 0)
# define pc_isdead(sd) ((sd)->state.dead_sit == 1)
# define pc_issit(sd) ((sd)->state.dead_sit == 2)
# define pc_setdir(sd, b) ((sd)->dir = (b))
# define pc_setchatid(sd, n) ((sd)->chatID = n)
inline bool pc_ishiding(MapSessionData *sd)     { return bool(sd->status.option & (OPTION::CHASEWALK | OPTION::CLOAK | OPTION::HIDE2)); }
inline bool pc_isinvisible(MapSessionData *sd)  { return bool(sd->status.option & OPTION::HIDE); }
# define pc_is50overweight(sd) (sd->weight * 2 >= sd->max_weight)
# define pc_is90overweight(sd) (sd->weight * 10 >= sd->max_weight * 9)

void pc_touch_all_relevant_npcs(MapSessionData *sd);  /* Checks all npcs/warps at the same location to see whether they
                                                                 ** should do something with the specified player. */

__attribute__((pure))
gm_level_t pc_isGM(MapSessionData *sd);
sint32 pc_iskiller(MapSessionData *src, MapSessionData *target);   // [MouseJstr]

void pc_invisibility(MapSessionData *sd, sint32 enabled);    // [Fate]
sint32 pc_counttargeted(MapSessionData *sd, BlockList *src,
                     AttackResult target_lv);
sint32 pc_setrestartvalue(MapSessionData *sd, sint32 type);
sint32 pc_makesavestatus(MapSessionData *);
sint32 pc_setnewpc(MapSessionData *, /*account_t,*/ charid_t, uint32, uint8);
sint32 pc_authok(account_t, sint32, time_t, sint16 tmw_version, const struct mmo_charstatus *);
sint32 pc_authfail(account_t);

EPOS pc_equippoint(MapSessionData *sd, sint32 n);

sint32 pc_checkskill(MapSessionData *sd, sint32 skill_id) __attribute__((pure));
sint32 pc_checkequip(MapSessionData *sd, EPOS pos);

sint32 pc_walktoxy(MapSessionData *, sint32, sint32);
sint32 pc_stop_walking(MapSessionData *, sint32);
sint32 pc_setpos(MapSessionData *, const Point&, BeingRemoveType);
sint32 pc_setsavepoint(MapSessionData *, const Point&);
sint32 pc_randomwarp(MapSessionData *sd, BeingRemoveType type);

sint32 pc_checkadditem(MapSessionData *, sint32, sint32);
sint32 pc_inventoryblank(MapSessionData *);
sint32 pc_search_inventory(MapSessionData *sd, sint32 item_id);
sint32 pc_payzeny(MapSessionData *, sint32);
PickupFail pc_additem(MapSessionData *, struct item *, sint32);
sint32 pc_getzeny(MapSessionData *, sint32);
sint32 pc_delitem(MapSessionData *, sint32, sint32, sint32);
sint32 pc_checkitem(MapSessionData *);
sint32 pc_count_all_items(MapSessionData *player, sint32 item_id);
sint32 pc_remove_items(MapSessionData *player, sint32 item_id,
                     sint32 count);

sint32 pc_takeitem(MapSessionData *, struct flooritem_data *);
sint32 pc_dropitem(MapSessionData *, sint32, sint32);

sint32 pc_calcstatus(MapSessionData *, bool);
sint32 pc_bonus(MapSessionData *, SP, sint32);
sint32 pc_skill(MapSessionData *, sint32, sint32, sint32);

sint32 pc_attack(MapSessionData *, BlockID, bool);
sint32 pc_stopattack(MapSessionData *);

sint32 pc_gainexp(MapSessionData *, sint32, sint32);

enum class PC_GAINEXP_REASON
{
    KILLING,
    HEALING,
    SCRIPT
};
sint32 pc_gainexp_reason(MapSessionData *, sint32, sint32, PC_GAINEXP_REASON reason);
sint32 pc_extract_healer_exp(MapSessionData *, sint32 max);    // [Fate] Used by healers: extract healer-xp from the target, return result (up to max)

sint32 pc_nextbaseexp(MapSessionData *);
sint32 pc_nextjobexp(MapSessionData *) __attribute__((pure));
sint32 pc_need_status_point(MapSessionData *, SP);
sint32 pc_statusup(MapSessionData *, SP);
sint32 pc_statusup2(MapSessionData *, SP, sint32);
sint32 pc_skillup(MapSessionData *, sint32);
sint32 pc_resetlvl(MapSessionData *, sint32 type);
sint32 pc_resetstate(MapSessionData *);
sint32 pc_resetskill(MapSessionData *);
sint32 pc_equipitem(MapSessionData *, sint32);

enum class CalcStatus
{
    NOW,
    LATER
};

sint32 pc_unequipitem(MapSessionData *, sint32, CalcStatus);
sint32 pc_unequipinvyitem(MapSessionData *, sint32, CalcStatus);
sint32 pc_useitem(MapSessionData *, sint32);

sint32 pc_damage(BlockList *, MapSessionData *, sint32);
sint32 pc_heal(MapSessionData *, sint32, sint32);
sint32 pc_itemheal(MapSessionData *sd, sint32 hp, sint32 sp);
sint32 pc_percentheal(MapSessionData *sd, sint32, sint32);
sint32 pc_setoption(MapSessionData *, OPTION);
sint32 pc_changelook(MapSessionData *, LOOK, sint32);

sint32 pc_readparam(MapSessionData *, SP);
sint32 pc_setparam(MapSessionData *, SP, sint32);
sint32 pc_readglobalreg(MapSessionData *, const char *) = delete;
sint32 pc_readglobalreg(MapSessionData *, const std::string&);
sint32 pc_setglobalreg(MapSessionData *, const char *, sint32) = delete;
sint32 pc_setglobalreg(MapSessionData *, const std::string&, sint32);
sint32 pc_readaccountreg(MapSessionData *, const char *) = delete;
sint32 pc_readaccountreg(MapSessionData *, const std::string&);
sint32 pc_setaccountreg(MapSessionData *, const char *, sint32) = delete;
sint32 pc_setaccountreg(MapSessionData *, const std::string&, sint32);
sint32 pc_readaccountreg2(MapSessionData *, const char *) = delete;
sint32 pc_readaccountreg2(MapSessionData *, const std::string&);
sint32 pc_setaccountreg2(MapSessionData *, const char *, sint32) = delete;
sint32 pc_setaccountreg2(MapSessionData *, const std::string&, sint32);

sint32 pc_addeventtimer(MapSessionData *sd, interval_t tick, const char *name);
sint32 pc_deleventtimer(MapSessionData *sd, const char *name);
sint32 pc_cleareventtimer(MapSessionData *sd);

void pc_calc_pvprank_timer(timer_id, tick_t, account_t);

bool pc_marriage(MapSessionData *sd, MapSessionData *dstsd);
bool pc_divorce(MapSessionData *sd);
MapSessionData *pc_get_partner(MapSessionData *sd) __attribute__((pure));
void pc_set_gm_level(account_t account_id, gm_level_t level);
void pc_setstand(MapSessionData *sd);

size_t pc_read_gm_account(sint32 fd);
void pc_setinvincibletimer(MapSessionData *sd, interval_t);
void pc_delinvincibletimer(MapSessionData *sd);
sint32 pc_logout(MapSessionData *sd);   // [fate] Player logs out

sint32 do_init_pc(void);

#endif // PC_HPP
