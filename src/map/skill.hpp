#ifndef SKILL_H
#define SKILL_H

#include "../common/timer.hpp"

#include "map.hpp"
#include "magic.hpp"

#define MAX_SKILL_DB			450
#define MAX_SKILL_PRODUCE_DB	 150
#define MAX_SKILL_ARROW_DB	 150
#define MAX_SKILL_ABRA_DB	 350

#define SKILL_POOL_FLAG		0x1 // is a pool skill
#define SKILL_POOL_ACTIVE	0x2 // is an active pool skill
#define SKILL_POOL_ACTIVATED	0x4 // pool skill has been activated (used for clif)

// スキルデータベース
struct skill_db
{
    int  range[MAX_SKILL_LEVEL], hit, inf, pl, nk, max, stat, poolflags, max_raise; // `max' is the global max, `max_raise' is the maximum attainable via skill-ups
    int  num[MAX_SKILL_LEVEL];
    int  cast[MAX_SKILL_LEVEL], delay[MAX_SKILL_LEVEL];
    int  upkeep_time[MAX_SKILL_LEVEL], upkeep_time2[MAX_SKILL_LEVEL];
    int  castcancel, cast_def_rate;
    int  inf2, maxcount, skill_type;
    int  blewcount[MAX_SKILL_LEVEL];
    int  hp[MAX_SKILL_LEVEL], sp[MAX_SKILL_LEVEL], mhp[MAX_SKILL_LEVEL],
        hp_rate[MAX_SKILL_LEVEL], sp_rate[MAX_SKILL_LEVEL],
        zeny[MAX_SKILL_LEVEL];
    int  weapon, state;
    int  itemid[10], amount[10];
    int  castnodex[MAX_SKILL_LEVEL];
};
extern struct skill_db skill_db[MAX_SKILL_DB];

struct skill_name_db
{
    int  id;                    // skill id
    const char *name;                 // search strings
    const char *desc;                 // description that shows up for search's
};
extern struct skill_name_db skill_names[];

struct block_list;
struct map_session_data;
struct skill_unit;
struct skill_unit_group;

int  do_init_skill (void);

// スキルデータベースへのアクセサ
int  skill_get_hit (int id);
int  skill_get_inf (int id);
int  skill_get_pl (int id);
int  skill_get_nk (int id);
int  skill_get_max (int id);
int  skill_get_max_raise (int id);
int  skill_get_range (int id, int lv);
int  skill_get_hp (int id, int lv);
int  skill_get_mhp (int id, int lv);
int  skill_get_sp (int id, int lv);
int  skill_get_zeny (int id, int lv);
int  skill_get_num (int id, int lv);
int  skill_get_cast (int id, int lv);
int  skill_get_delay (int id, int lv);
int  skill_get_time (int id, int lv);
int  skill_get_time2 (int id, int lv);
int  skill_get_castdef (int id);
int  skill_get_weapontype (int id);
int  skill_get_unit_id (int id, int flag);
int  skill_get_inf2 (int id);
int  skill_get_maxcount (int id);
int  skill_get_blewcount (int id, int lv);

// スキルの使用
int  skill_use_id (struct map_session_data *sd, int target_id,
                   int skill_num, int skill_lv);
int  skill_use_pos (struct map_session_data *sd,
                    int skill_x, int skill_y, int skill_num, int skill_lv);

int  skill_castend_map (struct map_session_data *sd, int skill_num,
                        const char *map);

int  skill_cleartimerskill (struct block_list *src);
int  skill_addtimerskill (struct block_list *src, unsigned int tick,
                          int target, int x, int y, int skill_id,
                          int skill_lv, int type, int flag);

// 追加効果
int  skill_additional_effect (struct block_list *src, struct block_list *bl,
                              int skillid, int skilllv, int attack_type,
                              unsigned int tick);

// ユニットスキル
struct skill_unit *skill_initunit (struct skill_unit_group *group, int idx,
                                   int x, int y);
int  skill_delunit (struct skill_unit *unit);
struct skill_unit_group *skill_initunitgroup (struct block_list *src,
                                              int count, int skillid,
                                              int skilllv, int unit_id);
int  skill_delunitgroup (struct skill_unit_group *group);
struct skill_unit_group_tickset *skill_unitgrouptickset_search (struct
                                                                block_list
                                                                *bl,
                                                                int group_id);
int  skill_unitgrouptickset_delete (struct block_list *bl, int group_id);
int  skill_clear_unitgroup (struct block_list *src);

int  skill_unit_ondamaged (struct skill_unit *src, struct block_list *bl,
                           int damage, unsigned int tick);

int  skill_castfix (struct block_list *bl, int time);
int  skill_delayfix (struct block_list *bl, int time);
int  skill_check_unit_range (int m, int x, int y, int range, int skillid);
int  skill_check_unit_range2 (int m, int x, int y, int range);
// -- moonsoul  (added skill_check_unit_cell)
int  skill_check_unit_cell (int skillid, int m, int x, int y, int unit_id);
int  skill_unit_out_all (struct block_list *bl, unsigned int tick, int range);
int  skill_unit_move (struct block_list *bl, unsigned int tick, int range);
int  skill_unit_move_unit_group (struct skill_unit_group *group, int m,
                                 int dx, int dy);

struct skill_unit_group *skill_check_dancing (struct block_list *src);
void skill_stop_dancing (struct block_list *src, int flag);

// 詠唱キャンセル
int  skill_castcancel (struct block_list *bl, int type);

int  skill_gangsterparadise (struct map_session_data *sd, int type);
void skill_brandishspear_first (struct square *tc, int dir, int x, int y);
void skill_brandishspear_dir (struct square *tc, int dir, int are);
void skill_devotion (struct map_session_data *md, int target);
void skill_devotion2 (struct block_list *bl, int crusader);
int  skill_devotion3 (struct block_list *bl, int target);
void skill_devotion_end (struct map_session_data *md,
                         struct map_session_data *sd, int target);

#define skill_calc_heal(bl,skill_lv) (( battle_get_lv(bl)+battle_get_int(bl) )/8 *(4+ skill_lv*8))

// その他
int  skill_check_cloaking (struct block_list *bl);
int  skill_is_danceskill (int id);

// ステータス異常
int  skill_status_effect (struct block_list *bl, int type, int val1, int val2,
                          int val3, int val4, int tick, int flag,
                          int spell_invocation);
int  skill_status_change_start (struct block_list *bl, int type, int val1,
                                int val2, int val3, int val4, int tick,
                                int flag);
void skill_status_change_timer (timer_id, tick_t, custom_id_t, custom_data_t);
int  skill_status_change_active (struct block_list *bl, int type);  // [fate]
int  skill_encchant_eremental_end (struct block_list *bl, int type);
int  skill_status_change_end (struct block_list *bl, int type, int tid);
int  skill_status_change_clear (struct block_list *bl, int type);

