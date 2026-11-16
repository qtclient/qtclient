#pragma once

// CIP-named game surfaces (promoted from cipwire).

#include <deque>
#include "client/gamecore.h"
#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QByteArray>
#include <QObject>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace tibia {
namespace protobuf {
namespace protocol {
class GameserverMessageBlessingsDialog;
class GameserverMessageBossDifficultySelection;
class GameserverMessageEditList;
class GameserverMessageEditText;
class GameserverMessagePremiumTrigger;
class GameserverMessageShowModalDialog;
class GameserverMessageShowMessageDialog;
class GameserverMessageLoginAdvice;
class GameserverMessagePlayerDataBasic;
class GameserverMessagePlayerSkills;
class GameserverMessagePlayerDataCurrent;
class GameserverMessagePlayerState;
class GameserverMessageSetTactics;
class GameserverMessageSpellDelay;
class GameserverMessageSpellGroupDelay;
class GameserverMessageMultiUseDelay;
class GameserverMessageResourceBalance;
class GameserverMessagePassiveAbilityData;
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

    /// Drops the recorded frame durations (CIP: on game session changes).
    void resetFrameDurations();

public Q_SLOTS:
    /// CIP moc private slot — qtclient public for stack wiring. Showing a
    /// dialog drops the recorded frame durations.
    void onDialogVisibleChanged(bool Visible);

private:
    void ensureDialogStackBound();

    /// CIP circular buffer of the last 200 frame durations and their sum.
    std::deque<qint64> m_frameDurations;
    qint64 m_frameDurationSum = 0;
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

    static void handleBlessingsDialogMessage(const ::tibia::protobuf::protocol::GameserverMessageBlessingsDialog& message);
};

/// What an update of the boss difficulty dialog carries: the selected
/// difficulty and the modifiers it brings.
struct TBossDifficultySelectionDialogUpdateData
{
    int selectedDifficulty = 0;
    std::vector<std::string> conModifiers;
    std::vector<std::string> proModifiers;

    bool operator==(const TBossDifficultySelectionDialogUpdateData&) const = default;
};

/// What opens the boss difficulty dialog: the update part plus the fight's
/// constants. `context` goes back to the server with every answer.
struct TBossDifficultySelectionDialogData : TBossDifficultySelectionDialogUpdateData
{
    bool isActiveUser = false;
    uint32_t context = 0;
    int raceId = 0;
    int lowestDifficulty = 0;
    int highestDifficultyOfGroup = 0;
    int personalHighestDifficulty = 0;
    float currentBadLuckDropBonus = 0.f;
    std::array<std::string, 5> playerNames;
};

/// Turns the boss difficulty selection message into open / update / close of
/// the dialog (published as onBossDifficultySelection / onBossDifficultyUpdate /
/// onBossDifficultyClose).
class TBossDifficultySelectionProtocolMessageHandler final : public cip::tibia::game::TBossDifficultySelectionProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::game::TBossDifficultySelectionProtocolMessageHandler::TBossDifficultySelectionProtocolMessageHandler;

    static void handleBossDifficultySelectionMessage(const ::tibia::protobuf::protocol::GameserverMessageBossDifficultySelection& message);
};

class TEditTextProtocolMessageHandler final : public cip::tibia::game::TEditTextProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::game::TEditTextProtocolMessageHandler::TEditTextProtocolMessageHandler;
    /// A text to read or edit (books, letters, signs).
    static void handleEditTextMessage(const ::tibia::protobuf::protocol::GameserverMessageEditText& message);
    /// A list to edit (house access lists).
    static void handleEditListMessage(const ::tibia::protobuf::protocol::GameserverMessageEditList& message);
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
    void onServerMessageEnqueued(int opcode);
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

/// CIP `game::TPlayerProtocolMessageHandler` — native Q→H for the player's own data.
class TPlayerProtocolMessageHandler final : public cip::tibia::game::TPlayerProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::game::TPlayerProtocolMessageHandler::TPlayerProtocolMessageHandler;

    static TPlayerProtocolMessageHandler* liveInstance();
    static void setLiveInstance(TPlayerProtocolMessageHandler* handler);

    /// Tip Q→H wire: peel structured opcodes from `serverMessageEnqueued`.
    void onServerMessageEnqueued(int opcode);

