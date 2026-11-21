#pragma once

// CIP-named game surfaces (promoted from cipwire).

#include <deque>
#include "client/gamecore.h"
#include "config/icharacterconfiguration.h"
#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QByteArray>
#include <QJsonArray>
#include <QObject>

#include <array>
#include <optional>
#include <cstdint>
#include <string>
#include <vector>

namespace tibia {
namespace protobuf {
namespace protocol {
class GameserverMessageVocationSpecificPlayerData;
class GameserverMessageBlessingsDialog;
class GameserverMessageBossDifficultySelection;
class GameserverMessageEditList;
class GameserverMessageEditGuildMessage;
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
class GameserverMessageBlessings;
class GameserverMessageClientCheck;
class GameserverMessageConfigureCreaturePodium;
class GameserverMessageDead;
class GameserverMessageGameEvent;
class GameserverMessageHirelingNameChange;
class GameserverMessageInspectionList;
class GameserverMessageLoginChallenge;
class GameserverMessageOutfit;
class GameserverMessageOutfitMemorial;
class GameserverMessagePing;
class GameserverMessagePingBack;
class GameserverMessageReadyForSecondaryConnection;
class GameserverMessageSnapBack;
class GameserverMessageCreatureSpeed;
class GameserverMessageSpellDataModification;
class GameserverMessageStop;
class GameserverMessageSwitchPreset;
class GameserverMessageUnlockedSpells;
class GameserverMessageWorldEntered;
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

/// The session's handler of the boss difficulty selection message: it opens,
/// updates or closes the dialog.
class TBossDifficultySelectionProtocolMessageHandler final : public cip::tibia::game::TBossDifficultySelectionProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::game::TBossDifficultySelectionProtocolMessageHandler::TBossDifficultySelectionProtocolMessageHandler;

    void handleBossDifficultySelectionMessage(const ::tibia::protobuf::protocol::GameserverMessageBossDifficultySelection& message);

Q_SIGNALS:
    void openBossDifficultySelectionDialog(const TBossDifficultySelectionDialogData& BossDifficultySelectionDialogData);
    void updateBossDifficultySelectionDialog(const TBossDifficultySelectionDialogUpdateData& BossDifficultySelectionDialogData);
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
    /// The guild message to edit.
    static void handleEditGuildMessageMessage(const ::tibia::protobuf::protocol::GameserverMessageEditGuildMessage& message);
};

class TGameEventProtocolMessageHandler final : public cip::tibia::game::TGameEventProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::game::TGameEventProtocolMessageHandler::TGameEventProtocolMessageHandler;

    void handleGameEventMessage(const ::tibia::protobuf::protocol::GameserverMessageGameEvent& message);
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

    void handleLoginAdviceMessage(const ::tibia::protobuf::protocol::GameserverMessageLoginAdvice& message);
    void handleLoginWaitMessage(const ::tibia::protobuf::protocol::GameserverMessageLoginWait& message);
    void handleDeadMessage(const ::tibia::protobuf::protocol::GameserverMessageDead& message);
    void handleReadyForSecondaryConnectionMessage(const ::tibia::protobuf::protocol::GameserverMessageReadyForSecondaryConnection& message);
    void handleInspectionListMessage(const ::tibia::protobuf::protocol::GameserverMessageInspectionList& message);
    void handleOutfitMemorialMessage(const ::tibia::protobuf::protocol::GameserverMessageOutfitMemorial& message);
    void handleUnlockedSpellsMessage(const ::tibia::protobuf::protocol::GameserverMessageUnlockedSpells& message);
    void handleShowMessageDialogMessage(const ::tibia::protobuf::protocol::GameserverMessageShowMessageDialog& message);
    void handleClientCheckMessage(const ::tibia::protobuf::protocol::GameserverMessageClientCheck& message);
    void handleWorldEnteredMessage(const ::tibia::protobuf::protocol::GameserverMessageWorldEntered& message);
    void handleLoginErrorMessage(const ::tibia::protobuf::protocol::GameserverMessageLoginError& message);
    void handleLoginChallengeMessage(const ::tibia::protobuf::protocol::GameserverMessageLoginChallenge& message);
    void handlePingMessage(const ::tibia::protobuf::protocol::GameserverMessagePing& message);
    void handlePingBackMessage(const ::tibia::protobuf::protocol::GameserverMessagePingBack& message);
    void handleSpellDataModificationMessage(const ::tibia::protobuf::protocol::GameserverMessageSpellDataModification& message);
};

class TGameWorldVolumeStorage final : public ::tibia::CipTyped {
public:
    TGameWorldVolumeStorage() = default;
};

/// The character's outfit presets (character configuration "outfitPresets", qtclient's
/// format {"presets": [...]}).
class TOutfitDialogStorage final : public cip::tibia::game::TOutfitDialogStorage, public config::ICharacterConfiguration
{
    Q_OBJECT
public:
    explicit TOutfitDialogStorage(QObject* parent = nullptr);
    ~TOutfitDialogStorage() override;
    /// The game client's storage, the one its character configuration manager
    /// loads (CIP: the session's); null without one. The sessions' own
    /// storages are not registered.
    static TOutfitDialogStorage* instance();
    static void setInstance(TOutfitDialogStorage* storage);