// mobスキルのため
int  skill_castend_nodamage_id (struct block_list *src, struct block_list *bl,
                                int skillid, int skilllv, unsigned int tick,
                                int flag);
int  skill_castend_damage_id (struct block_list *src, struct block_list *bl,
                              int skillid, int skilllv, unsigned int tick,
                              int flag);
int  skill_castend_pos2 (struct block_list *src, int x, int y, int skillid,
                         int skilllv, unsigned int tick, int flag);

// スキル攻撃一括処理
int  skill_attack (int attack_type, struct block_list *src,
                   struct block_list *dsrc, struct block_list *bl,
                   int skillid, int skilllv, unsigned int tick, int flag);

int  skill_update_heal_animation (struct map_session_data *sd); // [Fate]  Check whether the healing flag must be updated, do so if needed

void skill_reload (void);

enum
{
    ST_NONE, ST_HIDING, ST_CLOAKING, ST_HIDDEN,
    ST_SHIELD, ST_SIGHT, ST_EXPLOSIONSPIRITS,
    ST_RECOV_WEIGHT_RATE, ST_MOVE_ENABLE, ST_WATER,
};

const int SC_SENDMAX = 256;
const int SC_PROVOKE __attribute__((deprecated)) = 0;
const int SC_ENDURE __attribute__((deprecated)) = 1;
const int SC_TWOHANDQUICKEN __attribute__((deprecated)) = 2;
const int SC_CONCENTRATE __attribute__((deprecated)) = 3;
const int SC_HIDING __attribute__((deprecated)) = 4;
const int SC_CLOAKING __attribute__((deprecated)) = 5;
const int SC_ENCPOISON __attribute__((deprecated)) = 6;
const int SC_POISONREACT __attribute__((deprecated)) = 7;
const int SC_QUAGMIRE __attribute__((deprecated)) = 8;
const int SC_ANGELUS __attribute__((deprecated)) = 9;
const int SC_BLESSING __attribute__((deprecated)) = 10;
const int SC_SIGNUMCRUCIS __attribute__((deprecated)) = 11;
const int SC_INCREASEAGI __attribute__((deprecated)) = 12;
const int SC_DECREASEAGI __attribute__((deprecated)) = 13;
const int SC_SLOWPOISON = 14;
const int SC_IMPOSITIO __attribute__((deprecated)) = 15;
const int SC_SUFFRAGIUM __attribute__((deprecated)) = 16;
const int SC_ASPERSIO __attribute__((deprecated)) = 17;
const int SC_BENEDICTIO __attribute__((deprecated)) = 18;
const int SC_KYRIE __attribute__((deprecated)) = 19;
const int SC_MAGNIFICAT __attribute__((deprecated)) = 20;
const int SC_GLORIA __attribute__((deprecated)) = 21;
const int SC_AETERNA __attribute__((deprecated)) = 22;
const int SC_ADRENALINE __attribute__((deprecated)) = 23;
const int SC_WEAPONPERFECTION __attribute__((deprecated)) = 24;
const int SC_OVERTHRUST __attribute__((deprecated)) = 25;
const int SC_MAXIMIZEPOWER __attribute__((deprecated)) = 26;
const int SC_TRICKDEAD __attribute__((deprecated)) = 29;
const int SC_LOUD __attribute__((deprecated)) = 30;
const int SC_ENERGYCOAT __attribute__((deprecated)) = 31;
const int SC_BROKNARMOR __attribute__((deprecated)) = 32;
const int SC_BROKNWEAPON __attribute__((deprecated)) = 33;
const int SC_HALLUCINATION __attribute__((deprecated)) = 34;
const int SC_WEIGHT50 __attribute__((deprecated)) = 35;
const int SC_WEIGHT90 __attribute__((deprecated)) = 36;
const int SC_SPEEDPOTION0 = 37;
const int SC_SPEEDPOTION1 __attribute__((deprecated)) = 38;
const int SC_SPEEDPOTION2 __attribute__((deprecated)) = 39;

const int SC_STRIPWEAPON __attribute__((deprecated)) = 50;
const int SC_STRIPSHIELD __attribute__((deprecated)) = 51;
const int SC_STRIPARMOR __attribute__((deprecated)) = 52;
const int SC_STRIPHELM __attribute__((deprecated)) = 53;
const int SC_CP_WEAPON __attribute__((deprecated)) = 54;
const int SC_CP_SHIELD __attribute__((deprecated)) = 55;
const int SC_CP_ARMOR __attribute__((deprecated)) = 56;
const int SC_CP_HELM __attribute__((deprecated)) = 57;
const int SC_AUTOGUARD __attribute__((deprecated)) = 58;
const int SC_REFLECTSHIELD __attribute__((deprecated)) = 59;
const int SC_DEVOTION __attribute__((deprecated)) = 60;
const int SC_PROVIDENCE __attribute__((deprecated)) = 61;
const int SC_DEFENDER __attribute__((deprecated)) = 62;
const int SC_AUTOSPELL __attribute__((deprecated)) = 65;
const int SC_EXPLOSIONSPIRITS __attribute__((deprecated)) = 86;
const int SC_STEELBODY __attribute__((deprecated)) = 87;
const int SC_SPEARSQUICKEN __attribute__((deprecated)) = 68;

const int SC_HEALING = 70;

const int SC_SIGHTTRASHER __attribute__((deprecated)) = 73;

const int SC_COMBO __attribute__((deprecated)) = 89;
const int SC_FLAMELAUNCHER __attribute__((deprecated)) = 90;
const int SC_FROSTWEAPON __attribute__((deprecated)) = 91;
const int SC_LIGHTNINGLOADER __attribute__((deprecated)) = 92;
const int SC_SEISMICWEAPON __attribute__((deprecated)) = 93;

const int SC_AURABLADE __attribute__((deprecated)) = 103;
const int SC_PARRYING __attribute__((deprecated)) = 104;
const int SC_CONCENTRATION __attribute__((deprecated)) = 105;
const int SC_TENSIONRELAX __attribute__((deprecated)) = 106;
const int SC_BERSERK __attribute__((deprecated)) = 107;

const int SC_ASSUMPTIO __attribute__((deprecated)) = 110;

const int SC_MAGICPOWER __attribute__((deprecated)) = 113;

const int SC_TRUESIGHT __attribute__((deprecated)) = 115;
const int SC_WINDWALK __attribute__((deprecated)) = 116;
const int SC_MELTDOWN __attribute__((deprecated)) = 117;

const int SC_REJECTSWORD __attribute__((deprecated)) = 120;
const int SC_MARIONETTE __attribute__((deprecated)) = 121;

const int SC_HEADCRUSH __attribute__((deprecated)) = 124;
const int SC_JOINTBEAT __attribute__((deprecated)) = 125;
const int SC_BASILICA __attribute__((deprecated)) = 125;

