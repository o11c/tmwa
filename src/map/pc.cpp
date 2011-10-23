#include "pc.hpp"

#include "../common/mt_rand.hpp"
#include "../common/nullpo.hpp"
#include "../common/timer.hpp"
#include "../common/utils.hpp"

#include "battle.hpp"
#include "chrif.hpp"
#include "clif.hpp"
#include "itemdb.hpp"
#include "magic-stmt.hpp"
#include "main.hpp"
#include "npc.hpp"
#include "party.hpp"
#include "path.hpp"
#include "script.hpp"
#include "skill.hpp"
#include "storage.hpp"
#include "trade.hpp"

constexpr std::chrono::seconds PVP_CALCRANK_INTERVAL(1);

//define it here, since the ifdef only occurs in this file
#define USE_ASTRAL_SOUL_SKILL

#define STATE_BLIND 0x10

#ifdef USE_ASTRAL_SOUL_SKILL
#define MAGIC_SKILL_THRESHOLD 200   // [fate] At this threshold, the Astral Soul skill kicks in
#endif

#define MAP_LOG_STATS(sd, suffix)       \
        MAP_LOG_PC(sd, "STAT %d %d %d %d %d %d " suffix,            \
                   sd->status.stats[ATTR::STR], sd->status.stats[ATTR::AGI], sd->status.stats[ATTR::VIT], sd->status.stats[ATTR::INT], sd->status.stats[ATTR::DEX], sd->status.stats[ATTR::LUK])

#define MAP_LOG_XP(sd, suffix)  \
        MAP_LOG_PC(sd, "XP %d %d JOB %d %d %d ZENY %d + %d " suffix,            \
                   sd->status.base_level, sd->status.base_exp, sd->status.job_level, sd->status.job_exp, sd->status.skill_point,  sd->status.zeny, pc_readaccountreg(sd, std::string("BankAccount")))

#define MAP_LOG_MAGIC(sd, suffix)       \
        MAP_LOG_PC(sd, "MAGIC %d %d %d %d %d %d EXP %d %d " suffix,     \
                   sd->status.skill[TMW_MAGIC].lv,                      \
                   sd->status.skill[TMW_MAGIC_LIFE].lv,                 \
                   sd->status.skill[TMW_MAGIC_WAR].lv,                  \
                   sd->status.skill[TMW_MAGIC_TRANSMUTE].lv,            \
                   sd->status.skill[TMW_MAGIC_NATURE].lv,               \
                   sd->status.skill[TMW_MAGIC_ETHER].lv,                \
                   pc_readglobalreg(sd, std::string("MAGIC_EXPERIENCE")) & 0xffff,   \
                   (pc_readglobalreg(sd, std::string("MAGIC_EXPERIENCE")) >> 24) & 0xff)

static sint32 pc_isequip(MapSessionData *sd, sint32 n);
static sint32 pc_checkoverhp(MapSessionData *);
static sint32 pc_checkoversp(MapSessionData *);
static sint32 pc_can_reach(MapSessionData *, sint32, sint32);
static sint32 pc_checkbaselevelup(MapSessionData *sd);
static sint32 pc_checkjoblevelup(MapSessionData *sd);
static sint32 pc_nextbaseafter(MapSessionData *);
static sint32 pc_nextjobafter(MapSessionData *);
static sint32 pc_calc_pvprank(MapSessionData *sd);
static charid_t pc_ismarried(MapSessionData *sd);

__attribute__((deprecated))
constexpr static sint32 max_weight_base = 20000;
__attribute__((deprecated))
constexpr static sint32 hp_coefficient = 0;
__attribute__((deprecated))
constexpr static sint32 hp_coefficient2 = 500;
__attribute__((deprecated))
constexpr static sint32 hp_sigma_val[256] = {}; // 0, because it's calculated from hp_coefficient
__attribute__((deprecated))
constexpr static sint32 sp_coefficient = 100;

