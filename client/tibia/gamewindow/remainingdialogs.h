#pragma once

#include "store/storestorage.h"

#include "crashreportdata.h"

#include "authentication/authenticationjsondocumenthelper.h"
#include "client/gamecore.h"
#include "client/staticdata.h"
#include "gamewindow/dialogmanager.h"
#include "store/storestorage.h"
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
#include <utility>
#include <tuple>
#include <vector>

namespace tibia {
namespace input {
class IGameAction;
}
namespace gamewindow {

class TQMLCharacterData;

/// CipSoft SoulSealsDialog.qml monster table.
class TSoulSealsMonsterListModel final : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Role {
        RaceIdRole = Qt::UserRole + 1,
        RaceIDRole,
        NameRole,
        AnimusMasteryCompletedRole,
        SoulPitFightPriceRole,
        TotalAmountRole,
        CategoryRole
    };
    struct Entry {
        int raceId = 0;
        QString name;
        bool animusMasteryCompleted = false;
        int soulPitFightPrice = 0;
        int category = 0;
    };
    explicit TSoulSealsMonsterListModel(QObject* parent = nullptr);
    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void setEntries(std::vector<Entry> entries);
    Q_INVOKABLE int getRaceIDAtIndex(int rowIndex) const;
    const Entry* entryForRace(int raceId) const;

private:
    std::vector<Entry> m_entries;
};

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
    explicit TCharacterTradeDialogController(QObject* parent = nullptr);
    static TCharacterTradeDialogController* instance();

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

/// CIP TConfirmMoveObjectToStashDialogController — ConfirmMoveObjectToStashDialog.qml (v3).
class TConfirmMoveObjectToStashDialogController final
    : public TDialogController<cip::tibia::gamewindow::TConfirmMoveObjectToStashDialogController>
{
    Q_OBJECT
public:
    explicit TConfirmMoveObjectToStashDialogController(QObject* parent = nullptr);

    void setConfirmHandler(std::function<void(bool showAgain)> onYes);

public Q_SLOTS:
    void showDialog() override;
    void onYesClicked(bool ShowDialogAgain) override;
    void onNoClicked() override;

private:
    std::function<void(bool)> m_onYes;
};

/// CIP TCrashReportDialogController — CrashReportDialog.qml: shows a crash
/// report's annotations and completes them with the player's comment and the
/// report type before the client sends it.
class TCrashReportDialogController final : public TDialogController<cip::tibia::gamewindow::TCrashReportDialogController>
{
    Q_OBJECT
public:
    explicit TCrashReportDialogController(QObject* parent = nullptr);

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

/// CIP THirelingNameChangeController (v3) — empty moc; store rename owner.
class THirelingNameChangeController final : public cip::tibia::gamewindow::THirelingNameChangeController
{
    Q_OBJECT
public:
    explicit THirelingNameChangeController(QObject* parent = nullptr);
    static THirelingNameChangeController* instance();

private:
    static THirelingNameChangeController* s_instance;
};

/// CIP TInfoBannerController (v3) — tutorial/InfoBanner*.qml.
class TInfoBannerController final : public cip::tibia::gamewindow::TInfoBannerController
{
    Q_OBJECT
public:
    explicit TInfoBannerController(QQuickItem* mapWindowPane = nullptr, QObject* parent = nullptr);