const int SC_STONE __attribute__((deprecated)) = 128;
const int SC_FREEZE __attribute__((deprecated)) = 129;
const int SC_STAN __attribute__((deprecated)) = 130;
const int SC_SLEEP __attribute__((deprecated)) = 131;
const int SC_POISON = 132;
const int SC_CURSE __attribute__((deprecated)) = 133;
const int SC_SILENCE __attribute__((deprecated)) = 134;
const int SC_CONFUSION __attribute__((deprecated)) = 135;
const int SC_BLIND __attribute__((deprecated)) = 136;

const int SC_SAFETYWALL __attribute__((deprecated)) = 140;
const int SC_PNEUMA __attribute__((deprecated)) = 141;
const int SC_WATERBALL __attribute__((deprecated)) = 142;
const int SC_ANKLE __attribute__((deprecated)) = 143;
const int SC_DANCING __attribute__((deprecated)) = 144;
const int SC_KEEPING __attribute__((deprecated)) = 145;
const int SC_BARRIER __attribute__((deprecated)) = 146;

const int SC_MAGICROD __attribute__((deprecated)) = 149;
const int SC_SIGHT __attribute__((deprecated)) = 150;
const int SC_RUWACH __attribute__((deprecated)) = 151;
const int SC_AUTOCOUNTER __attribute__((deprecated)) = 152;
const int SC_VOLCANO __attribute__((deprecated)) = 153;
const int SC_DELUGE __attribute__((deprecated)) = 154;
const int SC_VIOLENTGALE __attribute__((deprecated)) = 155;
const int SC_BLADESTOP_WAIT __attribute__((deprecated)) = 156;
const int SC_BLADESTOP __attribute__((deprecated)) = 157;
const int SC_EXTREMITYFIST __attribute__((deprecated)) = 158;
const int SC_GRAFFITI __attribute__((deprecated)) = 159;
const int SC_ENSEMBLE __attribute__((deprecated)) = 159;

const int SC_LULLABY __attribute__((deprecated)) = 160;
const int SC_RICHMANKIM __attribute__((deprecated)) = 161;
const int SC_ETERNALCHAOS __attribute__((deprecated)) = 162;
const int SC_DRUMBATTLE __attribute__((deprecated)) = 163;
const int SC_NIBELUNGEN __attribute__((deprecated)) = 164;
const int SC_ROKISWEIL __attribute__((deprecated)) = 165;
const int SC_INTOABYSS __attribute__((deprecated)) = 166;
const int SC_SIEGFRIED __attribute__((deprecated)) = 167;
const int SC_DISSONANCE __attribute__((deprecated)) = 168;
const int SC_WHISTLE __attribute__((deprecated)) = 169;
const int SC_ASSNCROS __attribute__((deprecated)) = 170;
const int SC_POEMBRAGI __attribute__((deprecated)) = 171;
const int SC_APPLEIDUN __attribute__((deprecated)) = 172;
const int SC_UGLYDANCE __attribute__((deprecated)) = 173;
const int SC_HUMMING __attribute__((deprecated)) = 174;
const int SC_DONTFORGETME __attribute__((deprecated)) = 175;
const int SC_FORTUNE __attribute__((deprecated)) = 176;
const int SC_SERVICE4U __attribute__((deprecated)) = 177;
const int SC_FOGWALL __attribute__((deprecated)) = 178;
const int SC_GOSPEL __attribute__((deprecated)) = 179;
const int SC_SPIDERWEB __attribute__((deprecated)) = 180;
const int SC_MEMORIZE __attribute__((deprecated)) = 181;
const int SC_LANDPROTECTOR __attribute__((deprecated)) = 182;
const int SC_ADAPTATION __attribute__((deprecated)) = 183;
const int SC_CHASEWALK __attribute__((deprecated)) = 184;
const int SC_ATKPOT = 185;
const int SC_MATKPOT __attribute__((deprecated)) = 186;
const int SC_WEDDING __attribute__((deprecated)) = 187;
const int SC_NOCHAT __attribute__((deprecated)) = 188;
const int SC_SPLASHER __attribute__((deprecated)) = 189;
const int SC_SELFDESTRUCTION __attribute__((deprecated)) = 190;
const int SC_MINDBREAKER __attribute__((deprecated)) = 191;
const int SC_SPELLBREAKER __attribute__((deprecated)) = 192;

// Added for Fate's spells
const int SC_HIDE = 194;              // Hide from `detect' magic
const int SC_HALT_REGENERATE = 195;   // Suspend regeneration
const int SC_FLYING_BACKPACK = 196;   // Flying backpack
const int SC_MBARRIER = 197;          // Magical barrier; magic resistance (val1 : power (%))
const int SC_HASTE = 198;             // `Haste' spell (val1 : power)
const int SC_PHYS_SHIELD = 199;       // `Protect' spell; reduce damage (val1: power)

const int SC_DIVINA __attribute__((deprecated)) = 134; //SC_SILENCE;

extern int SkillStatusChangeTable[];

const int NV_EMOTE = 1;
const int NV_TRADE = 2;
const int NV_PARTY = 3;

const int SM_SWORD __attribute__((deprecated)) = 4;
const int SM_TWOHAND __attribute__((deprecated)) = 5;
const int SM_RECOVERY __attribute__((deprecated)) = 6;
const int SM_BASH __attribute__((deprecated)) = 7;
const int SM_PROVOKE __attribute__((deprecated)) = 8;
const int SM_MAGNUM __attribute__((deprecated)) = 9;
const int SM_ENDURE __attribute__((deprecated)) = 10;

const int MG_SRECOVERY __attribute__((deprecated)) = 11;
const int MG_SIGHT __attribute__((deprecated)) = 12;
const int MG_NAPALMBEAT __attribute__((deprecated)) = 13;
const int MG_SAFETYWALL __attribute__((deprecated)) = 14;
const int MG_SOULSTRIKE __attribute__((deprecated)) = 15;
const int MG_COLDBOLT __attribute__((deprecated)) = 16;
const int MG_FROSTDIVER __attribute__((deprecated)) = 17;
const int MG_STONECURSE __attribute__((deprecated)) = 18;
const int MG_FIREBALL __attribute__((deprecated)) = 19;
const int MG_FIREWALL __attribute__((deprecated)) = 20;
const int MG_FIREBOLT __attribute__((deprecated)) = 21;
const int MG_LIGHTNINGBOLT __attribute__((deprecated)) = 22;
const int MG_THUNDERSTORM __attribute__((deprecated)) = 23;

