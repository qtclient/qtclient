#include "highscores/highscoresciptypes.h"

#include "protocol/protocolciptypes.h"

#include <protocol.pb.h>

#include <string>

namespace tibia::highscores {

namespace {

THighscoresStorage* g_liveStorage = nullptr;

QString qstr(const std::string& text)
{
    return QString::fromStdString(text);
}

/// Client vocation ids are 1..5, promoted ones 11..15.
QString vocationName(const uint32_t vocation)
{
    static constexpr const char* Base[] = { "knight", "paladin", "sorcerer", "druid", "monk" };
    static constexpr const char* Promoted[] = { "knight_promoted", "paladin_promoted", "sorcerer_promoted", "druid_promoted", "monk_promoted" };
    const bool promoted = vocation >= 10;
    const uint32_t base = promoted ? vocation - 10 : vocation;
    if (base < 1 || base > 5)
        return qtTrId("none");
    return qtTrId(promoted ? Promoted[base - 1] : Base[base - 1]);
}

}

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    new THighscoresStorage(parent);
    new THighscoresProtocolMessageHandler(parent);
}

THighscoresStorage::THighscoresStorage(QObject* parent)
    : cip::tibia::highscores::THighscoresStorage(parent)
{
    g_liveStorage = this;
}

THighscoresStorage::~THighscoresStorage()
{
    if (g_liveStorage == this)
        g_liveStorage = nullptr;
}

THighscoresStorage* THighscoresStorage::liveInstance()
{
    return g_liveStorage;
}

void THighscoresProtocolMessageHandler::handleHighscoresMessage(const ::tibia::protobuf::protocol::GameserverMessageHighscores& m)
{
    THighscoresStorage* storage = THighscoresStorage::liveInstance();
    if (!storage)
        return;
    storage->m_state = static_cast<int>(m.no_data());
    if (storage->m_state != 0) {
        Q_EMIT storage->highScoresError();
        return;
    }

    storage->m_worlds.clear();
    for (const std::string& world : m.worlds())
        storage->m_worlds.append(qstr(world));
    storage->m_selectedWorld = qstr(m.selected_world());

    storage->m_vocations.clear();
    for (const auto& vocation : m.vocations())
        storage->m_vocations.emplace_back(vocation.vocation_id(), qstr(vocation.name()));
    storage->m_selectedVocation = m.selected_vocation();

    storage->m_categories.clear();
    for (const auto& category : m.categories())
        storage->m_categories.emplace_back(static_cast<int>(category.category_id()), qstr(category.name()));
    storage->m_selectedCategory = static_cast<int>(m.selected_category());

    storage->m_entries.clear();
    storage->m_entries.reserve(static_cast<size_t>(m.entries_size()));
    for (const auto& e : m.entries()) {
        storage->m_entries.push_back({ .rank = e.rank(), .character = qstr(e.name()), .loyaltyTitle = qstr(e.title()),
                                       .vocation = vocationName(e.vocation()), .world = qstr(e.world()),
                                       .level = static_cast<quint16>(e.level()), .isOwnCharacter = e.is_player(), .score = e.points() });
    }

    storage->m_selectedWorldTypes.assign(m.world_categories().begin(), m.world_categories().end());
    storage->m_battlEyeType = m.battleye_type();
    storage->m_page = static_cast<int>(m.page());
    storage->m_totalPages = static_cast<int>(m.page_count());
    storage->m_selectedIndex = m.selected_index();
    storage->m_showLoyaltyTitle = m.show_loyalty_title();
    storage->m_valueType = static_cast<int>(m.value_type());
    storage->m_lastUpdate = m.last_update();
    Q_EMIT storage->highScoresUpdated();
}

} // namespace tibia::highscores