    Q_INVOKABLE QQuickItem* mapWindowPane() override;
    void enqueue(cip::tibia::qmlcomponents::enums::EInfoBannerType type, const QJsonValue& extra = {});
    void clearQueue();

private:
    QPointer<QQuickItem> m_mapWindowPane;
};

/// CIP TLinkCopyWarningDialogController (names.txt TLinkCopyWarningDialogController).
class TLinkCopyWarningDialogController final : public cip::tibia::gamewindow::TLinkCopyWarningDialogController
{
    Q_OBJECT
public:
    explicit TLinkCopyWarningDialogController(QObject* parent = nullptr);

public Q_SLOTS:
    void onButtonClicked(cip::tibia::gamewindow::TMessageDialogButton::EMessageDialogButtonIdentifier ButtonIdentifier) override;
    void onYesClicked() override;
    void onNoClicked() override;

Q_SIGNALS:
    void copyConfirmed();
    void copyRejected();
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

/// One character the store offers as the new main character (CIP:
/// store::TPossibleMainCharacterInformation).
struct TPossibleMainCharacterInformation {
    QString characterName;
    QString worldName;
    QString vocation;
    int level = 0;
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
    void showDialog(uint32_t OfferIDForMainCharChange, std::vector<TPossibleMainCharacterInformation> PossibleMainCharInfo);
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
    std::vector<TPossibleMainCharacterInformation> m_possibleMainChars;
};

/// CIP TMarketCoinTransactionDetailsDialogController — MarketCoinTransactionDetails.qml.
class TMarketCoinTransactionDetailsDialogController final
    : public TDialogController<cip::tibia::gamewindow::TMarketCoinTransactionDetailsDialogController>
{
    Q_OBJECT
public:
    explicit TMarketCoinTransactionDetailsDialogController(QObject* parent = nullptr);

    /// Host bridge (until gw ports CIP's data getters): the storage's details.
    void setTransactionDetails(const store::TTransactionDetails& details);

public Q_SLOTS:
    void showDialog() override;
    void closeDialog() override;
    Q_INVOKABLE void requestClose() override;
};

/// MultiOfflineTrainingDialog.qml (CIP: TMultiOfflineTrainingDialogController):
/// the skills an offline training can train; a choice starts the training.
class TMultiOfflineTrainingDialogController final
    : public TDialogController<cip::tibia::gamewindow::TMultiOfflineTrainingDialogController>
{
    Q_OBJECT
public:
    explicit TMultiOfflineTrainingDialogController(QObject* parent = nullptr);
    ~TMultiOfflineTrainingDialogController() override;
    static TMultiOfflineTrainingDialogController* instance();

    /// CIP: SHOW_OFFLINE_TRAINING_SKILLS shows the dialog.
    void handleGameAction(const input::IGameAction& action);
    /// CIP: closes the dialog, rebuilds the skills and shows it again.
    void showSkillsDialog();
    /// CIP: closes the dialog and deletes the skill entries.
    void closeDialog();

    QList<QObject*> skillsModel() const override;

    Q_INVOKABLE void requestClose() override;
    /// CIP: sends the start of the training (with a session) and closes.
    Q_INVOKABLE void startOfflineTraining(cip::tibia::qmlcomponents::enums::EOfflineTrainingSkillType Skill) override;

private:
    void rebuildSkillsModel();
    void clearSkillsModel();

    static TMultiOfflineTrainingDialogController* s_instance;
    std::vector<client::gamecore::Hook> m_hooks;
};

/// CIP TOutfitMemorialDialogController — OutfitMemorialDialog.qml.
class TOutfitMemorialDialogController final : public TDialogController<cip::tibia::gamewindow::TOutfitMemorialDialogController>
{
    Q_OBJECT
public:
    explicit TOutfitMemorialDialogController(QObject* parent = nullptr);
    static TOutfitMemorialDialogController* instance();

    void showMemorial(const OutfitMemorialData& data);

    Q_INVOKABLE void requestClose() override;

private:
    static TOutfitMemorialDialogController* s_instance;
    std::vector<client::gamecore::Hook> m_hooks;
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

    explicit TShowImageDialogController(QObject* parent = nullptr);
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
    explicit TShowSpellBookDialogController(QObject* parent = nullptr);

    /// Closes the dialog and follows the new session's spell book handler.
    void onGameSessionChanged();

    Q_INVOKABLE void openCyclopedia() override;
    Q_INVOKABLE void requestClose() override;

public Q_SLOTS:
    void showSpellBookDialog(const QString& Text, int AppearanceID);
};

/// CIP TSoulSealsDialogController — taskboard/SoulSealsDialog.qml.
class TSoulSealsDialogController final : public TDialogController<cip::tibia::gamewindow::TSoulSealsDialogController>
{
    Q_OBJECT
public:
    explicit TSoulSealsDialogController(QObject* parent = nullptr);
    static TSoulSealsDialogController* instance();