const int AL_DP __attribute__((deprecated)) = 24;
const int AL_DEMONBANE __attribute__((deprecated)) = 25;
const int AL_RUWACH __attribute__((deprecated)) = 26;
const int AL_PNEUMA __attribute__((deprecated)) = 27;
const int AL_TELEPORT __attribute__((deprecated)) = 28;
const int AL_WARP __attribute__((deprecated)) = 29;
const int AL_HEAL __attribute__((deprecated)) = 30;
const int AL_INCAGI __attribute__((deprecated)) = 31;
const int AL_DECAGI __attribute__((deprecated)) = 32;
const int AL_HOLYWATER __attribute__((deprecated)) = 33;
const int AL_CRUCIS __attribute__((deprecated)) = 34;
const int AL_ANGELUS __attribute__((deprecated)) = 35;
const int AL_BLESSING __attribute__((deprecated)) = 36;
const int AL_CURE = 37;

const int MC_INCCARRY __attribute__((deprecated)) = 38;
const int MC_DISCOUNT __attribute__((deprecated)) = 39;
const int MC_OVERCHARGE __attribute__((deprecated)) = 40;

const int MC_IDENTIFY __attribute__((deprecated)) = 42;
const int MC_VENDING __attribute__((deprecated)) = 43;
const int MC_MAMMONITE __attribute__((deprecated)) = 44;

const int AC_OWL __attribute__((deprecated)) = 45;
const int AC_VULTURE __attribute__((deprecated)) = 46;
const int AC_CONCENTRATION __attribute__((deprecated)) = 47;
const int AC_DOUBLE __attribute__((deprecated)) = 48;
const int AC_SHOWER __attribute__((deprecated)) = 49;

const int TF_DOUBLE __attribute__((deprecated)) = 50;
const int TF_MISS __attribute__((deprecated)) = 51;
const int TF_STEAL __attribute__((deprecated)) = 52;
const int TF_HIDING __attribute__((deprecated)) = 53;
const int TF_POISON __attribute__((deprecated)) = 54;
const int TF_DETOXIFY __attribute__((deprecated)) = 55;

const int ALL_RESURRECTION __attribute__((deprecated)) = 56;

const int KN_SPEARMASTERY __attribute__((deprecated)) = 57;
const int KN_PIERCE __attribute__((deprecated)) = 58;
const int KN_BRANDISHSPEAR __attribute__((deprecated)) = 59;
const int KN_SPEARSTAB __attribute__((deprecated)) = 60;
const int KN_SPEARBOOMERANG __attribute__((deprecated)) = 61;
const int KN_TWOHANDQUICKEN __attribute__((deprecated)) = 62;
const int KN_AUTOCOUNTER __attribute__((deprecated)) = 63;
const int KN_BOWLINGBASH __attribute__((deprecated)) = 64;
const int KN_RIDING __attribute__((deprecated)) = 65;
const int KN_CAVALIERMASTERY __attribute__((deprecated)) = 66;

const int PR_MACEMASTERY __attribute__((deprecated)) = 67;
const int PR_IMPOSITIO __attribute__((deprecated)) = 68;
const int PR_SUFFRAGIUM __attribute__((deprecated)) = 69;
const int PR_ASPERSIO __attribute__((deprecated)) = 70;
const int PR_BENEDICTIO __attribute__((deprecated)) = 71;
const int PR_SANCTUARY __attribute__((deprecated)) = 72;
const int PR_SLOWPOISON __attribute__((deprecated)) = 73;
const int PR_STRECOVERY __attribute__((deprecated)) = 74;
const int PR_KYRIE __attribute__((deprecated)) = 75;
const int PR_MAGNIFICAT __attribute__((deprecated)) = 76;
const int PR_GLORIA __attribute__((deprecated)) = 77;
const int PR_LEXDIVINA __attribute__((deprecated)) = 78;
const int PR_TURNUNDEAD __attribute__((deprecated)) = 79;
const int PR_LEXAETERNA __attribute__((deprecated)) = 80;
const int PR_MAGNUS __attribute__((deprecated)) = 81;

const int WZ_FIREPILLAR __attribute__((deprecated)) = 82;
const int WZ_SIGHTRASHER __attribute__((deprecated)) = 83;
const int WZ_FIREIVY __attribute__((deprecated)) = 84;
const int WZ_METEOR __attribute__((deprecated)) = 85;
const int WZ_JUPITEL __attribute__((deprecated)) = 86;
const int WZ_VERMILION __attribute__((deprecated)) = 87;
const int WZ_WATERBALL __attribute__((deprecated)) = 88;
const int WZ_ICEWALL __attribute__((deprecated)) = 89;
const int WZ_FROSTNOVA __attribute__((deprecated)) = 90;
const int WZ_STORMGUST __attribute__((deprecated)) = 91;
const int WZ_EARTHSPIKE __attribute__((deprecated)) = 92;
const int WZ_HEAVENDRIVE __attribute__((deprecated)) = 93;
const int WZ_QUAGMIRE __attribute__((deprecated)) = 94;
const int WZ_ESTIMATION __attribute__((deprecated)) = 95;

const int BS_IRON __attribute__((deprecated)) = 96;
const int BS_STEEL __attribute__((deprecated)) = 97;
const int BS_ENCHANTEDSTONE __attribute__((deprecated)) = 98;
const int BS_ORIDEOCON __attribute__((deprecated)) = 99;
const int BS_DAGGER __attribute__((deprecated)) = 100;
const int BS_SWORD __attribute__((deprecated)) = 101;
const int BS_TWOHANDSWORD __attribute__((deprecated)) = 102;
const int BS_AXE __attribute__((deprecated)) = 103;
const int BS_MACE __attribute__((deprecated)) = 104;
const int BS_KNUCKLE __attribute__((deprecated)) = 105;
const int BS_SPEAR __attribute__((deprecated)) = 106;
const int BS_HILTBINDING __attribute__((deprecated)) = 107;
const int BS_FINDINGORE __attribute__((deprecated)) = 108;
const int BS_WEAPONRESEARCH __attribute__((deprecated)) = 109;
const int BS_REPAIRWEAPON __attribute__((deprecated)) = 110;
const int BS_SKINTEMPER __attribute__((deprecated)) = 111;
const int BS_HAMMERFALL __attribute__((deprecated)) = 112;
const int BS_ADRENALINE __attribute__((deprecated)) = 113;
const int BS_WEAPONPERFECT __attribute__((deprecated)) = 114;
const int BS_OVERTHRUST __attribute__((deprecated)) = 115;
const int BS_MAXIMIZE __attribute__((deprecated)) = 116;

