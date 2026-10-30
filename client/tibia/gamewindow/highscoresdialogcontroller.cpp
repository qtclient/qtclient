#include "highscoresdialogcontroller.h"

#include <tibia/world/game.h>

#include <QDateTime>
#include <QLocale>

#include <cmath>

namespace tibia::highscores {

namespace {

/// CIP 0x10102b8e0: client vocation ids are 1..5, promoted ones 11..15.
QString vocationName(const uint8_t vocation)
{
    static constexpr const char* Base[] = { "knight", "paladin", "sorcerer", "druid", "monk" };
    static constexpr const char* Promoted[] = { "knight_promoted", "paladin_promoted", "sorcerer_promoted", "druid_promoted", "monk_promoted" };
    const bool promoted = vocation >= 10;
    const int base = promoted ? vocation - 10 : vocation;
    if (base < 1 || base > 5)
        return qtTrId("none");
    return qtTrId(promoted ? Promoted[base - 1] : Base[base - 1]);
}

}

THighscoresEntriesModel::THighscoresEntriesModel(QList<THighscoreEntry> entries, QObject* parent)
    : cip::tibia::highscores::THighscoresEntriesModel(parent)
    , m_entries(std::move(entries))
{}

int THighscoresEntriesModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

QVariant THighscoresEntriesModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= m_entries.size())
        return {};
    const THighscoreEntry& entry = m_entries[index.row()];
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

THighscoresData::THighscoresData(QList<THighscoreEntry> entries, QObject* parent)
    : cip::tibia::highscores::THighscoresData(parent)
    , m_model(std::move(entries))
{
    m_highscoreEntryModel = &m_model;
}

}

