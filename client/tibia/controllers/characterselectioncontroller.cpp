#include "characterselectioncontroller.h"

#include "input/gameaction.h"
#include "input/gameactionhandlers.h"

#include <algorithm>

namespace tibia::gamewindow {

namespace {

class Outfit final : public cip::tibia::gamewindow::TOutfitQMLInformation
{
public:
    Outfit(const CharacterEntry& c, QObject* parent) : TOutfitQMLInformation(parent)
    {
        m_id = c.outfitId;
        m_headColor = c.headColor;
        m_torsoColor = c.torsoColor;
        m_legsColor = c.legsColor;
        m_detailColor = c.detailColor;
        m_firstAddOn = c.firstAddOn;
        m_secondAddOn = c.secondAddOn;
    }
};

class Row final : public cip::tibia::gamewindow::TCharacterInformationModelData
{
public:
    Row(const CharacterEntry& c, QObject* parent) : TCharacterInformationModelData(parent)
    {
        m_characterName = c.name;
        m_level = QString::number(c.level);
        m_vocation = c.vocation;
        m_world = c.world;
        // CIP world column second line is `(worldRules)`. Surface Online there so
        // CharacterSelection shows an online label without a full QML fork.
        if (c.isOnline) {
            m_worldRules = c.worldRules.isEmpty()
                ? QStringLiteral("Online")
                : QStringLiteral("%1 · Online").arg(c.worldRules);
        } else {
            m_worldRules = c.worldRules;
        }
        m_outfit = new Outfit(c, this);
        m_dailyRewardState = cip::tibia::qmlcomponents::enums::EDailyRewardState::DailyRewardStateNotAvailable;
        m_isMainCharacter = c.isMainCharacter;
        m_isPinned = c.isPinned;
        m_isHidden = c.isHidden;
    }
};

// Column order of CharacterSelection.qml: outfit, name, daily reward, level, vocation, world.
bool lessThan(const CharacterEntry& a, const CharacterEntry& b, const uint column)
{
    switch (column) {
        case 3: return a.level < b.level;
        case 4: return a.vocation.localeAwareCompare(b.vocation) < 0;
        case 5: return a.world.localeAwareCompare(b.world) < 0;
        default: return a.name.localeAwareCompare(b.name) < 0;
    }
}

}

TCharacterSelectionController::TCharacterSelectionController(QObject* parent) : cip::tibia::gamewindow::TCharacterSelectionController(parent)
{
    m_sortColumn = 1;
    m_sortOrder = Qt::AscendingOrder;
    m_showOutfits = true;
    m_clientSettingSmoothFiltering = true;
    m_recoverySetupComplete = true;
}

void TCharacterSelectionController::setAccount(const QString& premiumStatus, const bool isPremium,
                                               const QList<CharacterEntry>& characters, const int preferredEntryIndex)
{
    m_accountPremiumStatus = premiumStatus;
    m_isPremium = isPremium;
    m_showGetPremiumButton = !isPremium;
    m_entries = characters;
    rebuild();
    resolveLastSelectedIndex(preferredEntryIndex);
    Q_EMIT accountPremiumStatusChanged();
    Q_EMIT characterListChanged();
}

void TCharacterSelectionController::rebuild()
{
    m_rows.clear();
    for (int i = 0; i < m_entries.size(); ++i)
        if (m_showHiddenCharacters || !m_entries[i].isHidden)
            m_rows.push_back(i);

    const uint column = m_sortColumn;
    const bool descending = m_sortOrder == Qt::DescendingOrder;
    std::stable_sort(m_rows.begin(), m_rows.end(), [&](const int a, const int b) {
        const auto& ea = m_entries[a];
        const auto& eb = m_entries[b];
        if (ea.isPinned != eb.isPinned)
            return ea.isPinned;
        return descending ? lessThan(eb, ea, column) : lessThan(ea, eb, column);
    });

    for (QObject* row : std::as_const(m_characterList))
        row->deleteLater();
    m_characterList.clear();
    for (const int i : std::as_const(m_rows))
        m_characterList.push_back(new Row(m_entries[i], this));
}

void TCharacterSelectionController::resolveLastSelectedIndex(const int preferredEntryIndex)
{
    // CIP: keep the last confirmed character when still on the account; else the
    // preferred entry (login: ismaincharacter); else the first visible row.
    auto rowOfName = [this](const QString& name) -> int {
        if (name.isEmpty())
            return -1;
        for (int row = 0; row < m_rows.size(); ++row) {
            if (m_entries[m_rows[row]].name.compare(name, Qt::CaseInsensitive) == 0)
                return row;
        }
        return -1;
    };
    int row = rowOfName(m_lastSelectedName);
    if (row < 0 && preferredEntryIndex >= 0 && preferredEntryIndex < m_entries.size())
        row = rowOfName(m_entries[preferredEntryIndex].name);
    if (row < 0)
        row = 0;
    m_lastSelectedIndex = row;
    if (row >= 0 && row < m_rows.size())
        m_lastSelectedName = m_entries[m_rows[row]].name;
}

void TCharacterSelectionController::onCancelClicked()
{
    Q_EMIT cancelled();
    Q_EMIT dialogClosed();
}

void TCharacterSelectionController::createCharacterPressed()
{
    Q_EMIT createCharacterRequested(m_entries.isEmpty());
}

void TCharacterSelectionController::manageAccountPressed()
{
    input::publish(input::makeAction(input::EGameAction::OPEN_URL_ACCESS_ACCOUNT));
}

void TCharacterSelectionController::onCharacterSelectionConfirmed(const QList<int>& SelectedCharacters)
{
    if (SelectedCharacters.isEmpty())
        return;
    const int row = SelectedCharacters.first();
    if (row < 0 || row >= m_rows.size())
        return;
    m_lastSelectedIndex = row;
    m_lastSelectedName = m_entries[m_rows[row]].name;
    Q_EMIT characterChosen(m_entries[m_rows[row]]);
}

void TCharacterSelectionController::setShowHiddenCharacters(const bool Show)
{
    if (m_showHiddenCharacters == Show)
        return;
    m_showHiddenCharacters = Show;
    rebuild();
    resolveLastSelectedIndex(-1);
    Q_EMIT showHiddenCharactersChanged();
    Q_EMIT characterListChanged();
}

void TCharacterSelectionController::setShowOutfits(const bool Show)
{
    if (m_showOutfits == Show)
        return;
    m_showOutfits = Show;
    Q_EMIT showOutfitsChanged();
}

void TCharacterSelectionController::setSortOrder(const uint Column, const Qt::SortOrder SordOrder)
{
    m_sortColumn = Column;
    m_sortOrder = SordOrder;
    rebuild();
    resolveLastSelectedIndex(-1);
    Q_EMIT sortingChanged();
    Q_EMIT characterListChanged();
}

void TCharacterSelectionController::onPinnedChanged(const QString& CharName, const bool IsPinned)
{
    for (auto& e : m_entries)
        if (e.name == CharName)
            e.isPinned = IsPinned;
    rebuild();
    Q_EMIT characterListChanged();
}

int TCharacterSelectionController::getCharacterIndexForSearchString(const QString& SearchString)
{
    for (int row = 0; row < m_rows.size(); ++row)
        if (m_entries[m_rows[row]].name.startsWith(SearchString, Qt::CaseInsensitive))
            return row;
    return -1;
}

}