const int HT_SKIDTRAP __attribute__((deprecated)) = 117;
const int HT_LANDMINE __attribute__((deprecated)) = 118;
const int HT_ANKLESNARE __attribute__((deprecated)) = 119;
const int HT_SHOCKWAVE __attribute__((deprecated)) = 120;
const int HT_SANDMAN __attribute__((deprecated)) = 121;
const int HT_FLASHER __attribute__((deprecated)) = 122;
const int HT_FREEZINGTRAP __attribute__((deprecated)) = 123;
const int HT_BLASTMINE __attribute__((deprecated)) = 124;
const int HT_CLAYMORETRAP __attribute__((deprecated)) = 125;
const int HT_REMOVETRAP __attribute__((deprecated)) = 126;
const int HT_TALKIEBOX __attribute__((deprecated)) = 127;
const int HT_BEASTBANE __attribute__((deprecated)) = 128;
const int HT_FALCON __attribute__((deprecated)) = 129;
const int HT_STEELCROW __attribute__((deprecated)) = 130;
const int HT_BLITZBEAT __attribute__((deprecated)) = 131;
const int HT_DETECTING __attribute__((deprecated)) = 132;
const int HT_SPRINGTRAP __attribute__((deprecated)) = 133;

const int AS_RIGHT __attribute__((deprecated)) = 134;
const int AS_LEFT __attribute__((deprecated)) = 135;
const int AS_KATAR __attribute__((deprecated)) = 136;
const int AS_CLOAKING __attribute__((deprecated)) = 137;
const int AS_SONICBLOW __attribute__((deprecated)) = 138;
const int AS_GRIMTOOTH __attribute__((deprecated)) = 139;
const int AS_ENCHANTPOISON __attribute__((deprecated)) = 140;
const int AS_POISONREACT __attribute__((deprecated)) = 141;
const int AS_VENOMDUST __attribute__((deprecated)) = 142;
const int AS_SPLASHER __attribute__((deprecated)) = 143;

const int NV_FIRSTAID __attribute__((deprecated)) = 144;
const int NV_TRICKDEAD __attribute__((deprecated)) = 145;
const int SM_MOVINGRECOVERY __attribute__((deprecated)) = 146;
const int SM_FATALBLOW __attribute__((deprecated)) = 147;
const int SM_AUTOBERSERK __attribute__((deprecated)) = 148;
const int AC_MAKINGARROW __attribute__((deprecated)) = 149;
const int AC_CHARGEARROW __attribute__((deprecated)) = 150;
const int TF_SPRINKLESAND __attribute__((deprecated)) = 151;
const int TF_BACKSLIDING __attribute__((deprecated)) = 152;
const int TF_PICKSTONE __attribute__((deprecated)) = 153;
const int TF_THROWSTONE __attribute__((deprecated)) = 154;
const int MC_LOUD __attribute__((deprecated)) = 157;
const int AL_HOLYLIGHT __attribute__((deprecated)) = 158;
const int MG_ENERGYCOAT __attribute__((deprecated)) = 159;

const int NPC_PIERCINGATT __attribute__((deprecated)) = 160;
const int NPC_MENTALBREAKER __attribute__((deprecated)) = 161;
const int NPC_RANGEATTACK __attribute__((deprecated)) = 162;
const int NPC_ATTRICHANGE __attribute__((deprecated)) = 163;
const int NPC_CHANGEWATER __attribute__((deprecated)) = 164;
const int NPC_CHANGEGROUND __attribute__((deprecated)) = 165;
const int NPC_CHANGEFIRE __attribute__((deprecated)) = 166;
const int NPC_CHANGEWIND __attribute__((deprecated)) = 167;
const int NPC_CHANGEPOISON __attribute__((deprecated)) = 168;
const int NPC_CHANGEHOLY __attribute__((deprecated)) = 169;
const int NPC_CHANGEDARKNESS __attribute__((deprecated)) = 170;
const int NPC_CHANGETELEKINESIS __attribute__((deprecated)) = 171;
const int NPC_CRITICALSLASH __attribute__((deprecated)) = 172;
const int NPC_COMBOATTACK __attribute__((deprecated)) = 173;
const int NPC_GUIDEDATTACK __attribute__((deprecated)) = 174;
const int NPC_SELFDESTRUCTION __attribute__((deprecated)) = 175;
const int NPC_SPLASHATTACK __attribute__((deprecated)) = 176;
const int NPC_SUICIDE __attribute__((deprecated)) = 177;
const int NPC_POISON __attribute__((deprecated)) = 178;
const int NPC_BLINDATTACK __attribute__((deprecated)) = 179;
const int NPC_SILENCEATTACK __attribute__((deprecated)) = 180;
const int NPC_STUNATTACK __attribute__((deprecated)) = 181;
const int NPC_PETRIFYATTACK __attribute__((deprecated)) = 182;
const int NPC_CURSEATTACK __attribute__((deprecated)) = 183;
const int NPC_SLEEPATTACK __attribute__((deprecated)) = 184;
const int NPC_RANDOMATTACK __attribute__((deprecated)) = 185;
const int NPC_WATERATTACK __attribute__((deprecated)) = 186;
const int NPC_GROUNDATTACK __attribute__((deprecated)) = 187;
const int NPC_FIREATTACK __attribute__((deprecated)) = 188;
const int NPC_WINDATTACK __attribute__((deprecated)) = 189;
const int NPC_POISONATTACK __attribute__((deprecated)) = 190;
const int NPC_HOLYATTACK __attribute__((deprecated)) = 191;
const int NPC_DARKNESSATTACK __attribute__((deprecated)) = 192;
const int NPC_TELEKINESISATTACK __attribute__((deprecated)) = 193;
const int NPC_MAGICALATTACK __attribute__((deprecated)) = 194;
const int NPC_METAMORPHOSIS __attribute__((deprecated)) = 195;
const int NPC_PROVOCATION __attribute__((deprecated)) = 196;
const int NPC_SMOKING __attribute__((deprecated)) = 197;
const int NPC_SUMMONSLAVE __attribute__((deprecated)) = 198;
const int NPC_EMOTION __attribute__((deprecated)) = 199;
const int NPC_TRANSFORMATION __attribute__((deprecated)) = 200;
const int NPC_BLOODDRAIN __attribute__((deprecated)) = 201;
const int NPC_ENERGYDRAIN __attribute__((deprecated)) = 202;
const int NPC_KEEPING __attribute__((deprecated)) = 203;
const int NPC_DARKBREATH __attribute__((deprecated)) = 204;
const int NPC_DARKBLESSING __attribute__((deprecated)) = 205;
const int NPC_BARRIER __attribute__((deprecated)) = 206;
const int NPC_DEFENDER __attribute__((deprecated)) = 207;
const int NPC_LICK __attribute__((deprecated)) = 208;
const int NPC_HALLUCINATION __attribute__((deprecated)) = 209;
const int NPC_REBIRTH __attribute__((deprecated)) = 210;
const int NPC_SUMMONMONSTER __attribute__((deprecated)) = 211;

