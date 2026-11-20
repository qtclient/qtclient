#include "highscoresdialogcontroller.h"

#include "gamewindow/waitdialogcontroller.h"

#include "protocol/protocolciptypes.h"

#include <tibia/game/gamesession.h>
#include <tibia/game/tgamenotify.h>

#include <protocol.pb.h>

#include <QDateTime>
#include <QLocale>

#include <algorithm>
#include <cmath>

namespace tibia::highscores {

THighscoresEntriesModel::THighscoresEntriesModel(std::vector<THighscoreEntry> entries, QObject* parent)
    : cip::tibia::highscores::THighscoresEntriesModel(parent)
    , m_entries(std::move(entries))
{}

int THighscoresEntriesModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant THighscoresEntriesModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= static_cast<int>(m_entries.size()))
        return {};
    const THighscoreEntry& entry = m_entries[static_cast<size_t>(index.row())];
    switch (role) {
        case RankRole: return entry.rank;
        case CharacterRole: return entry.character;
        case VocationRole: return entry.vocation;
        case ScoreRole: return QLocale(QLocale::English).toString(static_cast<qlonglong>(entry.score));
        case WorldRole: return entry.world;
        case LevelRole: return static_cast<uint>(entry.level);
        case LoyaltyTitleRole: return entry.loyaltyTitle;
        case IsOwnCharacterRole: return entry.isOwnCharacter;
        default: return {};
    }
}

QHash<int, QByteArray> THighscoresEntriesModel::roleNames() const
{
    QHash<int, QByteArray> names = QAbstractListModel::roleNames();
    names.insert({ { CharacterRole, "character" }, { RankRole, "rank" }, { ScoreRole, "score" }, { VocationRole, "vocation" },
                   { WorldRole, "world" }, { LevelRole, "level" }, { LoyaltyTitleRole, "loyaltyTitle" }, { IsOwnCharacterRole, "isOwnCharacter" } });
    return names;
}

THighscoresData::THighscoresData(std::vector<THighscoreEntry> entries, QObject* parent)
    : cip::tibia::highscores::THighscoresData(parent)
    , m_model(std::move(entries))
{
    m_highscoreEntryModel = &m_model;
}

}

namespace tibia::gamewindow {

namespace {

constexpr int EntriesPerPage = 20;

}

void THighscoresWorldTypeModel::setEntries(QList<Entry> entries)
{
    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
}

int THighscoresWorldTypeModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

QVariant THighscoresWorldTypeModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= m_entries.size())
        return {};
    const Entry& entry = m_entries[index.row()];
    switch (role) {
        case Qt::DisplayRole:
        case NameRole: return entry.name;
        case CheckedRole: return entry.checked;
        case WorldTypeRole: return entry.worldType;
        default: return {};
    }
}

QHash<int, QByteArray> THighscoresWorldTypeModel::roleNames() const
{
    return { { NameRole, "name" }, { CheckedRole, "checked" }, { WorldTypeRole, "worldType" } };
}

THighscoresDialogController::THighscoresDialogController(QObject* parent)
    : TDialogController(QStringLiteral("HighscorePage.qml"), parent)
{
    m_showLoadingComponent = true;
    m_dataAvailable = true;
    m_indexOfSelectedWorld = -1;
    m_indexOfSelectedBattlEyeType = -1;
    m_indexOfSelectedVocation = -1;
    m_indexOfSelectedCategory = -1;
    m_worldTypeModel = &m_worldTypes;
    connectToGameSession(nullptr);
}

THighscoresDialogController::~THighscoresDialogController()
{
    delete m_highscoreData;
}

void THighscoresDialogController::connectToGameSession(const std::shared_ptr<game::IGameSession>&)
{
    closeDialog();
    if (m_storage)
        disconnect(m_storage, nullptr, this, nullptr);
    m_storage = highscores::THighscoresStorage::liveInstance();
    if (m_storage) {
        connect(m_storage, &highscores::THighscoresStorage::highScoresError, this, &THighscoresDialogController::onHighScoresError);
        connect(m_storage, &highscores::THighscoresStorage::highScoresUpdated, this, &THighscoresDialogController::onHighScoresUpdated);
    }
    updateWorldTypes();
}

