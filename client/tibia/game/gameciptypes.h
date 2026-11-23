#pragma once

// CIP-named game surfaces (promoted from cipwire).

#include <deque>
#include "client/gamecore.h"
#include "config/icharacterconfiguration.h"
#include "game/passiveabilityinfostorage.h"
#include "shared/cip_typed.h"

#include <cip_gen.h>
#include <client/staticdata.h>

#include <QByteArray>
#include <QJsonArray>
#include <QObject>
#include <QPointer>

#include <array>
#include <memory>
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
class GameclientMessageEnterWorld;
class GameclientMessageClientDetails;
class GameclientMessageGoNorth;
class GameclientMessageGoEast;
class GameclientMessageGoSouth;
class GameclientMessageGoWest;
class GameclientMessageStop;
class GameclientMessageCancel;
class GameclientMessageGoNorthEast;
class GameclientMessageGoSouthEast;
class GameclientMessageGoSouthWest;
class GameclientMessageGoNorthWest;
class GameclientMessageGoPath;
class GameclientMessageRotateNorth;
class GameclientMessageRotateEast;
class GameclientMessageRotateSouth;
class GameclientMessageRotateWest;
}
}
}

namespace tibia::input {
class TPlayerMovementIntentHandler;
}
namespace tibia::creatures {
enum class EMovementDirection : quint8;
}

namespace tibia::systemsurvey {
class TSystemSurveyValueRetriever;
}

namespace tibia::creatures {
class TCreatureMovementSpeedCalculator;
}

namespace tibia {
namespace game {

class TCooldownStorage;

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);

class TBlessingsProtocolMessageHandler final : public cip::tibia::game::TBlessingsProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::game::TBlessingsProtocolMessageHandler::TBlessingsProtocolMessageHandler;

    void handleBlessingsDialogMessage(const ::tibia::protobuf::protocol::GameserverMessageBlessingsDialog& message);

Q_SIGNALS:
    /// CIP requestBlessingsDialog: the blessings dialog's data.
    void requestBlessingsDialog(const BlessDialogData& BlessingDialogData);
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
    void handleEditTextMessage(const ::tibia::protobuf::protocol::GameserverMessageEditText& message);
    /// A list to edit (house access lists).
    void handleEditListMessage(const ::tibia::protobuf::protocol::GameserverMessageEditList& message);
    /// The protocol's edit list type (0 to 3); throws std::invalid_argument
    /// for any other value.
    static uint8_t editListType(uint32_t value);
    /// The guild message to edit.
    void handleEditGuildMessageMessage(const ::tibia::protobuf::protocol::GameserverMessageEditGuildMessage& message);

Q_SIGNALS:
    /// CIP requestEditTextDialog: the text with its edit id, the item's type,
    /// the longest text allowed, its author and date and whether it was traded
    /// recently.
    void requestEditTextDialog(uint32_t EditIdentifier, uint ItemTypeId, int MaximumTextLength, const QString& Text,
                               const QString& TextAuthor, const QString& TextDate, bool RecentlyTraded);
    /// CIP requestEditListDialog: the list with its edit id and list type.
    void requestEditListDialog(uint32_t EditIdentifier, uint8_t EditListType, const QString& Text);
};

class TGameEventProtocolMessageHandler final : public cip::tibia::game::TGameEventProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::game::TGameEventProtocolMessageHandler::TGameEventProtocolMessageHandler;

    void handleGameEventMessage(const ::tibia::protobuf::protocol::GameserverMessageGameEvent& message);
};

/// CIP `game::TGameProtocolMessageHandler`: the login, session end, death and
/// dialog messages of the game server, handed on as signals the session and
/// the game window's controllers connect.
class TGameProtocolMessageHandler final : public cip::tibia::game::TGameProtocolMessageHandler
{
    Q_OBJECT
public:
    /// The session's speed calculator takes the login's speed formula and beat.
    explicit TGameProtocolMessageHandler(std::shared_ptr<::tibia::creatures::TCreatureMovementSpeedCalculator> SpeedCalculator,
                                         QObject* parent = nullptr);

    /// CIP's ESessionEndInformation of a protocol reason (0..4, 99, 100);
    /// throws std::runtime_error("unhandled enum value") for any other.
    static int toSessionEndInformation(uint32_t Reason);

    /// The speed formula and server beat; then the PvP frame hint, the login
    /// success and the client details (the operating system's name).
    void handleLoginSuccessMessage(
        const ::tibia::protobuf::protocol::GameserverMessageLoginSuccess& LoginSuccessMessage);

