#pragma once

#include "store/storestorage.h"

#include "crashreportdata.h"

#include "authentication/authenticationjsondocumenthelper.h"
#include "client/gamecore.h"
#include "client/staticdata.h"
#include "gamewindow/dialogmanager.h"
#include "gamewindow/messagedialogcontroller.h"
#include "qtconnectionmanager.h"
#include "store/storestorage.h"
#include "taskboard/monsterlistmodelsortfilterproxy.h"
#include "trade/charactertrademodels.h"
#include "trade/tradeciptypes.h"

#include <cip_gen.h>

#include <QAbstractListModel>
#include <QJsonDocument>
#include <QJsonValue>
#include <QPair>
#include <QPointer>
#include <QQuickItem>
#include <QSet>
#include <QString>
#include <functional>
#include <memory>
#include <utility>
#include <vector>
#include <tuple>
#include <vector>

namespace tibia::game {
class IGameSession;
}

namespace tibia {
namespace game {
class IGameSession;
}
namespace input {
class IGameAction;
}
namespace gamewindow {

class TQMLCharacterData;

/// CIP TCharacterAuctionConfiguration (metaObject).
class TCharacterAuctionConfiguration final : public cip::tibia::gamewindow::TCharacterAuctionConfiguration
{
    Q_OBJECT
public:
    explicit TCharacterAuctionConfiguration(QObject* parent = nullptr);

    int selectedHighlightCount() const override { return static_cast<int>(m_highlights.size()); }
    int selectedItemCount() const override { return static_cast<int>(m_items.size()); }
    /// The chosen highlight ids (store items: their id + 0xffff).
    const QSet<int>& highlights() const { return m_highlights; }
    /// The chosen items: object id and upgrade tier.
    const QSet<QPair<int, int>>& items() const { return m_items; }
    void addHighlight(int highlightId);
    void removeHighlight(int highlightId);
    void clearHighlights();
    void addItem(int itemId, int upgradeTier);
    void removeItem(int itemId, int upgradeTier);
    void clearItems();

    void setAuctionBounds(int minimumBid, int minEnd, int maxEnd);
    /// Nothing configured or selected.
    void reset();

private:
    QSet<int> m_highlights;
    QSet<QPair<int, int>> m_items;
};

/// CIP TCharacterTradeDialogController — CharacterTradeDialog.qml.
class TCharacterTradeDialogController final : public TDialogController<cip::tibia::gamewindow::TCharacterTradeDialogController>
{
    Q_OBJECT
public:
    TCharacterTradeDialogController(QQmlEngine* Engine, QQuickItem* DialogParentItem, QObject* parent = nullptr);
    static TCharacterTradeDialogController* instance();

    /// CIP connector: the dialog closes and follows `GameSession`'s character
    /// trade configuration.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

