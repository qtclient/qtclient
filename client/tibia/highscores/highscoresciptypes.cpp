#include "highscores/highscoresciptypes.h"

#include "protocol/protocolciptypes.h"
#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>

#include <protocol.pb.h>

#include <string>
#include <tuple>
#include <vector>

namespace tibia::highscores {

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

void THighscoresProtocolMessageHandler::onServerMessageEnqueued(const int opcode, const QByteArray& /*classicFrame*/,
                                                                const bool applyClassic)
{
    using namespace ::tibia::protobuf::protocol;
    const auto* shadow = protocol::nativeServerMessage(opcode, applyClassic);
    if (!shadow || opcode != 177 || !shadow->HasExtension(GameserverMessageHighscores::gameserver_message_highscores))
        return;
    const auto& m = shadow->GetExtension(GameserverMessageHighscores::gameserver_message_highscores);
    if (m.no_data() != 0) {
        tibia::game::notify("onHighscoresNoData");
        return;
    }
    const std::vector<std::string> worlds(m.worlds().begin(), m.worlds().end());
    // The first vocation is "all vocations".
    std::string allVocationsName;
    std::vector<std::tuple<uint32_t, std::string>> vocations;
    for (int i = 0; i < m.vocations_size(); ++i) {
        if (i == 0)
            allVocationsName = m.vocations(0).name();
        else
            vocations.emplace_back(static_cast<uint32_t>(m.vocations(i).vocation_id()), m.vocations(i).name());
    }
    std::vector<std::tuple<uint8_t, std::string>> categories;
    for (const auto& c : m.categories())
        categories.emplace_back(static_cast<uint8_t>(c.category_id()), c.name());
    std::vector<std::tuple<uint32_t, std::string, std::string, uint8_t, std::string, uint16_t, uint8_t, uint64_t>> entries;
    for (const auto& e : m.entries())
        entries.emplace_back(e.rank(), e.name(), e.title(), static_cast<uint8_t>(e.vocation()), e.world(), static_cast<uint16_t>(e.level()),
                             static_cast<uint8_t>(e.is_player() ? 1 : 0), e.points());
    const auto worldType = static_cast<uint8_t>(m.world_categories_size() > 0 ? m.world_categories(0) : 0);
    tibia::game::processHighscore(worlds.empty() ? std::string() : worlds.front(), m.selected_world(), worldType,
                                  static_cast<uint8_t>(m.battleye_type()), vocations, categories, static_cast<uint16_t>(m.page()),
                                  static_cast<uint16_t>(m.page_count()), entries, m.last_update(), worlds, allVocationsName,
                                  static_cast<uint32_t>(m.selected_vocation()), static_cast<uint8_t>(m.selected_category()),
                                  static_cast<uint8_t>(m.selected_index()), m.show_loyalty_title(), static_cast<uint8_t>(m.value_type()));
}

} // namespace tibia::highscores