    /// Emits sessionEndInformation with the message's reason.
    void handleSessionEndInformationMessage(
        const ::tibia::protobuf::protocol::GameserverMessageSessionEndInformation& Message);

    /// A replay's start: the speed formula and server beat it was recorded with.
    void handleSessionDumpStartMessage(
        const ::tibia::protobuf::protocol::GameserverMessageSessionDumpStart& SessionDumpStartMessage);

    /// Enters the world again, unless the player died since the last enter
    /// (the death dialog decides then).
    void handlePendingStateEnteredMessage(
        const ::tibia::protobuf::protocol::GameserverMessagePendingStateEntered& PendingStateEnteredMessage);

    /// Stops entering the world on the next pending state; emits deadMessage and
    /// the death dialog request with the death type, the fair fight factor
    /// (an unfair death's penalty) and whether a subsequent blessing applies.
    void handleDeadMessage(const ::tibia::protobuf::protocol::GameserverMessageDead& DeadMessage);

    /// Emits requestOpenOutfitMemorialDialog with the message's costs and owners.
    void handleOutfitMemorialMessage(const ::tibia::protobuf::protocol::GameserverMessageOutfitMemorial& OutfitMemorialMessage);

    /// Emits requestShowMultiOfflineTrainingDialog.
    void handleShowMultiOfflineTrainingDialogMessage(
        const ::tibia::protobuf::protocol::GameserverMessageShowMultiOfflineTrainingDialog& Message);

    /// Sends an enter world message; the next pending state enters again.
    void emitSendEnterWorldMessage() override;

    void handleInspectionListMessage(const ::tibia::protobuf::protocol::GameserverMessageInspectionList& message);
    void handleShowMessageDialogMessage(const ::tibia::protobuf::protocol::GameserverMessageShowMessageDialog& message);
    void handleClientCheckMessage(const ::tibia::protobuf::protocol::GameserverMessageClientCheck& message);
    void handleWorldEnteredMessage(const ::tibia::protobuf::protocol::GameserverMessageWorldEntered& message);
    void handlePingMessage(const ::tibia::protobuf::protocol::GameserverMessagePing& message);
    void handlePingBackMessage(const ::tibia::protobuf::protocol::GameserverMessagePingBack& message);
    void handleSpellDataModificationMessage(const ::tibia::protobuf::protocol::GameserverMessageSpellDataModification& message);

Q_SIGNALS:
    void sendEnterWorldMessage(const ::tibia::protobuf::protocol::GameclientMessageEnterWorld& EnterWorldMessage);
    void sendClientDetails(const ::tibia::protobuf::protocol::GameclientMessageClientDetails& ClientDetailsMessage);
    /// CIP's ESessionEndInformation.
    void sessionEndInformation(int Reason);
    /// CIP's EDeathType (0 regular, 1 unfair, 2 adventurer's blessing, 3 no
    /// penalty) and TFairFightFactor.
    void requestOpenDeathDialog(int DeathType, int FairFightFactor, bool ApplicableForSubsequentBlessing);
    void requestOpenOutfitMemorialDialog(const OutfitMemorialData& MemorialData);
    /// CIP inspectObjectDataReceived: an inspected object's details.
    void inspectObjectDataReceived(const ItemInspectionData& InspectObjectData);
    /// CIP requestOpenInspectPlayerDialog: an inspected player's equipment and details.
    void requestOpenInspectPlayerDialog(const CyclopediaCharacterInspection& InspectedPlayer);

private:
    /// Sends the client details: the retriever's operating system name.
    void sendClientDetailsMessage(const ::tibia::systemsurvey::TSystemSurveyValueRetriever& ValueRetriever);

    std::shared_ptr<::tibia::creatures::TCreatureMovementSpeedCalculator> m_speedCalculator;
    /// Cleared by a death, set again when the world is entered.
    bool m_enterWorldOnPendingState = true;
};

class TGameWorldVolumeStorage final : public ::tibia::CipTyped {
public:
    TGameWorldVolumeStorage() = default;
};

/// The character's outfit presets and show-off socket presets (character
/// configuration "outfitdialog").
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

    QString configurationName() const override;
    /// The two preset lists under "customiseCharacterPresets" and
    /// "configureShowOffSocketPresets".
    QJsonObject configurationToJson() const override;
    /// Replaces both lists with the objects found under their keys (other
    /// values are dropped) and announces each list as changed.
    void configurationFromJson(const QJsonObject& json) override;
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

