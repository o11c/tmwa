#ifndef ITEMDB_HPP
#define ITEMDB_HPP

# include "itemdb.structs.hpp"

struct item_data *itemdb_searchname(const char *name);
struct item_data *itemdb_search(int nameid);
struct item_data *itemdb_exists(int nameid) __attribute__((pure));
#define itemdb_type(n) itemdb_search(n)->type
#define itemdb_atk(n) itemdb_search(n)->atk
#define itemdb_def(n) itemdb_search(n)->def
#define itemdb_look(n) itemdb_search(n)->look
#define itemdb_weight(n) itemdb_search(n)->weight
#define itemdb_equip(n) itemdb_search(n)->equip
#define itemdb_usescript(n) itemdb_search(n)->use_script
#define itemdb_equipscript(n) itemdb_search(n)->equip_script
#define itemdb_wlv(n) itemdb_search(n)->wlv
#define itemdb_range(n) itemdb_search(n)->range
#define itemdb_slot(n) itemdb_search(n)->slot
#define itemdb_available(n) (itemdb_exists(n) && itemdb_search(n)->flag.available)
#define itemdb_viewid(n) (itemdb_search(n)->view_id)

#define itemdb_value_buy(n) itemdb_search(n)->value_buy
#define itemdb_value_sell(n) itemdb_search(n)->value_sell

int itemdb_isequip(int);
int itemdb_isequip2(struct item_data *) __attribute__((pure));
int itemdb_isequip3(int);

void do_init_itemdb(void);

#endif // ITEMDB_HPP
