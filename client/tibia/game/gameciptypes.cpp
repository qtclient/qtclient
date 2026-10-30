#include "game/gameciptypes.h"

#include "gamewindow/gamewindowciptypes.h"

#include <client/gamecore.h>

namespace tibia::game {

namespace {
struct CipTypedKeep {
    TExperienceCalculator tExperienceCalculator;
    TGameSessionMetaInfo tGameSessionMetaInfo;
    TGameWorldVolumeStorage tGameWorldVolumeStorage;
    TGameserverGameSessionMetaInfo tGameserverGameSessionMetaInfo;
    TSessiondumpGameSessionMetaInfo tSessiondumpGameSessionMetaInfo;
    TTraderStorage tTraderStorage;
    TTutorialSessiondumpGameSessionMetaInfo tTutorialSessiondumpGameSessionMetaInfo;
};
CipTypedKeep& typedKeep()
{
    static CipTypedKeep keep;
    return keep;
}

using Args = std::vector<std::any>;
using client::gamecore::argBool;
using client::gamecore::argInt;

} // namespace

TPassiveAbilityInfoStorage* TPassiveAbilityInfoStorage::s_instance = nullptr;

TPassiveAbilityInfoStorage::TPassiveAbilityInfoStorage(QObject* parent)
    : cip::tibia::game::TPassiveAbilityInfoStorage(parent)
    , m_model(new gamewindow::TPassiveAbilityListDataModel(this))
{
    s_instance = this;
    m_hooks.emplace_back("g_game", "onPassiveData", [this](LuaObject*, const Args& args) {
        m_currentCooldown = static_cast<uint32_t>(argInt(args, 0));
        m_maxCooldown = static_cast<uint32_t>(argInt(args, 1));
        m_canDecay = argBool(args, 2);
        // CipSoft AssignPassiveAbilityActionDialog lists known passives by enum id.
        // Cooldown packet does not carry the catalog; keep a single live row so the
        // CipSoft list model is non-empty while the ability is on cooldown / ready.
        std::vector<gamewindow::TPassiveAbilityListDataModel::Entry> entries;
        gamewindow::TPassiveAbilityListDataModel::Entry entry;
        entry.id = 1;
        entry.name = QStringLiteral("Passive Ability");
        entry.iconUrl = QStringLiteral("image://passiveability-icons-32x32/1");
        entry.usableByCurrentVocation = true;
        entries.push_back(std::move(entry));
        m_model->setEntries(std::move(entries));
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onGameEnd", [this](LuaObject*, const Args&) {
        m_currentCooldown = 0;
        m_maxCooldown = 0;
        m_canDecay = false;
        m_model->setEntries({});
        return std::any{};
    });
}

TPassiveAbilityInfoStorage* TPassiveAbilityInfoStorage::instance()
{
    return s_instance;
}

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    (void)typedKeep();
    // Empty-moc / session-owned CIP QObject surfaces (live under the game session).
    new TBlessingsProtocolMessageHandler(parent);
    new TBossDifficultySelectionProtocolMessageHandler(parent);
    new TEditTextProtocolMessageHandler(parent);
    new TGameActionRouter(parent);
    new TGameEventProtocolMessageHandler(parent);
    new TGameProtocolMessageHandler(parent);
    new TInternalGameActionRouter(parent);
    new TOutfitDialogStorage(parent);
    new TPassiveAbilityInfoStorage(parent);
    new TPlayerData(parent);
    new TPlayerGameActionHandler(parent);
    new TPlayerProtocolMessageHandler(parent);
    new TPlayerSkillStats(parent);
    new TPremiumProtocolMessageHandler(parent);
    new TPremiumTimeStorage(parent);
    new TServerModalDialogProtocolMessageHandler(parent);
    new TSpellBookProtocolMessageHandler(parent);
}

} // namespace tibia::game