const int RG_SNATCHER __attribute__((deprecated)) = 212;
const int RG_STEALCOIN __attribute__((deprecated)) = 213;
const int RG_BACKSTAP __attribute__((deprecated)) = 214;
const int RG_TUNNELDRIVE __attribute__((deprecated)) = 215;
const int RG_RAID __attribute__((deprecated)) = 216;
const int RG_STRIPWEAPON __attribute__((deprecated)) = 217;
const int RG_STRIPSHIELD __attribute__((deprecated)) = 218;
const int RG_STRIPARMOR __attribute__((deprecated)) = 219;
const int RG_STRIPHELM __attribute__((deprecated)) = 220;
const int RG_INTIMIDATE __attribute__((deprecated)) = 221;
const int RG_GRAFFITI __attribute__((deprecated)) = 222;
const int RG_FLAGGRAFFITI __attribute__((deprecated)) = 223;
const int RG_CLEANER __attribute__((deprecated)) = 224;
const int RG_GANGSTER __attribute__((deprecated)) = 225;
const int RG_COMPULSION __attribute__((deprecated)) = 226;
const int RG_PLAGIARISM __attribute__((deprecated)) = 227;

const int AM_AXEMASTERY __attribute__((deprecated)) = 228;
const int AM_LEARNINGPOTION __attribute__((deprecated)) = 229;
const int AM_PHARMACY __attribute__((deprecated)) = 230;
const int AM_DEMONSTRATION __attribute__((deprecated)) = 231;
const int AM_ACIDTERROR __attribute__((deprecated)) = 232;
const int AM_POTIONPITCHER __attribute__((deprecated)) = 233;
const int AM_CANNIBALIZE __attribute__((deprecated)) = 234;
const int AM_SPHEREMINE __attribute__((deprecated)) = 235;
const int AM_CP_WEAPON __attribute__((deprecated)) = 236;
const int AM_CP_SHIELD __attribute__((deprecated)) = 237;
const int AM_CP_ARMOR __attribute__((deprecated)) = 238;
const int AM_CP_HELM __attribute__((deprecated)) = 239;
const int AM_BIOETHICS __attribute__((deprecated)) = 240;
const int AM_BIOTECHNOLOGY __attribute__((deprecated)) = 241;
const int AM_CREATECREATURE __attribute__((deprecated)) = 242;
const int AM_CULTIVATION __attribute__((deprecated)) = 243;
const int AM_FLAMECONTROL __attribute__((deprecated)) = 244;
const int AM_CALLHOMUN __attribute__((deprecated)) = 245;
const int AM_REST __attribute__((deprecated)) = 246;
const int AM_DRILLMASTER __attribute__((deprecated)) = 247;
const int AM_HEALHOMUN __attribute__((deprecated)) = 248;
const int AM_RESURRECTHOMUN __attribute__((deprecated)) = 249;

const int CR_TRUST __attribute__((deprecated)) = 250;
const int CR_AUTOGUARD __attribute__((deprecated)) = 251;
const int CR_SHIELDCHARGE __attribute__((deprecated)) = 252;
const int CR_SHIELDBOOMERANG __attribute__((deprecated)) = 253;
const int CR_REFLECTSHIELD __attribute__((deprecated)) = 254;
const int CR_HOLYCROSS __attribute__((deprecated)) = 255;
const int CR_GRANDCROSS __attribute__((deprecated)) = 256;
const int CR_DEVOTION __attribute__((deprecated)) = 257;
const int CR_PROVIDENCE __attribute__((deprecated)) = 258;
const int CR_DEFENDER __attribute__((deprecated)) = 259;
const int CR_SPEARQUICKEN __attribute__((deprecated)) = 260;

const int MO_IRONHAND __attribute__((deprecated)) = 261;
const int MO_SPIRITSRECOVERY __attribute__((deprecated)) = 262;
const int MO_CALLSPIRITS __attribute__((deprecated)) = 263;
const int MO_ABSORBSPIRITS __attribute__((deprecated)) = 264;
const int MO_TRIPLEATTACK __attribute__((deprecated)) = 265;
const int MO_BODYRELOCATION __attribute__((deprecated)) = 266;
const int MO_DODGE __attribute__((deprecated)) = 267;
const int MO_INVESTIGATE __attribute__((deprecated)) = 268;
const int MO_FINGEROFFENSIVE __attribute__((deprecated)) = 269;
const int MO_STEELBODY __attribute__((deprecated)) = 270;
const int MO_BLADESTOP __attribute__((deprecated)) = 271;
const int MO_EXPLOSIONSPIRITS __attribute__((deprecated)) = 272;
const int MO_EXTREMITYFIST __attribute__((deprecated)) = 273;
const int MO_CHAINCOMBO __attribute__((deprecated)) = 274;
const int MO_COMBOFINISH __attribute__((deprecated)) = 275;

const int SA_ADVANCEDBOOK __attribute__((deprecated)) = 276;
const int SA_CASTCANCEL __attribute__((deprecated)) = 277;
const int SA_MAGICROD __attribute__((deprecated)) = 278;
const int SA_SPELLBREAKER __attribute__((deprecated)) = 279;
const int SA_FREECAST __attribute__((deprecated)) = 280;
const int SA_AUTOSPELL __attribute__((deprecated)) = 281;
const int SA_FLAMELAUNCHER __attribute__((deprecated)) = 282;
const int SA_FROSTWEAPON __attribute__((deprecated)) = 283;
const int SA_LIGHTNINGLOADER __attribute__((deprecated)) = 284;
const int SA_SEISMICWEAPON __attribute__((deprecated)) = 285;
const int SA_DRAGONOLOGY __attribute__((deprecated)) = 286;
const int SA_VOLCANO __attribute__((deprecated)) = 287;
const int SA_DELUGE __attribute__((deprecated)) = 288;
const int SA_VIOLENTGALE __attribute__((deprecated)) = 289;
const int SA_LANDPROTECTOR __attribute__((deprecated)) = 290;
const int SA_DISPELL __attribute__((deprecated)) = 291;
const int SA_ABRACADABRA __attribute__((deprecated)) = 292;
const int SA_MONOCELL __attribute__((deprecated)) = 293;
const int SA_CLASSCHANGE __attribute__((deprecated)) = 294;
const int SA_SUMMONMONSTER __attribute__((deprecated)) = 295;
const int SA_REVERSEORCISH __attribute__((deprecated)) = 296;
const int SA_DEATH __attribute__((deprecated)) = 297;
const int SA_FORTUNE __attribute__((deprecated)) = 298;
const int SA_TAMINGMONSTER __attribute__((deprecated)) = 299;
const int SA_QUESTION __attribute__((deprecated)) = 300;
const int SA_GRAVITY __attribute__((deprecated)) = 301;
const int SA_LEVELUP __attribute__((deprecated)) = 302;
const int SA_INSTANTDEATH __attribute__((deprecated)) = 303;
const int SA_FULLRECOVERY __attribute__((deprecated)) = 304;
const int SA_COMA __attribute__((deprecated)) = 305;