    QObject* characterData() const override;
    QObject* auctionConfiguration() const override;
    QObject* conditionsModel() const override;
    QObject* highlightsModel() const override;
    QObject* itemsModel() const override;
    QObject* confirmationsModel() const override;
    bool hasUnfulfilledConditions() const override;
    bool hasConfirmationError() const override;
    QObject* characterOverviewHighlightsModel() const override;
    QObject* characterOverviewItemsModel() const override;

public Q_SLOTS:
    void showDialog() override;
    void closeDialog() override;
    void closeButtonClicked() override;
    void requestClose() override;
    void addHighlight(int HighlightID) override;
    void removeHighlight(int HighlightID) override;
    void addItem(int ItemID, int UpgradeTier) override;
    void removeItem(int ItemID, int UpgradeTier) override;
    void requestConditions() override;
    void requestCheckConfiguration() override;
    void requestConfirmConfiguration() override;

private:
    /// The auction the player configured: minimum bid, end, items, store
    /// items and highlights.
    struct TConfiguration
    {
        uint32_t minimumBid = 0;
        uint32_t auctionEnd = 0;
        std::vector<std::tuple<uint16_t, uint8_t>> items;
        std::vector<std::tuple<uint16_t, uint8_t>> storeItems;
        std::vector<uint16_t> highlights;
    };
    TConfiguration configuration() const;
    void onInfoUpdated(trade::TCharacterTradeStorage::DataType dataType);
    void refreshOverviewModels();
    static TCharacterTradeDialogController* s_instance;
    /// CIP +0xc8: the current session; +0xd8: its character trade storage.
    std::shared_ptr<game::IGameSession> m_gameSession;
    QPointer<trade::TCharacterTradeStorage> m_storage;
    /// CIP +0xa8.
    std::vector<QMetaObject::Connection> m_gameSessionConnections;
    TCharacterAuctionConfiguration* m_auction = nullptr;
    TCharacterTradeNotificationsAbstractListModel* m_conditions = nullptr;
    TTextualHighlightsSortProxyModel* m_highlights = nullptr;
    TItemHighlightsSortProxyModel* m_items = nullptr;
    TCharacterTradeNotificationsAbstractListModel* m_confirmations = nullptr;
    TTextualHighlightsAbstractListModel* m_overviewHighlights = nullptr;
    TItemHighlightsAbstractListModel* m_overviewItems = nullptr;
};

/// CIP TCharacterTradeDuePaymentDialogController.
class TCharacterTradeDuePaymentDialogController final
    : public TDialogController<cip::tibia::gamewindow::TCharacterTradeDuePaymentDialogController>
{
    Q_OBJECT
public:
    explicit TCharacterTradeDuePaymentDialogController(QObject* parent = nullptr);

    /// The login's due payments go to the list; the dialog opens unless the player chose not to be
    /// reminded until later. True when it opened.
    bool showDuePayments(const std::vector<authentication::TCharacterTradePayment>& payments);

public Q_SLOTS:
    /// "Do not show again" postpones the reminder by a day.
    void onClosePressed(bool DoNotShowAgain) override;
};

/// CharacterWorldTransferDialog.qml (CIP: TCharacterWorldTransferDialogController):
/// the store's world transfer offer for the character, the five transfer
/// conditions and the target worlds.
class TCharacterWorldTransferDialogController final
    : public TDialogController<cip::tibia::gamewindow::TCharacterWorldTransferDialogController>
{
    Q_OBJECT
public:
    explicit TCharacterWorldTransferDialogController(QObject* parent = nullptr);
    /// The dialog closes first (its conditions and worlds go).
    ~TCharacterWorldTransferDialogController() override;

    bool isExpressWorldTransfer() const override;
    /// Every condition is met.
    bool canTransfer() const override;

public Q_SLOTS:
    /// The dialog shows the offer's transfer information for the character.
    void showDialog(int OfferIDForWorldChange, const QString& CharacterName, const tibia::store::TWorldTransferInformation& WorldTransferInfo);
    void closeDialog() override;
    void requestClose() override;
    /// A chosen world confirms the offer and closes the dialog.
    void requestWorldTransfer(const QString& TargetWorld) override;

Q_SIGNALS:
    void targetWorldConfirmed(int OfferIDForWorldChange, const QString& WorldName);

private:
    /// Red skull, black skull, guild leader, house owner, coin offers: each
    /// condition is met when the character is not affected.
    void createConditions();
    void createWorlds();

    int m_offerId = 0;
    tibia::store::TWorldTransferInformation m_info;
};

/// CIP TCinematicSessiondumpPlayerController .
class TCinematicSessiondumpPlayerController final : public cip::tibia::gamewindow::TCinematicSessiondumpPlayerController
{
    Q_OBJECT
public:
    explicit TCinematicSessiondumpPlayerController(QObject* parent = nullptr);

public Q_SLOTS:
    void onBenchmarkWindowClosed() override;
};

/// CIP TConfirmMoveObjectToStashDialogController — ConfirmMoveObjectToStashDialog.qml
/// (v3): a move onto the stash or a stow of a container's content waits here
/// for the player's yes; then it goes out again marked as confirmed.
class TConfirmMoveObjectToStashDialogController final
    : public TDialogController<cip::tibia::gamewindow::TConfirmMoveObjectToStashDialogController>
{
    Q_OBJECT
public:
    explicit TConfirmMoveObjectToStashDialogController(QObject* parent = nullptr);

    /// SHOW_CONFIRM_MOVE_OBJECT_TO_STASH_DIALOG: its action waits, the dialog
    /// shows.
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction);
    /// The askBeforeStowContainerContent option.
    bool showThisDialogAgain() const override;
    /// The waiting action is dropped and the dialog closes.
    void closeDialog();

public Q_SLOTS:
    void showDialog() override;
    void onYesClicked(bool ShowDialogAgain) override;
    void onNoClicked() override;

private:
    ::tibia::input::IGameActionPtr m_gameAction;
};

/// CIP TCrashReportDialogController — CrashReportDialog.qml: shows a crash
/// report's annotations and completes them with the player's comment and the
/// report type before the client sends it.
class TCrashReportDialogController final : public TDialogController<cip::tibia::gamewindow::TCrashReportDialogController>
{
    Q_OBJECT
public:
    explicit TCrashReportDialogController(QObject* parent = nullptr);
    /// CIP: made in the engine under the game window's dialog parent item.
    TCrashReportDialogController(QQmlEngine* Engine, QQuickItem* DialogParentItem, QObject* parent = nullptr);

