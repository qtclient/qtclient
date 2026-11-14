#include "protocol/tradewire.h"

#include "protocol/inventoryitemwire.h"

#include <protocol.pb.h>

namespace tibia::protocol {

namespace {

using namespace ::tibia::protobuf::protocol;

template<typename Message>
bool readOffer(ClassicReader& r, Message* m)
{
    uint8_t count = 0;
    if (!r.string(*m->mutable_name()) || !r.u8(count))
        return false;
    for (uint8_t i = 0; i < count; ++i) {
        if (!readInventoryItem(r, m->add_items()))
            return false;
    }
    return true;
}

bool readNpcOffer(ClassicReader& r, GameserverMessageNPCOffer* m)
{
    uint16_t currency = 0, count = 0;
    if (!r.string(*m->mutable_npc_name()) || !r.u16(currency) || !r.string(*m->mutable_currency_name()) || !r.u16(count))
        return false;
    m->set_currency_object_id(currency);
    for (uint16_t i = 0; i < count; ++i) {
        ProtocolNpcOffer* offer = m->add_offers();
        uint16_t object = 0;
        uint8_t data = 0;
        uint32_t weight = 0, buy = 0, sell = 0;
        if (!r.u16(object) || !r.u8(data) || !r.string(*offer->mutable_name()) || !r.u32(weight) || !r.u32(buy)
            || !r.u32(sell))
            return false;
        offer->set_object_id(object);
        offer->set_data(data);
        offer->set_weight(weight);
        offer->set_buy_price(buy);
        offer->set_sell_price(sell);
    }
    return true;
}

}

bool isTradeMessage(const int opcode)
{
    return opcode >= 122 && opcode <= 127;
}

size_t fillTradeMessage(GameserverMessage& msg, const int opcode, ClassicReader& r)
{
    switch (opcode) {
    case 122:
        if (!readNpcOffer(r, msg.MutableExtension(GameserverMessageNPCOffer::gameserver_message_n_p_c_offer)))
            return 0;
        return r.i;
    case 123: {
        auto* m = msg.MutableExtension(GameserverMessagePlayerGoods::gameserver_message_player_goods);
        uint16_t count = 0;
        if (!r.u16(count))
            return 0;
        for (uint16_t i = 0; i < count; ++i) {
            uint16_t object = 0, amount = 0;
            if (!r.u16(object) || !r.u16(amount))
                return 0;
            ProtocolPlayerGood* good = m->add_goods();
            good->set_object_id(object);
            good->set_amount(amount);
        }
        return r.i;
    }
    case 124:
        msg.MutableExtension(GameserverMessageCloseNPCTrade::gameserver_message_close_n_p_c_trade);
        return r.i;
    case 125:
        if (!readOffer(r, msg.MutableExtension(GameserverMessageOwnOffer::gameserver_message_own_offer)))
            return 0;
        return r.i;
    case 126:
        if (!readOffer(r, msg.MutableExtension(GameserverMessageCounterOffer::gameserver_message_counter_offer)))
            return 0;
        return r.i;
    case 127:
        msg.MutableExtension(GameserverMessageCloseTrade::gameserver_message_close_trade);
        return r.i;
    default:
        return 0;
    }
}

}
