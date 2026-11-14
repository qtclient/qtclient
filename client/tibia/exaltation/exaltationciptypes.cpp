#include "exaltation/exaltationciptypes.h"
#include <vector>
#include <algorithm>
#include "protocol/protocolciptypes.h"
#include <tibia/game/gamenotify.h>
#include <protocol.pb.h>
#include "client/staticdata.h"

namespace tibia::exaltation {

namespace {
} // namespace

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    (void)parent;
}

} // namespace tibia::exaltation

namespace tibia::exaltation {

void TExaltationProtocolMessageHandler::onServerMessageEnqueued(const int opcode, const QByteArray& /*classicFrame*/, const bool applyClassic)
{
    using namespace ::tibia::protobuf::protocol;
    const auto* shadow = protocol::nativeServerMessage(opcode, applyClassic);
    if (!shadow)
        return;
    switch (opcode) {
    case 137:
        if (shadow->HasExtension(GameserverMessageCloseExaltationDialog::gameserver_message_close_exaltation_dialog))
            tibia::game::notify("onCloseForgeCloseWindows");
        break;
    case 135:
        if (shadow->HasExtension(GameserverMessageExaltationDialogRefresh::gameserver_message_exaltation_dialog_refresh)) {
            const auto& m = shadow->GetExtension(GameserverMessageExaltationDialogRefresh::gameserver_message_exaltation_dialog_refresh);
            const auto itemOf = [](const ProtocolForgeObject& o) {
                return ForgeItemInfo{ .id = static_cast<uint16_t>(o.object().object_id()), .tier = static_cast<uint8_t>(o.object().tier()),
                                      .count = static_cast<uint16_t>(o.count()) };
            };
            const auto transferOf = [&](const ProtocolForgeTransfer& t) {
                ForgeTransferData transfer;
                for (const auto& d : t.donors())
                    transfer.donors.push_back(itemOf(d));
                for (const auto& r : t.receivers())
                    transfer.receivers.push_back({ .id = static_cast<uint16_t>(r.object_id()), .tier = 0, .count = static_cast<uint16_t>(r.count()) });
                return transfer;
            };
            ForgeOpenData data;
            for (const auto& group : m.fusion_groups())
                for (const auto& o : group.objects())
                    data.fusionItems.push_back(itemOf(o));
            for (const auto& group : m.convergence_fusion_groups()) {
                std::vector<ForgeItemInfo> items;
                for (const auto& o : group.objects())
                    items.push_back(itemOf(o));
                data.convergenceFusion.push_back(std::move(items));
            }
            for (const auto& t : m.transfers())
                data.transfers.push_back(transferOf(t));
            for (const auto& t : m.convergence_transfers())
                data.convergenceTransfers.push_back(transferOf(t));
            data.dustLevel = static_cast<uint16_t>(m.dust_level());
            tibia::game::notify("onOpenForge", data);
        }
        break;
    case 134:
        if (shadow->HasExtension(GameserverMessageExaltationBaseData::gameserver_message_exaltation_base_data)) {
            const auto& m = shadow->GetExtension(GameserverMessageExaltationBaseData::gameserver_message_exaltation_base_data);
            const auto prices = [](const auto& list) {
                std::vector<ForgeTierPrice> out;
                for (const auto& p : list)
                    out.push_back({ .tier = static_cast<uint8_t>(p.tier()), .price = p.price() });
                return out;
            };
            ForgeConfigData config;
            for (const auto& c : m.class_prices())
                config.classPrices.push_back({ .classId = static_cast<uint8_t>(c.class_id()), .tiers = prices(c.tiers()) });
            for (const auto& g : m.fusion_grades())
                config.fusionGrades.push_back({ .tier = static_cast<uint8_t>(g.tier()), .exaltedCores = static_cast<uint8_t>(g.cores()) });
            config.convergenceFusionPrices = prices(m.convergence_fusion_prices());
            config.convergenceTransferPrices = prices(m.convergence_transfer_prices());
            config.hasConvergence = true;
            // The server sends only the dust level above 100; the forge rates are fixed.
            const auto dust = static_cast<uint8_t>(m.dust_level());
            config.maxDustLevel = 100 + dust;
            config.maxDustCap = (std::min<uint16_t>(dust, 225) + 5) * 20;
            config.dustPercent = 75;
            config.dustToSliver = 20;
            config.sliverToCore = 50;
            config.dustPercentUpgrade = 30;
            config.normalDustFusion = 100;
            config.convergenceDustFusion = 130;
            config.normalDustTransfer = 100;
            config.convergenceDustTransfer = 160;
            config.fusionChanceBase = 50;
            config.fusionChanceImproved = 15;
            config.fusionReduceTierLoss = 50;
            tibia::game::notify("forgeData", config);
        }
        break;
    case 138:
        if (shadow->HasExtension(GameserverMessageExaltationResult::gameserver_message_exaltation_result)) {
            const auto& m = shadow->GetExtension(GameserverMessageExaltationResult::gameserver_message_exaltation_result);
            ForgeResultData result;
            result.actionType = static_cast<uint8_t>(m.action_type());
            result.convergence = m.convergence();
            result.success = m.success();
            result.leftItemId = static_cast<uint16_t>(m.first_object().object_id());
            result.leftTier = static_cast<uint8_t>(m.first_object().tier());
            result.rightItemId = static_cast<uint16_t>(m.second_object().object_id());
            result.rightTier = static_cast<uint8_t>(m.second_object().tier());
            if (result.actionType != 1) { // a transfer has no bonus
                result.bonus = static_cast<uint8_t>(m.bonus());
                result.coreCount = static_cast<uint8_t>(m.core_count());
                if (m.has_bonus_object()) {
                    result.leftItemId = static_cast<uint16_t>(m.bonus_object().object_id());
                    result.leftTier = static_cast<uint8_t>(m.bonus_object().tier());
                }
            }
            tibia::game::notify("forgeResultData", result);
        }
        break;
    case 136:
        if (shadow->HasExtension(GameserverMessageExaltationHistory::gameserver_message_exaltation_history)) {
            const auto& m = shadow->GetExtension(GameserverMessageExaltationHistory::gameserver_message_exaltation_history);
            std::vector<ForgeHistory> history;
            history.reserve(static_cast<size_t>(m.entries_size()));
            for (const auto& e : m.entries())
                history.push_back(ForgeHistory{ e.timestamp(), static_cast<uint8_t>(e.action_type()), e.description(),
                                                static_cast<uint8_t>(e.bonus() ? 1 : 0) });
            tibia::game::notify("onBrowseForgeHistory", static_cast<uint16_t>(m.page()), static_cast<uint16_t>(m.last_page()),
                                static_cast<uint8_t>(m.entries_size()), history);
        }
        break;
    default:
        break;
    }
}

}