    /// The LogMessages and Character lines ("key: value"), one per line.
    QString minimalInformation() const override;
    QString characterName() const override;
    /// One "time: message" line per logged message.
    QString logMessages() const override;
    /// Comment and ReportType (Full | Minimal; a minimal report drops the log
    /// messages and the character name), then requestSendCrashReport.
    Q_INVOKABLE void sendReport(const QString& PlayerComment, bool FullReport) override;

public Q_SLOTS:
    void showDialog(std::shared_ptr<tibia::shared::TCrashReportData> CrashReportData);

private:
    std::shared_ptr<tibia::shared::TCrashReportData> m_crashReportData;
};

/// ImportantBalancingMessageDialog.qml (CIP: TEmergencyDialogController): the
/// important balancing message of the client's data file. OK with "show
/// again" cleared stores the message id as the last one seen.
class TEmergencyDialogController final : public TDialogController<cip::tibia::gamewindow::TEmergencyDialogController>
{
    Q_OBJECT
public:
    explicit TEmergencyDialogController(QObject* parent = nullptr);

    QVariant dialogData() const override;

public Q_SLOTS:
    void onOKClicked() override;

private:
    QJsonDocument m_data;
    int m_id = 0;
};

/// GenericConfirmWithDontShowAgainDialog.qml (CIP:
/// TGenericConfirmWithDontShowAgainDialogController): a yes/no question with a
/// "do not show again" check box. Yes emits yesClicked with the check box
/// state, No emits noClicked; both close the dialog.
class TGenericConfirmWithDontShowAgainDialogController final
    : public TDialogController<cip::tibia::gamewindow::TGenericConfirmWithDontShowAgainDialogController>
{
    Q_OBJECT
public:
    explicit TGenericConfirmWithDontShowAgainDialogController(QObject* parent = nullptr);
    TGenericConfirmWithDontShowAgainDialogController(const QString& caption, const QString& content, const QString& dontShowAgain,
                                                     bool showAgain, QObject* parent = nullptr);

public Q_SLOTS:
    void showDialog() override;
    void onYesClicked(bool ShowDialogAgain) override;
    void onNoClicked() override;
};



/// CIP TLoginConfirmationCodeEnterTextDialogController — LoginConfirmationCodeEnterDialog.qml:
/// the login confirmation code, the method to (re)send it with and where it goes.
class TLoginConfirmationCodeEnterTextDialogController final
    : public TDialogController<cip::tibia::gamewindow::TLoginConfirmationCodeEnterTextDialogController>
{
    Q_OBJECT
public:
    /// CIP ELoginConfirmationMethod as the dialog offers it.
    enum EMethod { EmailPrimary = 0, EmailSecondary = 1, Sms = 2 };
    /// CIP ConfirmationMethodTargets: where each method sends the code (empty: unavailable).
    struct ConfirmationMethodTargets
    {
        QString primaryEmail;
        QString secondaryEmail;
        QString sms;
        bool operator==(const ConfirmationMethodTargets&) const = default;
    };

    explicit TLoginConfirmationCodeEnterTextDialogController(QObject* parent = nullptr);
    /// CIP v12: shows the dialog with no code entered.
    void showDialog();

    QVariantMap availableMethods() const override { return const_cast<TLoginConfirmationCodeEnterTextDialogController*>(this)->getAvailableMethods(); }
    QString chosenMethod() const override { return const_cast<TLoginConfirmationCodeEnterTextDialogController*>(this)->getChosenMethod(); }
    void setChosenMethod(const QString& Value) override;
    void setAvailableMethods(const ConfirmationMethodTargets& Targets);

public Q_SLOTS:
    void onCancelClicked() override;
    void onOkClicked(const QString& EnteredText) override;
    QVariantMap getAvailableMethods() override;
    QString getChosenMethod() override;
    void onSendClicked() override;
    void getEnteredText() override;
    QString getAdditionalInfoText() override { return m_additionalInfoText; }
    bool getHasPendingConfirmationCode() override { return m_hasPendingCode; }
    void setHasPendingConfirmationCode(bool hasCode) override;

Q_SIGNALS:
    void confirmationCodeRequested(int Method);

private:
    ConfirmationMethodTargets m_targets;
    EMethod m_method = EmailPrimary;
};

/// MainCharacterChangeDialog.qml (CIP: TMainCharacterChangeDialogController):
/// the store's characters that can become the main character of the account.
class TMainCharacterChangeDialogController final
    : public TDialogController<cip::tibia::gamewindow::TMainCharacterChangeDialogController>
{
    Q_OBJECT
public:
    explicit TMainCharacterChangeDialogController(QObject* parent = nullptr);
    ~TMainCharacterChangeDialogController() override;

public Q_SLOTS:
    /// CIP: closes the dialog, keeps the offer and the characters and shows them.
    void showDialog(uint32_t OfferIDForMainCharChange, std::vector<store::TPossibleMainCharacterInformation> PossibleMainCharInfo);
    /// CIP: closes the dialog and deletes the character entries.
    void closeDialog() override;
    void requestClose() override;
    /// CIP: a chosen character confirms the change of the offer and closes.
    void requestMainCharChange(const QString& ChosenCharacter) override;

Q_SIGNALS:
    void mainCharChangeConfirmed(uint32_t OfferIDForMainCharChange, const QString& ChosenMainChar);

private:
    /// CIP: one TQmlPossibleMainChar per character of the offer.
    void createMainCharInfos();

    uint32_t m_offerId = 0;
    std::vector<store::TPossibleMainCharacterInformation> m_possibleMainChars;
};

/// MultiOfflineTrainingDialog.qml (CIP: TMultiOfflineTrainingDialogController):
/// the skills an offline training can train; a choice starts the training.
class TMultiOfflineTrainingDialogController final
    : public TDialogController<cip::tibia::gamewindow::TMultiOfflineTrainingDialogController>
{
    Q_OBJECT
public:
    TMultiOfflineTrainingDialogController(QQmlEngine* Engine, QQuickItem* DialogParentItem, QObject* parent = nullptr);
    ~TMultiOfflineTrainingDialogController() override;
    static TMultiOfflineTrainingDialogController* instance();

    /// CIP: SHOW_OFFLINE_TRAINING_SKILLS shows the dialog.
    void handleGameAction(const input::IGameAction& action);
    /// CIP: closes the dialog, rebuilds the skills and shows it again.
    void showSkillsDialog();
    /// CIP: closes the dialog and deletes the skill entries.
    void closeDialog();
    /// CIP: closes the dialog; the session's game protocol handler shows it.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

    QList<QObject*> skillsModel() const override;

    Q_INVOKABLE void requestClose() override;
    /// CIP: sends the start of the training (with a session) and closes.
    Q_INVOKABLE void startOfflineTraining(cip::tibia::qmlcomponents::enums::EOfflineTrainingSkillType Skill) override;

private:
    void rebuildSkillsModel();
    void clearSkillsModel();

    static TMultiOfflineTrainingDialogController* s_instance;
    std::shared_ptr<game::IGameSession> m_gameSession;
    std::vector<QMetaObject::Connection> m_gameSessionConnections;
};

/// CIP TOutfitMemorialDialogController — OutfitMemorialDialog.qml.
class TOutfitMemorialDialogController final : public TDialogController<cip::tibia::gamewindow::TOutfitMemorialDialogController>
{
    Q_OBJECT
public:
    TOutfitMemorialDialogController(QQmlEngine* Engine, QQuickItem* DialogParentItem, QObject* parent = nullptr);
    static TOutfitMemorialDialogController* instance();