const int BD_ADAPTATION __attribute__((deprecated)) = 306;
const int BD_ENCORE __attribute__((deprecated)) = 307;
const int BD_LULLABY __attribute__((deprecated)) = 308;
const int BD_RICHMANKIM __attribute__((deprecated)) = 309;
const int BD_ETERNALCHAOS __attribute__((deprecated)) = 310;
const int BD_DRUMBATTLEFIELD __attribute__((deprecated)) = 311;
const int BD_RINGNIBELUNGEN __attribute__((deprecated)) = 312;
const int BD_ROKISWEIL __attribute__((deprecated)) = 313;
const int BD_INTOABYSS __attribute__((deprecated)) = 314;
const int BD_SIEGFRIED __attribute__((deprecated)) = 315;
const int BD_RAGNAROK __attribute__((deprecated)) = 316;

const int BA_MUSICALLESSON __attribute__((deprecated)) = 317;
const int BA_MUSICALSTRIKE __attribute__((deprecated)) = 318;
const int BA_DISSONANCE __attribute__((deprecated)) = 319;
const int BA_FROSTJOKE __attribute__((deprecated)) = 320;
const int BA_WHISTLE __attribute__((deprecated)) = 321;
const int BA_ASSASSINCROSS __attribute__((deprecated)) = 322;
const int BA_POEMBRAGI __attribute__((deprecated)) = 323;
const int BA_APPLEIDUN __attribute__((deprecated)) = 324;

const int DC_DANCINGLESSON __attribute__((deprecated)) = 325;
const int DC_THROWARROW __attribute__((deprecated)) = 326;
const int DC_UGLYDANCE __attribute__((deprecated)) = 327;
const int DC_SCREAM __attribute__((deprecated)) = 328;
const int DC_HUMMING __attribute__((deprecated)) = 329;
const int DC_DONTFORGETME __attribute__((deprecated)) = 330;
const int DC_FORTUNEKISS __attribute__((deprecated)) = 331;
const int DC_SERVICEFORYOU __attribute__((deprecated)) = 332;

const int NPC_SELFDESTRUCTION2 __attribute__((deprecated)) = 333;

const int WE_MALE __attribute__((deprecated)) = 334;
const int WE_FEMALE __attribute__((deprecated)) = 335;
const int WE_CALLPARTNER __attribute__((deprecated)) = 336;

const int NPC_DARKCROSS __attribute__((deprecated)) = 338;

const int TMW_SKILLPOOL = 339;        // skill pool size

const int TMW_MAGIC = 340;
const int TMW_MAGIC_LIFE = 341;
const int TMW_MAGIC_WAR = 342;
const int TMW_MAGIC_TRANSMUTE = 343;
const int TMW_MAGIC_NATURE = 344;
const int TMW_MAGIC_ETHER = 345;
const int TMW_MAGIC_DARK __attribute__((deprecated)) = 346;
const int TMW_MAGIC_LIGHT __attribute__((deprecated)) = 347;

const int TMW_BRAWLING = 350;
const int TMW_LUCKY_COUNTER = 351;
const int TMW_SPEED = 352;
const int TMW_RESIST_POISON = 353;
const int TMW_ASTRAL_SOUL = 354;
const int TMW_RAGING = 355;

