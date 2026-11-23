#pragma once

#include "authentication/authenticationjsondocumenthelper.h"
#include "gamewindow/characterinformationmodeldata.h"

#include <cip_gen.h>

#include <QCollator>
#include <QList>
#include <QString>
#include <QTimer>

namespace tibia {
namespace gamewindow {

class TPreventAutoCloseEventFilter;

/// What CipSoft's character selection hands on for a login: the character and
/// how to reach its world (CIP TCharacterLoginData).
struct TCharacterLoginData
{
    QString characterName;
    QString worldAddressProtected;
    QString worldAddressUnprotected;
    int worldPortUnprotected = 0;
    int worldPortProtected = 0;
    QString worldName;
    bool antiCheatProtection = false;
    bool tutorial = false;
    bool isMale = false;
};

/// CharacterSelection.qml's controller (CIP TCharacterSelectionController).
/// It keeps the account's characters and worlds; sorting, the hidden
/// characters, the outfits and the pinned characters are client options.
class TCharacterSelectionController : public cip::tibia::gamewindow::TCharacterSelectionController
{
    Q_OBJECT
public:
    explicit TCharacterSelectionController(QObject* parent = nullptr);
    bool clientSettingSmoothFiltering() const override;

    /// The selection is shown: the auto-close and server save timers run, and
    /// the application's input events keep it open.
    virtual void open();
    /// The selection is hidden: its timers stop and input is no longer watched.
    virtual void close();

    /// CIP: the account line - frozen (state 1), suspended (state 2), else
    /// free, free premium (type 2) or premium with the days left, red under 14.
    static QString accountStatusText(int accountState, int premiumType, int premiumDays);
    /// CIP: the free / premium part of the account line.
    static QString premiumStatusText(int premiumType, int premiumDays);

    /// The account's characters and worlds; the table is rebuilt when they changed.
    void setCharacters(const authentication::TCharacterList& characters, const authentication::TWorldList& worlds);
    const authentication::TCharacterList& characters() const { return m_characters; }
    /// Premium type (0 free, 1 premium, 2 free premium) and days left.
    void setPremium(int premiumType, quint16 premiumDays);
    void setAccountState(int accountState);
    void setRecoverySetupComplete(bool complete);
    /// The character the table preselects.
    virtual void setLastSelectedCharacterName(const QString& name);
    QString lastSelectedCharacterName() const { return m_lastSelectedCharacterName; }
    /// Logs in with the row named `name` (the first row when there is none).
    void selectCharacter(const QString& name);
    /// The character finished the tutorial.
    virtual void clearTutorialFlag(const QString& name);
    /// What the last confirmation handed on, one entry per selected row.
    std::vector<TCharacterLoginData> loginData() const { return m_loginData; }

    QString accountPremiumStatus() const override;
    QList<QObject*> characterList() const override { return m_characterList; }
    int lastSelectedIndex() const override;
    uint sortColumn() const override;
    Qt::SortOrder sortOrder() const override;
    bool showGetPremiumButton() const override;
    bool isPremium() const override;
    bool showHiddenCharacters() const override;
    bool showOutfits() const override;
    bool recoverySetupComplete() const override { return m_recoverySetupComplete; }

    void onCancelClicked() override;
    void onCharacterSelectionConfirmed(const QList<int>& SelectedCharacters) override;
    void onOptionsChanged() override;
    void setShowHiddenCharacters(bool Show) override;
    void setShowOutfits(bool Show) override;
    void setSortOrder(uint Column, Qt::SortOrder SordOrder) override;
    void onPinnedChanged(const QString& CharName, bool IsPinned) override;
    int getCharacterIndexForSearchString(const QString& SearchString) override;
    /// SHOW_CREATE_CHARACTER_DIALOG; its parameter whether the account has no characters.
    Q_INVOKABLE void createCharacterPressed() override;
    /// OPEN_URL_ACCESS_ACCOUNT.
    void manageAccountPressed() override;
    /// OPEN_URL_GET_PREMIUM.
    void onGetPremiumClicked() override;
    /// OPEN_URL_PREMIUM_FEATURES.
    void onMorePremiumFeaturesClicked() override;
    /// Once a second: after the server save, characters whose daily reward
    /// was collected can collect again.
    void checkServerSave();

public Q_SLOTS:
    /// Thirty seconds without input: CHARACER_SELECTION_AUTO_CLOSE is
    /// published, the selection closes and aborts.
    void onAutocloseTimer();
    /// An input event: the auto-close countdown starts again.
    void onPreventAutoclose();

Q_SIGNALS:
    void requestCharacterLogin(const tibia::gamewindow::TCharacterLoginData& LoginData);

private:
    /// The character named `name` (case-sensitive), the first one when none
    /// is; -1 for an empty name.
    int characterIndex(const QString& name) const;
    /// The table rows: the shown characters with their worlds, sorted.
    void updateCharacterList();
    void sortCharacterList();

    int m_premiumDays = 0;
    int m_premiumType = 0;
    int m_accountState = 0;
    authentication::TCharacterList m_characters;
    QString m_lastSelectedCharacterName;
    authentication::TWorldList m_worlds;
    /// Watches the application's input while the selection is open.
    TPreventAutoCloseEventFilter* m_preventAutoCloseEventFilter;
    QCollator m_collator;
    /// Single shot: closes the selection after thirty seconds without input.
    QTimer m_autocloseTimer;
    QTimer m_serverSaveTimer;
    std::vector<TCharacterLoginData> m_loginData;
    uint m_lastServerSaveCheck = 0;
};

}
}
