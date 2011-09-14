#ifndef TRADE_HPP
#define TRADE_HPP

# include "map.structs.hpp"

void trade_traderequest(MapSessionData *sd, int32_t target_id);
void trade_tradeack(MapSessionData *sd, int32_t type);
void trade_tradeadditem(MapSessionData *sd, int32_t index, int32_t amount);
void trade_tradeok(MapSessionData *sd);
void trade_tradecancel(MapSessionData *sd);
void trade_tradecommit(MapSessionData *sd);
void trade_verifyzeny(MapSessionData *sd);

#endif // TRADE_HPP