QList<THighscoresDialogController::BattlEyeType> THighscoresDialogController::battlEyeTypes()
{
    return { { -1, qtTrId("highscores_battleye_any") },
             { 0, qtTrId("highscores_battleye_unprotected") },
             { 1, qtTrId("highscores_battleye_protected") },
             { 2, qtTrId("initially_protected") } };
}

void THighscoresDialogController::setShowLoadingComponent(const bool show)
{
    if (m_showLoadingComponent == show)
        return;
    m_showLoadingComponent = show;
    Q_EMIT showLoadingComponentChanged();
}

void THighscoresDialogController::setDataAvailable(const bool available)
{
    if (m_dataAvailable == available)
        return;
    m_dataAvailable = available;
    Q_EMIT dataAvailableChanged();
}

/// "All worlds" first.
QStringList THighscoresDialogController::availableWorlds() const
{
    if (!m_storage)
        return {};
    QStringList worlds{ qtTrId("highscores_worlds_all") };
    worlds.append(m_storage->worlds());
    return worlds;
}

QStringList THighscoresDialogController::availableBattlEyeTypes() const
{
    QStringList names;
    for (const BattlEyeType& type : battlEyeTypes())
        names.append(type.name);
    return names;
}

QStringList THighscoresDialogController::availableVocations() const
{
    QStringList names;
    if (m_storage) {
        for (const auto& [id, name] : m_storage->vocations())
            names.append(name);
    }
    return names;
}

QStringList THighscoresDialogController::availableCategories() const
{
    QStringList names;
    if (m_storage) {
        for (const auto& [id, name] : m_storage->categories())
            names.append(name);
    }
    return names;
}

int THighscoresDialogController::selectedPage() const
{
    return m_storage ? m_storage->page() : 0;
}

int THighscoresDialogController::totalPages() const
{
    return m_storage ? m_storage->totalPages() : 0;
}

int THighscoresDialogController::selectedIndex() const
{
    return m_storage ? m_storage->selectedIndex() : 0;
}

bool THighscoresDialogController::showLoyaltyTitle() const
{
    return m_storage && m_storage->showLoyaltyTitle();
}

/// Unknown types show as points.
cip::tibia::gamewindow::THighscoresDialogController::EValueType THighscoresDialogController::valueType() const
{
    const int type = m_storage ? m_storage->valueType() : Points;
    return type >= 0 && type < 3 ? static_cast<EValueType>(type) : Points;
}

QString THighscoresDialogController::dataActualisationText() const
{
    if (!m_storage)
        return {};
    const qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - static_cast<qint64>(m_storage->lastUpdate()) * 1000;
    return qtTrId("highscores_refreshtext").arg(formatRemainingTime(elapsed));
}

void THighscoresDialogController::setIndexOfSelectedWorld(const int v)
{
    if (m_indexOfSelectedWorld == v)
        return;
    m_indexOfSelectedWorld = v;
    Q_EMIT indexOfSelectedWorldChanged();
}

void THighscoresDialogController::setIndexOfSelectedBattlEyeType(const int v)
{
    if (m_indexOfSelectedBattlEyeType == v)
        return;
    m_indexOfSelectedBattlEyeType = v;
    Q_EMIT indexOfSelectedBattlEyeTypeChanged();
}

void THighscoresDialogController::setIndexOfSelectedVocation(const int v)
{
    if (m_indexOfSelectedVocation == v)
        return;
    m_indexOfSelectedVocation = v;
    Q_EMIT indexOfSelectedVocationChanged();
}

void THighscoresDialogController::setIndexOfSelectedCategory(const int v)
{
    if (m_indexOfSelectedCategory == v)
        return;
    m_indexOfSelectedCategory = v;
    Q_EMIT indexOfSelectedCategoryChanged();
}

/// The world behind the selected index; index 0 is "all worlds" (empty).
QString THighscoresDialogController::selectedWorldName() const
{
    const int index = m_indexOfSelectedWorld;
    if (m_storage && index > 0 && index <= m_storage->worlds().size())
        return m_storage->worlds()[index - 1];
    return QString();
}

