#ifndef TRADE_HPP
#define TRADE_HPP

# include "main.structs.hpp"

void trade_traderequest(MapSessionData *sd, account_t target_id);
void trade_tradeack(MapSessionData *sd, sint32 type);
void trade_tradeadditem(MapSessionData *sd, sint32 index, sint32 amount);
void trade_tradeok(MapSessionData *sd);
void trade_tradecancel(MapSessionData *sd);
void trade_tradecommit(MapSessionData *sd);
void trade_verifyzeny(MapSessionData *sd);

#endif // TRADE_HPP
