#pragma once

// CIP-named game surfaces (promoted from cipwire).

#include "client/gamecore.h"
#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

#include <cstdint>
#include <vector>

namespace tibia::gamewindow {
class TPassiveAbilityListDataModel;
}

namespace tibia::game {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TBlessingsProtocolMessageHandler final : public cip::tibia::game::TBlessingsProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::game::TBlessingsProtocolMessageHandler::TBlessingsProtocolMessageHandler;
};

class TBossDifficultySelectionProtocolMessageHandler final : public cip::tibia::game::TBossDifficultySelectionProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::game::TBossDifficultySelectionProtocolMessageHandler::TBossDifficultySelectionProtocolMessageHandler;
};

class TEditTextProtocolMessageHandler final : public cip::tibia::game::TEditTextProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::game::TEditTextProtocolMessageHandler::TEditTextProtocolMessageHandler;
};

class TExperienceCalculator final : public ::tibia::CipTyped {
public:
    TExperienceCalculator() = default;
};

class TGameActionRouter final : public cip::tibia::game::TGameActionRouter
{
    Q_OBJECT
public:
    using cip::tibia::game::TGameActionRouter::TGameActionRouter;
};

class TGameEventProtocolMessageHandler final : public cip::tibia::game::TGameEventProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::game::TGameEventProtocolMessageHandler::TGameEventProtocolMessageHandler;
};

class TGameProtocolMessageHandler final : public cip::tibia::game::TGameProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::game::TGameProtocolMessageHandler::TGameProtocolMessageHandler;
};

class TGameSessionBase final : public cip::tibia::game::TGameSessionBase
{
    Q_OBJECT
public:
    using cip::tibia::game::TGameSessionBase::TGameSessionBase;
};

class TGameSessionMetaInfo final : public ::tibia::CipTyped {
public:
    TGameSessionMetaInfo() = default;
};

class TGameWorldVolumeStorage final : public ::tibia::CipTyped {
public:
    TGameWorldVolumeStorage() = default;
};

class TGameserverGameSessionMetaInfo final : public ::tibia::CipTyped {
public:
    TGameserverGameSessionMetaInfo() = default;
};

class TInternalGameActionRouter final : public cip::tibia::game::TInternalGameActionRouter
{
    Q_OBJECT
public:
    using cip::tibia::game::TInternalGameActionRouter::TInternalGameActionRouter;
};

class TOutfitDialogStorage final : public cip::tibia::game::TOutfitDialogStorage
{
    Q_OBJECT
public:
    using cip::tibia::game::TOutfitDialogStorage::TOutfitDialogStorage;
};

class TPassiveAbilityInfoStorage final : public cip::tibia::game::TPassiveAbilityInfoStorage
{
    Q_OBJECT
public:
    explicit TPassiveAbilityInfoStorage(QObject* parent = nullptr);
    static TPassiveAbilityInfoStorage* instance();

    gamewindow::TPassiveAbilityListDataModel* listModel() const { return m_model; }
    uint32_t currentCooldown() const { return m_currentCooldown; }
    uint32_t maxCooldown() const { return m_maxCooldown; }
    bool canDecay() const { return m_canDecay; }

private:
    static TPassiveAbilityInfoStorage* s_instance;
    gamewindow::TPassiveAbilityListDataModel* m_model = nullptr;
    uint32_t m_currentCooldown = 0;
    uint32_t m_maxCooldown = 0;
    bool m_canDecay = false;
    std::vector<client::gamecore::Hook> m_hooks;
};

class TPlayerData final : public cip::tibia::game::TPlayerData
{
    Q_OBJECT
public:
    using cip::tibia::game::TPlayerData::TPlayerData;
};

class TPlayerGameActionHandler final : public cip::tibia::game::TPlayerGameActionHandler
{
    Q_OBJECT
public:
    using cip::tibia::game::TPlayerGameActionHandler::TPlayerGameActionHandler;
};

class TPlayerProtocolMessageHandler final : public cip::tibia::game::TPlayerProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::game::TPlayerProtocolMessageHandler::TPlayerProtocolMessageHandler;
};

class TPlayerSkillStats final : public cip::tibia::game::TPlayerSkillStats
{
    Q_OBJECT
public:
    using cip::tibia::game::TPlayerSkillStats::TPlayerSkillStats;
};

class TPremiumProtocolMessageHandler final : public cip::tibia::game::TPremiumProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::game::TPremiumProtocolMessageHandler::TPremiumProtocolMessageHandler;
};

class TPremiumTimeStorage final : public cip::tibia::game::TPremiumTimeStorage
{
    Q_OBJECT
public:
    using cip::tibia::game::TPremiumTimeStorage::TPremiumTimeStorage;
};

class TServerModalDialogProtocolMessageHandler final : public cip::tibia::game::TServerModalDialogProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::game::TServerModalDialogProtocolMessageHandler::TServerModalDialogProtocolMessageHandler;
};

class TSessiondumpGameSessionMetaInfo final : public ::tibia::CipTyped {
public:
    TSessiondumpGameSessionMetaInfo() = default;
};

class TSpellBookProtocolMessageHandler final : public cip::tibia::game::TSpellBookProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::game::TSpellBookProtocolMessageHandler::TSpellBookProtocolMessageHandler;
};

class TTraderStorage final : public ::tibia::CipTyped {
public:
    TTraderStorage() = default;
};

class TTutorialSessiondumpGameSession final : public cip::tibia::game::TTutorialSessiondumpGameSession
{
    Q_OBJECT
public:
    using cip::tibia::game::TTutorialSessiondumpGameSession::TTutorialSessiondumpGameSession;
};

class TTutorialSessiondumpGameSessionMetaInfo final : public ::tibia::CipTyped {
public:
    TTutorialSessiondumpGameSessionMetaInfo() = default;
};

} // namespace tibia::game