const int LK_AURABLADE __attribute__((deprecated)) = 356;
const int LK_PARRYING __attribute__((deprecated)) = 357;
const int LK_CONCENTRATION __attribute__((deprecated)) = 358;
const int LK_TENSIONRELAX __attribute__((deprecated)) = 359;
const int LK_BERSERK __attribute__((deprecated)) = 360;
const int LK_FURY __attribute__((deprecated)) = 361;
const int HP_ASSUMPTIO __attribute__((deprecated)) = 362;
const int HP_BASILICA __attribute__((deprecated)) = 363;
const int HP_MEDITATIO __attribute__((deprecated)) = 364;
const int HW_SOULDRAIN __attribute__((deprecated)) = 365;
const int HW_MAGICCRASHER __attribute__((deprecated)) = 366;
const int HW_MAGICPOWER __attribute__((deprecated)) = 367;
const int PA_PRESSURE __attribute__((deprecated)) = 368;
const int PA_SACRIFICE __attribute__((deprecated)) = 369;
const int PA_GOSPEL __attribute__((deprecated)) = 370;
const int CH_PALMSTRIKE __attribute__((deprecated)) = 371;
const int CH_TIGERFIST __attribute__((deprecated)) = 372;
const int CH_CHAINCRUSH __attribute__((deprecated)) = 373;
const int PF_HPCONVERSION __attribute__((deprecated)) = 374;
const int PF_SOULCHANGE __attribute__((deprecated)) = 375;
const int PF_SOULBURN __attribute__((deprecated)) = 376;
const int ASC_KATAR __attribute__((deprecated)) = 377;
const int ASC_HALLUCINATION __attribute__((deprecated)) = 378;
const int ASC_EDP __attribute__((deprecated)) = 379;
const int ASC_BREAKER __attribute__((deprecated)) = 380;
const int SN_SIGHT __attribute__((deprecated)) = 381;
const int SN_FALCONASSAULT __attribute__((deprecated)) = 382;
const int SN_SHARPSHOOTING __attribute__((deprecated)) = 383;
const int SN_WINDWALK __attribute__((deprecated)) = 384;
const int WS_MELTDOWN __attribute__((deprecated)) = 385;
const int WS_CREATECOIN __attribute__((deprecated)) = 386;
const int WS_CREATENUGGET __attribute__((deprecated)) = 387;
const int WS_SYSTEMCREATE __attribute__((deprecated)) = 389;
const int ST_CHASEWALK __attribute__((deprecated)) = 390;
const int ST_REJECTSWORD __attribute__((deprecated)) = 391;
const int ST_STEALBACKPACK __attribute__((deprecated)) = 392;
const int CR_ALCHEMY __attribute__((deprecated)) = 393;
const int CR_SYNTHESISPOTION __attribute__((deprecated)) = 394;
const int CG_ARROWVULCAN __attribute__((deprecated)) = 395;
const int CG_MOONLIT __attribute__((deprecated)) = 396;
const int CG_MARIONETTE __attribute__((deprecated)) = 397;
const int LK_SPIRALPIERCE __attribute__((deprecated)) = 398;
const int LK_HEADCRUSH __attribute__((deprecated)) = 399;
const int LK_JOINTBEAT __attribute__((deprecated)) = 400;
const int HW_NAPALMVULCAN __attribute__((deprecated)) = 401;
const int CH_SOULCOLLECT __attribute__((deprecated)) = 402;
const int PF_MINDBREAKER __attribute__((deprecated)) = 403;
const int PF_MEMORIZE __attribute__((deprecated)) = 404;
const int PF_FOGWALL __attribute__((deprecated)) = 405;
const int PF_SPIDERWEB __attribute__((deprecated)) = 406;
const int ASC_METEORASSAULT __attribute__((deprecated)) = 407;
const int ASC_CDP __attribute__((deprecated)) = 408;
const int WE_BABY __attribute__((deprecated)) = 409;
const int WE_CALLPARENT __attribute__((deprecated)) = 410;
const int WE_CALLBABY __attribute__((deprecated)) = 411;
const int TK_RUN __attribute__((deprecated)) = 412;
const int TK_READYSTORM __attribute__((deprecated)) = 413;
const int TK_STORMKICK __attribute__((deprecated)) = 414;
const int TK_READYDOWN __attribute__((deprecated)) = 415;
const int TK_DOWNKICK __attribute__((deprecated)) = 416;
const int TK_READYTURN __attribute__((deprecated)) = 417;
const int TK_TURNKICK __attribute__((deprecated)) = 418;
const int TK_READYCOUNTER __attribute__((deprecated)) = 419;
const int TK_COUNTER __attribute__((deprecated)) = 420;
const int TK_DODGE __attribute__((deprecated)) = 421;
const int TK_JUMPKICK __attribute__((deprecated)) = 422;
const int TK_HPTIME __attribute__((deprecated)) = 423;
const int TK_SPTIME __attribute__((deprecated)) = 424;
const int TK_POWER __attribute__((deprecated)) = 425;
const int TK_SEVENWIND __attribute__((deprecated)) = 426;
const int TK_HIGHJUMP __attribute__((deprecated)) = 427;
const int SG_FEEL __attribute__((deprecated)) = 428;
const int SG_SUN_WARM __attribute__((deprecated)) = 429;
const int SG_MOON_WARM __attribute__((deprecated)) = 430;
const int SG_STAR_WARM __attribute__((deprecated)) = 431;
const int SG_SUN_COMFORT __attribute__((deprecated)) = 432;
const int SG_MOON_COMFORT __attribute__((deprecated)) = 433;
const int SG_STAR_COMFORT __attribute__((deprecated)) = 434;
const int SG_HATE __attribute__((deprecated)) = 435;
const int SG_SUN_ANGER __attribute__((deprecated)) = 436;
const int SG_MOON_ANGER __attribute__((deprecated)) = 437;
const int SG_STAR_ANGER __attribute__((deprecated)) = 438;
const int SG_SUN_BLESS __attribute__((deprecated)) = 439;
const int SG_MOON_BLESS __attribute__((deprecated)) = 440;
const int SG_STAR_BLESS __attribute__((deprecated)) = 441;
const int SG_DEVIL __attribute__((deprecated)) = 442;
const int SG_FRIEND __attribute__((deprecated)) = 443;
const int SG_KNOWLEDGE __attribute__((deprecated)) = 444;
const int SG_FUSION __attribute__((deprecated)) = 445;
const int SL_ALCHEMIST __attribute__((deprecated)) = 446;
const int AM_BERSERKPITCHER __attribute__((deprecated)) = 447;
const int SL_MONK __attribute__((deprecated)) = 448;
const int SL_STAR __attribute__((deprecated)) = 449;
const int SL_SAGE __attribute__((deprecated)) = 450;
const int SL_CRUSADER __attribute__((deprecated)) = 451;
const int SL_SUPERNOVICE __attribute__((deprecated)) = 452;
const int SL_KNIGHT __attribute__((deprecated)) = 453;
const int SL_WIZARD __attribute__((deprecated)) = 454;
const int SL_PRIEST __attribute__((deprecated)) = 455;
const int SL_BARDDANCER __attribute__((deprecated)) = 456;
const int SL_ROGUE __attribute__((deprecated)) = 457;
const int SL_ASSASIN __attribute__((deprecated)) = 458;
const int SL_BLACKSMITH __attribute__((deprecated)) = 459;
const int BS_ADRENALINE2 __attribute__((deprecated)) = 460;
const int SL_HUNTER __attribute__((deprecated)) = 461;
const int SL_SOULLINKER __attribute__((deprecated)) = 462;
const int SL_KAIZEL __attribute__((deprecated)) = 463;
const int SL_KAAHI __attribute__((deprecated)) = 464;
const int SL_KAUPE __attribute__((deprecated)) = 465;
const int SL_KAITE __attribute__((deprecated)) = 466;
const int SL_KAINA __attribute__((deprecated)) = 467;
const int SL_STIN __attribute__((deprecated)) = 468;
const int SL_STUN __attribute__((deprecated)) = 469;
const int SL_SMA __attribute__((deprecated)) = 470;
const int SL_SWOO __attribute__((deprecated)) = 471;
const int SL_SKE __attribute__((deprecated)) = 472;
const int SL_SKA __attribute__((deprecated)) = 473;

// [Fate] Skill pools API

// Max. # of active entries in the skill pool
#define MAX_SKILL_POOL 3
// Max. # of skills that may be classified as pool skills in db/skill_db.txt
#define MAX_POOL_SKILLS 128

extern int skill_pool_skills[MAX_POOL_SKILLS];  // All pool skills
extern int skill_pool_skills_size;  // Number of entries in skill_pool_skills

void skill_pool_register (int id);   // [Fate] Remember that a certain skill ID belongs to a pool skill
int  skill_pool (struct map_session_data *sd, int *skills); // Yields all active skills in the skill pool; no more than MAX_SKILL_POOL.  Return is number of skills.
int  skill_pool_size (struct map_session_data *sd);
int  skill_pool_max (struct map_session_data *sd);  // Max. number of pool skills
void skill_pool_empty (struct map_session_data *sd);    // Deactivate all pool skills
int  skill_pool_activate (struct map_session_data *sd, int skill);  // Skill into skill pool.  Return is zero iff okay.
int  skill_pool_is_activated (struct map_session_data *sd, int skill);  // Skill into skill pool.  Return is zero when activated.
int  skill_pool_deactivate (struct map_session_data *sd, int skill);    // Skill out of skill pool.  Return is zero iff okay.
const char *skill_name (int skill);   // Yield configurable skill name
int  skill_stat (int skill);    // Yields the stat associated with a skill.  Returns zero if none, or SP_STR, SP_VIT, ... otherwise
int  skill_power (struct map_session_data *sd, int skill);  // Yields the power of a skill.  This is zero if the skill is unknown or if it's a pool skill that is outside of the skill pool,
                             // otherwise a value from 0 to 255 (with 200 being the `normal maximum')
int  skill_power_bl (struct block_list *bl, int skill); // Yields the power of a skill.  This is zero if the skill is unknown or if it's a pool skill that is outside of the skill pool,
                             // otherwise a value from 0 to 255 (with 200 being the `normal maximum')

#endif // SKILL_H
