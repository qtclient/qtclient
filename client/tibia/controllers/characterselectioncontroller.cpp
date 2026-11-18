#include "characterselectioncontroller.h"
#include "gamewindow/smoothfiltering.h"
#include "game/serversave.h"
#include "game/worldtype.h"

#include "config/clientoptions.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"

#include "hashhelper.h"

#include <QColor>
#include <QDateTime>
#include <QTimeZone>

#include <algorithm>

namespace tibia::gamewindow {

namespace {

using O = config::EClientOption;
using Row = TCharacterInformationModelData;

config::TClientOptions* options() { return config::TClientOptions::instance(); }

QByteArray pinHash(const QString& name) { return shared::THashHelper().getSha256Hash(name.toLocal8Bit()); }

/// The world column: its name, and "experimental" for a preview world.
QString worldColumn(const authentication::TWorld& world)
{
    QStringList parts{ world.name };
    if (world.previewState != 0)
        parts << qtTrId("characterselection_experimental_world");
    return parts.join(QStringLiteral(", "));
}

QString worldRulesColumn(const authentication::TWorld& world) { return QStringList{ game::worldTypeName(world.pvpType) }.join(QStringLiteral(", ")); }

/// Daily reward states in the order the column sorts them.
int dailyRewardRank(const int state)
{
    static constexpr int Ranks[] = { 1, 0, 2, -1 };
    return uint(state) < 4 ? Ranks[state] : -1;
}

}

bool TCharacterSelectionController::clientSettingSmoothFiltering() const { return smoothFilteringEnabled(); }

TCharacterSelectionController::TCharacterSelectionController(QObject* parent)
    : cip::tibia::gamewindow::TCharacterSelectionController(parent)
{
    m_lastServerSaveCheck = uint(QDateTime::currentSecsSinceEpoch());
    m_recoverySetupComplete = true;
    m_collator.setNumericMode(true);
    m_collator.setCaseSensitivity(Qt::CaseInsensitive);
    m_serverSaveTimer.setInterval(1000);
    connect(&m_serverSaveTimer, &QTimer::timeout, this, &TCharacterSelectionController::checkServerSave);
    m_serverSaveTimer.start();
    connect(options(), &config::TClientOptions::characterSelectionControllerOptionsChanged, this,
            &TCharacterSelectionController::onOptionsChanged);
    onOptionsChanged();
}

QString TCharacterSelectionController::accountStatusText(const int accountState, const int premiumType, const int premiumDays)
{
    if (accountState == 2)
        return qtTrId("characterselection_status_suspended");
    if (accountState == 1)
        return qtTrId("characterselection_status_frozen");
    return premiumStatusText(premiumType, premiumDays);
}

QString TCharacterSelectionController::premiumStatusText(const int premiumType, const int premiumDays)
{
    if (premiumType == 2)
        return qtTrId("characterselection_status_free_premium");
    if (premiumType == 0)
        return qtTrId("characterselection_status_free");
    const uint16_t days = static_cast<uint16_t>(premiumDays);
    QString text = QStringLiteral("%1 (%2%4%3)").arg(qtTrId("characterselection_status_premium"));
    QString timeLeft;
    if (days < 14) {
        // The remaining days in red.
        text = text.arg(QStringLiteral("<font color=%1>").arg(QColor(0xf8, 0x60, 0x60).name()));
        text = text.arg(QStringLiteral("</font>"));
        timeLeft = days == 1 ? qtTrId("characterselection_status_premiumtime").arg(1)
                             : qtTrId("characterselection_status_premiumtime_plural").arg(days);
    } else {
        text = text.arg(QString()).arg(QString());
        timeLeft = qtTrId("characterselection_status_premiumtime_plural").arg(days);
    }
    return text.arg(timeLeft);
}

QString TCharacterSelectionController::accountPremiumStatus() const
{
    return accountStatusText(m_accountState, m_premiumType, m_premiumDays);
}

int TCharacterSelectionController::lastSelectedIndex() const
{
    for (int row = 0; row < m_characterList.size(); ++row) {
        if (static_cast<const Row*>(m_characterList[row])->characterName() == m_lastSelectedCharacterName)
            return row;
    }
    return 0;
}

uint TCharacterSelectionController::sortColumn() const { return uint(options()->intOption(O::characterSelectionSortColumn)); }

Qt::SortOrder TCharacterSelectionController::sortOrder() const
{
    return options()->boolOption(O::characterSelectionSortAscendingOrder) ? Qt::AscendingOrder : Qt::DescendingOrder;
}

bool TCharacterSelectionController::showGetPremiumButton() const
{
    if (m_premiumType == 0)
        return true;
    return m_premiumType == 1 && m_premiumDays < 14;
}

bool TCharacterSelectionController::isPremium() const { return m_premiumType != 0; }

bool TCharacterSelectionController::showHiddenCharacters() const { return options()->boolOption(O::characterSelectionShowHidden); }

bool TCharacterSelectionController::showOutfits() const { return options()->boolOption(O::characterSelectionShowOutfits); }

void TCharacterSelectionController::setLastSelectedCharacterName(const QString& name)
{
    if (m_lastSelectedCharacterName == name)
        return;
    m_lastSelectedCharacterName = name;
    Q_EMIT characterListChanged();
}

void TCharacterSelectionController::selectCharacter(const QString& name)
{
    int selected = 0;
    for (int row = 0; row < m_characterList.size(); ++row) {
        if (static_cast<const Row*>(m_characterList[row])->characterName().compare(name, Qt::CaseSensitive) == 0) {
            selected = row;
            break;
        }
    }
    onCharacterSelectionConfirmed({ selected });
}

void TCharacterSelectionController::clearTutorialFlag(const QString& name)
{
    if (const int index = characterIndex(name); index >= 0 && size_t(index) < m_characters.size())
        m_characters[size_t(index)].tutorial = false;
    for (TCharacterLoginData& data : m_loginData) {
        if (data.characterName.compare(name, Qt::CaseSensitive) == 0)
            data.tutorial = false;
    }
}

int TCharacterSelectionController::characterIndex(const QString& name) const
{
    if (name.isEmpty())
        return -1;
    for (size_t i = 0; i < m_characters.size(); ++i) {
        if (m_characters[i].name.compare(name, Qt::CaseSensitive) == 0)
            return int(i);
    }
    return 0;
}

void TCharacterSelectionController::setPremium(const int premiumType, const quint16 premiumDays)
{
    if (m_premiumDays == premiumDays && m_premiumType == premiumType)
        return;
    m_premiumType = premiumType;
    m_premiumDays = premiumDays;
    Q_EMIT accountPremiumStatusChanged();
}

void TCharacterSelectionController::setAccountState(const int accountState) { m_accountState = accountState; }

void TCharacterSelectionController::setRecoverySetupComplete(const bool complete)
{
    if (m_recoverySetupComplete == complete)
        return;
    m_recoverySetupComplete = complete;
    Q_EMIT recoverySetupCompleteChanged();
}

void TCharacterSelectionController::setCharacters(const authentication::TCharacterList& characters, const authentication::TWorldList& worlds)
{
    const auto sameCharacter = [](const authentication::TCharacter& a, const authentication::TCharacter& b) {
        return a.name == b.name && a.worldId == b.worldId && a.level == b.level && a.vocation == b.vocation && a.isMale == b.isMale
               && a.isHidden == b.isHidden && a.isMainCharacter == b.isMainCharacter && a.tutorial == b.tutorial
               && a.dailyRewardState == b.dailyRewardState && a.outfitId == b.outfitId && a.headColor == b.headColor
               && a.torsoColor == b.torsoColor && a.legsColor == b.legsColor && a.detailColor == b.detailColor
               && a.firstAddOn == b.firstAddOn && a.secondAddOn == b.secondAddOn;
    };
    const auto sameWorld = [](const authentication::TWorld& a, const authentication::TWorld& b) {
        return a.id == b.id && a.name == b.name && a.externalAddressProtected == b.externalAddressProtected
               && a.externalAddressUnprotected == b.externalAddressUnprotected && a.externalPortUnprotected == b.externalPortUnprotected
               && a.externalPortProtected == b.externalPortProtected && a.location == b.location && a.previewState == b.previewState
               && a.antiCheatProtection == b.antiCheatProtection && a.pvpType == b.pvpType;
    };
    const bool unchanged =
        std::equal(m_characters.begin(), m_characters.end(), characters.begin(), characters.end(), sameCharacter)
        && std::equal(m_worlds.begin(), m_worlds.end(), worlds.begin(), worlds.end(),
                      [&](const auto& a, const auto& b) { return a.first == b.first && sameWorld(a.second, b.second); });
    if (unchanged)
        return;
    m_characters = characters;
    m_worlds = worlds;
    updateCharacterList();
}

void TCharacterSelectionController::updateCharacterList()
{
    const QList<QObject*> oldRows = std::exchange(m_characterList, {});
    const std::set<QByteArray>& pinned = options()->pinnedCharacters();
    const bool showHidden = options()->boolOption(O::characterSelectionShowHidden);
    for (const authentication::TCharacter& character : m_characters) {
        if (character.isHidden && !showHidden)
            continue;
        const auto world = m_worlds.find(character.worldId);
        const authentication::TWorld& info = world != m_worlds.end() ? world->second : authentication::TWorld{};
        m_characterList.push_back(new Row(character, QString::number(character.level), character.vocation, worldColumn(info),
                                          worldRulesColumn(info), pinned.count(pinHash(character.name)) != 0));
    }
    sortCharacterList();
    Q_EMIT characterListChanged();
    for (QObject* row : oldRows)
        row->deleteLater();
}

void TCharacterSelectionController::sortCharacterList()
{
    const uint column = sortColumn();
    const bool descending = sortOrder() == Qt::DescendingOrder;
    std::sort(m_characterList.begin(), m_characterList.end(), [&](const QObject* left, const QObject* right) {
        const auto* a = static_cast<const Row*>(left);
        const auto* b = static_cast<const Row*>(right);
        const int byName = m_collator.compare(a->characterName(), b->characterName());
        int result = 0;
        switch (column) {
        case 0:
            result = a->outfit()->property("id").toInt() - b->outfit()->property("id").toInt();
            break;
        case 1:
            result = byName;
            break;
        case 2:
            result = dailyRewardRank(int(a->dailyRewardState())) - dailyRewardRank(int(b->dailyRewardState()));
            break;
        case 3:
            result = m_collator.compare(a->level(), b->level());
            break;
        case 4:
            result = m_collator.compare(a->vocation(), b->vocation());
            break;
        case 5:
            result = m_collator.compare(a->world(), b->world());
            break;
        default:
            break;
        }
        if (descending)
            result = -result;
        if (result == 0)
            result = byName;
        // Pinned characters come first in either order.
        if (const int pinnedFirst = int(b->isPinned()) - int(a->isPinned()); pinnedFirst != 0)
            result = pinnedFirst;
        return result < 0;
    });
}

void TCharacterSelectionController::onCancelClicked()
{
    // The host closes the dialog.
    Q_EMIT abort();
}

void TCharacterSelectionController::onCharacterSelectionConfirmed(const QList<int>& SelectedCharacters)
{
    m_loginData.clear();
    for (const int row : SelectedCharacters) {
        QString name;
        if (row >= 0 && row < m_characterList.size())
            name = static_cast<const Row*>(m_characterList[row])->characterName();
        if (name.isEmpty())
            continue;
        const size_t index = size_t(characterIndex(name));
        if (index >= m_characters.size())
            continue;
        const authentication::TCharacter& character = m_characters[index];
        const authentication::TWorld& world = m_worlds[character.worldId];
        TCharacterLoginData data;
        data.characterName = character.name;
        data.worldAddressProtected = world.externalAddressProtected;
        data.worldAddressUnprotected = world.externalAddressUnprotected;
        data.worldPortUnprotected = world.externalPortUnprotected;
        data.worldPortProtected = world.externalPortProtected;
        data.worldName = world.name;
        data.antiCheatProtection = world.antiCheatProtection;
        data.tutorial = character.tutorial;
        data.isMale = character.isMale;
        m_loginData.push_back(data);
    }
    if (!m_loginData.empty())
        Q_EMIT requestCharacterLogin(m_loginData.front());
}

void TCharacterSelectionController::onOptionsChanged()
{
    Q_EMIT showOutfitsChanged();
    updateCharacterList();
    Q_EMIT sortingChanged();
}

void TCharacterSelectionController::setShowHiddenCharacters(const bool Show) { options()->setBoolOption(O::characterSelectionShowHidden, Show); }

void TCharacterSelectionController::setShowOutfits(const bool Show) { options()->setBoolOption(O::characterSelectionShowOutfits, Show); }

void TCharacterSelectionController::setSortOrder(const uint Column, const Qt::SortOrder SordOrder)
{
    options()->setIntOption(O::characterSelectionSortColumn, int(Column));
    options()->setBoolOption(O::characterSelectionSortAscendingOrder, SordOrder == Qt::AscendingOrder);
}

void TCharacterSelectionController::onPinnedChanged(const QString& CharName, const bool IsPinned)
{
    std::set<QByteArray> pinned = options()->pinnedCharacters();
    for (const authentication::TCharacter& character : m_characters) {
        if (character.name.compare(CharName, Qt::CaseSensitive) != 0)
            continue;
        if (IsPinned)
            pinned.insert(pinHash(character.name));
        else
            pinned.erase(pinHash(character.name));
    }
    options()->setPinnedCharacters(pinned);
}

int TCharacterSelectionController::getCharacterIndexForSearchString(const QString& SearchString)
{
    for (int row = 0; row < m_characterList.size(); ++row) {
        if (static_cast<const Row*>(m_characterList[row])->characterName().indexOf(SearchString, 0, Qt::CaseInsensitive) != -1)
            return row;
    }
    return -1;
}

void TCharacterSelectionController::createCharacterPressed()
{
    Q_EMIT publishGameAction(std::make_shared<input::gameactions::TGameActionShowCreateCharacterDialog>(m_characters.empty()));
}

void TCharacterSelectionController::manageAccountPressed()
{
    Q_EMIT publishGameAction(input::makeGameAction(input::EGameAction::OPEN_URL_ACCESS_ACCOUNT));
}

void TCharacterSelectionController::onGetPremiumClicked()
{
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionOpenGetPremiumUrl>());
}

void TCharacterSelectionController::onMorePremiumFeaturesClicked()
{
    Q_EMIT publishGameAction(input::makeGameAction(input::EGameAction::OPEN_URL_PREMIUM_FEATURES));
}

void TCharacterSelectionController::checkServerSave()
{
    const uint now = uint(QDateTime::currentSecsSinceEpoch());
    if (m_lastServerSaveCheck < uint(game::lastServerSave().toSecsSinceEpoch())) {
        for (authentication::TCharacter& character : m_characters) {
            if (character.dailyRewardState == 0)
                character.dailyRewardState = 1;
        }
        updateCharacterList();
    }
    m_lastServerSaveCheck = now;
}

}