namespace tibia::gamewindow {

namespace {

constexpr int EntriesPerPage = 20;

QString qstr(const std::string& text)
{
    return QString::fromStdString(text);
}

/// CIP 0x10095f760.
QString formatElapsed(const qint64 milliseconds)
{
    const uint seconds = static_cast<uint>(std::llround(static_cast<double>(milliseconds) / 1000.0));
    if (seconds < 61)
        return qtTrId(seconds == 1 ? "time_second" : "time_seconds_plural").arg(seconds);
    return qtTrId("time_minutes").arg(std::lround(seconds / 60.0));
}

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
    updateWorldTypes();
}

THighscoresDialogController::~THighscoresDialogController()
{
    delete m_highscoreData;
}

/// CIP 0x100e4d000.
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

/// CIP 0x100d85eb0: "all worlds" first.
QStringList THighscoresDialogController::availableWorlds() const
{
    QStringList worlds{ qtTrId("highscores_worlds_all") };
    for (const std::string& world : m_answer.worlds)
        worlds.append(qstr(world));
    return worlds;
}

/// CIP 0x100d86100.
QStringList THighscoresDialogController::availableBattlEyeTypes() const
{
    QStringList names;
    for (const BattlEyeType& type : battlEyeTypes())
        names.append(type.name);
    return names;
}

/// CIP 0x100d862a0.
QStringList THighscoresDialogController::availableVocations() const
{
    QStringList names{ qstr(m_answer.allVocationsName) };
    for (const auto& [id, name] : m_answer.vocations)
        names.append(qstr(name));
    return names;
}

/// CIP 0x100d863b0.
QStringList THighscoresDialogController::availableCategories() const
{
    QStringList names;
    for (const auto& [id, name] : m_answer.categories)
        names.append(qstr(name));
    return names;
}

int THighscoresDialogController::selectedIndex() const
{
    return m_answer.selectedIndex == 0xFF ? -1 : m_answer.selectedIndex;
}

/// CIP 0x100d866e0: unknown types show as points.
cip::tibia::gamewindow::THighscoresDialogController::EValueType THighscoresDialogController::valueType() const
{
    return m_answer.valueType < 3 ? static_cast<EValueType>(m_answer.valueType) : Points;
}

/// CIP 0x100d86770.
QString THighscoresDialogController::dataActualisationText() const
{
    const qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - static_cast<qint64>(m_answer.timestamp) * 1000;
    return qtTrId("highscores_refreshtext").arg(formatElapsed(elapsed));
}

/// CIP 0x100d84640 (and 0x100d848a0, 0x100d849f0, 0x100d84b40).
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

QString THighscoresDialogController::selectedWorldName() const
{
    const int index = m_indexOfSelectedWorld;
    if (index > 0 && index <= static_cast<int>(m_answer.worlds.size()))
        return qstr(m_answer.worlds[index - 1]);
    return {};
}

int THighscoresDialogController::selectedBattlEyeValue() const
{
    const QList<BattlEyeType> types = battlEyeTypes();
    const int index = m_indexOfSelectedBattlEyeType;
    return index >= 0 && index < types.size() ? types[index].value : -1;
}

int THighscoresDialogController::selectedVocationId() const
{
    const int index = m_indexOfSelectedVocation;
    if (index > 0 && index <= static_cast<int>(m_answer.vocations.size()))
        return static_cast<int>(std::get<0>(m_answer.vocations[index - 1]));
    return -1;
}

int THighscoresDialogController::selectedCategoryId() const
{
    const int index = m_indexOfSelectedCategory;
    if (index >= 0 && index < static_cast<int>(m_answer.categories.size()))
        return std::get<0>(m_answer.categories[index]);
    return 0;
}

void THighscoresDialogController::requestPage(const int page)
{
    setShowLoadingComponent(true);
    g_game.requestHighscore(0, static_cast<uint8_t>(selectedCategoryId()), static_cast<uint32_t>(selectedVocationId()),
                            selectedWorldName().toStdString(), 0, static_cast<uint8_t>(selectedBattlEyeValue()),
                            static_cast<uint16_t>(page), EntriesPerPage);
}

void THighscoresDialogController::requestOwnRank()
{
    setShowLoadingComponent(true);
    g_game.requestHighscore(1, static_cast<uint8_t>(selectedCategoryId()), static_cast<uint32_t>(selectedVocationId()),
                            selectedWorldName().toStdString(), 0, 0xFF, static_cast<uint16_t>(m_answer.page), EntriesPerPage);
}

/// CIP 0x100d84b60: the five PvP types (0x100e4ca30), checked when selected.
void THighscoresDialogController::updateWorldTypes()
{
    static constexpr const char* Names[] = { "highscores_world_pvp_open", "highscores_world_pvp_optional", "highscores_world_pvp_hardcore",
                                             "highscores_world_pvp_retro_open", "highscores_world_pvp_retro_hardcore" };
    QList<THighscoresWorldTypeModel::Entry> entries;
    for (int type = 0; type < 5; ++type)
        entries.append({ .worldType = type, .name = qtTrId(Names[type]), .checked = m_selectedWorldTypes.contains(type) });
    m_worldTypes.setEntries(std::move(entries));
}

void THighscoresDialogController::openDialog()
{
    showDialog();
    g_game.requestHighscore(0, 0, 0xFFFFFFFF, "OWN", 0, 0xFF, 1, EntriesPerPage);
}

void THighscoresDialogController::setAnswer(Answer answer)
{
    m_answer = std::move(answer);

    QList<highscores::THighscoreEntry> entries;
    entries.reserve(static_cast<qsizetype>(m_answer.entries.size()));
    for (const auto& [rank, name, title, vocation, world, level, isPlayer, points] : m_answer.entries) {
        entries.append({ .rank = rank, .character = qstr(name), .loyaltyTitle = qstr(title), .vocation = highscores::vocationName(vocation),
                         .world = qstr(world), .level = level, .isOwnCharacter = isPlayer != 0, .score = points });
    }
    auto* old = m_highscoreData;
    m_highscoreData = new highscores::THighscoresData(std::move(entries));
    delete old;

    setDataAvailable(true);
    setShowLoadingComponent(false);

    // CIP 0x100d84420: the selected world's index, 0 (all) when absent.
    int worldIndex = 0;
    if (!m_answer.selectedWorld.empty()) {
        const auto it = std::ranges::find(m_answer.worlds, m_answer.selectedWorld);
        if (it != m_answer.worlds.end())
            worldIndex = static_cast<int>(it - m_answer.worlds.begin()) + 1;
    }
    setIndexOfSelectedWorld(worldIndex);

    // CIP 0x100d84660.
    int battlEyeIndex = 0;
    const QList<BattlEyeType> types = battlEyeTypes();
    for (int i = 0; i < types.size(); ++i) {
        if (types[i].value == m_answer.battlEye) {
            battlEyeIndex = i;
            break;
        }
    }
    setIndexOfSelectedBattlEyeType(battlEyeIndex);

    // CIP 0x100d848c0: index 0 is "all vocations".
    int vocationIndex = 0;
    for (size_t i = 0; i < m_answer.vocations.size(); ++i) {
        if (std::get<0>(m_answer.vocations[i]) == m_answer.selectedVocation) {
            vocationIndex = static_cast<int>(i) + 1;
            break;
        }
    }
    setIndexOfSelectedVocation(vocationIndex);

    // CIP 0x100d84a10.
    int categoryIndex = 0;
    for (size_t i = 0; i < m_answer.categories.size(); ++i) {
        if (std::get<0>(m_answer.categories[i]) == m_answer.selectedCategory) {
            categoryIndex = static_cast<int>(i);
            break;
        }
    }
    setIndexOfSelectedCategory(categoryIndex);

    m_selectedWorldTypes.clear();
    updateWorldTypes();
    Q_EMIT dataChanged();
}

void THighscoresDialogController::setNoData()
{
    setDataAvailable(false);
    setShowLoadingComponent(false);
}

void THighscoresDialogController::clear()
{
    dialog()->close();
    m_answer = {};
    delete m_highscoreData;
    m_highscoreData = nullptr;
    Q_EMIT dataChanged();
}

/// CIP 0x100d86ba0.
void THighscoresDialogController::showDialog()
{
    closeDialog();
    setShowLoadingComponent(true);
    setDataAvailable(true);
    dialog()->show();
}

/// CIP 0x100d86c20.
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

/// CIP 0x100d86d90.
void THighscoresDialogController::onClickedFirstPage()
{
    requestPage(1);
}

/// CIP 0x100d87080.
void THighscoresDialogController::onClickedPreviousPage()
{
    if (m_answer.page > 1)
        requestPage(m_answer.page - 1);
}

/// CIP 0x100d87450.
void THighscoresDialogController::onClickedNextPage()
{
    if (m_answer.page < m_answer.totalPages)
        requestPage(m_answer.page + 1);
}

/// CIP 0x100d87870.
void THighscoresDialogController::onClickedLastPage()
{
    requestPage(m_answer.totalPages);
}

/// CIP 0x100d87bd0.
void THighscoresDialogController::onClickedShowMyPlaceButton()
{
    requestOwnRank();
}

/// CIP 0x100d87eb0.
void THighscoresDialogController::applyFilters()
{
    requestPage(1);
}

/// CIP 0x100d86980.
void THighscoresDialogController::requestClose()
{
    closeDialog();
}

/// CIP 0x100d86990.
void THighscoresDialogController::toggleSelectedWorldType(const int RawWorldType)
{
    if (!m_selectedWorldTypes.removeOne(RawWorldType))
        m_selectedWorldTypes.append(RawWorldType);
    updateWorldTypes();
}

}
