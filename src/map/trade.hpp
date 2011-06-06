#ifndef TRADE_H
#define TRADE_H

#include "map.hpp"
void trade_traderequest(MapSessionData *sd, int target_id);
void trade_tradeack(MapSessionData *sd, int type);
void trade_tradeadditem(MapSessionData *sd, int index, int amount);
void trade_tradeok(MapSessionData *sd);
void trade_tradecancel(MapSessionData *sd);
void trade_tradecommit(MapSessionData *sd);
void trade_verifyzeny(MapSessionData *sd);

#endif // TRADE_H