    void showMemorial(const OutfitMemorialData& data);
    /// CIP: the session's game protocol handler opens the memorial.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

    Q_INVOKABLE void requestClose() override;

private:
    static TOutfitMemorialDialogController* s_instance;
    std::shared_ptr<game::IGameSession> m_gameSession;
    std::vector<QMetaObject::Connection> m_gameSessionConnections;
};

/// CIP TReturnerRewardDialogController — ReturnerRewardNotificationDialog.qml.
class TReturnerRewardDialogController final : public TDialogController<cip::tibia::gamewindow::TReturnerRewardDialogController>
{
    Q_OBJECT
public:
    explicit TReturnerRewardDialogController(QObject* parent = nullptr);

    QString dialogText() const override;

public Q_SLOTS:
    void onContinueClicked() override;
};

/// CIP TShowImageDialogController — ShowImageDialog.qml (OPEN_SHOW_IMAGE_DIALOG).
class TShowImageDialogController final : public TDialogController<cip::tibia::gamewindow::TShowImageDialogController>
{
    Q_OBJECT
public:
    /// CIP: one image and its tab icon.
    struct TImage
    {
        QString path;
        QString tabIcon;
        bool operator==(const TImage&) const = default;
    };
    /// CIP: what the dialog shows.
    struct TData
    {
        bool showDragonHeader = false;
        bool showCrestHeader = false;
        bool showCaption = false;
        QString caption;
        QString bannerPath;
        std::vector<TImage> images;
        bool operator==(const TData&) const = default;
    };