/// CIP `game::TPlayerProtocolMessageHandler` — native Q→H for the player's own data.
class TPlayerProtocolMessageHandler final : public cip::tibia::game::TPlayerProtocolMessageHandler
{
    Q_OBJECT
public:
    /// Spell, spell group, multi-use and passive ability delays go to
    /// `cooldownStorage` (the session's).
    /// `allowAllToInspectPlayer`: the client option the server learns when the
    /// world is entered.
    explicit TPlayerProtocolMessageHandler(TCooldownStorage* cooldownStorage, bool allowAllToInspectPlayer = false,
                                           QObject* parent = nullptr);

    /// Publishes ALLOW_ALL_TO_INSPECT_PLAYER or WITHDRAW_ALLOW_ALL_TO_INSPECT_PLAYER
    /// from the option, then emits worldEntered.
    void handleWorldEnteredMessage(const ::tibia::protobuf::protocol::GameserverMessageWorldEntered& message);
    /// The login's expert PvP mode into the player data.
    void handleLoginSuccessMessage(const ::tibia::protobuf::protocol::GameserverMessageLoginSuccess& message);

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

    /// The live session's movement intent handler: a snap back or stop goes to
    /// it while set (CipSoft holds a shared pointer; qtclient's session state owns it).
    void setPlayerMovementIntentHandler(::tibia::input::TPlayerMovementIntentHandler* Handler);

    /// Each sends its (empty) movement message.
    void goNorth();
    void goEast();
    void goSouth();
    void goWest();
    void stop();
    void cancel();
    void goNorthEast();
    void goSouthEast();
    void goSouthWest();
    void goNorthWest();
    /// Sends the steps as GoPath directions (1 east, counter-clockwise to 8
    /// south-east); a non-step direction throws std::invalid_argument.
    void goPath(const std::vector<::tibia::creatures::EMovementDirection>& Path);
    void rotateNorth();
    void rotateEast();
    void rotateSouth();
    void rotateWest();

Q_SIGNALS:
    void sendGoNorth(const ::tibia::protobuf::protocol::GameclientMessageGoNorth& GoNorthMessage);
    void sendGoEast(const ::tibia::protobuf::protocol::GameclientMessageGoEast& GoEastMessage);
    void sendGoSouth(const ::tibia::protobuf::protocol::GameclientMessageGoSouth& GoSouthMessage);
    void sendGoWest(const ::tibia::protobuf::protocol::GameclientMessageGoWest& GoWestMessage);
    void sendStop(const ::tibia::protobuf::protocol::GameclientMessageStop& StopMessage);
    void sendCancel(const ::tibia::protobuf::protocol::GameclientMessageCancel& CancelMessage);
    void sendGoNorthEast(const ::tibia::protobuf::protocol::GameclientMessageGoNorthEast& GoNorthEastMessage);
    void sendGoSouthEast(const ::tibia::protobuf::protocol::GameclientMessageGoSouthEast& GoSouthEastMessage);
    void sendGoSouthWest(const ::tibia::protobuf::protocol::GameclientMessageGoSouthWest& GoSouthWestMessage);
    void sendGoNorthWest(const ::tibia::protobuf::protocol::GameclientMessageGoNorthWest& GoNorthWestMessage);
    void sendGoPath(const ::tibia::protobuf::protocol::GameclientMessageGoPath& GoPathMessage);
    void sendRotateNorth(const ::tibia::protobuf::protocol::GameclientMessageRotateNorth& RotateNorthMessage);
    void sendRotateEast(const ::tibia::protobuf::protocol::GameclientMessageRotateEast& RotateEastMessage);
    void sendRotateSouth(const ::tibia::protobuf::protocol::GameclientMessageRotateSouth& RotateSouthMessage);
    void sendRotateWest(const ::tibia::protobuf::protocol::GameclientMessageRotateWest& RotateWestMessage);
    /// The server asks for a hireling's name (the hireling creature, the object it was asked from).
    void showHirelingNameChangeConfiguration(quint32 CreatureID, quint32 ObjectID);

private:
    TCooldownStorage* m_cooldownStorage = nullptr;
    QPointer<::tibia::input::TPlayerMovementIntentHandler> m_playerMovementIntentHandler;
    bool m_allowAllToInspectPlayer = false;
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

public Q_SLOTS:
    /// Only the spell book dialog type: its text and the book's appearance.
    void handleShowMessageDialogMessage(const ::tibia::protobuf::protocol::GameserverMessageShowMessageDialog& Message);

Q_SIGNALS:
    void requestShowSpellBookDialog(const QString& text, int SpellBook);
};

}
}
