#pragma once

#include <cip_gen.h>

#include <QColor>
#include <QList>
#include <QString>

namespace tibia::gamewindow {

/// One row of the account's character list (login service "characters").
struct CharacterEntry
{
    QString name;
    int level = 0;
    QString vocation;
    QString world;
    QString worldRules;
    int outfitId = 0;
    QColor headColor;
    QColor torsoColor;
    QColor legsColor;
    QColor detailColor;
    bool firstAddOn = false;
    bool secondAddOn = false;
    bool isMainCharacter = false;
    bool isPinned = false;
    bool isHidden = false;
    /// Login `isonline` / in-game alt list: show Online under the world column.
    bool isOnline = false;
    /// Login `tutorial`: play CIP sessiondump cinematic before live Newhaven.
    bool tutorial = false;
};

/// CharacterSelection.qml's controller.
class TCharacterSelectionController final : public cip::tibia::gamewindow::TCharacterSelectionController
{
    Q_OBJECT
public:
    explicit TCharacterSelectionController(QObject* parent = nullptr);

    /// `preferredEntryIndex` is an index into `characters` (not the sorted view).
    /// Selection resolves as: last confirmed name if still present, else that entry,
    /// else the first visible row — matching CIP lastSelectedIndex.
    void setAccount(const QString& premiumStatus, bool isPremium, const QList<CharacterEntry>& characters,
                    int preferredEntryIndex = -1);
    const QList<CharacterEntry>& characters() const { return m_entries; }
    QString lastSelectedName() const { return m_lastSelectedName; }

    void onCancelClicked() override;
    void onCharacterSelectionConfirmed(const QList<int>& SelectedCharacters) override;
    void setShowHiddenCharacters(bool Show) override;
    void setShowOutfits(bool Show) override;
    void setSortOrder(uint Column, Qt::SortOrder SordOrder) override;
    void onPinnedChanged(const QString& CharName, bool IsPinned) override;
    int getCharacterIndexForSearchString(const QString& SearchString) override;
    /// CIP 0x100b73500: SHOW_CREATE_CHARACTER_DIALOG, its parameter whether
    /// the account has no characters.
    Q_INVOKABLE void createCharacterPressed() override;
    /// CIP: open the account site.
    void manageAccountPressed() override;

Q_SIGNALS:
    void characterChosen(const tibia::gamewindow::CharacterEntry& character);
    void cancelled();
    void createCharacterRequested(bool accountHasNoCharacters);

private:
    void rebuild();
    void resolveLastSelectedIndex(int preferredEntryIndex);

    QList<CharacterEntry> m_entries;
    /// Rows shown, as indices into m_entries.
    QList<int> m_rows;
    /// Last character confirmed with OK / double-click (CIP lastSelectedIndex identity).
    QString m_lastSelectedName;
};

}
