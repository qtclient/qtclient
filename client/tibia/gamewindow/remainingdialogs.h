#pragma once

#include "client/gamecore.h"
#include "gamewindow/dialogmanager.h"
#include "trade/tradeciptypes.h"

#include <cip_gen.h>

#include <QAbstractListModel>
#include <QJsonValue>
#include <QPair>
#include <QPointer>
#include <QQuickItem>
#include <QSet>
#include <QString>
#include <functional>
#include <utility>
#include <vector>

namespace tibia::gamewindow {

class TQMLCharacterData;

/// CipSoft CharacterTradeNotifications.qml roles: notification, status.
class TCharacterTradeNotificationModel final : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Role { NotificationRole = Qt::UserRole + 1, StatusRole };
    explicit TCharacterTradeNotificationModel(QObject* parent = nullptr);
    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void setEntries(std::vector<trade::TCharacterTradeConditionEntry> entries);

private:
    std::vector<trade::TCharacterTradeConditionEntry> m_entries;
};

/// CipSoft CharacterTradeHighlightPicker roles: categoryicon, highlight, highlightid, category.
class TCharacterTradeHighlightModel final : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Role { CategoryIconRole = Qt::UserRole + 1, HighlightRole, HighlightIdRole, CategoryRole };
    explicit TCharacterTradeHighlightModel(QObject* parent = nullptr);
    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void setEntries(std::vector<trade::TCharacterTradeHighlightEntry> entries);
    const std::vector<trade::TCharacterTradeHighlightEntry>& entries() const { return m_entries; }

private:
    std::vector<trade::TCharacterTradeHighlightEntry> m_entries;
};

/// CipSoft CharacterTradeItemPicker roles: id, name, amount, description, upgradetier.
class TCharacterTradeItemModel final : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Role { IdRole = Qt::UserRole + 1, NameRole, AmountRole, DescriptionRole, UpgradeTierRole };
    explicit TCharacterTradeItemModel(QObject* parent = nullptr);
    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void setEntries(std::vector<trade::TCharacterTradeItemEntry> entries);
    const std::vector<trade::TCharacterTradeItemEntry>& entries() const { return m_entries; }

private:
    std::vector<trade::TCharacterTradeItemEntry> m_entries;
};

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

/// CIP TCharacterAuctionConfiguration (metaObject 0x10080ec80).
class TCharacterAuctionConfiguration final : public cip::tibia::gamewindow::TCharacterAuctionConfiguration
{
    Q_OBJECT
public:
    explicit TCharacterAuctionConfiguration(QObject* parent = nullptr);

    void setSelectedHighlightCount(int count);
    void setSelectedItemCount(int count);
    void setAuctionBounds(int minimumBid, int minEnd, int maxEnd);
};

/// CIP TCharacterTradeDialogController — CharacterTradeDialog.qml.
class TCharacterTradeDialogController final : public TDialogController<cip::tibia::gamewindow::TCharacterTradeDialogController>
{
    Q_OBJECT
public:
    explicit TCharacterTradeDialogController(QObject* parent = nullptr);
    static TCharacterTradeDialogController* instance();

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
    void onStorageUpdated(trade::TCharacterTradeStorage::DataType dataType);
    void refreshOverviewModels();
    static TCharacterTradeDialogController* s_instance;
    TCharacterAuctionConfiguration* m_auction = nullptr;
    TCharacterTradeNotificationModel* m_conditions = nullptr;
    TCharacterTradeHighlightModel* m_highlights = nullptr;
    TCharacterTradeItemModel* m_items = nullptr;
    TCharacterTradeNotificationModel* m_confirmations = nullptr;
    TCharacterTradeHighlightModel* m_overviewHighlights = nullptr;
    TCharacterTradeItemModel* m_overviewItems = nullptr;
    QSet<int> m_selectedHighlightIds;
    QSet<QPair<int, int>> m_selectedItems;
};

/// CIP TCharacterTradeDuePaymentDialogController.
class TCharacterTradeDuePaymentDialogController final
    : public TDialogController<cip::tibia::gamewindow::TCharacterTradeDuePaymentDialogController>
{
    Q_OBJECT
public:
    explicit TCharacterTradeDuePaymentDialogController(QObject* parent = nullptr);

public Q_SLOTS:
    void onClosePressed(bool DoNotShowAgain) override;
};