    TShowImageDialogController(QQmlEngine* Engine, QQuickItem* DialogParentItem, QObject* parent = nullptr);
    ~TShowImageDialogController() override;
    static TShowImageDialogController* instance();

    /// CIP handleShowImageDialogGameAction: takes the data and opens the dialog.
    void showImages(const QStringList& paths, const QString& caption = {}, bool dragonHeader = false, bool crestHeader = false);

    uint imageCount() const override { return static_cast<uint>(m_data.images.size()); }
    QString currentImagePath() const override;

public Q_SLOTS:
    void requestClose() override;
    void nextClicked() override;
    void backClicked() override;
    void closeDialog() override;

private:
    void setData(const TData& data);
    void setCurrentIndex(uint index);
    static TShowImageDialogController* s_instance;
    TData m_data;
};

/// The spell book a player reads (ShowSpellBookDialog.qml).
class TShowSpellBookDialogController final : public TDialogController<cip::tibia::gamewindow::TShowSpellBookDialogController>
{
    Q_OBJECT
public:
    TShowSpellBookDialogController(QQmlEngine* Engine, QQuickItem* DialogParentItem, QObject* parent = nullptr);

    /// Closes the dialog and follows the session's spell book handler (none
    /// without a session).
    void connectToGameSession(const std::shared_ptr<tibia::game::IGameSession>& GameSession);

    Q_INVOKABLE void openCyclopedia() override;
    Q_INVOKABLE void requestClose() override;

public Q_SLOTS:
    void showSpellBookDialog(const QString& Text, int AppearanceID);

private:
    std::shared_ptr<tibia::game::IGameSession> m_gameSession;
    std::vector<QMetaObject::Connection> m_gameSessionConnections;
};

/// CIP TSoulSealsDialogController — taskboard/SoulSealsDialog.qml: the
/// soul pit races (common and uncommon bestiary monsters) filtered by
/// difficulty and name; fighting one asks for confirmation first.
class TSoulSealsDialogController final : public TDialogController<cip::tibia::gamewindow::TSoulSealsDialogController>
{
    Q_OBJECT
public:
    TSoulSealsDialogController(QQmlEngine* Engine, QQuickItem* DialogParentItem, QObject* parent = nullptr);

    /// Closes the dialogs and follows the session's soul seals, the task
    /// board handler (which opens the dialog) and the fight confirmation.
    void connectToGameSession(std::shared_ptr<game::IGameSession> GameSession);

    void setSelectedMonsterRaceID(int v) override;
    QString selectedMonstername() const override;
    int selectedFightPrice() const override;
    QString soulSealsString() const override;
    int availableSoulSeals() const override;
    QStringList availableMonsterDifficulties() const override;
    void setIndexOfSelectedDifficulty(int v) override;
    void setMonstersNameFilter(const QString& v) override;
    QObject* monsterList() const override;

public Q_SLOTS:
    /// Marks the races whose animus mastery is completed; a dialog that is not
    /// open yet opens with every difficulty and no name filter.
    void showDialog(const std::vector<uint16_t>& AnimusMasteryRaceIds);
    void closeDialog() override;
    void requestClose() override;
    void fightselectedMonster() override;

private:
    void onFightConfirmed();
    void onFightDiscarded();