int THighscoresDialogController::selectedBattlEyeValue() const
{
    if (!m_storage)
        return -1;
    const QList<BattlEyeType> types = battlEyeTypes();
    const int index = m_indexOfSelectedBattlEyeType;
    return index >= 0 && index < types.size() ? types[index].value : -1;
}

int THighscoresDialogController::selectedVocationId() const
{
    const int index = m_indexOfSelectedVocation;
    if (m_storage && index >= 0 && index < static_cast<int>(m_storage->vocations().size()))
        return m_storage->vocations()[static_cast<size_t>(index)].first;
    return -1;
}

int THighscoresDialogController::selectedCategoryId() const
{
    const int index = m_indexOfSelectedCategory;
    if (m_storage && index >= 0 && index < static_cast<int>(m_storage->categories().size()))
        return m_storage->categories()[static_cast<size_t>(index)].first;
    return 0;
}

void THighscoresDialogController::sendHighscoresRequest(const int action, const int page, const int category, const QString& world,
                                                        const int vocation, const std::vector<int>& worldTypes, const int battlEye)
{
    // Host: the session refuses game actions while it cannot perform them.
    if (!tibia::game::canPerformGameAction())
        return;
    using namespace ::tibia::protobuf::protocol;
    auto message = tibia::protocol::newGameclientMessage(177);
    auto* m = message->MutableExtension(GameclientMessageHighscores::gameclient_message_highscores);
    m->set_entries_per_page(EntriesPerPage);
    m->set_action(static_cast<uint32_t>(action));
    m->set_page(static_cast<uint32_t>(page));
    m->set_world(world.toStdString());
    for (const int type : worldTypes) {
        if (type >= 0 && type < 5)
            m->add_world_types(static_cast<uint32_t>(type));
    }
    m->set_vocation(static_cast<uint32_t>(vocation));
    m->set_battleye(static_cast<uint32_t>(battlEye));
    m->set_category(static_cast<uint32_t>(category));
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void THighscoresDialogController::requestPage(const int page)
{
    sendHighscoresRequest(0, page, selectedCategoryId(), selectedWorldName(), selectedVocationId(), m_selectedWorldTypes, selectedBattlEyeValue());
}

void THighscoresDialogController::openDialog()
{
    showDialog();
    sendHighscoresRequest(0, 1, 0, QStringLiteral("OWN"), -1, {}, -1);
}

void THighscoresDialogController::onHighScoresError()
{
    if (!m_storage || m_storage->state() != 1)
        return;
    setDataAvailable(false);
    setShowLoadingComponent(false);
}

void THighscoresDialogController::onHighScoresUpdated()
{
    if (m_storage) {
        auto* old = m_highscoreData;
        m_highscoreData = new highscores::THighscoresData(m_storage->entries());
        delete old;
    }
    setDataAvailable(true);
    setShowLoadingComponent(false);
    updateIndexOfSelectedWorld();
    updateIndexOfSelectedBattlEyeType();
    updateIndexOfSelectedVocation();
    updateIndexOfSelectedCategory();
    updateWorldTypes();
    Q_EMIT dataChanged();
}

/// The answer's world, 0 (all worlds) when it names none or an unknown one.
void THighscoresDialogController::updateIndexOfSelectedWorld()
{
    int worldIndex = 0;
    if (m_storage && !m_storage->selectedWorld().isEmpty()) {
        const int found = m_storage->worlds().indexOf(m_storage->selectedWorld());
        if (found >= 0)
            worldIndex = found + 1;
    }
    setIndexOfSelectedWorld(worldIndex);
}

/// The answer's BattlEye type in battlEyeTypes(), 0 (any) when unknown.
void THighscoresDialogController::updateIndexOfSelectedBattlEyeType()
{
    int battlEyeIndex = 0;
    if (m_storage) {
        const QList<BattlEyeType> types = battlEyeTypes();
        for (int i = 0; i < types.size(); ++i) {
            if (types[i].value == m_storage->battlEyeType()) {
                battlEyeIndex = i;
                break;
            }
        }
    }
    setIndexOfSelectedBattlEyeType(battlEyeIndex);
}

/// The answer's vocation in the vocation list, 0 when unknown.
void THighscoresDialogController::updateIndexOfSelectedVocation()
{
    int vocationIndex = 0;
    if (m_storage) {
        const auto& vocations = m_storage->vocations();
        for (size_t i = 0; i < vocations.size(); ++i) {
            if (vocations[i].first == m_storage->selectedVocation()) {
                vocationIndex = static_cast<int>(i);
                break;
            }
        }
    }
    setIndexOfSelectedVocation(vocationIndex);
}

/// The answer's category, 0 when unknown.
void THighscoresDialogController::updateIndexOfSelectedCategory()
{
    int categoryIndex = 0;
    if (m_storage) {
        const auto& categories = m_storage->categories();
        for (size_t i = 0; i < categories.size(); ++i) {
            if (categories[i].first == m_storage->selectedCategory()) {
                categoryIndex = static_cast<int>(i);
                break;
            }
        }
    }
    setIndexOfSelectedCategory(categoryIndex);
}

void THighscoresDialogController::updateWorldTypes()
{
    QList<THighscoresWorldTypeModel::Entry> entries;
    if (m_storage) {
        m_selectedWorldTypes.clear();
        static constexpr const char* Names[] = { "highscores_world_pvp_open", "highscores_world_pvp_optional", "highscores_world_pvp_hardcore",
                                                 "highscores_world_pvp_retro_open", "highscores_world_pvp_retro_hardcore" };
        const std::vector<int>& answered = m_storage->selectedWorldTypes();
        for (int type = 0; type < 5; ++type) {
            const bool checked = std::ranges::find(answered, type) != answered.end();
            if (checked)
                m_selectedWorldTypes.push_back(type);
            entries.append({ .worldType = type, .name = qtTrId(Names[type]), .checked = checked });
        }
    }
    m_worldTypes.setEntries(std::move(entries));
}

void THighscoresDialogController::clear()
{
    dialog()->close();
    delete m_highscoreData;
    m_highscoreData = nullptr;
    Q_EMIT dataChanged();
}

/// Resets the filters and shows the dialog loading.
void THighscoresDialogController::showDialog()
{
    closeDialog();
    setShowLoadingComponent(true);
    setDataAvailable(true);
    dialog()->show();
}

/// Forgets the selected filters and closes the dialog.
void THighscoresDialogController::closeDialog()
{
    m_indexOfSelectedWorld = -1;
    m_indexOfSelectedBattlEyeType = -1;
    m_indexOfSelectedVocation = -1;
    m_indexOfSelectedCategory = -1;
    m_selectedWorldTypes.clear();
    setShowLoadingComponent(true);
    setDataAvailable(true);
    dialog()->close();
}

void THighscoresDialogController::onClickedFirstPage()
{
    setShowLoadingComponent(true);
    if (m_storage)
        requestPage(1);
}

/// The loading view shows even when there is no previous page.
void THighscoresDialogController::onClickedPreviousPage()
{
    setShowLoadingComponent(true);
    if (m_storage && m_storage->page() > 1)
        requestPage(m_storage->page() - 1);
}

void THighscoresDialogController::onClickedNextPage()
{
    setShowLoadingComponent(true);
    if (m_storage && m_storage->page() >= 0 && m_storage->page() < m_storage->totalPages())
        requestPage(m_storage->page() + 1);
}

void THighscoresDialogController::onClickedLastPage()
{
    setShowLoadingComponent(true);
    if (m_storage)
        requestPage(m_storage->totalPages());
}

/// The own rank in the selected category, vocation and world.
void THighscoresDialogController::onClickedShowMyPlaceButton()
{
    setShowLoadingComponent(true);
    if (!m_storage)
        return;
    sendHighscoresRequest(1, m_storage->page(), selectedCategoryId(), selectedWorldName(), selectedVocationId(), {}, -1);
}

void THighscoresDialogController::applyFilters()
{
    onClickedFirstPage();
}

void THighscoresDialogController::requestClose()
{
    closeDialog();
}

/// The check boxes keep their own state; only the request list changes.
void THighscoresDialogController::toggleSelectedWorldType(const int RawWorldType)
{
    const auto it = std::ranges::find(m_selectedWorldTypes, RawWorldType);
    if (it != m_selectedWorldTypes.end())
        m_selectedWorldTypes.erase(it);
    else
        m_selectedWorldTypes.push_back(RawWorldType);
}

}