private:
    void handlePlayerDataBasicMessage(const ::tibia::protobuf::protocol::GameserverMessagePlayerDataBasic& message);
    void handlePlayerSkillsMessage(const ::tibia::protobuf::protocol::GameserverMessagePlayerSkills& message);
    void handlePlayerDataCurrentMessage(const ::tibia::protobuf::protocol::GameserverMessagePlayerDataCurrent& message);
    void handlePlayerStateMessage(const ::tibia::protobuf::protocol::GameserverMessagePlayerState& message);
    void handleSetTacticsMessage(const ::tibia::protobuf::protocol::GameserverMessageSetTactics& message);
    void handleSpellDelayMessage(const ::tibia::protobuf::protocol::GameserverMessageSpellDelay& message);
    void handleSpellGroupDelayMessage(const ::tibia::protobuf::protocol::GameserverMessageSpellGroupDelay& message);
    void handleMultiUseDelayMessage(const ::tibia::protobuf::protocol::GameserverMessageMultiUseDelay& message);
    void handleResourceBalanceMessage(const ::tibia::protobuf::protocol::GameserverMessageResourceBalance& message);
    void handlePassiveAbilityDataMessage(const ::tibia::protobuf::protocol::GameserverMessagePassiveAbilityData& message);
};

class TPremiumProtocolMessageHandler final : public cip::tibia::game::TPremiumProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::game::TPremiumProtocolMessageHandler::TPremiumProtocolMessageHandler;

    /// The premium features the server wants advertised.
    static void handlePremiumTriggerMessage(const ::tibia::protobuf::protocol::GameserverMessagePremiumTrigger& message);
    /// Hands the premium flag and its end to the premium time storage.
    static void handlePlayerDataBasicMessage(const ::tibia::protobuf::protocol::GameserverMessagePlayerDataBasic& message);
};

/// The account's premium state: the flag, the server time it ends at (0 for
/// none) and the local time it was received at, in epoch seconds.
class TPremiumTimeStorage final : public cip::tibia::game::TPremiumTimeStorage
{
    Q_OBJECT
public:
    explicit TPremiumTimeStorage(QObject* parent = nullptr);
    ~TPremiumTimeStorage() override;

    static TPremiumTimeStorage* liveInstance();

    bool isPremium() const { return m_premium; }
    /// Premium without an end date.
    bool hasUnlimitedPremium() const { return m_premiumUntil == 0 && m_premium; }
    /// Premium, or an end date still on record.
    bool hasPremiumRecord() const { return m_premiumUntil != 0 || m_premium; }
    /// The state as 0 (free), 1 (premium) or 2 (unlimited premium).
    int premiumState() const;
    bool isFreeAccount() const { return premiumState() == 0; }
    /// Whole days left between receiving the end date and the end date.
    uint16_t remainingDays() const;
    void setPremium(bool premium, uint32_t premiumUntil);
    void reset();

private:
    uint32_t m_premiumUntil = 0;
    bool m_premium = false;
    uint32_t m_receivedAt = 0;
};

class TServerModalDialogProtocolMessageHandler final : public cip::tibia::game::TServerModalDialogProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::game::TServerModalDialogProtocolMessageHandler::TServerModalDialogProtocolMessageHandler;

    static void handleShowModalDialogMessage(const ::tibia::protobuf::protocol::GameserverMessageShowModalDialog& message);
};

class TSessiondumpGameSessionMetaInfo final : public ::tibia::CipTyped {
public:
    TSessiondumpGameSessionMetaInfo() = default;
};

}
}

namespace tibia {
namespace game {

/// Opens the spell book dialog for the server's spell book message dialog.
class TSpellBookProtocolMessageHandler final : public cip::tibia::game::TSpellBookProtocolMessageHandler
{
    Q_OBJECT
public:
    explicit TSpellBookProtocolMessageHandler(QObject* parent = nullptr);
    ~TSpellBookProtocolMessageHandler() override;
    static TSpellBookProtocolMessageHandler* liveInstance();

public Q_SLOTS:
    /// Only the spell book dialog type: its text and the book's appearance.
    void handleShowMessageDialogMessage(const ::tibia::protobuf::protocol::GameserverMessageShowMessageDialog& Message);

Q_SIGNALS:
    void requestShowSpellBookDialog(const QString& text, int SpellBook);
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