    std::shared_ptr<game::IGameSession> m_gameSession;
    shared::TQtConnectionManager m_gameSessionConnections;
    std::vector<taskboard::TMonsterDifficultyFilter> m_difficultyFilters;
    taskboard::TMonsterListModelSortFilterProxy m_monsterListProxy;
    TMessageDialogController m_fightConfirmation;
};

/// CIP TSurveyInvitationDialogController — SurveyInvitatioDialog.qml.
/// CIP TSurveyInvitationDialogController (SurveyInvitatioDialog.qml): the
/// invitation (id, text, token, date) and the client options' survey status.
/// Yes opens the survey; yes and no answer it (status 2), ask later leaves it
/// open (status 1).
class TSurveyInvitationDialogController final : public TDialogController<cip::tibia::gamewindow::TSurveyInvitationDialogController>
{
    Q_OBJECT
public:
    explicit TSurveyInvitationDialogController(QObject* parent = nullptr);

    /// CIP: takes the survey's data; a survey the client options do not know
    /// yet starts unanswered (status 0), and "ask later" is offered while it is
    /// unanswered.
    void setSurveyData(const authentication::TSurveyData& Survey);
    /// CIP: shows the invitation unless the player answered it (status 2); a
    /// shown invitation counts as "ask later" (status 1). Whether it shows.
    bool showSurveyInvitation(const authentication::TSurveyData& Survey);

    /// The invitation's date, "MMMM d, yyyy" in British English.
    QString dateString() const override;

public Q_SLOTS:
    void onYesClicked() override;
    void onNoClicked() override;
    void onAskLaterClicked() override;

private:
    /// Stores `status` in the client options' survey invitation.
    void setSurveyStatus(int status);

    int m_surveyId = 0;
    QString m_token;
    quint32 m_date = 0;
};

/// CIP TTutorialOverlayItemController.
class TTutorialOverlayItemController final : public cip::tibia::gamewindow::TTutorialOverlayItemController
{
    Q_OBJECT
public:
    explicit TTutorialOverlayItemController(QObject* parent = nullptr);
    /// CIP: the controller of a tutorial overlay item; it owns the item.
    explicit TTutorialOverlayItemController(QQuickItem* item);
    /// The item loses its parent and is deleted later.
    ~TTutorialOverlayItemController() override;

    QQuickItem* item() const { return m_item; }

private:
    QQuickItem* m_item = nullptr;
};

/// CIP TTutorialOverlayMouseFilterController.
class TTutorialOverlayMouseFilterController final : public cip::tibia::gamewindow::TTutorialOverlayMouseFilterController
{
    Q_OBJECT
public:
    explicit TTutorialOverlayMouseFilterController(QObject* parent = nullptr);
    /// CIP: the controller of a tutorial overlay item; it owns the item.
    explicit TTutorialOverlayMouseFilterController(QQuickItem* item);
    /// The item loses its parent and is deleted later.
    ~TTutorialOverlayMouseFilterController() override;

    QQuickItem* item() const { return m_item; }

private:
    QQuickItem* m_item = nullptr;
};

/// CIP TTwoFactorCodeEnterTextDialogController — TwoFactorTextEnterDialog.qml.
class TTwoFactorCodeEnterTextDialogController final
    : public TDialogController<cip::tibia::gamewindow::TTwoFactorCodeEnterTextDialogController>
{
    Q_OBJECT
public:
    explicit TTwoFactorCodeEnterTextDialogController(QObject* parent = nullptr);

public Q_SLOTS:
    void onCancelClicked() override;
    void onOkClicked(const QString& EnteredText) override;
    void onButtonClicked() override;
    void onControllerFinished() override;
    bool getIsTrustedDevice() override;
    /// Sets the flag without a change signal.
    void setIsTrustedDevice(bool TrustedDevice) override;
    ETwoFactorMethod getTwoFactorMethod() override;
    /// Sets the method without a change signal.
    void setTwoFactorMethod(ETwoFactorMethod Method) override;
    QString getAdditionalInfoText() override;
    uint getMaxInputLength() override;
};

}
}