/// CIP TCharacterWorldTransferDialogController.
class TCharacterWorldTransferDialogController final
    : public TDialogController<cip::tibia::gamewindow::TCharacterWorldTransferDialogController>
{
    Q_OBJECT
public:
    explicit TCharacterWorldTransferDialogController(QObject* parent = nullptr);

    void setTransfer(const QString& characterName, bool express, QList<QObject*> conditions, QList<QObject*> worlds, bool canTransfer);

public Q_SLOTS:
    void closeDialog() override;
    void requestClose() override;
    void requestWorldTransfer(const QString& TargetWorld) override;

Q_SIGNALS:
    void worldTransferRequested(const QString& targetWorld, bool express);
};

/// CIP TCinematicSessiondumpPlayerController (0x100812300).
class TCinematicSessiondumpPlayerController final : public cip::tibia::gamewindow::TCinematicSessiondumpPlayerController
{
    Q_OBJECT
public:
    explicit TCinematicSessiondumpPlayerController(QObject* parent = nullptr);

public Q_SLOTS:
    void onBenchmarkWindowClosed() override;
};

/// CIP TConfirmMoveObjectToStashDialogController — ConfirmMoveObjectToStashDialog.qml (v3 0x10087d320).
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

/// CIP TCrashReportDialogController — CrashReportDialog.qml (v3 0x10087d660).
class TCrashReportDialogController final : public TDialogController<cip::tibia::gamewindow::TCrashReportDialogController>
{
    Q_OBJECT
public:
    explicit TCrashReportDialogController(QObject* parent = nullptr);

    void prepare(const QString& minimal, const QString& character, const QString& logs);
    Q_INVOKABLE void sendReport(const QString& PlayerComment, bool FullReport) override;
};

/// CIP TEmergencyDialogController — ImportantBalancingMessageDialog.qml (v3 0x10087e510).
class TEmergencyDialogController final : public TDialogController<cip::tibia::gamewindow::TEmergencyDialogController>
{
    Q_OBJECT
public:
    explicit TEmergencyDialogController(QObject* parent = nullptr);

    void showEmergency(const QVariant& data);

public Q_SLOTS:
    void onOKClicked() override;
};

/// CIP TGameSessionDisconnectReactionController (v3 0x10087f5d0).
class TGameSessionDisconnectReactionController final : public cip::tibia::gamewindow::TGameSessionDisconnectReactionController
{
    Q_OBJECT
public:
    explicit TGameSessionDisconnectReactionController(QObject* parent = nullptr);

    void requestLoginAgain();
    void requestQuitAndPatch();
};

/// CIP TGenericConfirmWithDontShowAgainDialogController (v3 0x10087f780).
class TGenericConfirmWithDontShowAgainDialogController final
    : public TDialogController<cip::tibia::gamewindow::TGenericConfirmWithDontShowAgainDialogController>
{
    Q_OBJECT
public:
    explicit TGenericConfirmWithDontShowAgainDialogController(QObject* parent = nullptr);

    void configure(const QString& caption, const QString& content, const QString& dontShowAgain, bool showAgain = true);
    void setConfirmHandler(std::function<void(bool showAgain)> onYes);

public Q_SLOTS:
    void showDialog() override;
    void onYesClicked(bool ShowDialogAgain) override;
    void onNoClicked() override;

private:
    std::function<void(bool)> m_onYes;
};

/// CIP THirelingNameChangeController (v3 0x10087f900) — empty moc; store rename owner.
class THirelingNameChangeController final : public cip::tibia::gamewindow::THirelingNameChangeController
{
    Q_OBJECT
public:
    explicit THirelingNameChangeController(QObject* parent = nullptr);
    static THirelingNameChangeController* instance();

private:
    static THirelingNameChangeController* s_instance;
};

/// CIP TInfoBannerController (v3 0x10087fe60) — tutorial/InfoBanner*.qml.
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

/// CIP TLenshelpController — LenshelpTemplate.qml (v3 0x100c4c920).
class TLenshelpController final : public cip::tibia::gamewindow::TLenshelpController
{
    Q_OBJECT
public:
    explicit TLenshelpController(QObject* parent = nullptr);
    static TLenshelpController* instance();