constexpr static std::chrono::milliseconds aspd_base[17] =
{
    std::chrono::milliseconds(650),
    std::chrono::milliseconds(700),
    std::chrono::milliseconds(750),
    std::chrono::milliseconds(600),
    std::chrono::milliseconds(2000),
    std::chrono::milliseconds(2000),
    std::chrono::milliseconds(800),
    std::chrono::milliseconds(2000),
    std::chrono::milliseconds(700),
    std::chrono::milliseconds(700),
    std::chrono::milliseconds(650),
    std::chrono::milliseconds(900),
    std::chrono::milliseconds(2000),
    std::chrono::milliseconds(2000),
    std::chrono::milliseconds(2000),
    std::chrono::milliseconds(2000),
    std::chrono::milliseconds(2000),
};
__attribute__((deprecated))
constexpr static char job_bonus[256] =
{
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

__attribute__((deprecated))
constexpr static sint32 exp_table_old[14][256]
{
    {9,16,25,36,77,112,153,200,253,320,385,490,585,700,830,970,1120,1260,1420,1620,1860,1990,2240,2504,2950,3426,3934,4474,6889,7995,9174,10425,11748,13967,15775,17678,19677,21773,30543,34212,38065,42102,46323,53026,58419,64041,69892,75973,102468,115254,128692,142784,157528,178184,196300,215198,234879,255341,330188,365914,403224,442116,482590,536948,585191,635278,687211,740988,925400,1473746,1594058,1718928,1848355,1982340,2230113,2386162,2547417,2713878,3206160,3681024,4022472,4377024,4744680,5125440,5767272,6204000,6655464,7121664,7602600,9738720,11649960,13643520,18339300,23836800,35658000,48687000,58135000,99999999,0},
    {9,16,25,36,77,112,153,200,253,320,385,490,585,700,830,970,1120,1260,1420,1620,1860,1990,2240,2504,2950,3426,3934,4474,6889,7995,9174,10425,11748,13967,15775,17678,19677,21773,30543,34212,38065,42102,46323,53026,58419,64041,69892,75973,102468,115254,128692,142784,157528,178184,196300,215198,234879,255341,330188,365914,403224,442116,482590,536948,585191,635278,687211,740988,925400,1473746,1594058,1718928,1848355,1982340,2230113,2386162,2547417,2713878,3206160,3681024,4022472,4377024,4744680,5125440,5767272,6204000,6655464,7121664,7602600,9738720,11649960,13643520,18339300,23836800,35658000,48687000,58135000,99999999,0},
    {9,16,25,36,77,112,153,200,253,320,385,490,585,700,830,970,1120,1260,1420,1620,1860,1990,2240,2504,2950,3426,3934,4474,6889,7995,9174,10425,11748,13967,15775,17678,19677,21773,30543,34212,38065,42102,46323,53026,58419,64041,69892,75973,102468,115254,128692,142784,157528,178184,196300,215198,234879,255341,330188,365914,403224,442116,482590,536948,585191,635278,687211,740988,925400,1473746,1594058,1718928,1848355,1982340,2230113,2386162,2547417,2713878,3206160,3681024,4022472,4377024,4744680,5125440,5767272,6204000,6655464,7121664,7602600,9738720,11649960,13643520,18339300,23836800,35658000,48687000,58135000,99999999,0},
    {9,16,25,36,77,112,153,200,253,320,385,490,585,700,830,970,1120,1260,1420,1620,1860,1990,2240,2504,2950,3426,3934,4474,6889,7995,9174,10425,11748,13967,15775,17678,19677,21773,30543,34212,38065,42102,46323,53026,58419,64041,69892,75973,102468,115254,128692,142784,157528,178184,196300,215198,234879,255341,330188,365914,403224,442116,482590,536948,585191,635278,687211,740988,925400,1473746,1594058,1718928,1848355,1982340,2230113,2386162,2547417,2713878,3206160,3681024,4022472,4377024,4744680,5125440,5767272,6204000,6655464,7121664,7602600,9738720,11649960,13643520,18339300,23836800,35658000,48687000,58135000,99999999,0},
    {10,18,28,40,85,123,168,220,278,400,41,613,731,875,1038,1213,1400,1575,1775,2268,2604,2786,3136,3506,4130,4796,5508,6264,9645,12392,14220,16159,18209,21649,24451,27401,30499,33748,47342,58160,64711,71573,78749,90144,99312,108870,118816,129154,174196,213220,238080,264150,291427,329640,363155,398116,434526,472381,610848,731828,806448,884232,965180,1073896,1170382,1270556,1374422,1481976,1850800,3389616,3666333,3953534,4251217,4559382,5129260,5488173,5859059,6241919,7374168,9570662,10458427,11380262,12336168,13326144,14994907,16130400,17304200,18516326,19766760,29216160,34949880,40930560,55017900,71510400,106974000,146061000,174405000,200000000,0},
    {10,18,28,40,85,123,168,220,278,400,41,613,731,875,1038,1213,1400,1575,1775,2268,2604,2786,3136,3506,4130,4796,5508,6264,9645,12392,14220,16159,18209,21649,24451,27401,30499,33748,47342,58160,64711,71573,78749,90144,99312,108870,118816,129154,174196,213220,238080,264150,291427,329640,363155,398116,434526,472381,610848,731828,806448,884232,965180,1073896,1170382,1270556,1374422,1481976,1850800,3389616,3666333,3953534,4251217,4559382,5129260,5488173,5859059,6241919,7374168,9570662,10458427,11380262,12336168,13326144,14994907,16130400,17304200,18516326,19766760,29216160,34949880,40930560,55017900,71510400,106974000,146061000,174405000,200000000,0},
    {10,18,28,40,85,123,168,220,278,400,41,613,731,875,1038,1213,1400,1575,1775,2268,2604,2786,3136,3506,4130,4796,5508,6264,9645,12392,14220,16159,18209,21649,24451,27401,30499,33748,47342,58160,64711,71573,78749,90144,99312,108870,118816,129154,174196,213220,238080,264150,291427,329640,363155,398116,434526,472381,610848,731828,806448,884232,965180,1073896,1170382,1270556,1374422,1481976,1850800,3389616,3666333,3953534,4251217,4559382,5129260,5488173,5859059,6241919,7374168,9570662,10458427,11380262,12336168,13326144,14994907,16130400,17304200,18516326,19766760,29216160,34949880,40930560,55017900,71510400,106974000,146061000,174405000,200000000,0},
    {10,18,28,40,91,151,205,268,340,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {30,43,58,76,116,180,220,272,336,520,604,699,802,948,1125,1668,1937,2226,3040,3988,5564,6272,7021,9114,11473,15290,16891,18570,23229,28359,36478,39716,43088,52417,62495,78160,84175,90404,107611,125915,153941,191781,204351,248352,286212,386371,409795,482092,509596,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {144,184,284,348,603,887,1096,1598,2540,3676,4290,4946,6679,9492,12770,14344,16005,20642,27434,35108,38577,42206,52708,66971,82688,89544,96669,117821,144921,174201,186677,199584,238617,286366,337147,358435,380376,447685,526989,610246,644736,793535,921810,1106758,1260955,1487304,1557657,1990632,2083386,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {30,43,58,76,116,180,220,272,336,520,604,699,802,948,1125,1668,1937,2226,3040,3988,5564,6272,7021,9114,11473,15290,16891,18570,23229,28359,36478,39716,43088,52417,62495,78160,84175,90404,107611,125915,153941,191781,204351,248352,286212,386371,409795,482092,509596,982092,992092,1002092,1012092,1022092,1032092,1042092,1052092,1062092,1072092,1082092,1092092,1102092,1112092,1122092,1132092,1142092,1152092,1162092,1172092,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {11,20,31,44,100,166,226,295,374,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {60,86,116,152,232,360,440,544,672,1040,1208,1398,1604,1896,2250,3336,3874,4452,6080,7976,11128,12544,14042,18228,28683,38225,42228,46425,58073,70898,91195,99290,107720,131043,156238,195408,210430,226010,269028,314788,384853,479453,510878,620880,715530,965928,1024488,1205230,1273990,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {288,368,568,696,1206,1774,2192,3196,5080,7352,8580,9892,13358,18984,31925,35860,40013,51605,68585,87770,96443,105515,131770,167428,206720,223860,241673,294553,362303,479053,513362,548856,656197,787507,927154,985696,1046034,1231134,1449220,1678177,1773024,2182221,2534978,3043585,3782865,4461912,4672971,5971896,6250158,6875174,7562691,8318960,9150856,10065942,11877812,14015818,16538655,19515624,23028437,28094693,34275525,41816141,51015692,62239144,79666104,101972614,130524946,167071930,213852071,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
};

constexpr static sint32 exp_table_new[256][14] =
{
    {9,9,9,9,10,10,10,10,30,144,30,11,60,288},
    {16,16,16,16,18,18,18,18,43,184,43,20,86,368},
    {25,25,25,25,28,28,28,28,58,284,58,31,116,568},
    {36,36,36,36,40,40,40,40,76,348,76,44,152,696},
    {77,77,77,77,85,85,85,91,116,603,116,100,232,1206},
    {112,112,112,112,123,123,123,151,180,887,180,166,360,1774},
    {153,153,153,153,168,168,168,205,220,1096,220,226,440,2192},
    {200,200,200,200,220,220,220,268,272,1598,272,295,544,3196},
    {253,253,253,253,278,278,278,340,336,2540,336,374,672,5080},
    {320,320,320,320,400,400,400,0,520,3676,520,0,1040,7352},
    {385,385,385,385,41,41,41,0,604,4290,604,0,1208,8580},
    {490,490,490,490,613,613,613,0,699,4946,699,0,1398,9892},
    {585,585,585,585,731,731,731,0,802,6679,802,0,1604,13358},
    {700,700,700,700,875,875,875,0,948,9492,948,0,1896,18984},
    {830,830,830,830,1038,1038,1038,0,1125,12770,1125,0,2250,31925},
    {970,970,970,970,1213,1213,1213,0,1668,14344,1668,0,3336,35860},
    {1120,1120,1120,1120,1400,1400,1400,0,1937,16005,1937,0,3874,40013},
    {1260,1260,1260,1260,1575,1575,1575,0,2226,20642,2226,0,4452,51605},
    {1420,1420,1420,1420,1775,1775,1775,0,3040,27434,3040,0,6080,68585},
    {1620,1620,1620,1620,2268,2268,2268,0,3988,35108,3988,0,7976,87770},
    {1860,1860,1860,1860,2604,2604,2604,0,5564,38577,5564,0,11128,96443},
    {1990,1990,1990,1990,2786,2786,2786,0,6272,42206,6272,0,12544,105515},
    {2240,2240,2240,2240,3136,3136,3136,0,7021,52708,7021,0,14042,131770},
    {2504,2504,2504,2504,3506,3506,3506,0,9114,66971,9114,0,18228,167428},
    {2950,2950,2950,2950,4130,4130,4130,0,11473,82688,11473,0,28683,206720},
    {3426,3426,3426,3426,4796,4796,4796,0,15290,89544,15290,0,38225,223860},
    {3934,3934,3934,3934,5508,5508,5508,0,16891,96669,16891,0,42228,241673},
    {4474,4474,4474,4474,6264,6264,6264,0,18570,117821,18570,0,46425,294553},
    {6889,6889,6889,6889,9645,9645,9645,0,23229,144921,23229,0,58073,362303},
    {7995,7995,7995,7995,12392,12392,12392,0,28359,174201,28359,0,70898,479053},
    {9174,9174,9174,9174,14220,14220,14220,0,36478,186677,36478,0,91195,513362},
    {10425,10425,10425,10425,16159,16159,16159,0,39716,199584,39716,0,99290,548856},
    {11748,11748,11748,11748,18209,18209,18209,0,43088,238617,43088,0,107720,656197},
    {13967,13967,13967,13967,21649,21649,21649,0,52417,286366,52417,0,131043,787507},
    {15775,15775,15775,15775,24451,24451,24451,0,62495,337147,62495,0,156238,927154},
    {17678,17678,17678,17678,27401,27401,27401,0,78160,358435,78160,0,195408,985696},
    {19677,19677,19677,19677,30499,30499,30499,0,84175,380376,84175,0,210430,1046034},
    {21773,21773,21773,21773,33748,33748,33748,0,90404,447685,90404,0,226010,1231134},
    {30543,30543,30543,30543,47342,47342,47342,0,107611,526989,107611,0,269028,1449220},
    {34212,34212,34212,34212,58160,58160,58160,0,125915,610246,125915,0,314788,1678177},
    {38065,38065,38065,38065,64711,64711,64711,0,153941,644736,153941,0,384853,1773024},
    {42102,42102,42102,42102,71573,71573,71573,0,191781,793535,191781,0,479453,2182221},
    {46323,46323,46323,46323,78749,78749,78749,0,204351,921810,204351,0,510878,2534978},
    {53026,53026,53026,53026,90144,90144,90144,0,248352,1106758,248352,0,620880,3043585},
    {58419,58419,58419,58419,99312,99312,99312,0,286212,1260955,286212,0,715530,3782865},
    {64041,64041,64041,64041,108870,108870,108870,0,386371,1487304,386371,0,965928,4461912},
    {69892,69892,69892,69892,118816,118816,118816,0,409795,1557657,409795,0,1024488,4672971},
    {75973,75973,75973,75973,129154,129154,129154,0,482092,1990632,482092,0,1205230,5971896},
    {102468,102468,102468,102468,174196,174196,174196,0,509596,2083386,509596,0,1273990,6250158},
    {115254,115254,115254,115254,213220,213220,213220,0,0,0,982092,0,0,6875174},
    {128692,128692,128692,128692,238080,238080,238080,0,0,0,992092,0,0,7562691},
    {142784,142784,142784,142784,264150,264150,264150,0,0,0,1002092,0,0,8318960},
    {157528,157528,157528,157528,291427,291427,291427,0,0,0,1012092,0,0,9150856},
    {178184,178184,178184,178184,329640,329640,329640,0,0,0,1022092,0,0,10065942},
    {196300,196300,196300,196300,363155,363155,363155,0,0,0,1032092,0,0,11877812},
    {215198,215198,215198,215198,398116,398116,398116,0,0,0,1042092,0,0,14015818},
    {234879,234879,234879,234879,434526,434526,434526,0,0,0,1052092,0,0,16538655},
    {255341,255341,255341,255341,472381,472381,472381,0,0,0,1062092,0,0,19515624},
    {330188,330188,330188,330188,610848,610848,610848,0,0,0,1072092,0,0,23028437},
    {365914,365914,365914,365914,731828,731828,731828,0,0,0,1082092,0,0,28094693},
    {403224,403224,403224,403224,806448,806448,806448,0,0,0,1092092,0,0,34275525},
    {442116,442116,442116,442116,884232,884232,884232,0,0,0,1102092,0,0,41816141},
    {482590,482590,482590,482590,965180,965180,965180,0,0,0,1112092,0,0,51015692},
    {536948,536948,536948,536948,1073896,1073896,1073896,0,0,0,1122092,0,0,62239144},
    {585191,585191,585191,585191,1170382,1170382,1170382,0,0,0,1132092,0,0,79666104},
    {635278,635278,635278,635278,1270556,1270556,1270556,0,0,0,1142092,0,0,101972614},
    {687211,687211,687211,687211,1374422,1374422,1374422,0,0,0,1152092,0,0,130524946},
    {740988,740988,740988,740988,1481976,1481976,1481976,0,0,0,1162092,0,0,167071930},
    {925400,925400,925400,925400,1850800,1850800,1850800,0,0,0,1172092,0,0,213852071},
    {1473746,1473746,1473746,1473746,3389616,3389616,3389616,0,0,0,0,0,0,0},
    {1594058,1594058,1594058,1594058,3666333,3666333,3666333,0,0,0,0,0,0,0},
    {1718928,1718928,1718928,1718928,3953534,3953534,3953534,0,0,0,0,0,0,0},
    {1848355,1848355,1848355,1848355,4251217,4251217,4251217,0,0,0,0,0,0,0},
    {1982340,1982340,1982340,1982340,4559382,4559382,4559382,0,0,0,0,0,0,0},
    {2230113,2230113,2230113,2230113,5129260,5129260,5129260,0,0,0,0,0,0,0},
    {2386162,2386162,2386162,2386162,5488173,5488173,5488173,0,0,0,0,0,0,0},
    {2547417,2547417,2547417,2547417,5859059,5859059,5859059,0,0,0,0,0,0,0},
    {2713878,2713878,2713878,2713878,6241919,6241919,6241919,0,0,0,0,0,0,0},
    {3206160,3206160,3206160,3206160,7374168,7374168,7374168,0,0,0,0,0,0,0},
    {3681024,3681024,3681024,3681024,9570662,9570662,9570662,0,0,0,0,0,0,0},
    {4022472,4022472,4022472,4022472,10458427,10458427,10458427,0,0,0,0,0,0,0},
    {4377024,4377024,4377024,4377024,11380262,11380262,11380262,0,0,0,0,0,0,0},
    {4744680,4744680,4744680,4744680,12336168,12336168,12336168,0,0,0,0,0,0,0},
    {5125440,5125440,5125440,5125440,13326144,13326144,13326144,0,0,0,0,0,0,0},
    {5767272,5767272,5767272,5767272,14994907,14994907,14994907,0,0,0,0,0,0,0},
    {6204000,6204000,6204000,6204000,16130400,16130400,16130400,0,0,0,0,0,0,0},
    {6655464,6655464,6655464,6655464,17304200,17304200,17304200,0,0,0,0,0,0,0},
    {7121664,7121664,7121664,7121664,18516326,18516326,18516326,0,0,0,0,0,0,0},
    {7602600,7602600,7602600,7602600,19766760,19766760,19766760,0,0,0,0,0,0,0},
    {9738720,9738720,9738720,9738720,29216160,29216160,29216160,0,0,0,0,0,0,0},
    {11649960,11649960,11649960,11649960,34949880,34949880,34949880,0,0,0,0,0,0,0},
    {13643520,13643520,13643520,13643520,40930560,40930560,40930560,0,0,0,0,0,0,0},
    {18339300,18339300,18339300,18339300,55017900,55017900,55017900,0,0,0,0,0,0,0},
    {23836800,23836800,23836800,23836800,71510400,71510400,71510400,0,0,0,0,0,0,0},
    {35658000,35658000,35658000,35658000,106974000,106974000,106974000,0,0,0,0,0,0,0},
    {48687000,48687000,48687000,48687000,146061000,146061000,146061000,0,0,0,0,0,0,0},
    {58135000,58135000,58135000,58135000,174405000,174405000,174405000,0,0,0,0,0,0,0},
    {99999999,99999999,99999999,99999999,200000000,200000000,200000000,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0},
};

constexpr static sint32 stat_point_delta[256] =
{
    (0 + 14) / 4,
    (1 + 14) / 4,
    (2 + 14) / 4,
    (3 + 14) / 4,
    (4 + 14) / 4,
    (5 + 14) / 4,
    (6 + 14) / 4,
    (7 + 14) / 4,
    (8 + 14) / 4,
    (9 + 14) / 4,
    (10 + 14) / 4,
    (11 + 14) / 4,
    (12 + 14) / 4,
    (13 + 14) / 4,
    (14 + 14) / 4,
    (15 + 14) / 4,
    (16 + 14) / 4,
    (17 + 14) / 4,
    (18 + 14) / 4,
    (19 + 14) / 4,
    (20 + 14) / 4,
    (21 + 14) / 4,
    (22 + 14) / 4,
    (23 + 14) / 4,
    (24 + 14) / 4,
    (25 + 14) / 4,
    (26 + 14) / 4,
    (27 + 14) / 4,
    (28 + 14) / 4,
    (29 + 14) / 4,
    (30 + 14) / 4,
    (31 + 14) / 4,
    (32 + 14) / 4,
    (33 + 14) / 4,
    (34 + 14) / 4,
    (35 + 14) / 4,
    (36 + 14) / 4,
    (37 + 14) / 4,
    (38 + 14) / 4,
    (39 + 14) / 4,
    (40 + 14) / 4,
    (41 + 14) / 4,
    (42 + 14) / 4,
    (43 + 14) / 4,
    (44 + 14) / 4,
    (45 + 14) / 4,
    (46 + 14) / 4,
    (47 + 14) / 4,
    (48 + 14) / 4,
    (49 + 14) / 4,
    (50 + 14) / 4,
    (51 + 14) / 4,
    (52 + 14) / 4,
    (53 + 14) / 4,
    (54 + 14) / 4,
    (55 + 14) / 4,
    (56 + 14) / 4,
    (57 + 14) / 4,
    (58 + 14) / 4,
    (59 + 14) / 4,
    (60 + 14) / 4,
    (61 + 14) / 4,
    (62 + 14) / 4,
    (63 + 14) / 4,
    (64 + 14) / 4,
    (65 + 14) / 4,
    (66 + 14) / 4,
    (67 + 14) / 4,
    (68 + 14) / 4,
    (69 + 14) / 4,
    (70 + 14) / 4,
    (71 + 14) / 4,
    (72 + 14) / 4,
    (73 + 14) / 4,
    (74 + 14) / 4,
    (75 + 14) / 4,
    (76 + 14) / 4,
    (77 + 14) / 4,
    (78 + 14) / 4,
    (79 + 14) / 4,
    (80 + 14) / 4,
    (81 + 14) / 4,
    (82 + 14) / 4,
    (83 + 14) / 4,
    (84 + 14) / 4,
    (85 + 14) / 4,
    (86 + 14) / 4,
    (87 + 14) / 4,
    (88 + 14) / 4,
    (89 + 14) / 4,
    (90 + 14) / 4,
    (91 + 14) / 4,
    (92 + 14) / 4,
    (93 + 14) / 4,
    (94 + 14) / 4,
    (95 + 14) / 4,
    (96 + 14) / 4,
    (97 + 14) / 4,
    (98 + 14) / 4,
    (99 + 14) / 4,
    (100 + 14) / 4,
    (101 + 14) / 4,
    (102 + 14) / 4,
    (103 + 14) / 4,
    (104 + 14) / 4,
    (105 + 14) / 4,
    (106 + 14) / 4,
    (107 + 14) / 4,
    (108 + 14) / 4,
    (109 + 14) / 4,
    (110 + 14) / 4,
    (111 + 14) / 4,
    (112 + 14) / 4,
    (113 + 14) / 4,
    (114 + 14) / 4,
    (115 + 14) / 4,
    (116 + 14) / 4,
    (117 + 14) / 4,
    (118 + 14) / 4,
    (119 + 14) / 4,
    (120 + 14) / 4,
    (121 + 14) / 4,
    (122 + 14) / 4,
    (123 + 14) / 4,
    (124 + 14) / 4,
    (125 + 14) / 4,
    (126 + 14) / 4,
    (127 + 14) / 4,
    (128 + 14) / 4,
    (129 + 14) / 4,
    (130 + 14) / 4,
    (131 + 14) / 4,
    (132 + 14) / 4,
    (133 + 14) / 4,
    (134 + 14) / 4,
    (135 + 14) / 4,
    (136 + 14) / 4,
    (137 + 14) / 4,
    (138 + 14) / 4,
    (139 + 14) / 4,
    (140 + 14) / 4,
    (141 + 14) / 4,
    (142 + 14) / 4,
    (143 + 14) / 4,
    (144 + 14) / 4,
    (145 + 14) / 4,
    (146 + 14) / 4,
    (147 + 14) / 4,
    (148 + 14) / 4,
    (149 + 14) / 4,
    (150 + 14) / 4,
    (151 + 14) / 4,
    (152 + 14) / 4,
    (153 + 14) / 4,
    (154 + 14) / 4,
    (155 + 14) / 4,
    (156 + 14) / 4,
    (157 + 14) / 4,
    (158 + 14) / 4,
    (159 + 14) / 4,
    (160 + 14) / 4,
    (161 + 14) / 4,
    (162 + 14) / 4,
    (163 + 14) / 4,
    (164 + 14) / 4,
    (165 + 14) / 4,
    (166 + 14) / 4,
    (167 + 14) / 4,
    (168 + 14) / 4,
    (169 + 14) / 4,
    (170 + 14) / 4,
    (171 + 14) / 4,
    (172 + 14) / 4,
    (173 + 14) / 4,
    (174 + 14) / 4,
    (175 + 14) / 4,
    (176 + 14) / 4,
    (177 + 14) / 4,
    (178 + 14) / 4,
    (179 + 14) / 4,
    (180 + 14) / 4,
    (181 + 14) / 4,
    (182 + 14) / 4,
    (183 + 14) / 4,
    (184 + 14) / 4,
    (185 + 14) / 4,
    (186 + 14) / 4,
    (187 + 14) / 4,
    (188 + 14) / 4,
    (189 + 14) / 4,
    (190 + 14) / 4,
    (191 + 14) / 4,
    (192 + 14) / 4,
    (193 + 14) / 4,
    (194 + 14) / 4,
    (195 + 14) / 4,
    (196 + 14) / 4,
    (197 + 14) / 4,
    (198 + 14) / 4,
    (199 + 14) / 4,
    (200 + 14) / 4,
    (201 + 14) / 4,
    (202 + 14) / 4,
    (203 + 14) / 4,
    (204 + 14) / 4,
    (205 + 14) / 4,
    (206 + 14) / 4,
    (207 + 14) / 4,
    (208 + 14) / 4,
    (209 + 14) / 4,
    (210 + 14) / 4,
    (211 + 14) / 4,
    (212 + 14) / 4,
    (213 + 14) / 4,
    (214 + 14) / 4,
    (215 + 14) / 4,
    (216 + 14) / 4,
    (217 + 14) / 4,
    (218 + 14) / 4,
    (219 + 14) / 4,
    (220 + 14) / 4,
    (221 + 14) / 4,
    (222 + 14) / 4,
    (223 + 14) / 4,
    (224 + 14) / 4,
    (225 + 14) / 4,
    (226 + 14) / 4,
    (227 + 14) / 4,
    (228 + 14) / 4,
    (229 + 14) / 4,
    (230 + 14) / 4,
    (231 + 14) / 4,
    (232 + 14) / 4,
    (233 + 14) / 4,
    (234 + 14) / 4,
    (235 + 14) / 4,
    (236 + 14) / 4,
    (237 + 14) / 4,
    (238 + 14) / 4,
    (239 + 14) / 4,
    (240 + 14) / 4,
    (241 + 14) / 4,
    (242 + 14) / 4,
    (243 + 14) / 4,
    (244 + 14) / 4,
    (245 + 14) / 4,
    (246 + 14) / 4,
    (247 + 14) / 4,
    (248 + 14) / 4,
    (249 + 14) / 4,
    (250 + 14) / 4,
    (251 + 14) / 4,
    (252 + 14) / 4,
    (253 + 14) / 4,
    (254 + 14) / 4,
    (255 + 14) / 4,
};

// the config file gave different numbers ...
// so, if you restat immediately when starting, you get 24 more points
constexpr static sint32 stat_point_oops = 0; // -24;
// how many stat points you have, AFTER you have at least 1
constexpr static sint32 stat_points[256] =
{
    stat_point_oops + 45, // there isn't really a level 0, but let's simplify things
    stat_point_oops + 48,
    stat_point_oops + 52,
    stat_point_oops + 56,
    stat_point_oops + 60,
    stat_point_oops + 64,
    stat_point_oops + 69,
    stat_point_oops + 74,
    stat_point_oops + 79,
    stat_point_oops + 84,
    stat_point_oops + 90,
    stat_point_oops + 96,
    stat_point_oops + 102,
    stat_point_oops + 108,
    stat_point_oops + 115,
    stat_point_oops + 122,
    stat_point_oops + 129,
    stat_point_oops + 136,
    stat_point_oops + 144,
    stat_point_oops + 152,
    stat_point_oops + 160,
    stat_point_oops + 168,
    stat_point_oops + 177,
    stat_point_oops + 186,
    stat_point_oops + 195,
    stat_point_oops + 204,
    stat_point_oops + 214,
    stat_point_oops + 224,
    stat_point_oops + 234,
    stat_point_oops + 244,
    stat_point_oops + 255,
    stat_point_oops + 266,
    stat_point_oops + 277,
    stat_point_oops + 288,
    stat_point_oops + 300,
    stat_point_oops + 312,
    stat_point_oops + 324,
    stat_point_oops + 336,
    stat_point_oops + 349,
    stat_point_oops + 362,
    stat_point_oops + 375,
    stat_point_oops + 388,
    stat_point_oops + 402,
    stat_point_oops + 416,
    stat_point_oops + 430,
    stat_point_oops + 444,
    stat_point_oops + 459,
    stat_point_oops + 474,
    stat_point_oops + 489,
    stat_point_oops + 504,
    stat_point_oops + 520,
    stat_point_oops + 536,
    stat_point_oops + 552,
    stat_point_oops + 568,
    stat_point_oops + 585,
    stat_point_oops + 602,
    stat_point_oops + 619,
    stat_point_oops + 636,
    stat_point_oops + 654,
    stat_point_oops + 672,
    stat_point_oops + 690,
    stat_point_oops + 708,
    stat_point_oops + 727,
    stat_point_oops + 746,
    stat_point_oops + 765,
    stat_point_oops + 784,
    stat_point_oops + 804,
    stat_point_oops + 824,
    stat_point_oops + 844,
    stat_point_oops + 864,
    stat_point_oops + 885,
    stat_point_oops + 906,
    stat_point_oops + 927,
    stat_point_oops + 948,
    stat_point_oops + 970,
    stat_point_oops + 992,
    stat_point_oops + 1014,
    stat_point_oops + 1036,
    stat_point_oops + 1059,
    stat_point_oops + 1082,
    stat_point_oops + 1105,
    stat_point_oops + 1128,
    stat_point_oops + 1152,
    stat_point_oops + 1176,
    stat_point_oops + 1200,
    stat_point_oops + 1224,
    stat_point_oops + 1249,
    stat_point_oops + 1274,
    stat_point_oops + 1299,
    stat_point_oops + 1324,
    stat_point_oops + 1350,
    stat_point_oops + 1376,
    stat_point_oops + 1402,
    stat_point_oops + 1428,
    stat_point_oops + 1455,
    stat_point_oops + 1482,
    stat_point_oops + 1509,
    stat_point_oops + 1536,
    stat_point_oops + 1564,
    stat_point_oops + 1592,
    stat_point_oops + 1620,
    stat_point_oops + 1648,
    stat_point_oops + 1677,
    stat_point_oops + 1706,
    stat_point_oops + 1735,
    stat_point_oops + 1764,
    stat_point_oops + 1794,
    stat_point_oops + 1824,
    stat_point_oops + 1854,
    stat_point_oops + 1884,
    stat_point_oops + 1915,
    stat_point_oops + 1946,
    stat_point_oops + 1977,
    stat_point_oops + 2008,
    stat_point_oops + 2040,
    stat_point_oops + 2072,
    stat_point_oops + 2104,
    stat_point_oops + 2136,
    stat_point_oops + 2169,
    stat_point_oops + 2202,
    stat_point_oops + 2235,
    stat_point_oops + 2268,
    stat_point_oops + 2302,
    stat_point_oops + 2336,
    stat_point_oops + 2370,
    stat_point_oops + 2404,
    stat_point_oops + 2439,
    stat_point_oops + 2474,
    stat_point_oops + 2509,
    stat_point_oops + 2544,
    stat_point_oops + 2580,
    stat_point_oops + 2616,
    stat_point_oops + 2652,
    stat_point_oops + 2688,
    stat_point_oops + 2725,
    stat_point_oops + 2762,
    stat_point_oops + 2799,
    stat_point_oops + 2836,
    stat_point_oops + 2874,
    stat_point_oops + 2912,
    stat_point_oops + 2950,
    stat_point_oops + 2988,
    stat_point_oops + 3027,
    stat_point_oops + 3066,
    stat_point_oops + 3105,
    stat_point_oops + 3144,
    stat_point_oops + 3184,
    stat_point_oops + 3224,
    stat_point_oops + 3264,
    stat_point_oops + 3304,
    stat_point_oops + 3345,
    stat_point_oops + 3386,
    stat_point_oops + 3427,
    stat_point_oops + 3468,
    stat_point_oops + 3510,
    stat_point_oops + 3552,
    stat_point_oops + 3594,
    stat_point_oops + 3636,
    stat_point_oops + 3679,
    stat_point_oops + 3722,
    stat_point_oops + 3765,
    stat_point_oops + 3808,
    stat_point_oops + 3852,
    stat_point_oops + 3896,
    stat_point_oops + 3940,
    stat_point_oops + 3984,
    stat_point_oops + 4029,
    stat_point_oops + 4074,
    stat_point_oops + 4119,
    stat_point_oops + 4164,
    stat_point_oops + 4210,
    stat_point_oops + 4256,
    stat_point_oops + 4302,
    stat_point_oops + 4348,
    stat_point_oops + 4395,
    stat_point_oops + 4442,
    stat_point_oops + 4489,
    stat_point_oops + 4536,
    stat_point_oops + 4584,
    stat_point_oops + 4632,
    stat_point_oops + 4680,
    stat_point_oops + 4728,
    stat_point_oops + 4777,
    stat_point_oops + 4826,
    stat_point_oops + 4875,
    stat_point_oops + 4924,
    stat_point_oops + 4974,
    stat_point_oops + 5024,
    stat_point_oops + 5074,
    stat_point_oops + 5124,
    stat_point_oops + 5175,
    stat_point_oops + 5226,
    stat_point_oops + 5277,
    stat_point_oops + 5328,
    stat_point_oops + 5380,
    stat_point_oops + 5432,
    stat_point_oops + 5484,
    stat_point_oops + 5536,
    stat_point_oops + 5589,
    stat_point_oops + 5642,
    stat_point_oops + 5695,
    stat_point_oops + 5748,
    stat_point_oops + 5802,
    stat_point_oops + 5856,
    stat_point_oops + 5910,
    stat_point_oops + 5964,
    stat_point_oops + 6019,
    stat_point_oops + 6074,
    stat_point_oops + 6129,
    stat_point_oops + 6184,
    stat_point_oops + 6240,
    stat_point_oops + 6296,
    stat_point_oops + 6352,
    stat_point_oops + 6408,
    stat_point_oops + 6465,
    stat_point_oops + 6522,
    stat_point_oops + 6579,
    stat_point_oops + 6636,
    stat_point_oops + 6694,
    stat_point_oops + 6752,
    stat_point_oops + 6810,
    stat_point_oops + 6868,
    stat_point_oops + 6927,
    stat_point_oops + 6986,
    stat_point_oops + 7045,
    stat_point_oops + 7104,
    stat_point_oops + 7164,
    stat_point_oops + 7224,
    stat_point_oops + 7284,
    stat_point_oops + 7344,
    stat_point_oops + 7405,
    stat_point_oops + 7466,
    stat_point_oops + 7527,
    stat_point_oops + 7588,
    stat_point_oops + 7650,
    stat_point_oops + 7712,
    stat_point_oops + 7774,
    stat_point_oops + 7836,
    stat_point_oops + 7899,
    stat_point_oops + 7962,
    stat_point_oops + 8025,
    stat_point_oops + 8088,
    stat_point_oops + 8152,
    stat_point_oops + 8216,
    stat_point_oops + 8280,
    stat_point_oops + 8344,
    stat_point_oops + 8409,
    stat_point_oops + 8474,
    stat_point_oops + 8539,
    stat_point_oops + 8604,
    stat_point_oops + 8670,
    stat_point_oops + 8736,
    stat_point_oops + 8802,
    stat_point_oops + 8868,
    stat_point_oops + 8935,
    stat_point_oops + 9002,
};

constexpr static sint32 dirx[8] = { 0, -1, -1, -1, 0, 1, 1, 1 };
constexpr static sint32 diry[8] = { 1, 1, 0, -1, -1, -1, 0, 1 };

static DMap<account_t, gm_level_t> gm_accounts;

gm_level_t pc_isGM(MapSessionData *sd)
{
    return gm_accounts.get(sd->id);
}

sint32 pc_iskiller(MapSessionData *src, MapSessionData *target)
{
    nullpo_ret(src);

    if (src->type != BL_PC)
        return 0;
    if (src->special_state.killer)
        return 1;

    if (target->type != BL_PC)
        return 0;
    if (target->special_state.killable)
        return 1;

    return 0;
}

void pc_set_gm_level(account_t account_id, gm_level_t level)
{
    gm_accounts.set(account_id, level);
}

static sint32 distance(sint32 x_0, sint32 y_0, sint32 x_1, sint32 y_1)
{
    sint32 dx, dy;

    dx = abs(x_0 - x_1);
    dy = abs(y_0 - y_1);
    return dx > dy ? dx : dy;
}

static void pc_invincible_timer(timer_id, tick_t, BlockID id)
{
    MapSessionData *sd = map_id2sd(id);

    if (!sd)
        return;

    sd->invincible_timer = NULL;
}

void pc_setinvincibletimer(MapSessionData *sd, interval_t val)
{
    nullpo_retv(sd);

    if (sd->invincible_timer)
        delete_timer(sd->invincible_timer);
    sd->invincible_timer = add_timer(gettick() + val, pc_invincible_timer, sd->id);
}

void pc_delinvincibletimer(MapSessionData *sd)
{
    nullpo_retv(sd);

    if (sd->invincible_timer)
    {
        delete_timer(sd->invincible_timer);
        sd->invincible_timer = NULL;
    }
}

sint32 pc_setrestartvalue(MapSessionData *sd, sint32 type)
{
    nullpo_ret(sd);

    //-----------------------
    // 死亡した
    if (sd->special_state.restart_full_recover)
    {                           // オシリスカード
        sd->status.hp = sd->status.max_hp;
        sd->status.sp = sd->status.max_sp;
    }
    else
    {
        if (battle_config.restart_hp_rate < 50)
        {                       //ノビは半分回復
            sd->status.hp = (sd->status.max_hp) / 2;
        }
        else
        {
            if (battle_config.restart_hp_rate <= 0)
                sd->status.hp = 1;
            else
            {
                sd->status.hp =
                    sd->status.max_hp * battle_config.restart_hp_rate / 100;
                if (sd->status.hp <= 0)
                    sd->status.hp = 1;
            }
        }
        if (battle_config.restart_sp_rate > 0)
        {
            sint32 sp = sd->status.max_sp * battle_config.restart_sp_rate / 100;
            if (sd->status.sp < sp)
                sd->status.sp = sp;
        }
    }
    if (type & 1)
        clif_updatestatus(sd, SP::HP);
    if (type & 1)
        clif_updatestatus(sd, SP::SP);

    sd->heal_xp = 0;            // [Fate] Set gainable xp for healing this player to 0

    return 0;
}

/*==========================================
 * 自分をロックしているMOBの数を数える(foreachclient)
 *------------------------------------------
 */
static void pc_counttargeted_sub(BlockList *bl, BlockID id, sint32 *c,
                                 BlockList *src, AttackResult target_lv)
{
    nullpo_retv(bl);
    nullpo_retv(c);

    if (id == bl->id || (src && id == src->id))
        return;
    if (bl->type == BL_PC)
    {
        MapSessionData *sd = static_cast<MapSessionData *>(bl);
        if (sd && sd->attacktarget == id && sd->attacktimer
            && sd->attacktarget_lv >= target_lv)
            (*c)++;
    }
    else if (bl->type == BL_MOB)
    {
        struct mob_data *md = static_cast<struct mob_data *>(bl);
        if (md && md->target_id == id && md->timer
            && md->state.state == MS::ATTACK && md->target_lv >= target_lv)

            (*c)++;
        //printf("md->target_lv:%d, target_lv:%d\n", ((struct mob_data *)bl)->target_lv, target_lv);
    }
}

sint32 pc_counttargeted(MapSessionData *sd, BlockList *src,
                      AttackResult target_lv)
{
    sint32 c = 0;
    map_foreachinarea(pc_counttargeted_sub, sd->m,
                      sd->x - AREA_SIZE, sd->y - AREA_SIZE,
                      sd->x + AREA_SIZE, sd->y + AREA_SIZE, BL_NUL,
                      sd->id, &c, src, target_lv);
    return c;
}

/*==========================================
 * ローカルプロトタイプ宣言 (必要な物のみ)
 *------------------------------------------
 */
static sint32 pc_walktoxy_sub(MapSessionData *);

/*==========================================
 * saveに必要なステータス修正を行なう
 *------------------------------------------
 */
sint32 pc_makesavestatus(MapSessionData *sd)
{
    nullpo_ret(sd);

    // 死亡状態だったのでhpを1、位置をセーブ場所に変更
    if (pc_isdead(sd))
    {
        pc_setrestartvalue(sd, 0);
        sd->status.last_point = sd->status.save_point;
    }
    else
    {
        sd->status.last_point.map = sd->mapname;
        sd->status.last_point.x = sd->x;
        sd->status.last_point.y = sd->y;
    }

    // セーブ禁止マップだったので指定位置に移動
    if (maps[sd->m].flag.nosave)
    {
        map_data_local *m = &maps[sd->m];
        if (strcmp(&m->save.map, "SavePoint") == 0)
            sd->status.last_point = sd->status.save_point;
        else
            sd->status.last_point = m->save;
    }
    return 0;
}

/*==========================================
 * 接続時の初期化
 *------------------------------------------
 */
sint32 pc_setnewpc(MapSessionData *sd, /*account_t account_id,*/ charid_t char_id,
                uint32 login_id1, uint8 sex)
{
    nullpo_ret(sd);

    //sd->id = account_id;
    sd->char_id = char_id;
    sd->login_id1 = login_id1;
    sd->login_id2 = 0;          // at this point, we can not know the value :(
    sd->sex = sex;
    sd->state.auth = 0;
    sd->canact_tick = sd->canmove_tick = gettick();
    sd->canlog_tick = gettick();
    sd->state.waitingdisconnect = 0;

    return 0;
}

EPOS pc_equippoint(MapSessionData *sd, sint32 n)
{
    nullpo_retr(EPOS(), sd);

    if (!sd->inventory_data[n])
        return EPOS();

    return sd->inventory_data[n]->equip;
}

static sint32 pc_setinventorydata(MapSessionData *sd)
{
    sint32 i, id;

    nullpo_ret(sd);

    for (i = 0; i < MAX_INVENTORY; i++)
    {
        id = sd->status.inventory[i].nameid;
        sd->inventory_data[i] = itemdb_search(id);
    }
    return 0;
}

static sint32 pc_calcweapontype(MapSessionData *sd)
{
    nullpo_ret(sd);

    if (sd->weapontype1 != 0 && sd->weapontype2 == 0)
        sd->status.weapon = sd->weapontype1;
    if (sd->weapontype1 == 0 && sd->weapontype2 != 0)   // 左手武器 Only
        sd->status.weapon = sd->weapontype2;
    else if (sd->weapontype1 == 1 && sd->weapontype2 == 1)  // 双短剣
        sd->status.weapon = 0x11;
    else if (sd->weapontype1 == 2 && sd->weapontype2 == 2)  // 双単手剣
        sd->status.weapon = 0x12;
    else if (sd->weapontype1 == 6 && sd->weapontype2 == 6)  // 双単手斧
        sd->status.weapon = 0x13;
    else if ((sd->weapontype1 == 1 && sd->weapontype2 == 2) || (sd->weapontype1 == 2 && sd->weapontype2 == 1))  // 短剣 - 単手剣
        sd->status.weapon = 0x14;
    else if ((sd->weapontype1 == 1 && sd->weapontype2 == 6) || (sd->weapontype1 == 6 && sd->weapontype2 == 1))  // 短剣 - 斧
        sd->status.weapon = 0x15;
    else if ((sd->weapontype1 == 2 && sd->weapontype2 == 6) || (sd->weapontype1 == 6 && sd->weapontype2 == 2))  // 単手剣 - 斧
        sd->status.weapon = 0x16;
    else
        sd->status.weapon = sd->weapontype1;

    return 0;
}

static sint32 pc_setequipindex(MapSessionData *sd)
{
    nullpo_ret(sd);

    for (sint16& elt : sd->equip_index)
        elt = -1;

    for (sint32 i = 0; i < MAX_INVENTORY; i++)
    {
        if (!sd->status.inventory[i].nameid)
            continue;
        if (!sd->status.inventory[i].equip)
            continue;
        for (EQUIP j : EQUIPs)
            if (sd->status.inventory[i].equip & equip_pos[j])
                sd->equip_index[j] = i;
        if (sd->status.inventory[i].equip & EPOS::WEAPON)
        {
            if (sd->inventory_data[i])
                sd->weapontype1 = sd->inventory_data[i]->look;
            else
                sd->weapontype1 = 0;
        }
        if (sd->status.inventory[i].equip & EPOS::SHIELD)
        {
            if (sd->inventory_data[i])
            {
                if (sd->inventory_data[i]->type == 4)
                {
                    if (sd->status.inventory[i].equip == EPOS::SHIELD)
                        sd->weapontype2 = sd->inventory_data[i]->look;
                    else
                        sd->weapontype2 = 0;
                }
                else
                    sd->weapontype2 = 0;
            }
            else
                sd->weapontype2 = 0;
        }
    }
    pc_calcweapontype(sd);

    return 0;
}

sint32 pc_isequip(MapSessionData *sd, sint32 n)
{
    struct item_data *item;
    //転生や養子の場合の元の職業を算出する

    nullpo_ret(sd);

    item = sd->inventory_data[n];

    if (item == NULL)
        return 0;
    if (item->sex != 2 && sd->status.sex != item->sex)
        return 0;
    if (item->elv && sd->status.base_level < item->elv)
        return 0;

    if (maps[sd->m].flag.pvp
        && (item->flag.no_equip == 1 || item->flag.no_equip == 3))
        return 0;
    return 1;
}

/*==========================================
 * session idに問題無し
 * char鯖から送られてきたステータスを設定
 *------------------------------------------
 */
sint32 pc_authok(account_t id, sint32 login_id2, time_t connect_until_time,
                 sint16 tmw_version, const struct mmo_charstatus *st)
{
    MapSessionData *sd = NULL;

    struct party *p;
    tick_t tick = gettick();

    sd = map_id2sd(id);
    if (sd == NULL)
        return 1;

    sd->login_id2 = login_id2;
    sd->tmw_version = tmw_version;

    memcpy(&sd->status, st, sizeof(*st));

    if (sd->status.sex != sd->sex)
    {
        clif_authfail_fd(sd->fd, 0);
        return 1;
    }

    MAP_LOG_STATS(sd, "LOGIN");
    MAP_LOG_XP(sd, "LOGIN");
    MAP_LOG_MAGIC(sd, "LOGIN");

    memset(&sd->state, 0, sizeof(sd->state));
    // 基本的な初期化
    sd->state.connect_new = 1;
    sd->prev = sd->next = NULL;

    sd->weapontype1 = sd->weapontype2 = 0;
    sd->speed = DEFAULT_WALK_SPEED;
    sd->state.dead_sit = 0;
    sd->dir = Direction::S;
    sd->head_dir = Direction::S;
    sd->state.auth = 1;
    sd->walktimer = NULL;
    sd->attacktimer = NULL;
    sd->invincible_timer = NULL;

    sd->deal_locked = 0;
    sd->trade_partner = DEFAULT;

    sd->inchealhptick = DEFAULT;
    sd->inchealsptick = DEFAULT;
    sd->hp_sub = DEFAULT;
    sd->sp_sub = DEFAULT;
    sd->quick_regeneration_hp.amount = 0;
    sd->quick_regeneration_sp.amount = 0;
    sd->heal_xp = 0;
    sd->canact_tick = tick;
    sd->canmove_tick = tick;
    sd->attackabletime = tick;
    sd->cast_tick = tick;

    sd->doridori_counter = 0;

    // アカウント変数の送信要求
    intif_request_accountreg(sd);

    // アイテムチェック
    pc_setinventorydata(sd);
    pc_checkitem(sd);

    // ステータス異常の初期化
    for (sint32 i = 0; i < MAX_STATUSCHANGE; i++)
    {
        sd->sc_data[i].timer = NULL;
        sd->sc_data[i].val1 = 0;
    }
    sd->sc_count = 0;

    sd->status.option &= OPTION::MASK;

    // パーティー関係の初期化
    sd->party_sent = false;
    sd->party_invite = DEFAULT;
    sd->party_x = -1;
    sd->party_y = -1;
    sd->party_hp = -1;

    // イベント関係の初期化
    memset(sd->eventqueue, 0, sizeof(sd->eventqueue));
    for (sint32 i = 0; i < MAX_EVENTTIMER; i++)
        sd->eventtimer[i].tid = NULL;

    // 位置の設定
    pc_setpos(sd, sd->status.last_point, BeingRemoveType::ZERO);

    // パーティ、ギルドデータの要求
    if (sd->status.party_id && (p = party_search(sd->status.party_id)) == NULL)
        party_request_info(sd->status.party_id);

    // pvpの設定
    sd->pvp_rank = 0;
    sd->pvp_point = 0;
    sd->pvp_timer = NULL;

    // 通知

    clif_authok(sd);
    map_addnickdb(sd);
    if (map_charid2nick(sd->status.char_id) == NULL)
        map_addchariddb(sd->status.char_id, sd->status.name);

    //スパノビ用死にカウンターのスクリプト変数からの読み出しとsdへのセット
    sd->die_counter = pc_readglobalreg(sd, std::string("PC_DIE_COUNTER"));

    // ステータス初期計算など
    pc_calcstatus(sd, 1);

    if (pc_isGM(sd))
    {
        PRINTF("Connection accepted: character '%s' (account: %d; GM level %d).\n",
               sd->status.name, sd->status.account_id, pc_isGM(sd));
        clif_updatestatus(sd, SP::GM);
    }
    else
        PRINTF("Connection accepted: Character '%s' (account: %d).\n",
               sd->status.name, sd->status.account_id);

    // Message of the Dayの送信
    {
        char buf[256];
        FILE *fp;
        if ((fp = fopen_(motd_txt, "r")) != NULL)
        {
            while (fgets(buf, sizeof(buf) - 1, fp) != NULL)
            {
                for (sint32 i = 0; buf[i]; i++)
                {
                    if (buf[i] == '\r' || buf[i] == '\n')
                    {
                        buf[i] = 0;
                        break;
                    }
                }
                clif_displaymessage(sd->fd, buf);
            }
            fclose_(fp);
        }
    }

    sd->state.auto_ban_in_progress = 0;

    // Initialize antispam vars
    sd->chat_reset_due = sd->chat_lines_in = sd->chat_total_repeats =
        sd->chat_repeat_reset_due = 0;
    sd->chat_lastmsg[0] = '\0';

    memset(sd->flood_rates, 0, sizeof(sd->flood_rates));
    sd->packet_flood_reset_due = sd->packet_flood_in = 0;

    // message of the limited time of the account
    if (connect_until_time != 0)
    {                           // don't display if it's unlimited or unknow value
        char tmpstr[1024];
        strftime(tmpstr, sizeof(tmpstr), "Your account time limit is: %d-%m-%Y %H:%M:%S.", gmtime(&connect_until_time));
        clif_whisper_message(sd->fd, whisper_server_name, tmpstr,
                          strlen(tmpstr) + 1);
    }
    pc_calcstatus(sd, 1);

    return 0;
}

/*==========================================
 * session idに問題ありなので後始末
 *------------------------------------------
 */
sint32 pc_authfail(account_t id)
{
    MapSessionData *sd;

    sd = map_id2sd(id);
    if (sd == NULL)
        return 1;

    clif_authfail_fd(sd->fd, 0);

    return 0;
}

static sint32 pc_calc_skillpoint(MapSessionData *sd)
{
    sint32 i, skill_points = 0;

    nullpo_ret(sd);

    for (i = 0; i < skill_pool_skills_size; i++) {
        sint32 lv = sd->status.skill[skill_pool_skills[i]].lv;
        if (lv)
            skill_points += ((lv * (lv - 1)) >> 1) - 1;
    }

    return skill_points;
}

static void pc_set_weapon_look(MapSessionData *sd)
{
    if (sd->attack_spell_override)
        clif_changelook(sd, LOOK::WEAPON,
                         sd->attack_spell_look_override);
    else
        clif_changelook(sd, LOOK::WEAPON, sd->status.weapon);
}

/*==========================================
 * パラメータ計算
 * first==0の時、計算対象のパラメータが呼び出し前から
 * 変 化した場合自動でsendするが、
 * 能動的に変化させたパラメータは自前でsendするように
 *------------------------------------------
 */
sint32 pc_calcstatus(MapSessionData *sd, bool first)
{
    sint32 b_max_hp, b_max_sp, b_hp, b_sp, b_weight, b_max_weight,
        b_hit, b_flee;
    earray<sint32, ATTR, ATTR::COUNT> b_paramb, b_parame;
    sint32 b_watk, b_def, b_watk2, b_def2, b_flee2, b_critical,
        b_attackrange, b_matk1, b_matk2, b_mdef, b_mdef2;
    sint32 b_base_atk;
    struct skill b_skill[MAX_SKILL];
    sint32 aspd_rate, wele, wele_, def_ele, refinedef = 0;
    sint32 str, dstr, dex;

    nullpo_ret(sd);

    interval_t b_speed = sd->speed;
    b_max_hp = sd->status.max_hp;
    b_max_sp = sd->status.max_sp;
    b_hp = sd->status.hp;
    b_sp = sd->status.sp;
    b_weight = sd->weight;
    b_max_weight = sd->max_weight;
    b_paramb = sd->paramb;
    b_parame = sd->paramc;
    memcpy(b_skill, &sd->status.skill, sizeof(b_skill));
    b_hit = sd->hit;
    b_flee = sd->flee;
    interval_t b_aspd = sd->aspd;
    b_watk = sd->watk;
    b_def = sd->def;
    b_watk2 = sd->watk2;
    b_def2 = sd->def2;
    b_flee2 = sd->flee2;
    b_critical = sd->critical;
    b_attackrange = sd->attackrange;
    b_matk1 = sd->matk1;
    b_matk2 = sd->matk2;
    b_mdef = sd->mdef;
    b_mdef2 = sd->mdef2;
    b_base_atk = sd->base_atk;

    sd->max_weight = max_weight_base + sd->status.stats[ATTR::STR] * 300;

    if (first & 1)
    {
        sd->weight = 0;
        for (sint32 i = 0; i < MAX_INVENTORY; i++)
        {
            if (!sd->status.inventory[i].nameid || !sd->inventory_data[i])
                continue;
            sd->weight += sd->inventory_data[i]->weight * sd->status.inventory[i].amount;
        }
    }

    for (ATTR attr : ATTRs)
    {
        sd->paramb[attr] = 0;
        sd->parame[attr] = 0;
    }
    sd->hit = 0;
    sd->flee = 0;
    sd->flee2 = 0;
    sd->critical = 0;
    sd->aspd = DEFAULT;
    sd->watk = 0;
    sd->def = 0;
    sd->mdef = 0;
    sd->watk2 = 0;
    sd->def2 = 0;
    sd->mdef2 = 0;
    sd->status.max_hp = 0;
    sd->status.max_sp = 0;
    sd->attackrange = 0;
    sd->attackrange_ = 0;
    sd->atk_ele = 0;
    sd->def_ele = 0;
    sd->star = 0;
    sd->matk1 = 0;
    sd->matk2 = 0;
    sd->speed = DEFAULT_WALK_SPEED;
    sd->hprate = 100;
    sd->sprate = 100;
    sd->castrate = 100;
    sd->dsprate = 100;
    sd->base_atk = 0;
    sd->arrow_atk = 0;
    sd->arrow_ele = 0;
    sd->arrow_hit = 0;
    sd->arrow_range = 0;
    sd->nhealhp = sd->nhealsp = sd->nshealhp = sd->nshealsp = sd->nsshealhp =
        sd->nsshealsp = 0;
    memset(&sd->special_state, 0, sizeof(sd->special_state));

    sd->watk_ = 0;              //二刀流用(仮)
    sd->watk_2 = 0;
    sd->atk_ele_ = 0;
    sd->star_ = 0;

    sd->aspd_rate = 100;
    sd->speed_rate = 100;
    sd->hprecov_rate = 100;
    sd->sprecov_rate = 100;
    sd->critical_def = 0;
    sd->double_rate = 0;
    sd->near_attack_def_rate = sd->long_attack_def_rate = 0;
    sd->atk_rate = sd->matk_rate = 100;
    sd->ignore_def_ele = sd->ignore_def_race = 0;
    sd->ignore_def_ele_ = sd->ignore_def_race_ = 0;
    sd->ignore_mdef_ele = sd->ignore_mdef_race = 0;
    sd->arrow_cri = 0;
    sd->magic_def_rate = sd->misc_def_rate = 0;
    sd->perfect_hit = 0;
    sd->critical_rate = sd->hit_rate = sd->flee_rate = sd->flee2_rate = 100;
    sd->def_rate = sd->def2_rate = sd->mdef_rate = sd->mdef2_rate = 100;
    sd->def_ratio_atk_ele = sd->def_ratio_atk_ele_ = 0;
    sd->def_ratio_atk_race = sd->def_ratio_atk_race_ = 0;
    sd->get_zeny_num = 0;
    sd->speed_add_rate = sd->aspd_add_rate = 100;
    sd->double_add_rate = sd->perfect_hit_add = sd->get_zeny_add_num = 0;
    sd->splash_range = sd->splash_add_range = 0;
    sd->short_weapon_damage_return = sd->long_weapon_damage_return = 0;
    sd->magic_damage_return = 0;    //AppleGirl Was Here
    sd->random_attack_increase_add = sd->random_attack_increase_per = 0;

    sd->spellpower_bonus_target = 0;

    for (EQUIP i : EQUIPs_no_arrow)
    {
        // if (i == EQUIP::ARROW) continue;
        sint32 idx = sd->equip_index[i];
        if (idx < 0)
            continue;
        if (i == EQUIP::WEAPON && sd->equip_index[EQUIP::SHIELD] == idx)
            continue;
        if (i == EQUIP::CHEST && sd->equip_index[EQUIP::LEGS] == idx)
            continue;
        if (i == EQUIP::HELMET && (sd->equip_index[EQUIP::CHEST] == idx || sd->equip_index[EQUIP::LEGS] == idx))
            continue;

        if (sd->inventory_data[idx])
        {
            sd->spellpower_bonus_target += sd->inventory_data[idx]->magic_bonus;
        }
    }

#ifdef USE_ASTRAL_SOUL_SKILL
    if (sd->spellpower_bonus_target < 0)
        sd->spellpower_bonus_target =
            (sd->spellpower_bonus_target * 256) /
            (min(128 + skill_power(sd, TMW_ASTRAL_SOUL), 256));
#endif

    if (sd->spellpower_bonus_target < sd->spellpower_bonus_current)
        sd->spellpower_bonus_current = sd->spellpower_bonus_target;

    wele = sd->atk_ele;
    wele_ = sd->atk_ele_;
    def_ele = sd->def_ele;

    // 装備品によるステータス変化はここで実行
    for (EQUIP i : EQUIPs_no_arrow)
    {
        sint32 idx = sd->equip_index[i];
        if (idx < 0)
            continue;
        if (i == EQUIP::WEAPON && sd->equip_index[EQUIP::SHIELD] == idx)
            continue;
        if (i == EQUIP::CHEST && sd->equip_index[EQUIP::LEGS] == idx)
            continue;
        if (i == EQUIP::HELMET && (sd->equip_index[EQUIP::CHEST] == idx || sd->equip_index[EQUIP::LEGS] == idx))
            continue;
        if (sd->inventory_data[idx])
        {
            sd->def += sd->inventory_data[idx]->def;
            if (sd->inventory_data[idx]->type == 4)
            {
                if (i == EQUIP::SHIELD && sd->status.inventory[idx].equip == EPOS::SHIELD)
                {
                    //二刀流用データ入力
                    sd->watk_ += sd->inventory_data[idx]->atk;

                    sd->attackrange_ += sd->inventory_data[idx]->range;
                    sd->state.lr_flag = 1;
                    ArgRec arg[] =
                    {
                        { "@slotId", static_cast<sint32>(i) },
                        { "@itemId", sd->inventory_data[idx]->nameid }
                    };
                    run_script_l(sd->inventory_data[idx]->equip_script, 0, sd->id, DEFAULT, ARRAY_SIZEOF(arg), arg);
                    sd->state.lr_flag = 0;
                }
                else
                {               //二刀流武器以外
                    ArgRec arg[] =
                    {
                        { "@slotId", static_cast<sint32>(i) },
                        { "@itemId", sd->inventory_data[idx]->nameid }
                    };
                    sd->watk += sd->inventory_data[idx]->atk;

                    sd->attackrange += sd->inventory_data[idx]->range;
                    run_script_l(sd->inventory_data[idx]->equip_script, 0, sd->id, DEFAULT, ARRAY_SIZEOF(arg), arg);
                }
            }
            else if (sd->inventory_data[idx]->type == 5)
            {
                ArgRec arg[] =
                {
                    { "@slotId", static_cast<sint32>(i) },
                    { "@itemId", sd->inventory_data[idx]->nameid }
                };
                sd->watk += sd->inventory_data[idx]->atk;
                run_script_l(sd->inventory_data[idx]->equip_script, 0, sd->id, DEFAULT, ARRAY_SIZEOF(arg), arg);
            }
        }
    }

    if (battle_is_unarmed(sd))
    {
        sd->watk += skill_power(sd, TMW_BRAWLING) / 3; // +66 for 200
        sd->watk2 += skill_power(sd, TMW_BRAWLING) >> 3;   // +25 for 200
        sd->watk_ += skill_power(sd, TMW_BRAWLING) / 3;    // +66 for 200
        sd->watk_2 += skill_power(sd, TMW_BRAWLING) >> 3;  // +25 for 200
    }

    sint32 idx = sd->equip_index[EQUIP::ARROW];
    if (idx >= 0 && sd->inventory_data[idx])
    {
        ArgRec arg[] =
        {
            { "@slotId", static_cast<sint32>(EQUIP::ARROW) },
            { "@itemId", sd->inventory_data[idx]->nameid }
        };
        sd->state.lr_flag = 2;
        run_script_l(sd->inventory_data[idx]->equip_script, 0, sd->id, DEFAULT, ARRAY_SIZEOF(arg), arg);
        sd->state.lr_flag = 0;
        sd->arrow_atk += sd->inventory_data[idx]->atk;
    }
    sd->def += (refinedef + 50) / 100;

    if (sd->attackrange < 1)
        sd->attackrange = 1;
    if (sd->attackrange_ < 1)
        sd->attackrange_ = 1;
    if (sd->attackrange < sd->attackrange_)
        sd->attackrange = sd->attackrange_;
    if (sd->status.weapon == 11)
        sd->attackrange += sd->arrow_range;
    if (wele > 0)
        sd->atk_ele = wele;
    if (wele_ > 0)
        sd->atk_ele_ = wele_;
    if (def_ele > 0)
        sd->def_ele = def_ele;
    sd->double_rate += sd->double_add_rate;
    sd->perfect_hit += sd->perfect_hit_add;
    sd->get_zeny_num += sd->get_zeny_add_num;
    sd->splash_range += sd->splash_add_range;
    if (sd->speed_add_rate != 100)
        sd->speed_rate += sd->speed_add_rate - 100;
    if (sd->aspd_add_rate != 100)
        sd->aspd_rate += sd->aspd_add_rate - 100;

    sd->speed -= std::chrono::milliseconds(skill_power(sd, TMW_SPEED) >> 3);
    sd->aspd_rate -= skill_power(sd, TMW_SPEED) / 10;
    if (sd->aspd_rate < 20)
        sd->aspd_rate = 20;

    for (ATTR i : ATTRs)
    {
        sd->paramc[i] = sd->status.stats[i] + sd->paramb[i] + sd->parame[i];
        if (sd->paramc[i] < 0)
            sd->paramc[i] = 0;
    }

    if (sd->status.weapon == 11 || sd->status.weapon == 13 || sd->status.weapon == 14)
    {
        str = sd->paramc[ATTR::DEX];
        dex = sd->paramc[ATTR::STR];
    }
    else
    {
        str = sd->paramc[ATTR::STR];
        dex = sd->paramc[ATTR::DEX];
        sd->critical += ((dex * 3) >> 1);
    }
    dstr = str / 10;
    sd->base_atk += str + dstr * dstr + dex / 5 + sd->paramc[ATTR::LUK] / 5;
//fprintf(stderr, "baseatk = %d = x + %d + %d + %d + %d\n", sd->base_atk, str, dstr*dstr, dex/5, sd->paramc[ATTR::LUK]/5);
    sd->matk1 += sd->paramc[ATTR::INT] + (sd->paramc[ATTR::INT] / 5) * (sd->paramc[ATTR::INT] / 5);
    sd->matk2 += sd->paramc[ATTR::INT] + (sd->paramc[ATTR::INT] / 7) * (sd->paramc[ATTR::INT] / 7);
    if (sd->matk1 < sd->matk2)
        std::swap(sd->matk1, sd->matk2);
    // [Fate] New tmw magic system
    sd->matk1 += unwrap(sd->status.base_level) + sd->spellpower_bonus_current;
#ifdef USE_ASTRAL_SOUL_SKILL
    if (sd->matk1 > MAGIC_SKILL_THRESHOLD)
    {
        sint32 bonus = sd->matk1 - MAGIC_SKILL_THRESHOLD;
        // Ok if you are above a certain threshold, you get only (1/8) of that matk1
        // if you have Astral soul skill you can get the whole power again (and additionally the 1/8 added)
        sd->matk1 = MAGIC_SKILL_THRESHOLD + (bonus >> 3) + ((3 * bonus * skill_power(sd, TMW_ASTRAL_SOUL)) >> 9);
    }
#endif
    sd->matk2 = 0;
    if (sd->matk1 < 0)
        sd->matk1 = 0;

    sd->hit += sd->paramc[ATTR::DEX] + unwrap(sd->status.base_level);
    sd->flee += sd->paramc[ATTR::AGI] + unwrap(sd->status.base_level);
    sd->def2 += sd->paramc[ATTR::VIT];
    sd->mdef2 += sd->paramc[ATTR::INT];
    sd->flee2 += sd->paramc[ATTR::LUK] + 10;
    sd->critical += (sd->paramc[ATTR::LUK] * 3) + 10;

    // 200 is the maximum of the skill
    // so critical chance can get multiplied by ~1.5 and setting def2 to a third when skill maxed out
    // def2 is the defence gained by vit, whereas "def", which is gained by armor, stays as is
    sint32 spbsk = skill_power(sd, TMW_RAGING);
    if (spbsk != 0 && sd->attackrange <= 2)
    {
        sd->critical = (sd->critical * (128 + spbsk)) / 256;
        sd->def2 = (sd->def2 * 128) / (128 + spbsk);
    }

    if (sd->base_atk < 1)
        sd->base_atk = 1;
    if (sd->critical_rate != 100)
        sd->critical = (sd->critical * sd->critical_rate) / 100;
    if (sd->critical < 10)
        sd->critical = 10;
    if (sd->hit_rate != 100)
        sd->hit = (sd->hit * sd->hit_rate) / 100;
    if (sd->hit < 1)
        sd->hit = 1;
    if (sd->flee_rate != 100)
        sd->flee = (sd->flee * sd->flee_rate) / 100;
    if (sd->flee < 1)
        sd->flee = 1;
    if (sd->flee2_rate != 100)
        sd->flee2 = (sd->flee2 * sd->flee2_rate) / 100;
    if (sd->flee2 < 10)
        sd->flee2 = 10;
    if (sd->def_rate != 100)
        sd->def = (sd->def * sd->def_rate) / 100;
    if (sd->def < 0)
        sd->def = 0;
    if (sd->def2_rate != 100)
        sd->def2 = (sd->def2 * sd->def2_rate) / 100;
    if (sd->def2 < 1)
        sd->def2 = 1;
    if (sd->mdef_rate != 100)
        sd->mdef = (sd->mdef * sd->mdef_rate) / 100;
    if (sd->mdef < 0)
        sd->mdef = 0;
    if (sd->mdef2_rate != 100)
        sd->mdef2 = (sd->mdef2 * sd->mdef2_rate) / 100;
    if (sd->mdef2 < 1)
        sd->mdef2 = 1;

    // 二刀流 ASPD 修正
    if (sd->status.weapon <= 16)
        sd->aspd += aspd_base[sd->status.weapon]
            - (sd->paramc[ATTR::AGI] * 4 + sd->paramc[ATTR::DEX]) * aspd_base[sd->status.weapon] / 1000;
    else
        sd->aspd += ((aspd_base[sd->weapontype1] -
                      (sd->paramc[ATTR::AGI] * 4 + sd->paramc[ATTR::DEX]) * aspd_base[sd->weapontype1] / 1000
                     ) +
                     (aspd_base[sd->weapontype2] -
                      (sd->paramc[ATTR::AGI] * 4 + sd->paramc[ATTR::DEX]) * aspd_base[sd->weapontype2] / 1000
                     )
                    ) * 140 / 200;

    aspd_rate = sd->aspd_rate;

    //攻撃速度増加

    if (sd->attackrange > 2)
    {                           // [fate] ranged weapon?
        sd->attackrange += min(skill_power(sd, AC_OWL) / 60, 3);
        sd->hit += skill_power(sd, AC_OWL) / 10;   // 20 for 200
    }

    sd->max_weight += 1000;

    uint8 bl = unwrap(sd->status.base_level);

    sd->status.max_hp +=
        (3500 + bl * 500 + 0) / 100 * (100 + sd->paramc [ATTR::VIT]) / 100 + sd->parame[ATTR::VIT];
    if (sd->hprate != 100)
        sd->status.max_hp = sd->status.max_hp * sd->hprate / 100;

    if (sd->status.max_hp > battle_config.max_hp)   // removed negative max hp bug by Valaris
        sd->status.max_hp = battle_config.max_hp;
    if (sd->status.max_hp <= 0)
        sd->status.max_hp = 1;  // end

    // 最大SP計算
    sd->status.max_sp +=
        ((sp_coefficient * bl) + 1000) / 100 * (100 + sd->paramc[ATTR::INT]) / 100 + (sd->parame[ATTR::INT]);
    if (sd->sprate != 100)
        sd->status.max_sp = sd->status.max_sp * sd->sprate / 100;

    if (sd->status.max_sp < 0 || sd->status.max_sp > battle_config.max_sp)
        sd->status.max_sp = battle_config.max_sp;

    //自然回復HP
    sd->nhealhp = 1 + (sd->paramc[ATTR::VIT] / 5) + (sd->status.max_hp / 200);
    //自然回復SP
    sd->nhealsp = 1 + (sd->paramc[ATTR::INT] / 6) + (sd->status.max_sp / 100);
    if (sd->paramc[ATTR::INT] >= 120)
        sd->nhealsp += ((sd->paramc[ATTR::INT] - 120) >> 1) + 4;

    if (sd->hprecov_rate != 100)
    {
        sd->nhealhp = sd->nhealhp * sd->hprecov_rate / 100;
        if (sd->nhealhp < 1)
            sd->nhealhp = 1;
    }
    if (sd->sprecov_rate != 100)
    {
        sd->nhealsp = sd->nhealsp * sd->sprecov_rate / 100;
        if (sd->nhealsp < 1)
            sd->nhealsp = 1;
    }

    if (sd->sc_count)
    {
        if (sd->sc_data[SC_POISON].timer) // 毒状態
            sd->def2 = sd->def2 * 75 / 100;

        if (sd->sc_data[SC_ATKPOT].timer)
            sd->watk += sd->sc_data[SC_ATKPOT].val1;

        if (sd->sc_data[SC_SPEEDPOTION0].timer)
            aspd_rate -= sd->sc_data[SC_SPEEDPOTION0].val1;

        if (sd->sc_data[SC_HASTE].timer)
            aspd_rate -= sd->sc_data[SC_HASTE].val1;

        /// Slow down attacks if protected
        // because of this, many players don't want the protection spell
        if (sd->sc_data[SC_PHYS_SHIELD].timer)
            aspd_rate += sd->sc_data[SC_PHYS_SHIELD].val1;
    }

    if (sd->speed_rate != 100)
        sd->speed = sd->speed * sd->speed_rate / 100;
    if (sd->speed < interval_t(1))
        sd->speed = interval_t(1);
    if (aspd_rate != 100)
        sd->aspd = sd->aspd * aspd_rate / 100;

    if (sd->attack_spell_override)
        sd->aspd = sd->attack_spell_delay;

    if (sd->aspd < std::chrono::milliseconds(battle_config.max_aspd))
        sd->aspd = std::chrono::milliseconds(battle_config.max_aspd);
    sd->amotion = sd->aspd;
    sd->dmotion = std::chrono::milliseconds(800 - sd->paramc[ATTR::AGI] * 4);
    if (sd->dmotion < std::chrono::milliseconds(400))
        sd->dmotion = std::chrono::milliseconds(400);

    if (sd->status.hp > sd->status.max_hp)
        sd->status.hp = sd->status.max_hp;
    if (sd->status.sp > sd->status.max_sp)
        sd->status.sp = sd->status.max_sp;

    if (first & 4)
        return 0;
    if (first & 3)
    {
        clif_updatestatus(sd, SP::SPEED);
        clif_updatestatus(sd, SP::MAXHP);
        clif_updatestatus(sd, SP::MAXSP);
        if (first & 1)
        {
            clif_updatestatus(sd, SP::HP);
            clif_updatestatus(sd, SP::SP);
        }
        return 0;
    }

    if (memcmp(b_skill, sd->status.skill, sizeof(sd->status.skill))
        || b_attackrange != sd->attackrange)
        clif_skillinfoblock(sd);   // スキル送信

    if (b_speed != sd->speed)
        clif_updatestatus(sd, SP::SPEED);
    if (b_weight != sd->weight)
        clif_updatestatus(sd, SP::WEIGHT);
    if (b_max_weight != sd->max_weight)
    {
        clif_updatestatus(sd, SP::MAXWEIGHT);
    }
    for (ATTR i : ATTRs)
        if (b_paramb[i] + b_parame[i] != sd->paramb[i] + sd->parame[i])
            clif_updatestatus(sd, ATTR_TO_SP_BASE(i));
    if (b_hit != sd->hit)
        clif_updatestatus(sd, SP::HIT);
    if (b_flee != sd->flee)
        clif_updatestatus(sd, SP::FLEE1);
    if (b_aspd != sd->aspd)
        clif_updatestatus(sd, SP::ASPD);
    if (b_watk != sd->watk || b_base_atk != sd->base_atk)
        clif_updatestatus(sd, SP::ATK1);
    if (b_def != sd->def)
        clif_updatestatus(sd, SP::DEF1);
    if (b_watk2 != sd->watk2)
        clif_updatestatus(sd, SP::ATK2);
    if (b_def2 != sd->def2)
        clif_updatestatus(sd, SP::DEF2);
    if (b_flee2 != sd->flee2)
        clif_updatestatus(sd, SP::FLEE2);
    if (b_critical != sd->critical)
        clif_updatestatus(sd, SP::CRITICAL);
    if (b_matk1 != sd->matk1)
        clif_updatestatus(sd, SP::MATK1);
    if (b_matk2 != sd->matk2)
        clif_updatestatus(sd, SP::MATK2);
    if (b_mdef != sd->mdef)
        clif_updatestatus(sd, SP::MDEF1);
    if (b_mdef2 != sd->mdef2)
        clif_updatestatus(sd, SP::MDEF2);
    if (b_attackrange != sd->attackrange)
        clif_updatestatus(sd, SP::ATTACKRANGE);
    if (b_max_hp != sd->status.max_hp)
        clif_updatestatus(sd, SP::MAXHP);
    if (b_max_sp != sd->status.max_sp)
        clif_updatestatus(sd, SP::MAXSP);
    if (b_hp != sd->status.hp)
        clif_updatestatus(sd, SP::HP);
    if (b_sp != sd->status.sp)
        clif_updatestatus(sd, SP::SP);

/*      if (before.cart_num != before.cart_num || before.cart_max_num != before.cart_max_num ||
                before.cart_weight != before.cart_weight || before.cart_max_weight != before.cart_max_weight )
                clif_updatestatus(sd, SP::CARTINFO); */

    return 0;
}

/*==========================================
 * 装 備品による能力等のボーナス設定
 *------------------------------------------
 */
sint32 pc_bonus(MapSessionData *sd, SP type, sint32 val)
{
    nullpo_ret(sd);

    switch (type)
    {
        case SP::STR:
        case SP::AGI:
        case SP::VIT:
        case SP::INT:
        case SP::DEX:
        case SP::LUK:
            if (sd->state.lr_flag != 2)
                sd->parame[ATTR_FROM_SP_BASE(type)] += val;
            break;
        case SP::ATK1:
            if (!sd->state.lr_flag)
                sd->watk += val;
            else if (sd->state.lr_flag == 1)
                sd->watk_ += val;
            break;
        case SP::ATK2:
            if (!sd->state.lr_flag)
                sd->watk2 += val;
            else if (sd->state.lr_flag == 1)
                sd->watk_2 += val;
            break;
        case SP::BASE_ATK:
            if (sd->state.lr_flag != 2)
                sd->base_atk += val;
            break;
        case SP::MATK1:
            if (sd->state.lr_flag != 2)
                sd->matk1 += val;
            break;
        case SP::MATK2:
            if (sd->state.lr_flag != 2)
                sd->matk2 += val;
            break;
        case SP::MATK:
            if (sd->state.lr_flag != 2)
            {
                sd->matk1 += val;
                sd->matk2 += val;
            }
            break;
        case SP::DEF1:
            if (sd->state.lr_flag != 2)
                sd->def += val;
            break;
        case SP::MDEF1:
            if (sd->state.lr_flag != 2)
                sd->mdef += val;
            break;
        case SP::MDEF2:
            if (sd->state.lr_flag != 2)
                sd->mdef += val;
            break;
        case SP::HIT:
            if (sd->state.lr_flag != 2)
                sd->hit += val;
            else
                sd->arrow_hit += val;
            break;
        case SP::FLEE1:
            if (sd->state.lr_flag != 2)
                sd->flee += val;
            break;
        case SP::FLEE2:
            if (sd->state.lr_flag != 2)
                sd->flee2 += val * 10;
            break;
        case SP::CRITICAL:
            if (sd->state.lr_flag != 2)
                sd->critical += val * 10;
            else
                sd->arrow_cri += val * 10;
            break;
        case SP::ATKELE:
            if (!sd->state.lr_flag)
                sd->atk_ele = val;
            else if (sd->state.lr_flag == 1)
                sd->atk_ele_ = val;
            else if (sd->state.lr_flag == 2)
                sd->arrow_ele = val;
            break;
        case SP::DEFELE:
            if (sd->state.lr_flag != 2)
                sd->def_ele = val;
            break;
        case SP::MAXHP:
            if (sd->state.lr_flag != 2)
                sd->status.max_hp += val;
            break;
        case SP::MAXSP:
            if (sd->state.lr_flag != 2)
                sd->status.max_sp += val;
            break;
        case SP::CASTRATE:
            if (sd->state.lr_flag != 2)
                sd->castrate += val;
            break;
        case SP::MAXHPRATE:
            if (sd->state.lr_flag != 2)
                sd->hprate += val;
            break;
        case SP::MAXSPRATE:
            if (sd->state.lr_flag != 2)
                sd->sprate += val;
            break;
        case SP::SPRATE:
            if (sd->state.lr_flag != 2)
                sd->dsprate += val;
            break;
        case SP::ATTACKRANGE:
            if (!sd->state.lr_flag)
                sd->attackrange += val;
            else if (sd->state.lr_flag == 1)
                sd->attackrange_ += val;
            else if (sd->state.lr_flag == 2)
                sd->arrow_range += val;
            break;
        case SP::ADD_SPEED:
            if (sd->state.lr_flag != 2)
                sd->speed -= std::chrono::milliseconds(val);
            break;
        case SP::SPEED_RATE:
            if (sd->state.lr_flag != 2)
            {
                if (sd->speed_rate > 100 - val)
                    sd->speed_rate = 100 - val;
            }
            break;
        case SP::SPEED_ADDRATE:
            if (sd->state.lr_flag != 2)
                sd->speed_add_rate = sd->speed_add_rate * (100 - val) / 100;
            break;
        case SP::ASPD:
            if (sd->state.lr_flag != 2)
                sd->aspd -= std::chrono::milliseconds(val * 10);
            break;
        case SP::ASPD_RATE:
            if (sd->state.lr_flag != 2)
            {
                if (sd->aspd_rate > 100 - val)
                    sd->aspd_rate = 100 - val;
            }
            break;
        case SP::ASPD_ADDRATE:
            if (sd->state.lr_flag != 2)
                sd->aspd_add_rate = sd->aspd_add_rate * (100 - val) / 100;
            break;
        case SP::HP_RECOV_RATE:
            if (sd->state.lr_flag != 2)
                sd->hprecov_rate += val;
            break;
        case SP::SP_RECOV_RATE:
            if (sd->state.lr_flag != 2)
                sd->sprecov_rate += val;
            break;
        case SP::CRITICAL_DEF:
            if (sd->state.lr_flag != 2)
                sd->critical_def += val;
            break;
        case SP::NEAR_ATK_DEF:
            if (sd->state.lr_flag != 2)
                sd->near_attack_def_rate += val;
            break;
        case SP::LONG_ATK_DEF:
            if (sd->state.lr_flag != 2)
                sd->long_attack_def_rate += val;
            break;
        case SP::DOUBLE_RATE:
            if (sd->state.lr_flag == 0 && sd->double_rate < val)
                sd->double_rate = val;
            break;
        case SP::DOUBLE_ADD_RATE:
            if (sd->state.lr_flag == 0)
                sd->double_add_rate += val;
            break;
        case SP::MATK_RATE:
            if (sd->state.lr_flag != 2)
                sd->matk_rate += val;
            break;
        case SP::IGNORE_DEF_ELE:
            if (!sd->state.lr_flag)
                sd->ignore_def_ele |= 1 << val;
            else if (sd->state.lr_flag == 1)
                sd->ignore_def_ele_ |= 1 << val;
            break;
        case SP::IGNORE_DEF_RACE:
            if (!sd->state.lr_flag)
                sd->ignore_def_race |= 1 << val;
            else if (sd->state.lr_flag == 1)
                sd->ignore_def_race_ |= 1 << val;
            break;
        case SP::ATK_RATE:
            if (sd->state.lr_flag != 2)
                sd->atk_rate += val;
            break;
        case SP::MAGIC_ATK_DEF:
            if (sd->state.lr_flag != 2)
                sd->magic_def_rate += val;
            break;
        case SP::MISC_ATK_DEF:
            if (sd->state.lr_flag != 2)
                sd->misc_def_rate += val;
            break;
        case SP::IGNORE_MDEF_ELE:
            if (sd->state.lr_flag != 2)
                sd->ignore_mdef_ele |= 1 << val;
            break;
        case SP::IGNORE_MDEF_RACE:
            if (sd->state.lr_flag != 2)
                sd->ignore_mdef_race |= 1 << val;
            break;
        case SP::PERFECT_HIT_RATE:
            if (sd->state.lr_flag != 2 && sd->perfect_hit < val)
                sd->perfect_hit = val;
            break;
        case SP::PERFECT_HIT_ADD_RATE:
            if (sd->state.lr_flag != 2)
                sd->perfect_hit_add += val;
            break;
        case SP::CRITICAL_RATE:
            if (sd->state.lr_flag != 2)
                sd->critical_rate += val;
            break;
        case SP::GET_ZENY_NUM:
            if (sd->state.lr_flag != 2 && sd->get_zeny_num < val)
                sd->get_zeny_num = val;
            break;
        case SP::ADD_GET_ZENY_NUM:
            if (sd->state.lr_flag != 2)
                sd->get_zeny_add_num += val;
            break;
        case SP::DEF_RATIO_ATK_ELE:
            if (!sd->state.lr_flag)
                sd->def_ratio_atk_ele |= 1 << val;
            else if (sd->state.lr_flag == 1)
                sd->def_ratio_atk_ele_ |= 1 << val;
            break;
        case SP::DEF_RATIO_ATK_RACE:
            if (!sd->state.lr_flag)
                sd->def_ratio_atk_race |= 1 << val;
            else if (sd->state.lr_flag == 1)
                sd->def_ratio_atk_race_ |= 1 << val;
            break;
        case SP::HIT_RATE:
            if (sd->state.lr_flag != 2)
                sd->hit_rate += val;
            break;
        case SP::FLEE_RATE:
            if (sd->state.lr_flag != 2)
                sd->flee_rate += val;
            break;
        case SP::FLEE2_RATE:
            if (sd->state.lr_flag != 2)
                sd->flee2_rate += val;
            break;
        case SP::DEF_RATE:
            if (sd->state.lr_flag != 2)
                sd->def_rate += val;
            break;
        case SP::DEF2_RATE:
            if (sd->state.lr_flag != 2)
                sd->def2_rate += val;
            break;
        case SP::MDEF_RATE:
            if (sd->state.lr_flag != 2)
                sd->mdef_rate += val;
            break;
        case SP::MDEF2_RATE:
            if (sd->state.lr_flag != 2)
                sd->mdef2_rate += val;
            break;
        case SP::RESTART_FULL_RECORVER:
            if (sd->state.lr_flag != 2)
                sd->special_state.restart_full_recover = 1;
            break;
        case SP::NO_CASTCANCEL:
            if (sd->state.lr_flag != 2)
                sd->special_state.no_castcancel = 1;
            break;
        case SP::NO_CASTCANCEL2:
            if (sd->state.lr_flag != 2)
                sd->special_state.no_castcancel2 = 1;
            break;
        case SP::NO_MAGIC_DAMAGE:
            if (sd->state.lr_flag != 2)
                sd->special_state.no_magic_damage = 1;
            break;
        case SP::NO_WEAPON_DAMAGE:
            if (sd->state.lr_flag != 2)
                sd->special_state.no_weapon_damage = 1;
            break;
        case SP::NO_GEMSTONE:
            if (sd->state.lr_flag != 2)
                sd->special_state.no_gemstone = 1;
            break;
        case SP::SPLASH_RANGE:
            if (sd->state.lr_flag != 2 && sd->splash_range < val)
                sd->splash_range = val;
            break;
        case SP::SPLASH_ADD_RANGE:
            if (sd->state.lr_flag != 2)
                sd->splash_add_range += val;
            break;
        case SP::SHORT_WEAPON_DAMAGE_RETURN:
            if (sd->state.lr_flag != 2)
                sd->short_weapon_damage_return += val;
            break;
        case SP::LONG_WEAPON_DAMAGE_RETURN:
            if (sd->state.lr_flag != 2)
                sd->long_weapon_damage_return += val;
            break;
        case SP::MAGIC_DAMAGE_RETURN:   //AppleGirl Was Here
            if (sd->state.lr_flag != 2)
                sd->magic_damage_return += val;
            break;
        case SP::ALL_STATS:     // [Valaris]
            if (sd->state.lr_flag != 2)
            {
                for (ATTR attr : ATTRs)
                {
                    sd->parame[attr] += val;
                    clif_updatestatus(sd, ATTR_TO_SP_BASE(attr));
                }
            }
            break;
        case SP::AGI_VIT:       // [Valaris]
            if (sd->state.lr_flag != 2)
            {
                sd->parame[ATTR::AGI] += val;
                sd->parame[ATTR::VIT] += val;
                clif_updatestatus(sd, SP::AGI);
                clif_updatestatus(sd, SP::AGI);
            }
            break;
        case SP::AGI_DEX_STR:   // [Valaris]
            if (sd->state.lr_flag != 2)
            {
                sd->parame[ATTR::AGI] += val;
                sd->parame[ATTR::DEX] += val;
                sd->parame[ATTR::STR] += val;
                clif_updatestatus(sd, SP::AGI);
                clif_updatestatus(sd, SP::DEX);
                clif_updatestatus(sd, SP::STR);
            }
            break;
        case SP::PERFECT_HIDE:  // [Valaris]
            if (sd->state.lr_flag != 2)
            {
                sd->perfect_hiding = 1;
            }
            break;
        default:
            map_log("pc_bonus: unknown type %d %d !\n", static_cast<sint32>(type), val);
            break;
    }
    return 0;
}

/*==========================================
 * スクリプトによるスキル所得
 *------------------------------------------
 */
sint32 pc_skill(MapSessionData *sd, sint32 id, sint32 level, sint32 flag)
{
    nullpo_ret(sd);

    if (level > MAX_SKILL_LEVEL)
    {
        map_log("suppormap_logt card skill only!\n");
        return 0;
    }
    if (!flag && (sd->status.skill[id].id == id || level == 0))
    {                           // クエスト所得ならここで条件を確認して送信する
        sd->status.skill[id].lv = level;
        pc_calcstatus(sd, 0);
        clif_skillinfoblock(sd);
    }
    else if (sd->status.skill[id].lv < level)
    {                           // 覚えられるがlvが小さいなら
        sd->status.skill[id].id = id;
        sd->status.skill[id].lv = level;
    }

    return 0;
}

//
// アイテム物
//

/*==========================================
 * アイテムを買った時に、新しいアイテム欄を使うか、
 * 3万個制限にかかるか確認
 *------------------------------------------
 */
sint32 pc_checkadditem(MapSessionData *sd, sint32 nameid, sint32 amount)
{
    sint32 i;

    nullpo_ret(sd);

    if (itemdb_isequip(nameid))
        return ADDITEM_NEW;

    for (i = 0; i < MAX_INVENTORY; i++)
    {
        if (sd->status.inventory[i].nameid == nameid)
        {
            if (sd->status.inventory[i].amount + amount > MAX_AMOUNT)
                return ADDITEM_OVERAMOUNT;
            return ADDITEM_EXIST;
        }
    }

    if (amount > MAX_AMOUNT)
        return ADDITEM_OVERAMOUNT;
    return ADDITEM_NEW;
}

/*==========================================
 * 空きアイテム欄の個数
 *------------------------------------------
 */
sint32 pc_inventoryblank(MapSessionData *sd)
{
    sint32 i, b;

    nullpo_ret(sd);

    for (i = 0, b = 0; i < MAX_INVENTORY; i++)
    {
        if (sd->status.inventory[i].nameid == 0)
            b++;
    }

    return b;
}

/*==========================================
 * お金を払う
 *------------------------------------------
 */
sint32 pc_payzeny(MapSessionData *sd, sint32 zeny)
{
    double z;

    nullpo_ret(sd);

    z = sd->status.zeny;
    if (sd->status.zeny < zeny || z - zeny > MAX_ZENY)
        return 1;
    sd->status.zeny -= zeny;
    clif_updatestatus(sd, SP::ZENY);

    return 0;
}

/*==========================================
 * お金を得る
 *------------------------------------------
 */
sint32 pc_getzeny(MapSessionData *sd, sint32 zeny)
{
    double z;

    nullpo_ret(sd);

    z = sd->status.zeny;
    if (z + zeny > MAX_ZENY)
    {
        zeny = 0;
        sd->status.zeny = MAX_ZENY;
    }
    sd->status.zeny += zeny;
    clif_updatestatus(sd, SP::ZENY);

    return 0;
}

/*==========================================
 * アイテムを探して、インデックスを返す
 *------------------------------------------
 */
sint32 pc_search_inventory(MapSessionData *sd, sint32 item_id)
{
    sint32 i;

    nullpo_retr(-1, sd);

    for (i = 0; i < MAX_INVENTORY; i++)
    {
        if (sd->status.inventory[i].nameid == item_id &&
            (sd->status.inventory[i].amount > 0 || item_id == 0))
            return i;
    }

    return -1;
}

sint32 pc_count_all_items(MapSessionData *player, sint32 item_id)
{
    sint32 i;
    sint32 count = 0;

    nullpo_ret(player);

    for (i = 0; i < MAX_INVENTORY; i++)
    {
        if (player->status.inventory[i].nameid == item_id)
            count += player->status.inventory[i].amount;
    }

    return count;
}

sint32 pc_remove_items(MapSessionData *player, sint32 item_id, sint32 count)
{
    sint32 i;

    nullpo_ret(player);

    for (i = 0; i < MAX_INVENTORY && count; i++)
    {
        if (player->status.inventory[i].nameid == item_id)
        {
            sint32 to_delete = count;
            /* only delete as much as we have */
            if (to_delete > player->status.inventory[i].amount)
                to_delete = player->status.inventory[i].amount;

            count -= to_delete;

            pc_delitem(player, i, to_delete,
                        0 /* means `really delete and update status' */ );

            if (!count)
                return 0;
        }
    }
    return 0;
}

/*==========================================
 * アイテム追加。個数のみitem構造体の数字を無視
 *------------------------------------------
 */
PickupFail pc_additem(MapSessionData *sd, struct item *item_data,
                      sint32 amount)
{

    MAP_LOG_PC(sd, "PICKUP %d %d", item_data->nameid, amount);

    nullpo_retr(PickupFail::BAD_ITEM, sd);
    nullpo_retr(PickupFail::BAD_ITEM, item_data);

    if (item_data->nameid <= 0 || amount <= 0)
        return PickupFail::BAD_ITEM;
    struct item_data *data = itemdb_search(item_data->nameid);
    sint32 w = data->weight * amount;
    if (w + sd->weight > sd->max_weight)
        return PickupFail::TOO_HEAVY;

    sint32 i = MAX_INVENTORY;

    if (!itemdb_isequip2(data))
    {
        // 装 備品ではないので、既所有品なら個数のみ変化させる
        for (i = 0; i < MAX_INVENTORY; i++)
            if (compare_item(&sd->status.inventory[i], item_data))
            {
                if (sd->status.inventory[i].amount + amount > MAX_AMOUNT)
                    return PickupFail::STACK_FULL;
                sd->status.inventory[i].amount += amount;
                clif_additem(sd, i, amount, PickupFail::OKAY);
                break;
            }
    }
    if (i >= MAX_INVENTORY)
    {
        // 装 備品か未所有品だったので空き欄へ追加
        i = pc_search_inventory(sd, 0);
        if (i >= 0)
        {
            memcpy(&sd->status.inventory[i], item_data,
                    sizeof(sd->status.inventory[0]));

            if (item_data->equip != EPOS::NONE)
                sd->status.inventory[i].equip = EPOS::NONE;

            sd->status.inventory[i].amount = amount;
            sd->inventory_data[i] = data;
            clif_additem(sd, i, amount, PickupFail::OKAY);
        }
        else
            return PickupFail::INV_FULL;
    }
    sd->weight += w;
    clif_updatestatus(sd, SP::WEIGHT);

    return PickupFail::OKAY;
}

/*==========================================
 * アイテムを減らす
 *------------------------------------------
 */
sint32 pc_delitem(MapSessionData *sd, sint32 n, sint32 amount, sint32 type)
{
    nullpo_retr(1, sd);

    if (sd->trade_partner)
        trade_tradecancel(sd);

    if (sd->status.inventory[n].nameid == 0 || amount <= 0
        || sd->status.inventory[n].amount < amount
        || sd->inventory_data[n] == NULL)
        return 1;

    sd->status.inventory[n].amount -= amount;
    sd->weight -= sd->inventory_data[n]->weight * amount;
    if (sd->status.inventory[n].amount <= 0)
    {
        if (sd->status.inventory[n].equip != EPOS::NONE)
            pc_unequipitem(sd, n, CalcStatus::NOW);
        memset(&sd->status.inventory[n], 0,
                sizeof(sd->status.inventory[0]));
        sd->inventory_data[n] = NULL;
    }
    if (!(type & 1))
        clif_delitem(sd, n, amount);
    if (!(type & 2))
        clif_updatestatus(sd, SP::WEIGHT);

    return 0;
}

/*==========================================
 * アイテムを落す
 *------------------------------------------
 */
sint32 pc_dropitem(MapSessionData *sd, sint32 n, sint32 amount)
{
    nullpo_retr(1, sd);

    if (sd->trade_partner || sd->npc_id || sd->state.storage_flag)
        return 0;               // no dropping while trading/npc/storage

    if (n < 0 || n >= MAX_INVENTORY)
        return 0;

    if (amount <= 0)
        return 0;

    pc_unequipinvyitem(sd, n, CalcStatus::NOW);

    if (sd->status.inventory[n].nameid <= 0 ||
        sd->status.inventory[n].amount < amount ||
        sd->trade_partner || sd->status.inventory[n].amount <= 0)
        return 1;
    map_addflooritem(&sd->status.inventory[n], amount, sd->m, sd->x,
                      sd->y, NULL, NULL, NULL);
    pc_delitem(sd, n, amount, 0);

    return 0;
}

/*==========================================
 * アイテムを拾う
 *------------------------------------------
 */

static sint32 can_pick_item_up_from(MapSessionData *self, BlockID other_id)
{
    struct party *p = party_search(self->status.party_id);

    /* From ourselves or from no-one? */
    if (!self || self->id == other_id || !other_id)
        return 1;

    MapSessionData *other = map_id2sd(other_id);

    /* Other no longer exists? */
    if (!other)
        return 1;

    /* From our partner? */
    if (self->status.partner_id == other->status.char_id)
        return 1;

    /* From a party member? */
    if (self->status.party_id
        && self->status.party_id == other->status.party_id
        && p && p->item != 0)
        return 1;

    /* From someone who is far away? */
    /* On another map? */
    if (other->m != self->m)
        return 1;
    else
    {
        sint32 distance_x = abs(other->x - self->x);
        sint32 distance_y = abs(other->y - self->y);
        return max(distance_x, distance_y) > battle_config.drop_pickup_safety_zone;
    }
}

sint32 pc_takeitem(MapSessionData *sd, struct flooritem_data *fitem)
{
    tick_t tick = gettick();
    sint32 can_take;

    nullpo_ret(sd);
    nullpo_ret(fitem);

    /* Sometimes the owners reported to us are buggy: */
    // Then why didn't you fix it?

    if (fitem->first_get_id == fitem->third_get_id
        || fitem->second_get_id == fitem->third_get_id)
        fitem->third_get_id = DEFAULT;

    if (fitem->first_get_id == fitem->second_get_id)
    {
        fitem->second_get_id = fitem->third_get_id;
        fitem->third_get_id = DEFAULT;
    }

    can_take = can_pick_item_up_from(sd, fitem->first_get_id);
    if (!can_take)
        can_take = fitem->first_get_tick <= tick
            && can_pick_item_up_from(sd, fitem->second_get_id);

    if (!can_take)
        can_take = fitem->second_get_tick <= tick
            && can_pick_item_up_from(sd, fitem->third_get_id);

    if (!can_take)
        can_take = fitem->third_get_tick <= tick;

    if (can_take)
    {
        /* Can pick up */
        PickupFail flag = pc_additem(sd, &fitem->item_data, fitem->item_data.amount);
        if (flag != PickupFail::OKAY)
            // 重量overで取得失敗
            clif_additem(sd, 0, 0, flag);
        else
        {
            // 取得成功
            if (sd->attacktimer)
                pc_stopattack(sd);
            clif_takeitem(sd, fitem);
            map_clearflooritem(fitem->id);
        }
        return 0;
    }

    /* Otherwise, we can't pick up */
    clif_additem(sd, 0, 0, PickupFail::DROP_STEAL);
    return 0;
}

static sint32 pc_isUseitem(MapSessionData *sd, sint32 n)
{
    struct item_data *item;
    sint32 nameid;

    nullpo_ret(sd);

    item = sd->inventory_data[n];
    nameid = sd->status.inventory[n].nameid;

    if (item == NULL)
        return 0;
    if (itemdb_type(nameid) != 0)
        return 0;
    if (nameid == 601 && maps[sd->m].flag.noteleport)
    {
        return 0;
    }

    if (nameid == 602 && maps[sd->m].flag.noreturn)
        return 0;
    if (nameid == 604 && maps[sd->m].flag.nobranch)
        return 0;
    if (item->sex != 2 && sd->status.sex != item->sex)
        return 0;
    if (item->elv && sd->status.base_level < item->elv)
        return 0;

    return 1;
}

/*==========================================
 * アイテムを使う
 *------------------------------------------
 */
sint32 pc_useitem(MapSessionData *sd, sint32 n)
{
    sint32 amount;

    nullpo_retr(1, sd);

    if (n >= 0 && n < MAX_INVENTORY && sd->inventory_data[n])
    {
        amount = sd->status.inventory[n].amount;
        if (sd->status.inventory[n].nameid <= 0
            || sd->status.inventory[n].amount <= 0
            || !pc_isUseitem(sd, n))
        {
            clif_useitemack(sd, n, 0, 0);
            return 1;
        }

        run_script(sd->inventory_data[n]->use_script, 0, sd->id, DEFAULT);

        clif_useitemack(sd, n, amount - 1, 1);
        pc_delitem(sd, n, 1, 1);
    }

    return 0;
}

//
//
//
/*==========================================
 * PCの位置設定
 *------------------------------------------
 */
sint32 pc_setpos(MapSessionData *sd, const Point& point_org, BeingRemoveType clrtype)
{
    sint32 m = 0;

    nullpo_ret(sd);

    if (sd->trade_partner)      // 取引を中断する
        trade_tradecancel(sd);
    if (sd->state.storage_flag == 1)
        storage_storage_quit(sd);  // 倉庫を開いてるなら保存する

    if (sd->party_invite)   // パーティ勧誘を拒否する
        party_reply_invite(sd, sd->party_invite_account, 0);

    skill_castcancel(sd);  // 詠唱中断
    pc_stop_walking(sd, 0);    // 歩行中断
    pc_stopattack(sd);         // 攻撃中断

    Point point = point_org;
    if (!point.map.contains(".gat") && point.map.length() < 12)
    {
        strcat(&point.map, ".gat");
    }

    m = map_mapname2mapid(point.map);
    if (m < 0)
    {
        if (sd->mapname[0])
        {
            IP_Address ip;
            in_port_t port;
            if (map_mapname2ipport(point.map, &ip, &port))
            {
                clif_being_remove(sd, clrtype);
                map_delblock(sd);
                sd->mapname = point.map;
                sd->x = point.x;
                sd->y = point.y;
                sd->state.waitingdisconnect = 1;
                pc_makesavestatus(sd);
                //The storage close routines save the char data. [Skotlex]
                if (!sd->state.storage_flag)
                    chrif_save(sd);
                else if (sd->state.storage_flag == 1)
                    storage_storage_quit(sd);

                chrif_changemapserver(sd, point, ip, port);
                return 0;
            }
        }
#if 0
        clif_authfail_fd(sd->fd, 0);   // cancel
        clif_setwaitclose(sd->fd);
#endif
        return 1;
    }

    sint32 x = point.x;
    sint32 y = point.y;
    if (x < 0 || x >= maps[m].xs || y < 0 || y >= maps[m].ys)
        x = y = 0;
    if ((x == 0 && y == 0) || (read_gat(m, x, y) & MapCell::SOLID))
    {
        if (x || y)
        {
            map_log("stacked (%d,%d)\n", x, y);
        }
        do
        {
            x = MRAND(maps[m].xs - 2) + 1;
            y = MRAND(maps[m].ys - 2) + 1;
        }
        while (read_gat(m, x, y) & MapCell::SOLID);
    }

    if (sd->mapname[0] && sd->prev != NULL)
    {
        clif_being_remove(sd, clrtype);
        map_delblock(sd);
        clif_changemap(sd, point); // [MouseJstr]
    }

    sd->mapname = point.map;
    sd->m = m;
    sd->to_x = x;
    sd->to_y = y;

    // moved and changed dance effect stopping

    sd->x = x;
    sd->y = y;

//  map_addblock(sd);  // ブロック登録とspawnは
//  clif_spawnpc(sd);

    return 0;
}

/*==========================================
 * PCのランダムワープ
 *------------------------------------------
 */
sint32 pc_randomwarp(MapSessionData *sd, BeingRemoveType type)
{
    nullpo_ret(sd);

    sint32 m = sd->m;

    if (maps[sd->m].flag.noteleport)  // テレポート禁止
        return 0;

    sint32 i = 0;
    sint16 x, y;
    do
    {
        x = MRAND(maps[m].xs - 2) + 1;
        y = MRAND(maps[m].ys - 2) + 1;
    }
    while ((read_gat(m, x, y) & MapCell::SOLID) && (i++) < 1000);

    if (i < 1000)
        pc_setpos(sd, Point{maps[m].name, x, y}, type);

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
sint32 pc_can_reach(MapSessionData *sd, sint32 x, sint32 y)
{
    struct walkpath_data wpd;

    nullpo_ret(sd);

    if (sd->x == x && sd->y == y) // 同じマス
        return 1;

    // 障害物判定
    wpd.path_len = 0;
    wpd.path_pos = 0;
    wpd.path_half = 0;
    return path_search(&wpd, sd->m, sd->x, sd->y, x, y, 0) != -1;
}

//
// 歩 行物
//
/*==========================================
 * 次の1歩にかかる時間を計算
 *------------------------------------------
 */
static interval_t calc_next_walk_step(MapSessionData *sd)
{
    nullpo_ret(sd);

    if (sd->walkpath.path_pos >= sd->walkpath.path_len)
        return interval_t::zero();
    if (static_cast<sint32>(sd->walkpath.path[sd->walkpath.path_pos]) & 1)
        return sd->speed * 14 / 10;

    return sd->speed;
}

/*==========================================
 * 半歩進む(timer関数)
 *------------------------------------------
 */
static void pc_walk(timer_id, tick_t tick, BlockID id, uint8 data)
{
    MapSessionData *sd;
    sint32 moveblock;
    sint32 x, y, dx, dy;

    sd = map_id2sd(id);
    if (sd == NULL)
        return;

    sd->walktimer = NULL;
    if (sd->walkpath.path_pos >= sd->walkpath.path_len
        || sd->walkpath.path_pos != data)
        return;

    sd->walkpath.path_half ^= 1;
    if (sd->walkpath.path_half == 0)
    {                           // マス目中心へ到着
        sd->walkpath.path_pos++;
        if (sd->state.change_walk_target)
        {
            pc_walktoxy_sub(sd);
            return;
        }
    }
    else
    {                           // マス目境界へ到着
        if (static_cast<sint32>(sd->walkpath.path[sd->walkpath.path_pos]) >= 8)
            return;

        x = sd->x;
        y = sd->y;

        if (map_getcell(sd->m, x, y) & MapCell::SOLID)
        {
            pc_stop_walking(sd, 1);
            return;
        }
        sd->dir = sd->head_dir = sd->walkpath.path[sd->walkpath.path_pos];
        dx = dirx[static_cast<sint32>(sd->dir)];
        dy = diry[static_cast<sint32>(sd->dir)];

        if (map_getcell(sd->m, x + dx, y + dy) & MapCell::SOLID)
        {
            pc_walktoxy_sub(sd);
            return;
        }

        moveblock = (x / BLOCK_SIZE != (x + dx) / BLOCK_SIZE
                     || y / BLOCK_SIZE != (y + dy) / BLOCK_SIZE);

        map_foreachinmovearea(clif_pcoutsight, sd->m, x - AREA_SIZE,
                              y - AREA_SIZE, x + AREA_SIZE, y + AREA_SIZE,
                              dx, dy, BL_NUL, sd);

        x += dx;
        y += dy;

        if (moveblock)
            map_delblock(sd);
        sd->x = x;
        sd->y = y;
        if (moveblock)
            map_addblock(sd);

        map_foreachinmovearea(clif_pcinsight, sd->m, x - AREA_SIZE,
                              y - AREA_SIZE, x + AREA_SIZE, y + AREA_SIZE,
                              -dx, -dy, BL_NUL, sd);

        if (sd->status.party_id)
        {                       // パーティのＨＰ情報通知検査
            struct party *p = party_search(sd->status.party_id);
            if (p != NULL)
            {
                bool p_flag = 0;
                map_foreachinmovearea(party_send_hp_check, sd->m,
                                      x - AREA_SIZE, y - AREA_SIZE,
                                      x + AREA_SIZE, y + AREA_SIZE, -dx, -dy,
                                      BL_PC, sd->status.party_id, &p_flag);
                if (p_flag)
                    sd->party_hp = -1;
            }
        }

        if (map_getcell(sd->m, x, y) & MapCell::NPC)
            npc_touch_areanpc(sd, sd->m, x, y);
        else
            sd->areanpc_id = DEFAULT;
    }
    interval_t i = calc_next_walk_step(sd);

    if (i != interval_t::zero())
    {
        i /= 2;
        if (i < interval_t(1) && sd->walkpath.path_half == 0)
            i = interval_t(1);
        sd->walktimer = add_timer(tick + i, pc_walk, id, sd->walkpath.path_pos);
    }
}

/*==========================================
 * 移動可能か確認して、可能なら歩行開始
 *------------------------------------------
 */
static sint32 pc_walktoxy_sub(MapSessionData *sd)
{
    struct walkpath_data wpd;

    nullpo_retr(1, sd);

    if (path_search(&wpd, sd->m, sd->x, sd->y, sd->to_x, sd->to_y, 0))
        return 1;
    memcpy(&sd->walkpath, &wpd, sizeof(wpd));

    clif_walkok(sd);
    sd->state.change_walk_target = 0;

    interval_t i = calc_next_walk_step(sd);
    if (i != interval_t::zero())
    {
        i /= 4;
        sd->walktimer = add_timer(gettick() + i, pc_walk, sd->id, static_cast<uint8>(0));
    }
    clif_movechar(sd);

    return 0;
}

/*==========================================
 * pc歩 行要求
 *------------------------------------------
 */
sint32 pc_walktoxy(MapSessionData *sd, sint32 x, sint32 y)
{

    nullpo_ret(sd);

    sd->to_x = x;
    sd->to_y = y;

    if (pc_issit(sd))
        pc_setstand(sd);

    if (sd->walktimer && sd->state.change_walk_target == 0)
    {
        // 現在歩いている最中の目的地変更なのでマス目の中心に来た時に
        // timer関数からpc_walktoxy_subを呼ぶようにする
        sd->state.change_walk_target = 1;
    }
    else
    {
        pc_walktoxy_sub(sd);
    }

    return 0;
}

/*==========================================
 * 歩 行停止
 *------------------------------------------
 */
sint32 pc_stop_walking(MapSessionData *sd, sint32 type)
{
    nullpo_ret(sd);

    if (sd->walktimer)
    {
        delete_timer(sd->walktimer);
        sd->walktimer = NULL;
    }
    sd->walkpath.path_len = 0;
    sd->to_x = sd->x;
    sd->to_y = sd->y;
    if (type & 0x01)
        clif_stop(sd);
    if (type & 0x02 && battle_config.pc_damage_delay)
    {
        tick_t tick = gettick();
        interval_t delay = battle_get_dmotion(sd);
        if (sd->canmove_tick < tick)
            sd->canmove_tick = tick + delay;
    }

    return 0;
}

void pc_touch_all_relevant_npcs(MapSessionData *sd)
{
    if (map_getcell(sd->m, sd->x, sd->y) & MapCell::NPC)
        npc_touch_areanpc(sd, sd->m, sd->x, sd->y);
    else
        sd->areanpc_id = DEFAULT;
}

//
// 武器戦闘
//
/*==========================================
 * スキルの検索 所有していた場合Lvが返る
 *------------------------------------------
 */
sint32 pc_checkskill(MapSessionData *sd, sint32 skill_id)
{
    if (sd == NULL)
        return 0;
    if (skill_id >= MAX_SKILL)
    {
        return 0;
    }

    if (sd->status.skill[skill_id].id == skill_id)
        return (sd->status.skill[skill_id].lv);

    return 0;
}

/*==========================================
 * 装 備品のチェック
 *------------------------------------------
 */
sint32 pc_checkequip(MapSessionData *sd, EPOS pos)
{
    nullpo_retr(-1, sd);

    for (EQUIP i : EQUIPs)
    {
        if (pos & equip_pos[i])
            return sd->equip_index[i];
    }

    return -1;
}

/*==========================================
 * PCの攻撃 (timer関数)
 *------------------------------------------
 */
static void pc_attack_timer(timer_id, tick_t tick, BlockID id)
{
    MapSessionData *sd;
    BlockList *bl;
    sint32 dist, range;

    sd = map_id2sd(id);
    if (sd == NULL)
        return;
    sd->attacktimer = NULL;

    if (sd->prev == NULL)
        return;

    bl = map_id2bl(sd->attacktarget);
    if (bl == NULL || bl->prev == NULL)
        return;

    if (bl->type == BL_PC && pc_isdead(static_cast<MapSessionData *>(bl)))
        return;

    // 同じmapでないなら攻撃しない
    // PCが死んでても攻撃しない
    if (sd->m != bl->m || pc_isdead(sd))
        return;

    if (sd->opt1 > 0 || pc_ishiding(sd)) // 異常などで攻撃できない
        return;

    OPTION *opt = battle_get_option(bl);
    if (opt && *opt & (OPTION::HIDE | OPTION::CLOAK | OPTION::HIDE2))
        return;

    if (!battle_config.sdelay_attack_enable)
    {
        if (tick < sd->canact_tick)
            return;
    }

    if (sd->attackabletime > tick)
        return;               // cannot attack yet

    interval_t attack_spell_delay = sd->attack_spell_delay;
    if (sd->attack_spell_override   // [Fate] If we have an active attack spell, use that
        && spell_attack(id, sd->attacktarget))
    {
        // Return if the spell succeeded.  If the spell had disspiated, spell_attack() may fail.
        sd->attackabletime = tick + attack_spell_delay;

    }
    else
    {
        dist = distance(sd->x, sd->y, bl->x, bl->y);
        range = sd->attackrange;
        if (sd->status.weapon != 11)
            range++;
        if (dist > range)
        {                       // 届 かないので移動
            //if (pc_can_reach(sd, bl->x, bl->y))
            //clif_movetoattack(sd, bl);
            return;
        }

        if (dist <= range && !battle_check_range(sd, bl, range))
        {
            if (pc_can_reach(sd, bl->x, bl->y) && sd->canmove_tick < tick)
                // TMW client doesn't support this
                //pc_walktoxy(sd, bl->x, bl->y);
                clif_movetoattack(sd, bl);
            sd->attackabletime = tick + (sd->aspd * 2);
        }
        else
        {
            if (battle_config.pc_attack_direction_change)
                sd->dir = sd->head_dir = map_calc_dir(sd, bl->x, bl->y);  // 向き設定

            if (sd->walktimer)
                pc_stop_walking(sd, 1);

            map_freeblock_lock();
            pc_stop_walking(sd, 0);
            sd->attacktarget_lv = battle_weapon_attack(sd, bl, tick);
            map_freeblock_unlock();
            sd->attackabletime = tick + (sd->aspd * 2);
            if (sd->attackabletime <= tick)
                sd->attackabletime = tick + std::chrono::milliseconds(battle_config.max_aspd * 2);
        }
    }

    if (sd->state.attack_continue)
    {
        sd->attacktimer =
            add_timer(sd->attackabletime, pc_attack_timer, sd->id);
    }
}

/*==========================================
 * 攻撃要求
 * typeが1なら継続攻撃
 *------------------------------------------
 */
sint32 pc_attack(MapSessionData *sd, BlockID target_id, bool type)
{
    BlockList *bl;

    nullpo_ret(sd);

    bl = map_id2bl(target_id);
    if (bl == NULL)
        return 1;

    if (bl->type == BL_NPC)
    {                           // monster npcs [Valaris]
        npc_click(sd, target_id);
        return 0;
    }

    if (!battle_check_target(sd, bl))
        return 1;
    if (sd->attacktimer)
        pc_stopattack(sd);
    sd->attacktarget = target_id;
    sd->state.attack_continue = type;

    interval_t d = sd->attackabletime - gettick();
    if (d > interval_t::zero() && d < std::chrono::seconds(2))
    {
        sd->attacktimer = add_timer(sd->attackabletime, pc_attack_timer, sd->id);
    }
    else
    {
        // 本来timer関数なので引数を合わせる
        pc_attack_timer(NULL, gettick(), sd->id);
    }

    return 0;
}

/*==========================================
 * 継続攻撃停止
 *------------------------------------------
 */
sint32 pc_stopattack(MapSessionData *sd)
{
    nullpo_ret(sd);

    if (sd->attacktimer)
    {
        delete_timer(sd->attacktimer);
        sd->attacktimer = NULL;
    }
    sd->attacktarget = DEFAULT;
    sd->state.attack_continue = 0;

    return 0;
}

sint32 pc_checkbaselevelup(MapSessionData *sd)
{
    sint32 next = pc_nextbaseexp(sd);

    nullpo_ret(sd);

    if (sd->status.base_exp >= next && next > 0)
    {
        // base側レベルアップ処理
        sd->status.base_exp -= next;

        sd->status.base_level++;
        sd->status.status_point += (unwrap(sd->status.base_level) + 14) / 4;
        clif_updatestatus(sd, SP::STATUSPOINT);
        clif_updatestatus(sd, SP::BASELEVEL);
        clif_updatestatus(sd, SP::NEXTBASEEXP);
        pc_calcstatus(sd, 0);
        pc_heal(sd, sd->status.max_hp, sd->status.max_sp);

        clif_misceffect(sd, 0);
        //レベルアップしたのでパーティー情報を更新する
        //(公平範囲チェック)
        party_send_movemap(sd);
        MAP_LOG_XP(sd, "LEVELUP");
        return 1;
    }

    return 0;
}

static int raise_cost(int x)
{
    return (x * (x - 1)) >> 1;
}

/*========================================
 * Compute the maximum for sd->skill_point, i.e., the max. number of skill points that can still be filled in
 *----------------------------------------
 */
static sint32 pc_skillpt_potential(MapSessionData *sd)
{
    sint32 skill_id;
    sint32 potential = 0;

    for (skill_id = 0; skill_id < MAX_SKILL; skill_id++)
        if (sd->status.skill[skill_id].id != 0
            && sd->status.skill[skill_id].lv < skill_db[skill_id].max_raise)
            potential += raise_cost(skill_db[skill_id].max_raise)
                - raise_cost(sd->status.skill[skill_id].lv);

    return potential;
}

sint32 pc_checkjoblevelup(MapSessionData *sd)
{
    sint32 next = pc_nextjobexp(sd);

    nullpo_ret(sd);

    if (sd->status.job_exp >= next && next > 0)
    {
        if (pc_skillpt_potential(sd) < sd->status.skill_point)
        {                       // [Fate] Bah, this is is painful.
            // But the alternative is quite error-prone, and eAthena has far worse performance issues...
            sd->status.job_exp = next - 1;
            pc_calcstatus(sd, 0);
            return 0;
        }

        // job側レベルアップ処理
        sd->status.job_exp -= next;
        clif_updatestatus(sd, SP::NEXTJOBEXP);
        sd->status.skill_point++;
        clif_updatestatus(sd, SP::SKILLPOINT);
        pc_calcstatus(sd, 0);

        MAP_LOG_PC(sd, "SKILLPOINTS-UP %d", sd->status.skill_point);

        if (unwrap(sd->status.job_level) < 250
            && unwrap(sd->status.job_level) < unwrap(sd->status.base_level) * 2)
            sd->status.job_level++; // Make levelling up a little harder

        clif_misceffect(sd, 1);
        return 1;
    }

    return 0;
}

/*==========================================
 * 経験値取得
 *------------------------------------------
 */
sint32 pc_gainexp(MapSessionData *sd, sint32 base_exp, sint32 job_exp)
{
    return pc_gainexp_reason(sd, base_exp, job_exp, PC_GAINEXP_REASON::KILLING);
}

sint32 pc_gainexp_reason(MapSessionData *sd, sint32 base_exp, sint32 job_exp,
                      PC_GAINEXP_REASON reason)
{
    char output[256];
    nullpo_ret(sd);

    if (sd->prev == NULL || pc_isdead(sd))
        return 0;

    if ((battle_config.pvp_exp == 0) && maps[sd->m].flag.pvp) // [MouseJstr]
        return 0;               // no exp on pvp maps

    MAP_LOG_PC(sd, "GAINXP %d %d %s", base_exp, job_exp,
               (reason == PC_GAINEXP_REASON::SCRIPT)
               ? "SCRIPTXP"
               : ((reason == PC_GAINEXP_REASON::HEALING)
                  ? "HEALXP"
                  : "KILLXP"));

    if (!battle_config.multi_level_up && pc_nextbaseafter(sd))
    {
        while (sd->status.base_exp + base_exp >= pc_nextbaseafter(sd)
               && sd->status.base_exp <= pc_nextbaseexp(sd)
               && pc_nextbaseafter(sd) > 0)
        {
            base_exp *= .90;
        }
    }

    sd->status.base_exp += base_exp;

    // [Fate] Adjust experience points that healers can extract from this character
    if (reason != PC_GAINEXP_REASON::HEALING)
    {
        const sint32 max_heal_xp = 20 + (unwrap(sd->status.base_level) * unwrap(sd->status.base_level));

        sd->heal_xp += base_exp;
        if (sd->heal_xp > max_heal_xp)
            sd->heal_xp = max_heal_xp;
    }

    if (sd->status.base_exp < 0)
        sd->status.base_exp = 0;

    while (pc_checkbaselevelup(sd));

    clif_updatestatus(sd, SP::BASEEXP);
    if (!battle_config.multi_level_up && pc_nextjobafter(sd))
    {
        while (sd->status.job_exp + job_exp >= pc_nextjobafter(sd)
               && sd->status.job_exp <= pc_nextjobexp(sd)
               && pc_nextjobafter(sd) > 0)
        {
            job_exp *= .90;
        }
    }

    sd->status.job_exp += job_exp;
    if (sd->status.job_exp < 0)
        sd->status.job_exp = 0;

    while (pc_checkjoblevelup(sd));

    clif_updatestatus(sd, SP::JOBEXP);

    if (battle_config.disp_experience)
    {
        sprintf(output,
                 "Experienced Gained Base:%d Job:%d", base_exp, job_exp);
        clif_disp_onlyself(sd, output, strlen(output));
    }

    return 0;
}

sint32 pc_extract_healer_exp(MapSessionData *sd, sint32 max)
{
    sint32 amount;
    nullpo_ret(sd);

    amount = sd->heal_xp;
    if (max < amount)
        amount = max;

    sd->heal_xp -= amount;
    return amount;
}

/*==========================================
 * base level側必要経験値計算
 *------------------------------------------
 */
sint32 pc_nextbaseexp(MapSessionData *sd)
{
    nullpo_ret(sd);

    return exp_table_new[unwrap(sd->status.base_level) - 1][0];
}

/*==========================================
 * job level側必要経験値計算
 *------------------------------------------
 */
sint32 pc_nextjobexp(MapSessionData *sd)
{
    // [fate]  For normal levels, this ranges from 20k to 50k, depending on job level.
    // Job level is at most twice the player's experience level (base_level).  Levelling
    // from 2 to 9 is 44 points, i.e., 880k to 2.2M job experience points (this is per
    // skill, obviously.)

    return 20000 + unwrap(sd->status.job_level) * 150;
}

/*==========================================
 * base level after next [Valaris]
 *------------------------------------------
 */
sint32 pc_nextbaseafter(MapSessionData *sd)
{
    nullpo_ret(sd);

    return exp_table_new[unwrap(sd->status.base_level)][0];
}

/*==========================================
 * job level after next [Valaris]
 *------------------------------------------
 */
sint32 pc_nextjobafter(MapSessionData *sd)
{
    nullpo_ret(sd);

    return exp_table_new[unwrap(sd->status.job_level)][7];
}

/*==========================================
 * 必要ステータスポイント計算
 *------------------------------------------
 */
sint32 pc_need_status_point(MapSessionData *sd, SP type)
{
    nullpo_retr(-1, sd);

    if (!SP_IS_BASE_ATTR(type))
        return -1;

    return (sd->status.stats[ATTR_FROM_SP_BASE(type)] + 9) / 10 + 1;
}

/*==========================================
 * 能力値成長
 *------------------------------------------
 */
sint32 pc_statusup(MapSessionData *sd, SP type)
{
    sint32 need, val = 0;

    nullpo_ret(sd);

    switch (type)
    {
        case SP::STR:
            val = sd->status.stats[ATTR::STR];
            break;
        case SP::AGI:
            val = sd->status.stats[ATTR::AGI];
            break;
        case SP::VIT:
            val = sd->status.stats[ATTR::VIT];
            break;
        case SP::INT:
            val = sd->status.stats[ATTR::INT];
            break;
        case SP::DEX:
            val = sd->status.stats[ATTR::DEX];
            break;
        case SP::LUK:
            val = sd->status.stats[ATTR::LUK];
            break;
    }

    need = pc_need_status_point(sd, type);
    if (type < SP::STR || type > SP::LUK || need < 0
        || need > sd->status.status_point
        || val >= battle_config.max_parameter)
    {
        clif_statusupack(sd, type, false, val);
        return 1;
    }
    switch (type)
    {
        case SP::STR:
            val = ++sd->status.stats[ATTR::STR];
            break;
        case SP::AGI:
            val = ++sd->status.stats[ATTR::AGI];
            break;
        case SP::VIT:
            val = ++sd->status.stats[ATTR::VIT];
            break;
        case SP::INT:
            val = ++sd->status.stats[ATTR::INT];
            break;
        case SP::DEX:
            val = ++sd->status.stats[ATTR::DEX];
            break;
        case SP::LUK:
            val = ++sd->status.stats[ATTR::LUK];
            break;
    }
    sd->status.status_point -= need;
    if (need != pc_need_status_point(sd, type))
    {
        clif_updatestatus(sd, ATTR_TO_SP_UP(ATTR_FROM_SP_BASE(type)));
    }
    clif_updatestatus(sd, SP::STATUSPOINT);
    clif_updatestatus(sd, type);
    pc_calcstatus(sd, 0);
    clif_statusupack(sd, type, true, val);

    MAP_LOG_STATS(sd, "STATUP");

    return 0;
}

/*==========================================
 * 能力値成長
 *------------------------------------------
 */
sint32 pc_statusup2(MapSessionData *sd, SP type, sint32 val)
{
    nullpo_ret(sd);

    if (type < SP::STR || type > SP::LUK)
    {
        clif_statusupack(sd, type, false, 0);
        return 1;
    }
    switch (type)
    {
        case SP::STR:
            if (sd->status.stats[ATTR::STR] + val >= battle_config.max_parameter)
                val = battle_config.max_parameter;
            else if (sd->status.stats[ATTR::STR] + val < 1)
                val = 1;
            else
                val += sd->status.stats[ATTR::STR];
            sd->status.stats[ATTR::STR] = val;
            break;
        case SP::AGI:
            if (sd->status.stats[ATTR::AGI] + val >= battle_config.max_parameter)
                val = battle_config.max_parameter;
            else if (sd->status.stats[ATTR::AGI] + val < 1)
                val = 1;
            else
                val += sd->status.stats[ATTR::AGI];
            sd->status.stats[ATTR::AGI] = val;
            break;
        case SP::VIT:
            if (sd->status.stats[ATTR::VIT] + val >= battle_config.max_parameter)
                val = battle_config.max_parameter;
            else if (sd->status.stats[ATTR::VIT] + val < 1)
                val = 1;
            else
                val += sd->status.stats[ATTR::VIT];
            sd->status.stats[ATTR::VIT] = val;
            break;
        case SP::INT:
            if (sd->status.stats[ATTR::INT] + val >= battle_config.max_parameter)
                val = battle_config.max_parameter;
            else if (sd->status.stats[ATTR::INT] + val < 1)
                val = 1;
            else
                val += sd->status.stats[ATTR::INT];
            sd->status.stats[ATTR::INT] = val;
            break;
        case SP::DEX:
            if (sd->status.stats[ATTR::DEX] + val >= battle_config.max_parameter)
                val = battle_config.max_parameter;
            else if (sd->status.stats[ATTR::DEX] + val < 1)
                val = 1;
            else
                val += sd->status.stats[ATTR::DEX];
            sd->status.stats[ATTR::DEX] = val;
            break;
        case SP::LUK:
            if (sd->status.stats[ATTR::LUK] + val >= battle_config.max_parameter)
                val = battle_config.max_parameter;
            else if (sd->status.stats[ATTR::LUK] + val < 1)
                val = 1;
            else
                val = sd->status.stats[ATTR::LUK] + val;
            sd->status.stats[ATTR::LUK] = val;
            break;
    }
    clif_updatestatus(sd, ATTR_TO_SP_UP(ATTR_FROM_SP_BASE(type)));
    clif_updatestatus(sd, type);
    pc_calcstatus(sd, 0);
    clif_statusupack(sd, type, true, val);
    MAP_LOG_STATS(sd, "STATUP2");

    return 0;
}

/*==========================================
 * スキルポイント割り振り
 *------------------------------------------
 */
sint32 pc_skillup(MapSessionData *sd, sint32 skill_num)
{
    nullpo_ret(sd);

    if (sd->status.skill[skill_num].id != 0
        && sd->status.skill_point >= sd->status.skill[skill_num].lv
        && sd->status.skill[skill_num].lv < skill_db[skill_num].max_raise)
    {
        sd->status.skill_point -= sd->status.skill[skill_num].lv;
        sd->status.skill[skill_num].lv++;

        pc_calcstatus(sd, 0);
        clif_skillup(sd, skill_num);
        clif_updatestatus(sd, SP::SKILLPOINT);
        clif_skillinfoblock(sd);
        MAP_LOG_PC(sd, "SKILLUP %d %d %d",
                   skill_num, sd->status.skill[skill_num].lv, skill_power(sd, skill_num));
    }

    return 0;
}

/*==========================================
 * /resetlvl
 *------------------------------------------
 */
sint32 pc_resetlvl(MapSessionData *sd, sint32 type)
{
    nullpo_ret(sd);

    for (sint32 i = 1; i < MAX_SKILL; i++)
    {
        sd->status.skill[i].lv = 0;
    }

    if (type == 1)
    {
        sd->status.skill_point = 0;
        sd->status.base_level = level_t(1);
        sd->status.job_level = level_t(1);
        sd->status.base_exp = 0;
        sd->status.job_exp = 0;
        sd->status.option = OPTION::NONE;

        sd->status.stats[ATTR::STR] = 1;
        sd->status.stats[ATTR::AGI] = 1;
        sd->status.stats[ATTR::VIT] = 1;
        sd->status.stats[ATTR::INT] = 1;
        sd->status.stats[ATTR::DEX] = 1;
        sd->status.stats[ATTR::LUK] = 1;
    }

    if (type == 2)
    {
        sd->status.skill_point = 0;
        sd->status.base_level = level_t(1);
        sd->status.job_level = level_t(1);
        sd->status.base_exp = 0;
        sd->status.job_exp = 0;
    }
    if (type == 3)
    {
        sd->status.base_level = level_t(1);
        sd->status.base_exp = 0;
    }
    if (type == 4)
    {
        sd->status.job_level = level_t(1);
        sd->status.job_exp = 0;
    }

    clif_updatestatus(sd, SP::STATUSPOINT);
    clif_updatestatus(sd, SP::STR);
    clif_updatestatus(sd, SP::AGI);
    clif_updatestatus(sd, SP::VIT);
    clif_updatestatus(sd, SP::INT);
    clif_updatestatus(sd, SP::DEX);
    clif_updatestatus(sd, SP::LUK);
    clif_updatestatus(sd, SP::BASELEVEL);
    clif_updatestatus(sd, SP::JOBLEVEL);
    clif_updatestatus(sd, SP::STATUSPOINT);
    clif_updatestatus(sd, SP::NEXTBASEEXP);
    clif_updatestatus(sd, SP::NEXTJOBEXP);
    clif_updatestatus(sd, SP::SKILLPOINT);

    clif_updatestatus(sd, SP::USTR);    // Updates needed stat points - Valaris
    clif_updatestatus(sd, SP::UAGI);
    clif_updatestatus(sd, SP::UVIT);
    clif_updatestatus(sd, SP::UINT);
    clif_updatestatus(sd, SP::UDEX);
    clif_updatestatus(sd, SP::ULUK);    // End Addition

    for (EQUIP i : EQUIPs)
    {
        // unequip items that can't be equipped by base 1 [Valaris]
        if (sd->equip_index[i] >= 0)
            if (!pc_isequip(sd, sd->equip_index[i]))
            {
                pc_unequipitem(sd, sd->equip_index[i], CalcStatus::LATER);
                sd->equip_index[i] = -1;
            }
    }

    clif_skillinfoblock(sd);
    pc_calcstatus(sd, 0);

    MAP_LOG_STATS(sd, "STATRESET");

    return 0;
}

/*==========================================
 * /resetstate
 *------------------------------------------
 */
sint32 pc_resetstate(MapSessionData *sd)
{
    nullpo_ret(sd);

    sd->status.status_point = stat_points[unwrap(sd->status.base_level)];

    clif_updatestatus(sd, SP::STATUSPOINT);

    sd->status.stats[ATTR::STR] = 1;
    sd->status.stats[ATTR::AGI] = 1;
    sd->status.stats[ATTR::VIT] = 1;
    sd->status.stats[ATTR::INT] = 1;
    sd->status.stats[ATTR::DEX] = 1;
    sd->status.stats[ATTR::LUK] = 1;

    clif_updatestatus(sd, SP::STR);
    clif_updatestatus(sd, SP::AGI);
    clif_updatestatus(sd, SP::VIT);
    clif_updatestatus(sd, SP::INT);
    clif_updatestatus(sd, SP::DEX);
    clif_updatestatus(sd, SP::LUK);

    clif_updatestatus(sd, SP::USTR);    // Updates needed stat points - Valaris
    clif_updatestatus(sd, SP::UAGI);
    clif_updatestatus(sd, SP::UVIT);
    clif_updatestatus(sd, SP::UINT);
    clif_updatestatus(sd, SP::UDEX);
    clif_updatestatus(sd, SP::ULUK);    // End Addition

    pc_calcstatus(sd, 0);

    return 0;
}

/*==========================================
 * /resetskill
 *------------------------------------------
 */
sint32 pc_resetskill(MapSessionData *sd)
{
    sint32 i, skill;

    nullpo_ret(sd);

    sd->status.skill_point += pc_calc_skillpoint(sd);

    for (i = 1; i < MAX_SKILL; i++)
        if ((skill = pc_checkskill(sd, i)) > 0)
        {
            sd->status.skill[i].lv = 0;
            sd->status.skill[i].flags = 0;
        }

    clif_updatestatus(sd, SP::SKILLPOINT);
    clif_skillinfoblock(sd);
    pc_calcstatus(sd, 0);

    return 0;
}

/*==========================================
 * pcにダメージを与える
 *------------------------------------------
 */
sint32 pc_damage(BlockList *src, MapSessionData *sd,
               sint32 damage)
{
    sint32 i = 0, j = 0;
    nullpo_ret(sd);

    // 既に死んでいたら無効
    if (pc_isdead(sd))
        return 0;
    // 座ってたら立ち上がる
    if (pc_issit(sd))
    {
        pc_setstand(sd);
    }

    if (src)
    {
        if (src->type == BL_PC)
        {
            MAP_LOG_PC(sd, "INJURED-BY PC%d FOR %d",
                       static_cast<MapSessionData *>(src)->status.char_id,
                       damage);
        }
        else
        {
            MAP_LOG_PC(sd, "INJURED-BY MOB%d FOR %d", src->id, damage);
        }
    }
    else
        MAP_LOG_PC(sd, "INJURED-BY null FOR %d", damage);

    pc_stop_walking(sd, 3);

    sd->status.hp -= damage;

    if (sd->status.hp > 0)
    {
        // まだ生きているならHP更新
        clif_updatestatus(sd, SP::HP);

        sd->canlog_tick = gettick();

        if (sd->status.party_id)
        {                       // on-the-fly party hp updates [Valaris]
            struct party *p = party_search(sd->status.party_id);
            if (p != NULL)
                clif_party_hp(p, sd);
        }                       // end addition [Valaris]

        return 0;
    }

    MAP_LOG_PC(sd, "DEAD%s", "");

    // Character is dead!

    sd->status.hp = 0;
    // [Fate] Stop quickregen
    sd->quick_regeneration_hp.amount = 0;
    sd->quick_regeneration_sp.amount = 0;
    skill_update_heal_animation(sd);

    pc_setdead(sd);

    pc_stop_walking(sd, 0);
    skill_castcancel(sd);  // 詠唱の中止
    clif_being_remove(sd, BeingRemoveType::DEAD);
    pc_setglobalreg(sd, std::string("PC_DIE_COUNTER"), ++sd->die_counter);  //死にカウンター書き込み
    skill_status_change_clear(sd, 0); // ステータス異常を解除する
    clif_updatestatus(sd, SP::HP);
    pc_calcstatus(sd, 0);
    // [Fate] Reset magic
    sd->cast_tick = gettick();
    magic_stop_completely(sd);

    if (battle_config.death_penalty_type > 0 && sd->status.base_level >= level_t(20))
    {                           // changed penalty options, added death by player if pk_mode [Valaris]
        if (!maps[sd->m].flag.nopenalty)
        {
            if (battle_config.death_penalty_type == 1
                && battle_config.death_penalty_base > 0)
                sd->status.base_exp -=
                    pc_nextbaseexp(sd) *
                    static_cast<double>(battle_config.death_penalty_base) / 10000;
            if (battle_config.pk_mode && src && src->type == BL_PC)
                sd->status.base_exp -=
                    pc_nextbaseexp(sd) *
                    static_cast<double>(battle_config.death_penalty_base) / 10000;
            else if (battle_config.death_penalty_type == 2
                     && battle_config.death_penalty_base > 0)
            {
                if (pc_nextbaseexp(sd) > 0)
                    sd->status.base_exp -=
                        static_cast<double>(sd->status.base_exp) *
                        battle_config.death_penalty_base / 10000;
                if (battle_config.pk_mode && src && src->type == BL_PC)
                    sd->status.base_exp -=
                        static_cast<double>(sd->status.base_exp) *
                        battle_config.death_penalty_base / 10000;
            }
            if (sd->status.base_exp < 0)
                sd->status.base_exp = 0;
            clif_updatestatus(sd, SP::BASEEXP);

            if (battle_config.death_penalty_type == 1
                && battle_config.death_penalty_job > 0)
                sd->status.job_exp -=
                    pc_nextjobexp(sd) *
                    static_cast<double>(battle_config.death_penalty_job) / 10000;
            if (battle_config.pk_mode && src && src->type == BL_PC)
                sd->status.job_exp -=
                    pc_nextjobexp(sd) *
                    static_cast<double>(battle_config.death_penalty_job) / 10000;
            else if (battle_config.death_penalty_type == 2
                     && battle_config.death_penalty_job > 0)
            {
                if (pc_nextjobexp(sd) > 0)
                    sd->status.job_exp -=
                        sd->status.job_exp *
                        static_cast<double>(battle_config.death_penalty_job) / 10000;
                if (battle_config.pk_mode && src && src->type == BL_PC)
                    sd->status.job_exp -=
                        sd->status.job_exp *
                        static_cast<double>(battle_config.death_penalty_job) / 10000;
            }
            if (sd->status.job_exp < 0)
                sd->status.job_exp = 0;
            clif_updatestatus(sd, SP::JOBEXP);
        }
    }
    //ナイトメアモードアイテムドロップ
    if (maps[sd->m].flag.pvp_nightmaredrop)
    {                           // Moved this outside so it works when PVP isnt enabled and during pk mode [Ancyker]
        for (j = 0; j < MAX_DROP_PER_MAP; j++)
        {
            sint32 id = maps[sd->m].drop_list[j].drop_id;
            sint32 type = maps[sd->m].drop_list[j].drop_type;
            sint32 per = maps[sd->m].drop_list[j].drop_per;
            if (id == 0)
                continue;
            if (id == -1)
            {                   //ランダムドロップ
                sint32 eq_num = 0, eq_n[MAX_INVENTORY];
                memset(eq_n, 0, sizeof(eq_n));
                //先ず装備しているアイテム数をカウント
                for (i = 0; i < MAX_INVENTORY; i++)
                {
                    sint32 k;
                    if ((type == 1 && sd->status.inventory[i].equip == EPOS::NONE)
                        || (type == 2 && sd->status.inventory[i].equip != EPOS::NONE)
                        || type == 3)
                    {
                        //InventoryIndexを格納
                        for (k = 0; k < MAX_INVENTORY; k++)
                        {
                            if (eq_n[k] <= 0)
                            {
                                eq_n[k] = i;
                                break;
                            }
                        }
                        eq_num++;
                    }
                }
                if (eq_num > 0)
                {
                    sint32 n = eq_n[MRAND(eq_num)];  //該当アイテムの中からランダム
                    if (MRAND(10000) < per)
                    {
                        if (sd->status.inventory[n].equip != EPOS::NONE)
                            pc_unequipitem(sd, n, CalcStatus::NOW);
                        pc_dropitem(sd, n, 1);
                    }
                }
            }
            else if (id > 0)
            {
                for (i = 0; i < MAX_INVENTORY; i++)
                {
                    if (sd->status.inventory[i].nameid == id    //ItemIDが一致していて
                        && MRAND(10000) < per  //ドロップ率判定もOKで
                        && ((type == 1 && sd->status.inventory[i].equip == EPOS::NONE)   //タイプ判定もOKならドロップ
                            || (type == 2 && sd->status.inventory[i].equip != EPOS::NONE)
                            || type == 3))
                    {
                        if (sd->status.inventory[i].equip != EPOS::NONE)
                            pc_unequipitem(sd, i, CalcStatus::NOW);
                        pc_dropitem(sd, i, 1);
                        break;
                    }
                }
            }
        }
    }
    // pvp
    if (maps[sd->m].flag.pvp && !battle_config.pk_mode)
    {                           // disable certain pvp functions on pk_mode [Valaris]
        //ランキング計算
        if (!maps[sd->m].flag.pvp_nocalcrank)
        {
            sd->pvp_point -= 5;
            if (src && src->type == BL_PC)
                static_cast<MapSessionData *>(src)->pvp_point++;
            //} //fixed wrong '{' placement by Lupus
            pc_setdead(sd);
        }
        // 強制送還
        if (sd->pvp_point < 0)
        {
            sd->pvp_point = 0;
            pc_setstand(sd);
            pc_setrestartvalue(sd, 3);
            pc_setpos(sd, sd->status.save_point, BeingRemoveType::ZERO);
        }
    }

    if (src && src->type == BL_PC)
    {
        // [Fate] PK death, trigger scripts
        ArgRec arg[] =
        {
            { "@killerrid", unwrap(src->id) },
            { "@victimrid", unwrap(sd->id) },
            { "@victimlvl", unwrap(sd->status.base_level) },
        };
        npc_event_doall_l("OnPCKilledEvent", sd->id, ARRAY_SIZEOF(arg), arg);
        npc_event_doall_l("OnPCKillEvent", src->id, ARRAY_SIZEOF(arg), arg);
    }
    npc_event_doall_l("OnPCDieEvent", sd->id, 0, NULL);

    return 0;
}

//
// script関 連
//
/*==========================================
 * script用PCステータス読み出し
 *------------------------------------------
 */
sint32 pc_readparam(MapSessionData *sd, SP type)
{
    sint32 val = 0;

    nullpo_ret(sd);

    switch (type)
    {
        case SP::SKILLPOINT:
            val = sd->status.skill_point;
            break;
        case SP::STATUSPOINT:
            val = sd->status.status_point;
            break;
        case SP::ZENY:
            val = sd->status.zeny;
            break;
        case SP::BASELEVEL:
            val = unwrap(sd->status.base_level);
            break;
        case SP::JOBLEVEL:
            val = unwrap(sd->status.job_level);
            break;
        case SP::SEX:
            val = sd->sex;
            break;
        case SP::WEIGHT:
            val = sd->weight;
            break;
        case SP::MAXWEIGHT:
            val = sd->max_weight;
            break;
        case SP::BASEEXP:
            val = sd->status.base_exp;
            break;
        case SP::JOBEXP:
            val = sd->status.job_exp;
            break;
        case SP::NEXTBASEEXP:
            val = pc_nextbaseexp(sd);
            break;
        case SP::NEXTJOBEXP:
            val = pc_nextjobexp(sd);
            break;
        case SP::HP:
            val = sd->status.hp;
            break;
        case SP::MAXHP:
            val = sd->status.max_hp;
            break;
        case SP::SP:
            val = sd->status.sp;
            break;
        case SP::MAXSP:
            val = sd->status.max_sp;
            break;
        case SP::STR:
            val = sd->status.stats[ATTR::STR];
            break;
        case SP::AGI:
            val = sd->status.stats[ATTR::AGI];
            break;
        case SP::VIT:
            val = sd->status.stats[ATTR::VIT];
            break;
        case SP::INT:
            val = sd->status.stats[ATTR::INT];
            break;
        case SP::DEX:
            val = sd->status.stats[ATTR::DEX];
            break;
        case SP::LUK:
            val = sd->status.stats[ATTR::LUK];
            break;
        case SP::FAME:
            val = sd->fame;
            break;
    }

    return val;
}

/*==========================================
 * script用PCステータス設定
 *------------------------------------------
 */
sint32 pc_setparam(MapSessionData *sd, SP type, sint32 val)
{
    sint32 i = 0;

    nullpo_ret(sd);

    switch (type)
    {
        case SP::BASELEVEL:
            if (level_t(val) > sd->status.base_level)
            {
                for (i = 1; i <= (val - unwrap(sd->status.base_level)); i++)
                    sd->status.status_point += (unwrap(sd->status.base_level) + i + 14) / 4;
            }
            sd->status.base_level = level_t(val);
            sd->status.base_exp = 0;
            clif_updatestatus(sd, SP::BASELEVEL);
            clif_updatestatus(sd, SP::NEXTBASEEXP);
            clif_updatestatus(sd, SP::STATUSPOINT);
            clif_updatestatus(sd, SP::BASEEXP);
            pc_calcstatus(sd, 0);
            pc_heal(sd, sd->status.max_hp, sd->status.max_sp);
            break;
        case SP::JOBLEVEL:
            if (level_t(val) >= sd->status.job_level)
            {
                level_t up_level = level_t(10);
                if (level_t(val) > up_level)
                    val = unwrap(up_level);
                sd->status.skill_point += val - unwrap(sd->status.job_level);
                sd->status.job_level = level_t(val);
                sd->status.job_exp = 0;
                clif_updatestatus(sd, SP::JOBLEVEL);
                clif_updatestatus(sd, SP::NEXTJOBEXP);
                clif_updatestatus(sd, SP::JOBEXP);
                clif_updatestatus(sd, SP::SKILLPOINT);
                pc_calcstatus(sd, 0);
                clif_misceffect(sd, 1);
            }
            else
            {
                sd->status.job_level = level_t(val);
                sd->status.job_exp = 0;
                clif_updatestatus(sd, SP::JOBLEVEL);
                clif_updatestatus(sd, SP::NEXTJOBEXP);
                clif_updatestatus(sd, SP::JOBEXP);
                pc_calcstatus(sd, 0);
            }
            clif_updatestatus(sd, type);
            break;
        case SP::SKILLPOINT:
            sd->status.skill_point = val;
            break;
        case SP::STATUSPOINT:
            sd->status.status_point = val;
            break;
        case SP::ZENY:
            sd->status.zeny = val;
            break;
        case SP::BASEEXP:
            if (pc_nextbaseexp(sd) > 0)
            {
                sd->status.base_exp = val;
                if (sd->status.base_exp < 0)
                    sd->status.base_exp = 0;
                pc_checkbaselevelup(sd);
            }
            break;
        case SP::JOBEXP:
            if (pc_nextjobexp(sd) > 0)
            {
                sd->status.job_exp = val;
                if (sd->status.job_exp < 0)
                    sd->status.job_exp = 0;
                pc_checkjoblevelup(sd);
            }
            break;
        case SP::SEX:
            sd->sex = val;
            break;
        case SP::WEIGHT:
            sd->weight = val;
            break;
        case SP::MAXWEIGHT:
            sd->max_weight = val;
            break;
        case SP::HP:
            sd->status.hp = val;
            break;
        case SP::MAXHP:
            sd->status.max_hp = val;
            break;
        case SP::SP:
            sd->status.sp = val;
            break;
        case SP::MAXSP:
            sd->status.max_sp = val;
            break;
        case SP::STR:
            sd->status.stats[ATTR::STR] = val;
            break;
        case SP::AGI:
            sd->status.stats[ATTR::AGI] = val;
            break;
        case SP::VIT:
            sd->status.stats[ATTR::VIT] = val;
            break;
        case SP::INT:
            sd->status.stats[ATTR::INT] = val;
            break;
        case SP::DEX:
            sd->status.stats[ATTR::DEX] = val;
            break;
        case SP::LUK:
            sd->status.stats[ATTR::LUK] = val;
            break;
        case SP::FAME:
            sd->fame = val;
            break;
    }
    clif_updatestatus(sd, type);

    return 0;
}

/*==========================================
 * HP/SP回復
 *------------------------------------------
 */
sint32 pc_heal(MapSessionData *sd, sint32 hp, sint32 sp)
{
//  if (battle_config.battle_log)
//      printf("heal %d %d\n", hp, sp);

    nullpo_ret(sd);

    if (pc_checkoverhp(sd))
    {
        if (hp > 0)
            hp = 0;
    }
    if (pc_checkoversp(sd))
    {
        if (sp > 0)
            sp = 0;
    }

    if (hp + sd->status.hp > sd->status.max_hp)
        hp = sd->status.max_hp - sd->status.hp;
    if (sp + sd->status.sp > sd->status.max_sp)
        sp = sd->status.max_sp - sd->status.sp;
    sd->status.hp += hp;
    if (sd->status.hp <= 0)
    {
        sd->status.hp = 0;
        pc_damage(NULL, sd, 1);
        hp = 0;
    }
    sd->status.sp += sp;
    if (sd->status.sp <= 0)
        sd->status.sp = 0;
    if (hp)
        clif_updatestatus(sd, SP::HP);
    if (sp)
        clif_updatestatus(sd, SP::SP);

    if (sd->status.party_id)
    {                           // on-the-fly party hp updates [Valaris]
        struct party *p = party_search(sd->status.party_id);
        if (p != NULL)
            clif_party_hp(p, sd);
    }                           // end addition [Valaris]

    return hp + sp;
}

/*==========================================
 * HP/SP回復
 *------------------------------------------
 */
static sint32 pc_itemheal_effect(MapSessionData *sd, sint32 hp, sint32 sp);

static sint32                     // Compute how quickly we regenerate (less is faster) for that amount
pc_heal_quick_speed(sint32 amount)
{
    if (amount >= 100)
    {
        if (amount >= 500)
            return 0;
        if (amount >= 250)
            return 1;
        return 2;
    }
    else
    {                           // < 100
        if (amount >= 50)
            return 3;
        if (amount >= 20)
            return 4;
        return 5;
    }
}

static void pc_heal_quick_accumulate(sint32 new_amount,
                          struct quick_regeneration *quick_regen, sint32 max)
{
    sint32 current_amount = quick_regen->amount;
    sint32 current_speed = quick_regen->speed;
    sint32 new_speed = pc_heal_quick_speed(new_amount);

    sint32 average_speed = ((new_speed * new_amount) + (current_speed * current_amount)) / (current_amount + new_amount); // new_amount > 0, current_amount >= 0

    quick_regen->speed = average_speed;
    quick_regen->amount = min(current_amount + new_amount, max);

    quick_regen->tickdelay = min(quick_regen->speed, quick_regen->tickdelay);
}

sint32 pc_itemheal(MapSessionData *sd, sint32 hp, sint32 sp)
{
    /* defer healing */
    if (hp > 0)
    {
        pc_heal_quick_accumulate(hp,
                                  &sd->quick_regeneration_hp,
                                  sd->status.max_hp - sd->status.hp);
        hp = 0;
    }
    if (sp > 0)
    {
        pc_heal_quick_accumulate(sp,
                                  &sd->quick_regeneration_sp,
                                  sd->status.max_sp - sd->status.sp);

        sp = 0;
    }

    /* Hurt right away, if necessary */
    if (hp < 0 || sp < 0)
        pc_itemheal_effect(sd, hp, sp);

    return 0;
}

/* pc_itemheal_effect is invoked once every 0.5s whenever the pc
 * has health recovery queued up(cf. pc_natural_heal_sub).
 */
static sint32 pc_itemheal_effect(MapSessionData *sd, sint32 hp, sint32 sp)
{
    sint32 bonus;
//  if (battle_config.battle_log)
//      printf("heal %d %d\n", hp, sp);

    nullpo_ret(sd);

    if (pc_checkoverhp(sd))
    {
        if (hp > 0)
            hp = 0;
    }
    if (pc_checkoversp(sd))
    {
        if (sp > 0)
            sp = 0;
    }
    if (hp > 0)
    {
        bonus = (sd->paramc[ATTR::VIT] << 1) + 100;
        if (bonus != 100)
            hp = hp * bonus / 100;
        bonus = 100;
    }
    if (sp > 0)
    {
        bonus = (sd->paramc[ATTR::INT] << 1) + 100;
        if (bonus != 100)
            sp = sp * bonus / 100;
        bonus = 100;
    }
    if (hp + sd->status.hp > sd->status.max_hp)
        hp = sd->status.max_hp - sd->status.hp;
    if (sp + sd->status.sp > sd->status.max_sp)
        sp = sd->status.max_sp - sd->status.sp;
    sd->status.hp += hp;
    if (sd->status.hp <= 0)
    {
        sd->status.hp = 0;
        pc_damage(NULL, sd, 1);
        hp = 0;
    }
    sd->status.sp += sp;
    if (sd->status.sp <= 0)
        sd->status.sp = 0;
    if (hp)
        clif_updatestatus(sd, SP::HP);
    if (sp)
        clif_updatestatus(sd, SP::SP);

    return 0;
}

/*==========================================
 * HP/SP回復
 *------------------------------------------
 */
sint32 pc_percentheal(MapSessionData *sd, sint32 hp, sint32 sp)
{
    nullpo_ret(sd);

    if (pc_checkoverhp(sd))
    {
        if (hp > 0)
            hp = 0;
    }
    if (pc_checkoversp(sd))
    {
        if (sp > 0)
            sp = 0;
    }
    if (hp)
    {
        if (hp >= 100)
        {
            sd->status.hp = sd->status.max_hp;
        }
        else if (hp <= -100)
        {
            sd->status.hp = 0;
            pc_damage(NULL, sd, 1);
        }
        else
        {
            sd->status.hp += sd->status.max_hp * hp / 100;
            if (sd->status.hp > sd->status.max_hp)
                sd->status.hp = sd->status.max_hp;
            if (sd->status.hp <= 0)
            {
                sd->status.hp = 0;
                pc_damage(NULL, sd, 1);
                hp = 0;
            }
        }
    }
    if (sp)
    {
        if (sp >= 100)
        {
            sd->status.sp = sd->status.max_sp;
        }
        else if (sp <= -100)
        {
            sd->status.sp = 0;
        }
        else
        {
            sd->status.sp += sd->status.max_sp * sp / 100;
            if (sd->status.sp > sd->status.max_sp)
                sd->status.sp = sd->status.max_sp;
            if (sd->status.sp < 0)
                sd->status.sp = 0;
        }
    }
    if (hp)
        clif_updatestatus(sd, SP::HP);
    if (sp)
        clif_updatestatus(sd, SP::SP);

    return 0;
}

/*==========================================
 * 見た目変更
 *------------------------------------------
 */
sint32 pc_changelook(MapSessionData *sd, LOOK type, sint32 val)
{
    nullpo_ret(sd);

    switch (type)
    {
        case LOOK::HAIR:
            sd->status.hair = val;
            break;
        case LOOK::WEAPON:
            sd->status.weapon = val;
            break;
        case LOOK::LEGS:
            sd->status.legs = val;
            break;
        case LOOK::HEAD:
            sd->status.head = val;
            break;
        case LOOK::CHEST:
            sd->status.chest = val;
            break;
        case LOOK::HAIR_COLOR:
            sd->status.hair_color = val;
            break;
        case LOOK::SHIELD:
            sd->status.shield = val;
            break;
        case LOOK::SHOES:
            break;
    }
    clif_changelook(sd, type, val);

    return 0;
}

/*==========================================
 * 付属品(鷹,ペコ,カート)設定
 *------------------------------------------
 */
sint32 pc_setoption(MapSessionData *sd, OPTION type)
{
    nullpo_ret(sd);

    sd->status.option = type;
    clif_changeoption(sd);
    pc_calcstatus(sd, 0);

    return 0;
}

/*==========================================
 * script用グローバル変数の値を読む
 *------------------------------------------
 */
sint32 pc_readglobalreg(MapSessionData *sd, const std::string& reg)
{
    nullpo_ret(sd);

    for (sint32 i = 0; i < sd->status.global_reg_num; i++)
        if (sd->status.global_reg[i].str == reg)
            return sd->status.global_reg[i].value;

    return 0;
}

/*==========================================
 * script用グローバル変数の値を設定
 *------------------------------------------
 */
sint32 pc_setglobalreg(MapSessionData *sd, const std::string& reg, sint32 val)
{
    nullpo_ret(sd);

    //PC_DIE_COUNTERがスクリプトなどで変更された時の処理
    if (reg == "PC_DIE_COUNTER" && sd->die_counter != val)
    {
        sd->die_counter = val;
        pc_calcstatus(sd, 0);
    }
    if (val == 0)
    {
        for (sint32 i = 0; i < sd->status.global_reg_num; i++)
        {
            if (sd->status.global_reg[i].str == reg)
            {
                sd->status.global_reg[i] = sd->status.global_reg[--sd->status.global_reg_num];
                break;
            }
        }
        return 0;
    }
    for (sint32 i = 0; i < sd->status.global_reg_num; i++)
    {
        if (sd->status.global_reg[i].str == reg)
        {
            sd->status.global_reg[i].value = val;
            return 0;
        }
    }
    if (sd->status.global_reg_num < GLOBAL_REG_NUM)
    {
        STRZCPY(sd->status.global_reg[sd->status.global_reg_num].str, reg.c_str());
        sd->status.global_reg[sd->status.global_reg_num++].value = val;
        return 0;
    }
    map_log("%s: couldn't set %s (GLOBAL_REG_NUM = %d)\n", __func__,
            reg.c_str(), GLOBAL_REG_NUM);

    return 1;
}

/*==========================================
 * script用アカウント変数の値を読む
 *------------------------------------------
 */
sint32 pc_readaccountreg(MapSessionData *sd, const std::string& reg)
{
    nullpo_ret(sd);

    for (sint32 i = 0; i < sd->status.account_reg_num; i++)
    {
        if (sd->status.account_reg[i].str == reg)
            return sd->status.account_reg[i].value;
    }

    return 0;
}

/*==========================================
 * script用アカウント変数の値を設定
 *------------------------------------------
 */
sint32 pc_setaccountreg(MapSessionData *sd, const std::string& reg, sint32 val)
{
    nullpo_ret(sd);

    if (val == 0)
    {
        for (sint32 i = 0; i < sd->status.account_reg_num; i++)
        {
            if (sd->status.account_reg[i].str == reg)
            {
                sd->status.account_reg[i] = sd->status.account_reg[--sd->status.account_reg_num];
                break;
            }
        }
        intif_saveaccountreg(sd);
        return 0;
    }
    for (sint32 i = 0; i < sd->status.account_reg_num; i++)
    {
        if (sd->status.account_reg[i].str == reg)
        {
            sd->status.account_reg[i].value = val;
            intif_saveaccountreg(sd);
            return 0;
        }
    }
    if (sd->status.account_reg_num < ACCOUNT_REG_NUM)
    {
        STRZCPY(sd->status.account_reg[sd->status.account_reg_num].str, reg.c_str());
        sd->status.account_reg[sd->status.account_reg_num++].value = val;
        intif_saveaccountreg(sd);
        return 0;
    }
    map_log("%s: couldn't set %s (ACCOUNT_REG_NUM = %d)\n", __func__,
            reg.c_str(), ACCOUNT_REG_NUM);

    return 1;
}

/*==========================================
 * script用アカウント変数2の値を読む
 *------------------------------------------
 */
sint32 pc_readaccountreg2(MapSessionData *sd, const std::string& reg)
{
    nullpo_ret(sd);

    for (sint32 i = 0; i < sd->status.account_reg2_num; i++)
    {
        if (sd->status.account_reg2[i].str == reg)
            return sd->status.account_reg2[i].value;
    }

    return 0;
}

/*==========================================
 * script用アカウント変数2の値を設定
 *------------------------------------------
 */
sint32 pc_setaccountreg2(MapSessionData *sd, const std::string& reg, sint32 val)
{
    nullpo_retr(1, sd);

    if (val == 0)
    {
        for (sint32 i = 0; i < sd->status.account_reg2_num; i++)
        {
            if (sd->status.account_reg2[i].str == reg)
            {
                sd->status.account_reg2[i] = sd->status.account_reg2[--sd->status.account_reg2_num];
                break;
            }
        }
        chrif_saveaccountreg2(sd);
        return 0;
    }
    for (sint32 i = 0; i < sd->status.account_reg2_num; i++)
    {
        if (sd->status.account_reg2[i].str == reg)
        {
            sd->status.account_reg2[i].value = val;
            chrif_saveaccountreg2(sd);
            return 0;
        }
    }
    if (sd->status.account_reg2_num < ACCOUNT_REG2_NUM)
    {
        STRZCPY(sd->status.account_reg2[sd->status.account_reg2_num].str, reg.c_str());
        sd->status.account_reg2[sd->status.account_reg2_num++].value = val;
        chrif_saveaccountreg2(sd);
        return 0;
    }
    map_log("%s: couldn't set %s (ACCOUNT_REG2_NUM = %d)\n", __func__,
            reg.c_str(), ACCOUNT_REG2_NUM);

    return 1;
}

/*==========================================
 * イベントタイマー処理
 *------------------------------------------
 */
static void pc_eventtimer(timer_id tid, tick_t, BlockID id, char *data)
{
    MapSessionData *sd = map_id2sd(id);
    sint32 i;
    if (sd == NULL)
        return;

    for (i = 0; i < MAX_EVENTTIMER; i++)
    {
        if (sd->eventtimer[i].tid == tid)
        {
            sd->eventtimer[i].tid = NULL;
            npc_event(sd, data, 0);
            break;
        }
    }
    free(data);
    if (i == MAX_EVENTTIMER)
    {
        map_log("pc_eventtimer: no such event timer\n");
    }
}

/*==========================================
 * イベントタイマー追加
 *------------------------------------------
 */
sint32 pc_addeventtimer(MapSessionData *sd, interval_t tick, const char *name)
{
    sint32 i;

    nullpo_ret(sd);

    for (i = 0; i < MAX_EVENTTIMER; i++)
        if (sd->eventtimer[i].tid == NULL)
            break;

    if (i < MAX_EVENTTIMER)
    {
        char *evname;
        CREATE(evname, char, 24);
        strncpy(evname, name, 24);
        evname[23] = '\0';
        sd->eventtimer[i].name = evname;
        sd->eventtimer[i].tid = add_timer(gettick() + tick, pc_eventtimer, sd->id, evname);
        return 1;
    }

    return 0;
}

/*==========================================
 * イベントタイマー削除
 *------------------------------------------
 */
sint32 pc_deleventtimer(MapSessionData *sd, const char *name)
{
    sint32 i;

    nullpo_ret(sd);

    for (i = 0; i < MAX_EVENTTIMER; i++)
        if (sd->eventtimer[i].tid && strcmp(sd->eventtimer[i].name, name) == 0)
        {
            delete_timer(sd->eventtimer[i].tid);
            sd->eventtimer[i].tid = NULL;
            break;
        }

    return 0;
}

/*==========================================
 * イベントタイマー全削除
 *------------------------------------------
 */
sint32 pc_cleareventtimer(MapSessionData *sd)
{
    sint32 i;

    nullpo_ret(sd);

    for (i = 0; i < MAX_EVENTTIMER; i++)
        if (sd->eventtimer[i].tid)
        {
            delete_timer(sd->eventtimer[i].tid);
            sd->eventtimer[i].tid = NULL;
        }

    return 0;
}

//
// 装 備物
//
/*==========================================
 * アイテムを装備する
 *------------------------------------------
 */
static sint32 pc_signal_advanced_equipment_change(MapSessionData *sd, sint32 n)
{
    if (sd->status.inventory[n].equip & EPOS::SHOES)
        clif_changelook(sd, LOOK::SHOES, 0);
    if (sd->status.inventory[n].equip & EPOS::GLOVES)
        clif_changelook(sd, LOOK::GLOVES, 0);
    if (sd->status.inventory[n].equip & EPOS::CAPE)
        clif_changelook(sd, LOOK::CAPE, 0);
    if (sd->status.inventory[n].equip & EPOS::MISC1)
        clif_changelook(sd, LOOK::MISC1, 0);
    if (sd->status.inventory[n].equip & EPOS::MISC2)
        clif_changelook(sd, LOOK::MISC2, 0);
    return 0;
}

sint32 pc_equipitem(MapSessionData *sd, sint32 n)
{
    nullpo_ret(sd);

    if (n < 0 || n >= MAX_INVENTORY)
    {
        clif_equipitemack(sd, 0, EPOS::NONE, false);
        return 0;
    }

    sint32 nameid = sd->status.inventory[n].nameid;
    struct item_data *id = sd->inventory_data[n];
    EPOS pos = pc_equippoint(sd, n);

    map_log("equip %d(%d) %x:%x\n", nameid, n,
            static_cast<uint16>(id->equip), static_cast<uint16>(pos));
    if (!pc_isequip(sd, n) || !pos)
    {                           // [Valaris]
        clif_equipitemack(sd, n, EPOS::NONE, false);    // fail
        return 0;
    }

    if (pos == (EPOS::CAPE | EPOS::MISC2))
    {                           // アクセサリ用例外処理
        EPOS epor = EPOS::NONE;
        if (sd->equip_index[EQUIP::MISC2] >= 0)
            epor |= sd->status.inventory[sd->equip_index[EQUIP::MISC2]].equip;
        if (sd->equip_index[EQUIP::CAPE] >= 0)
            epor |= sd->status.inventory[sd->equip_index[EQUIP::CAPE]].equip;
        epor &= (EPOS::CAPE | EPOS::MISC2);
        pos = epor == EPOS::CAPE
            ? EPOS::MISC2
            : EPOS::CAPE;
    }

    // BUG: This code is *definitely* not doing what it is supposed to be doing
    sint32 arrow = pc_search_inventory(sd, pc_checkequip(sd, (EPOS::LEGS | EPOS::CAPE)));    // Added by RoVeRT
    for (EQUIP i : EQUIPs)
    {
        if (pos & equip_pos[i])
        {
            if (sd->equip_index[i] >= 0)    //Slot taken, remove item from there.
                pc_unequipitem(sd, sd->equip_index[i], CalcStatus::LATER);
            sd->equip_index[i] = n;
        }
    }
    // 弓矢装備
    if (pos == EPOS::ARROW)
    {
        clif_arrowequip(sd, n);
        clif_arrow_fail(sd, ArrowFail::EQUIPPING);
    }
    else
    {
        /* Don't update re-equipping if we're using a spell */
        if (!(pos == EPOS::GLOVES && sd->attack_spell_override))
            clif_equipitemack(sd, n, pos, 1);
    }

    for (EQUIP i : EQUIPs)
    {
        if (pos & equip_pos[i])
            sd->equip_index[i] = n;
    }
    sd->status.inventory[n].equip = pos;

    sint32 view = 0;
    if (sd->inventory_data[n])
    {
        view = sd->inventory_data[n]->look;
        if (view == 0)
            view = sd->inventory_data[n]->nameid;
    }

    if (sd->status.inventory[n].equip & EPOS::WEAPON)
    {
        sd->weapontype1 = view;
        pc_calcweapontype(sd);
        pc_set_weapon_look(sd);
    }
    if (sd->status.inventory[n].equip & EPOS::SHIELD)
    {
        if (sd->inventory_data[n])
        {
            if (sd->inventory_data[n]->type == 4)
            {
                sd->status.shield = 0;
                if (sd->status.inventory[n].equip == EPOS::SHIELD)
                    sd->weapontype2 = view;
            }
            else if (sd->inventory_data[n]->type == 5)
            {
                sd->status.shield = view;
                sd->weapontype2 = 0;
            }
        }
        else
            sd->status.shield = sd->weapontype2 = 0;
        pc_calcweapontype(sd);
        clif_changelook(sd, LOOK::SHIELD, sd->status.shield);
    }
    if (sd->status.inventory[n].equip & EPOS::LEGS)
    {
        sd->status.legs = view;
        clif_changelook(sd, LOOK::LEGS, sd->status.legs);
    }
    if (sd->status.inventory[n].equip & EPOS::HELMET)
    {
        sd->status.head = view;
        clif_changelook(sd, LOOK::HEAD, sd->status.head);
    }
    if (sd->status.inventory[n].equip & EPOS::CHEST)
    {
        sd->status.chest = view;
        clif_changelook(sd, LOOK::CHEST, sd->status.chest);
    }
    pc_signal_advanced_equipment_change(sd, n);

    if (itemdb_look(sd->status.inventory[n].nameid) == 11 && arrow)
    {                           // Added by RoVeRT
        clif_arrowequip(sd, arrow);
        sd->status.inventory[arrow].equip = EPOS::ARROW;
    }
    pc_calcstatus(sd, 0);
    return 0;
}

/*==========================================
 * 装 備した物を外す
 *------------------------------------------
 */
sint32 pc_unequipitem(MapSessionData *sd, sint32 n, CalcStatus type)
{
    nullpo_ret(sd);

    map_log("unequip %d %x:%x\n", n,
            static_cast<uint16>(pc_equippoint(sd, n)),
            static_cast<uint16>(sd->status.inventory[n].equip));
    if (sd->status.inventory[n].equip != EPOS::NONE)
    {
        for (EQUIP i : EQUIPs)
        {
            if (sd->status.inventory[n].equip & equip_pos[i])
                sd->equip_index[i] = -1;
        }
        if (sd->status.inventory[n].equip & EPOS::WEAPON)
        {
            sd->weapontype1 = 0;
            sd->status.weapon = sd->weapontype2;
            pc_calcweapontype(sd);
            pc_set_weapon_look(sd);
        }
        if (sd->status.inventory[n].equip & EPOS::SHIELD)
        {
            sd->status.shield = sd->weapontype2 = 0;
            pc_calcweapontype(sd);
            clif_changelook(sd, LOOK::SHIELD, sd->status.shield);
        }
        if (sd->status.inventory[n].equip & EPOS::LEGS)
        {
            sd->status.legs = 0;
            clif_changelook(sd, LOOK::LEGS,
                             sd->status.legs);
        }
        if (sd->status.inventory[n].equip & EPOS::HELMET)
        {
            sd->status.head = 0;
            clif_changelook(sd, LOOK::HEAD, sd->status.head);
        }
        if (sd->status.inventory[n].equip & EPOS::CHEST)
        {
            sd->status.chest = 0;
            clif_changelook(sd, LOOK::CHEST, sd->status.chest);
        }
        pc_signal_advanced_equipment_change(sd, n);

        clif_unequipitemack(sd, n, sd->status.inventory[n].equip, true);
        sd->status.inventory[n].equip = EPOS::NONE;
    }
    else
    {
        clif_unequipitemack(sd, n, EPOS::NONE, false);
    }
    if (type == CalcStatus::NOW)
        pc_calcstatus(sd, 0);

    return 0;
}

sint32 pc_unequipinvyitem(MapSessionData *sd, sint32 n, CalcStatus type)
{
    nullpo_retr(1, sd);

    for (EQUIP i : EQUIPs)
    {
        if (equip_pos[i] != EPOS::NONE && sd->equip_index[i] == n)
        {
            //Slot taken, remove item from there.
            pc_unequipitem(sd, sd->equip_index[i], type);
            sd->equip_index[i] = -1;
        }
    }

    return 0;
}

/*==========================================
 * アイテムのindex番号を詰めたり
 * 装 備品の装備可能チェックを行なう
 *------------------------------------------
 */
sint32 pc_checkitem(MapSessionData *sd)
{
    sint32 i, j, k, id, calc_flag = 0;
    struct item_data *it = NULL;

    nullpo_ret(sd);

    // 所持品空き詰め
    for (i = j = 0; i < MAX_INVENTORY; i++)
    {
        if ((id = sd->status.inventory[i].nameid) == 0)
            continue;
        if (!itemdb_available(id))
        {
            MAP_LOG("illegal item id %d in %d[%s] inventory.\n",
                    id, sd->id, sd->status.name);
            pc_delitem(sd, i, sd->status.inventory[i].amount, 3);
            continue;
        }
        if (i > j)
        {
            memcpy(&sd->status.inventory[j], &sd->status.inventory[i],
                    sizeof(struct item));
            sd->inventory_data[j] = sd->inventory_data[i];
        }
        j++;
    }
    if (j < MAX_INVENTORY)
        memset(&sd->status.inventory[j], 0,
                sizeof(struct item) * (MAX_INVENTORY - j));
    for (k = j; k < MAX_INVENTORY; k++)
        sd->inventory_data[k] = NULL;

    // 装 備位置チェック

    for (i = 0; i < MAX_INVENTORY; i++)
    {

        it = sd->inventory_data[i];

        if (sd->status.inventory[i].nameid == 0)
            continue;
        if (sd->status.inventory[i].equip & ~pc_equippoint(sd, i))
        {
            sd->status.inventory[i].equip = EPOS::NONE;
            calc_flag = 1;
        }
        //装備制限チェック
        if (sd->status.inventory[i].equip != EPOS::NONE && maps[sd->m].flag.pvp
            && (it->flag.no_equip == 1 || it->flag.no_equip == 3))
        {                       //PvP制限
            sd->status.inventory[i].equip = EPOS::NONE;
            calc_flag = 1;
        }
    }

    pc_setequipindex(sd);
    if (calc_flag)
        pc_calcstatus(sd, 2);

    return 0;
}

sint32 pc_checkoverhp(MapSessionData *sd)
{
    nullpo_ret(sd);

    if (sd->status.hp == sd->status.max_hp)
        return 1;
    if (sd->status.hp > sd->status.max_hp)
    {
        sd->status.hp = sd->status.max_hp;
        clif_updatestatus(sd, SP::HP);
        return 2;
    }

    return 0;
}

sint32 pc_checkoversp(MapSessionData *sd)
{
    nullpo_ret(sd);

    if (sd->status.sp == sd->status.max_sp)
        return 1;
    if (sd->status.sp > sd->status.max_sp)
    {
        sd->status.sp = sd->status.max_sp;
        clif_updatestatus(sd, SP::SP);
        return 2;
    }

    return 0;
}

/*==========================================
 * PVP順位計算用(foreachinarea)
 *------------------------------------------
 */
static void pc_calc_pvprank_sub(BlockList *bl, MapSessionData *sd2)
{
    MapSessionData *sd1;

    nullpo_retv(bl);
    nullpo_retv(sd1 = static_cast<MapSessionData *>(bl));
    nullpo_retv(sd2);

    if (sd1->pvp_point > sd2->pvp_point)
        sd2->pvp_rank++;
}

/*==========================================
 * PVP順位計算
 *------------------------------------------
 */
sint32 pc_calc_pvprank(MapSessionData *sd)
{
    nullpo_ret(sd);
    map_data_local *m = &maps[sd->m];

    if (!(m->flag.pvp))
        return 0;
    sd->pvp_rank = 1;
    map_foreachinarea(pc_calc_pvprank_sub, sd->m, 0, 0, m->xs, m->ys,
                      BL_PC, sd);
    return sd->pvp_rank;
}

/*==========================================
 * PVP順位計算(timer)
 *------------------------------------------
 */
void pc_calc_pvprank_timer(timer_id, tick_t, account_t id)
{
    MapSessionData *sd = NULL;
    if (battle_config.pk_mode)  // disable pvp ranking if pk_mode on [Valaris]
        return;

    sd = map_id2sd(id);
    if (sd == NULL)
        return;
    sd->pvp_timer = NULL;
    if (pc_calc_pvprank(sd) > 0)
        sd->pvp_timer = add_timer(gettick() + PVP_CALCRANK_INTERVAL,
                                  pc_calc_pvprank_timer, id);
}

/*==========================================
 * sdは結婚しているか(既婚の場合は相方のchar_idを返す)
 *------------------------------------------
 */
charid_t pc_ismarried(MapSessionData *sd)
{
    nullpo_ret(sd);

    return sd->status.partner_id;
}

/*==========================================
 * sdがdstsdと結婚(dstsd→sdの結婚処理も同時に行う)
 *------------------------------------------
 */
bool pc_marriage(MapSessionData *sd, MapSessionData *dstsd)
{
    if (pc_ismarried(sd) || pc_ismarried(dstsd))
        return false;
    sd->status.partner_id = dstsd->status.char_id;
    dstsd->status.partner_id = sd->status.char_id;
    return true;
}

/*==========================================
 * sdが離婚(相手はsd->status.partner_idに依る)(相手も同時に離婚・結婚指輪自動剥奪)
 *------------------------------------------
 */
bool pc_divorce(MapSessionData *sd)
{
    MapSessionData *p_sd = NULL;
    if (sd == NULL || !pc_ismarried(sd))
        return false;

    // If both are on map server we don't need to bother the char server
    if ((p_sd = map_nick2sd(map_charid2nick(sd->status.partner_id))) != NULL)
    {
        if (p_sd->status.partner_id != sd->status.char_id
            || sd->status.partner_id != p_sd->status.char_id)
        {
            PRINTF("pc_divorce: Illegal partner_id sd=%d p_sd=%d\n",
                    sd->status.partner_id, p_sd->status.partner_id);
            return false;
        }
        p_sd->status.partner_id = DEFAULT;
        sd->status.partner_id = DEFAULT;

        if (sd->npc_flags.divorce)
        {
            sd->npc_flags.divorce = 0;
            map_scriptcont(sd, sd->npc_id);
        }
    }
    else
        chrif_send_divorce(sd->status.char_id);

    return true;
}

/*==========================================
 * sdの相方のmap_session_dataを返す
 *------------------------------------------
 */
MapSessionData *pc_get_partner(MapSessionData *sd)
{
    MapSessionData *p_sd = NULL;
    if (sd == NULL || !pc_ismarried(sd))
        return NULL;

    const char *nick = map_charid2nick(sd->status.partner_id);

    if (nick == NULL)
        return NULL;

    if ((p_sd = map_nick2sd(nick)) == NULL)
        return NULL;

    return p_sd;
}

//
// 自然回復物
//
/*==========================================
 * SP回復量計算
 *------------------------------------------
 */
static tick_t natural_heal_tick, natural_heal_prev_tick;
interval_t natural_heal_diff_tick;

static interval_t pc_spheal(MapSessionData *sd)
{
    nullpo_ret(sd);

    if (pc_issit(sd))
        return natural_heal_diff_tick * 2;

    return natural_heal_diff_tick;
}

/*==========================================
 * HP回復量計算
 *------------------------------------------
 */
static interval_t pc_hpheal(MapSessionData *sd)
{
    nullpo_ret(sd);

    if (pc_issit(sd))
        return natural_heal_diff_tick * 2;

    return natural_heal_diff_tick;
}

static sint32 pc_natural_heal_hp(MapSessionData *sd)
{
    sint32 bhp;
    sint32 bonus, hp_flag;

    nullpo_ret(sd);

    if (pc_checkoverhp(sd))
    {
        sd->hp_sub = DEFAULT;
        sd->inchealhptick = DEFAULT;
        return 0;
    }

    bhp = sd->status.hp;
    hp_flag = 0;

    if (sd->walktimer == NULL)
    {
        sd->hp_sub += pc_hpheal(sd);
        sd->inchealhptick += natural_heal_diff_tick;
    }
    else if (hp_flag)
    {
        sd->hp_sub += pc_hpheal(sd);
        sd->inchealhptick = DEFAULT;
    }
    else
    {
        sd->hp_sub = DEFAULT;
        sd->inchealhptick = DEFAULT;
        return 0;
    }

    if (sd->hp_sub >= std::chrono::milliseconds(battle_config.natural_healhp_interval))
    {
        bonus = sd->nhealhp;
        if (hp_flag)
        {
            bonus >>= 2;
            if (bonus <= 0)
                bonus = 1;
        }
        while (sd->hp_sub >= std::chrono::milliseconds(battle_config.natural_healhp_interval))
        {
            sd->hp_sub -= std::chrono::milliseconds(battle_config.natural_healhp_interval);
            if (sd->status.hp + bonus <= sd->status.max_hp)
                sd->status.hp += bonus;
            else
            {
                sd->status.hp = sd->status.max_hp;
                sd->hp_sub = DEFAULT;
                sd->inchealhptick = DEFAULT;
            }
        }
    }
    if (bhp != sd->status.hp)
        clif_updatestatus(sd, SP::HP);

    if (sd->nshealhp > 0)
    {
        if (sd->inchealhptick >= std::chrono::milliseconds(battle_config.natural_heal_skill_interval)
            && sd->status.hp < sd->status.max_hp)
        {
            bonus = sd->nshealhp;
            while (sd->inchealhptick >= std::chrono::milliseconds(battle_config.natural_heal_skill_interval))
            {
                sd->inchealhptick -= std::chrono::milliseconds(battle_config.natural_heal_skill_interval);
                if (sd->status.hp + bonus <= sd->status.max_hp)
                    sd->status.hp += bonus;
                else
                {
                    bonus = sd->status.max_hp - sd->status.hp;
                    sd->status.hp = sd->status.max_hp;
                    sd->hp_sub = DEFAULT;
                    sd->inchealhptick = DEFAULT;
                }
            }
        }
    }
    else
        sd->inchealhptick = DEFAULT;

    return 0;
}

static sint32 pc_natural_heal_sp(MapSessionData *sd)
{
    sint32 bsp;
    sint32 bonus;

    nullpo_ret(sd);

    if (pc_checkoversp(sd))
    {
        sd->sp_sub = DEFAULT;
        sd->inchealsptick = DEFAULT;
        return 0;
    }

    bsp = sd->status.sp;

    sd->sp_sub += pc_spheal(sd);
    if (sd->walktimer == NULL)
        sd->inchealsptick += natural_heal_diff_tick;
    else
        sd->inchealsptick = DEFAULT;

    if (sd->sp_sub >= std::chrono::milliseconds(battle_config.natural_healsp_interval))
    {
        bonus = sd->nhealsp;;
        while (sd->sp_sub >= std::chrono::milliseconds(battle_config.natural_healsp_interval))
        {
            sd->sp_sub -= std::chrono::milliseconds(battle_config.natural_healsp_interval);
            if (sd->status.sp + bonus <= sd->status.max_sp)
                sd->status.sp += bonus;
            else
            {
                sd->status.sp = sd->status.max_sp;
                sd->sp_sub = DEFAULT;
                sd->inchealsptick = DEFAULT;
            }
        }
    }

    if (bsp != sd->status.sp)
        clif_updatestatus(sd, SP::SP);

    if (sd->nshealsp > 0)
    {
        if (sd->inchealsptick >= std::chrono::milliseconds(battle_config.natural_heal_skill_interval)
            && sd->status.sp < sd->status.max_sp)
        {
            bonus = sd->nshealsp;
            sd->doridori_counter = 0;
            while (sd->inchealsptick >= std::chrono::milliseconds(battle_config.natural_heal_skill_interval))
            {
                sd->inchealsptick -= std::chrono::milliseconds(battle_config.natural_heal_skill_interval);
                if (sd->status.sp + bonus <= sd->status.max_sp)
                    sd->status.sp += bonus;
                else
                {
                    bonus = sd->status.max_sp - sd->status.sp;
                    sd->status.sp = sd->status.max_sp;
                    sd->sp_sub = DEFAULT;
                    sd->inchealsptick = DEFAULT;
                }
            }
        }
    }
    else
        sd->inchealsptick = DEFAULT;

    return 0;
}

static sint32 pc_quickregenerate_effect(struct quick_regeneration *quick_regen,
                           sint32 heal_speed)
{
    if (!(quick_regen->tickdelay--))
    {
        sint32 bonus =
            min(heal_speed * battle_config.itemheal_regeneration_factor,
                 quick_regen->amount);

        quick_regen->amount -= bonus;

        quick_regen->tickdelay = quick_regen->speed;

        return bonus;
    }

    return 0;
}

static void pc_natural_heal_sub(MapSessionData *sd)
{
    nullpo_retv(sd);

    if (sd->heal_xp > 0)
    {
        if (sd->heal_xp < 64)
            --sd->heal_xp;      // [Fate] Slowly reduce XP that healers can get for healing this char
        else
            sd->heal_xp -= (sd->heal_xp >> 6);
    }

    // Hijack this callback:  Adjust spellpower bonus
    if (sd->spellpower_bonus_target < sd->spellpower_bonus_current)
    {
        sd->spellpower_bonus_current = sd->spellpower_bonus_target;
        pc_calcstatus(sd, 0);
    }
    else if (sd->spellpower_bonus_target > sd->spellpower_bonus_current)
    {
        sd->spellpower_bonus_current +=
            1 +
            ((sd->spellpower_bonus_target -
              sd->spellpower_bonus_current) >> 5);
        pc_calcstatus(sd, 0);
    }

    if (sd->sc_data[SC_HALT_REGENERATE].timer)
        return;

    if (sd->quick_regeneration_hp.amount || sd->quick_regeneration_sp.amount)
    {
        sint32 hp_bonus =
                pc_quickregenerate_effect(&sd->quick_regeneration_hp,
                                          (sd->sc_data[SC_POISON].timer == NULL
                                              || sd->sc_data[SC_SLOWPOISON].timer)
                                          ? sd->nhealhp
                                          : 1);   // [fate] slow down when poisoned
        sint32 sp_bonus = pc_quickregenerate_effect(&sd->quick_regeneration_sp,
                                                 sd->nhealsp);

        pc_itemheal_effect(sd, hp_bonus, sp_bonus);
    }
    skill_update_heal_animation(sd);   // if needed.

    if ((sd->sc_data[SC_FLYING_BACKPACK].timer
            || battle_config.natural_heal_weight_rate > 100
            || sd->weight * 100 / sd->max_weight < battle_config.natural_heal_weight_rate)
        && !pc_isdead(sd)
        && !pc_ishiding(sd)
        && sd->sc_data[SC_POISON].timer == NULL)
    {
        pc_natural_heal_hp(sd);
        pc_natural_heal_sp(sd);
    }
    else
    {
        sd->hp_sub = DEFAULT;
        sd->inchealhptick = DEFAULT;
        sd->sp_sub = DEFAULT;
        sd->inchealsptick = DEFAULT;
    }
}

/*==========================================
 * HP/SP自然回復 (interval timer関数)
 *------------------------------------------
 */
static void pc_natural_heal(timer_id, tick_t tick)
{
    natural_heal_tick = tick;
    natural_heal_diff_tick = natural_heal_tick - natural_heal_prev_tick;
    for (MapSessionData *sd : auth_sessions)
        pc_natural_heal_sub(sd);

    natural_heal_prev_tick = tick;
}

/*==========================================
 * セーブポイントの保存
 *------------------------------------------
 */
sint32 pc_setsavepoint(MapSessionData *sd, const Point& point)
{
    nullpo_ret(sd);

    sd->status.save_point = point;

    return 0;
}

/*==========================================
 * 自動セーブ 各クライアント
 *------------------------------------------
 */
static sint32 last_save_fd, save_flag;
static void pc_autosave_sub(MapSessionData *sd)
{
    nullpo_retv(sd);

    if (save_flag == 0 && sd->fd > last_save_fd)
    {
        pc_makesavestatus(sd);
        chrif_save(sd);

        save_flag = 1;
        last_save_fd = sd->fd;
    }
}

/*==========================================
 * 自動セーブ (timer関数)
 *------------------------------------------
 */
static void pc_autosave(timer_id, tick_t)
{
    save_flag = 0;
    for (MapSessionData *sd : auth_sessions)
        pc_autosave_sub(sd);
    if (save_flag == 0)
        last_save_fd = 0;

    interval_t interval = autosave_interval / (clif_countusers() + 1);
    if (interval < interval_t(1))
        interval = interval_t(1);
    add_timer(gettick() + interval, pc_autosave);
}

size_t pc_read_gm_account(sint32 fd)
{
    gm_accounts.clear();
    for (sint32 i = 4; i < RFIFOW(fd, 2); i += 5)
    {
        account_t account_id = account_t(RFIFOL(fd, i));
        gm_level_t level = gm_level_t(RFIFOB(fd, i + 4));

        gm_accounts.set(account_id, level);
    }
    return gm_accounts.size();
}

void pc_setstand(MapSessionData *sd)
{
    nullpo_retv(sd);

    sd->state.dead_sit = 0;
}

/*==========================================
 * pc関 係初期化
 *------------------------------------------
 */
sint32 do_init_pc(void)
{
    natural_heal_prev_tick = gettick() + NATURAL_HEAL_INTERVAL;
    add_timer_interval(natural_heal_prev_tick, NATURAL_HEAL_INTERVAL, pc_natural_heal);
    add_timer(gettick() + autosave_interval, pc_autosave);
    return 0;
}

void pc_invisibility(MapSessionData *sd, sint32 enabled)
{
    if (enabled && !(sd->status.option & OPTION::INVISIBILITY))
    {
        clif_being_remove(sd, BeingRemoveType::WARP);
        sd->status.option |= OPTION::INVISIBILITY;
        clif_status_change(sd, CLIF_OPTION_SC_INVISIBILITY, 1);
    }
    else if (!enabled)
    {
        sd->status.option &= ~OPTION::INVISIBILITY;
        clif_status_change(sd, CLIF_OPTION_SC_INVISIBILITY, 0);
        pc_setpos(sd, Point{maps[sd->m].name, sd->x, sd->y}, BeingRemoveType::WARP);
    }
}

sint32 pc_logout(MapSessionData *sd) // [fate] Player logs out
{
    if (!sd)
        return 0;

    if (sd->sc_data[SC_POISON].timer)
        sd->status.hp = 1;      // Logging out while poisoned -> bad

    pc_setglobalreg(sd, std::string("MAGIC_CAST_TICK"), 0);

    MAP_LOG_STATS(sd, "LOGOUT");
    return 0;
}