    QString configurationName() const override { return QStringLiteral("outfitPresets"); }
    QJsonObject configurationToJson() const override
    {
        return QJsonObject{ { QStringLiteral("presets"), m_presets }, { QStringLiteral("showOffPresets"), m_showOffPresets } };
    }
    void configurationFromJson(const QJsonObject& json) override
    {
        m_presets = json.value(QStringLiteral("presets")).toArray();
        m_showOffPresets = json.value(QStringLiteral("showOffPresets")).toArray();
    }
    void resetConfiguration() override
    {
        m_presets = {};
        m_showOffPresets = {};
    }

    /// The outfit presets and the show-off socket presets; an edit is announced
    /// with announcePresetsChanged.
    QJsonArray& presets() { return m_presets; }
    QJsonArray& showOffPresets() { return m_showOffPresets; }
    /// Announces configurationChangedFromUserInput.
    void announcePresetsChanged();

private:
    QJsonArray m_presets;
    QJsonArray m_showOffPresets;
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

public:
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
    /// The monk's harmony, serenity and virtue spells.
    void handleVocationSpecificPlayerDataMessage(
        const ::tibia::protobuf::protocol::GameserverMessageVocationSpecificPlayerData& message);
    void handleOutfitMessage(const ::tibia::protobuf::protocol::GameserverMessageOutfit& message);
    void handleBlessingsMessage(const ::tibia::protobuf::protocol::GameserverMessageBlessings& message);
    void handleConfigureCreaturePodiumMessage(const ::tibia::protobuf::protocol::GameserverMessageConfigureCreaturePodium& message);
    void handleSwitchPresetMessage(const ::tibia::protobuf::protocol::GameserverMessageSwitchPreset& message);
    void handleCreatureSpeedMessage(const ::tibia::protobuf::protocol::GameserverMessageCreatureSpeed& message);
    void handleSnapBackMessage(const ::tibia::protobuf::protocol::GameserverMessageSnapBack& message);
    void handleStopMessage(const ::tibia::protobuf::protocol::GameserverMessageStop& message);
    void handleHirelingNameChangeMessage(const ::tibia::protobuf::protocol::GameserverMessageHirelingNameChange& message);
};

class TPremiumProtocolMessageHandler final : public cip::tibia::game::TPremiumProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::game::TPremiumProtocolMessageHandler::TPremiumProtocolMessageHandler;

    /// The premium features the server wants advertised.
    static void handlePremiumTriggerMessage(const ::tibia::protobuf::protocol::GameserverMessagePremiumTrigger& message);
    /// The client's premium feature for a protocol feature id, none for an unknown one.
    static std::optional<uint8_t> premiumFeature(uint32_t trigger);
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

}
}