    void showHelp(const QString& caption, const QString& content);
    void hideHelp();

private:
    static TLenshelpController* s_instance;
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

/// CIP TLoginConfirmationCodeEnterTextDialogController — LoginConfirmationCodeEnterDialog.qml.
class TLoginConfirmationCodeEnterTextDialogController final
    : public TDialogController<cip::tibia::gamewindow::TLoginConfirmationCodeEnterTextDialogController>
{
    Q_OBJECT
public:
    explicit TLoginConfirmationCodeEnterTextDialogController(QObject* parent = nullptr);

public Q_SLOTS:
    void onCancelClicked() override;
    void onOkClicked(const QString& EnteredText) override;
    void onSendClicked() override;
    void getEnteredText() override;
    void setHasPendingConfirmationCode(bool hasCode) override;

Q_SIGNALS:
    void codeEntered(const QString& code);
    void confirmationCodeResendRequested();
};

/// CIP TMainCharacterChangeDialogController — MainCharacterChangeDialog.qml.
class TMainCharacterChangeDialogController final
    : public TDialogController<cip::tibia::gamewindow::TMainCharacterChangeDialogController>
{
    Q_OBJECT
public:
    explicit TMainCharacterChangeDialogController(QObject* parent = nullptr);

public Q_SLOTS:
    void closeDialog() override;
    void requestClose() override;
    void requestMainCharChange(const QString& ChosenCharacter) override;

Q_SIGNALS:
    void mainCharacterChangeRequested(const QString& chosenCharacter);
};

/// CIP TMarketCoinTransactionDetailsDialogController — MarketCoinTransactionDetails.qml.
class TMarketCoinTransactionDetailsDialogController final
    : public TDialogController<cip::tibia::gamewindow::TMarketCoinTransactionDetailsDialogController>
{
    Q_OBJECT
public:
    explicit TMarketCoinTransactionDetailsDialogController(QObject* parent = nullptr);

public Q_SLOTS:
    void showDialog() override;
    void closeDialog() override;
    Q_INVOKABLE void requestClose() override;
};

/// CIP TMultiOfflineTrainingDialogController — MultiOfflineTrainingDialog.qml.
class TMultiOfflineTrainingDialogController final
    : public TDialogController<cip::tibia::gamewindow::TMultiOfflineTrainingDialogController>
{
    Q_OBJECT
public:
    explicit TMultiOfflineTrainingDialogController(QObject* parent = nullptr);
    static TMultiOfflineTrainingDialogController* instance();

    void showSkillsDialog();

    Q_INVOKABLE void requestClose() override;
    Q_INVOKABLE void startOfflineTraining(cip::tibia::qmlcomponents::enums::EOfflineTrainingSkillType Skill) override;

private:
    static TMultiOfflineTrainingDialogController* s_instance;
};

/// CIP TOutfitMemorialDialogController — OutfitMemorialDialog.qml.
class TOutfitMemorialDialogController final : public TDialogController<cip::tibia::gamewindow::TOutfitMemorialDialogController>
{
    Q_OBJECT
public:
    explicit TOutfitMemorialDialogController(QObject* parent = nullptr);
    static TOutfitMemorialDialogController* instance();

    void showMemorial(const QString& title);

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

    void showReward(const QString& text);

public Q_SLOTS:
    void onContinueClicked() override;
};

/// CIP TShowImageDialogController — ShowImageDialog.qml (OPEN_SHOW_IMAGE_DIALOG).
class TShowImageDialogController final : public TDialogController<cip::tibia::gamewindow::TShowImageDialogController>
{
    Q_OBJECT
public:
    explicit TShowImageDialogController(QObject* parent = nullptr);
    static TShowImageDialogController* instance();

    void showImages(const QStringList& paths, const QString& caption = {}, bool dragonHeader = false, bool crestHeader = false);

public Q_SLOTS:
    void requestClose() override;
    void nextClicked() override;
    void backClicked() override;
    void closeDialog() override;

private:
    void refreshCurrent();
    static TShowImageDialogController* s_instance;
    QStringList m_paths;
};

/// CIP TShowSpellBookDialogController — ShowSpellBookDialog.qml (v3 0x100e20f20).
class TShowSpellBookDialogController final : public TDialogController<cip::tibia::gamewindow::TShowSpellBookDialogController>
{
    Q_OBJECT
public:
    explicit TShowSpellBookDialogController(QObject* parent = nullptr);

    void showSpellBook(int spellBookId, const QString& text);

    Q_INVOKABLE void openCyclopedia() override;
    Q_INVOKABLE void requestClose() override;
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

/// CIP TTutorialOverlayController (v3 0x100882170) — tutorial/TutorialOverlayTemplate.qml.
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

} // namespace tibia::gamewindow
