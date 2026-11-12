#pragma once

// CIP-named game surfaces (promoted from cipwire).

#include "client/gamecore.h"
#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QByteArray>
#include <QObject>

#include <cstdint>
#include <vector>

namespace tibia {
namespace protobuf {
namespace protocol {
class GameserverMessageLoginAdvice;
class GameserverMessageLoginError;
class GameserverMessageLoginSuccess;
class GameserverMessageLoginWait;
class GameserverMessagePendingStateEntered;
class GameserverMessageSessionDumpMarker;
class GameserverMessageSessionDumpStart;
class GameserverMessageSessionEndInformation;
class GameserverMessageShowMultiOfflineTrainingDialog;
}
}
}

namespace tibia {
namespace gamewindow {
class TPassiveAbilityListDataModel;
}
}

namespace tibia {
namespace game {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);

/// CIP `game::FPSDropsLogger` — pauses drop logging while a modal dialog is up
/// (CIP private slot `onDialogVisibleChanged`). Tip binds
/// `TDialogStackController::dialogVisible` lazily; does not own the FPS ring.
class FPSDropsLogger final : public cip::tibia::game::FPSDropsLogger
{
    Q_OBJECT
public:
    explicit FPSDropsLogger(QObject* parent = nullptr);
    ~FPSDropsLogger() override;

    static FPSDropsLogger* instance();

    bool dialogVisible() const { return m_dialogVisible; }
    /// CIP: suppress drop samples while any stack dialog is visible.
    bool loggingSuppressed() const { return m_dialogVisible; }

public Q_SLOTS:
    /// CIP moc private slot — qtclient public for stack wiring.
    void onDialogVisibleChanged(bool Visible);

private:
    void ensureDialogStackBound();

    bool m_dialogVisible = false;
    bool m_dialogStackBound = false;
    QMetaObject::Connection m_dialogConn;
    QMetaObject::Connection m_gameStartedConn;
};

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

/// CIP `game::TGameProtocolMessageHandler` — native Q→H for session dump/end.
class TGameProtocolMessageHandler final : public cip::tibia::game::TGameProtocolMessageHandler
{
    Q_OBJECT
public:
    explicit TGameProtocolMessageHandler(QObject* parent = nullptr);

    static TGameProtocolMessageHandler* liveInstance();
    static void setLiveInstance(TGameProtocolMessageHandler* handler);

    /// CIP slot: SessionDumpStart (S 0x04 / 4) — native Q→H apply.
    void handleSessionDumpStartMessage(
        const ::tibia::protobuf::protocol::GameserverMessageSessionDumpStart& SessionDumpStartMessage);

    /// CIP slot: PendingStateEntered (S 0x0A / 10) — native Q→H apply.
    void handlePendingStateEnteredMessage(
        const ::tibia::protobuf::protocol::GameserverMessagePendingStateEntered& PendingStateEnteredMessage);

    /// CIP slot: SessionEndInformation (S 0x18 / 24) — native Q→H apply.
    void handleSessionEndInformationMessage(
        const ::tibia::protobuf::protocol::GameserverMessageSessionEndInformation& Message);

    /// CIP slot: ShowMultiOfflineTrainingDialog (S 0x1B / 27) — native Q→H apply.
    void handleShowMultiOfflineTrainingDialogMessage(
        const ::tibia::protobuf::protocol::GameserverMessageShowMultiOfflineTrainingDialog& Message);

    /// CIP slot: LoginSuccess (S 0x17 / 23) — native Q→H apply.
    void handleLoginSuccessMessage(
        const ::tibia::protobuf::protocol::GameserverMessageLoginSuccess& LoginSuccessMessage);

    /// Tip Q→H wire: peel structured opcodes from `serverMessageEnqueued`.
    void onServerMessageEnqueued(int opcode, const QByteArray& classicFrame, bool applyClassic);
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

}
}