    void setSelectedMonsterRaceID(int v) override;
    void setIndexOfSelectedDifficulty(int v) override;
    void setMonstersNameFilter(const QString& v) override;

public Q_SLOTS:
    void closeDialog() override;
    void requestClose() override;
    void fightselectedMonster() override;

private:
    void applyEntries(std::vector<TSoulSealsMonsterListModel::Entry> entries);
    void refreshSelection();
    void rebuildVisible();
    static TSoulSealsDialogController* s_instance;
    TSoulSealsMonsterListModel* m_allMonsters = nullptr;
    TSoulSealsMonsterListModel* m_visibleMonsters = nullptr;
    std::vector<client::gamecore::Hook> m_hooks;
};

/// CIP TSurveyInvitationDialogController — SurveyInvitatioDialog.qml.
class TSurveyInvitationDialogController final : public TDialogController<cip::tibia::gamewindow::TSurveyInvitationDialogController>
{
    Q_OBJECT
public:
    explicit TSurveyInvitationDialogController(QObject* parent = nullptr);

    void showInvitation(const QString& text, const QString& date, bool askLater);

public Q_SLOTS:
    void onYesClicked() override;
    void onNoClicked() override;
    void onAskLaterClicked() override;
};

/// CIP TTaskboardHuntingTaskRewardPurchaseConfirmationDialogController.
class TTaskboardHuntingTaskRewardPurchaseConfirmationDialogController final
    : public TDialogController<cip::tibia::gamewindow::TTaskboardHuntingTaskRewardPurchaseConfirmationDialogController>
{
    Q_OBJECT
public:
    explicit TTaskboardHuntingTaskRewardPurchaseConfirmationDialogController(QObject* parent = nullptr);

    void prepare(int appearanceId, int optionalId, const QString& title, const QString& cost, const QVariant& rewardType);

    Q_INVOKABLE void purchaseRequested() override;
    Q_INVOKABLE void closeRequested() override;

Q_SIGNALS:
    void purchaseAccepted(int optionalId);
};

/// CIP TTutorialOverlayController (v3) — tutorial/TutorialOverlayTemplate.qml.
class TTutorialOverlayController final : public TDialogController<cip::tibia::gamewindow::TTutorialOverlayController>
{
    Q_OBJECT
public:
    explicit TTutorialOverlayController(QObject* parent = nullptr);

public Q_SLOTS:
    void onMouseFilterAreaEntered() override;
    void onMouseFilterAreaExited() override;
};

/// CIP TTutorialOverlayItemController.
class TTutorialOverlayItemController final : public cip::tibia::gamewindow::TTutorialOverlayItemController
{
    Q_OBJECT
public:
    explicit TTutorialOverlayItemController(QObject* parent = nullptr);
};

/// CIP TTutorialOverlayMouseFilterController.
class TTutorialOverlayMouseFilterController final : public cip::tibia::gamewindow::TTutorialOverlayMouseFilterController
{
    Q_OBJECT
public:
    explicit TTutorialOverlayMouseFilterController(QObject* parent = nullptr);
};

/// CIP TTutorialOverlayMouseTrailAnimationController.
class TTutorialOverlayMouseTrailAnimationController final
    : public cip::tibia::gamewindow::TTutorialOverlayMouseTrailAnimationController
{
    Q_OBJECT
public:
    explicit TTutorialOverlayMouseTrailAnimationController(QObject* parent = nullptr);
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
    void setIsTrustedDevice(bool TrustedDevice) override;
    ETwoFactorMethod getTwoFactorMethod() override;
    QString getAdditionalInfoText() override;
    uint getMaxInputLength() override;

Q_SIGNALS:
    void codeEntered(const QString& code, bool trustedDevice);
    void emailResendRequested();
};

}
}
