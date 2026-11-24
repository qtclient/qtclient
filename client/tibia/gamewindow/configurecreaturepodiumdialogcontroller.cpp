#include "configurecreaturepodiumdialogcontroller.h"
#include "config/clientoptions.h"
#include "game/configurecreaturepodiumdata.h"
#include "game/gameciptypes.h"
#include "game/gamesessionbase.h"
#include "gamewindow/outfitdialogcontroller.h"

#include "input/gameaction.h"
#include "input/gameactionhandlers.h"

#include <tibia/assets/things.h>
#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/thingsession_forward.h>

namespace tibia::gamewindow {

namespace {

using LookDir = cip::tibia::qmlcomponents::TOutfitAppearanceInstanceQmlType::ECreatureLookDirection;
using Args = std::vector<std::any>;

QColor paletteColor(const int index)
{
    return qmlcomponents::outfitpalette::color(index);
}

LookDir lookDirectionFromProtocol(const uint8_t direction)
{
    switch (direction) {
        case 0: return LookDir::NORTH;
        case 1: return LookDir::EAST;
        case 3: return LookDir::WEST;
        default: return LookDir::SOUTH;
    }
}

uint8_t protocolFromLookDirection(const LookDir direction)
{
    switch (direction) {
        case LookDir::NORTH: return 0;
        case LookDir::EAST: return 1;
        case LookDir::WEST: return 3;
        default: return 2;
    }
}

/// The outfit's look: the creature look type, or the object of an object outfit.
int lookOf(const Outfit& outfit)
{
    return outfit.getId() != 0 ? outfit.getId() : outfit.getAuxId();
}

bool sameOutfit(const Outfit& a, const Outfit& b)
{
    return lookOf(a) == lookOf(b) && a.getHead() == b.getHead() && a.getBody() == b.getBody() && a.getLegs() == b.getLegs()
           && a.getFeet() == b.getFeet() && a.getAddons() == b.getAddons() && (a.getId() == 0) == (b.getId() == 0);
}

TBossSkinModel::Entry entryFromRace(const uint16_t raceId, const std::string& name, const Outfit& outfit)
{
    TBossSkinModel::Entry entry;
    entry.raceId = raceId;
    if (!name.empty() || outfit.getId() != 0 || outfit.getAuxId() != 0) {
        entry.name = QString::fromStdString(name);
        entry.outfit = outfit;
        return entry;
    }
    const RaceType& race = tibia::game::raceData(raceId);
    entry.name = QString::fromStdString(race.name);
    entry.outfit = race.outfit;
    return entry;
}

}

TBossSkinModel::TBossSkinModel(QObject* parent)
    : cip::TBossSkinModel(parent)
{
    connect(this, &QAbstractItemModel::rowsInserted, this, &TBossSkinModel::rowCountChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &TBossSkinModel::rowCountChanged);
}

void TBossSkinModel::setEntries(QList<Entry> entries)
{
    beginResetModel();
    m_entries = std::move(entries);
    m_count = m_entries.size();
    endResetModel();
    Q_EMIT rowCountChanged();
}

const TBossSkinModel::Entry* TBossSkinModel::entryAt(const int row) const
{
    if (row < 0 || row >= m_entries.size())
        return nullptr;
    return &m_entries[row];
}

int TBossSkinModel::count() const
{
    return m_count;
}

int TBossSkinModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

QVariant TBossSkinModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= m_entries.size())
        return {};
    const Entry& entry = m_entries[index.row()];
    const Outfit& outfit = entry.outfit;
    const bool objectOutfit = outfit.getCategory() == ThingCategoryItem || outfit.getId() == 0;
    const int outfitId = objectOutfit ? outfit.getAuxId() : outfit.getId();
    switch (role) {
        case OutfitIDRole: return outfitId;
        case OutfitNameRole: return entry.name;
        case FirstAddOnRole: return (outfit.getAddons() & 1) != 0;
        case SecondAddOnRole: return (outfit.getAddons() & 2) != 0;
        case OutfitTypeRole: return 0;
        case RaceRole: return entry.raceId;
        case HeadColorRole: return paletteColor(outfit.getHead());
        case TorsoColorRole: return paletteColor(outfit.getBody());
        case LegsColorRole: return paletteColor(outfit.getLegs());
        case DetailColorRole: return paletteColor(outfit.getFeet());
        case IsObjectOutfitRole: return objectOutfit;
        default: return {};
    }
}

QHash<int, QByteArray> TBossSkinModel::roleNames() const
{
    return {
        { OutfitIDRole, "outfitID" },
        { OutfitNameRole, "outfitName" },
        { FirstAddOnRole, "firstAddOn" },
        { SecondAddOnRole, "secondAddOn" },
        { OutfitTypeRole, "outfitType" },
        { HeadColorRole, "headColor" },
        { TorsoColorRole, "torsoColor" },
        { LegsColorRole, "legsColor" },
        { DetailColorRole, "detailColor" },
        { IsObjectOutfitRole, "isObjectOutfit" },
        { RaceRole, "race" },
    };
}

TBossSkinSortProxyModel::TBossSkinSortProxyModel(TBossSkinModel* source, QObject* parent)
    : cip::TBossSkinSortProxyModel(parent)
{
    setSourceModel(source);
}

QString TBossSkinSortProxyModel::outfitNameFilter() const
{
    return m_outfitNameFilter;
}

void TBossSkinSortProxyModel::setOutfitNameFilter(const QString& v)
{
    if (v == m_outfitNameFilter)
        return;
    beginFilterChange();
    m_outfitNameFilter = v;
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
    Q_EMIT filterChanged();
}

/// CIP vicinity: clear name filter and re-evaluate rows.
void TBossSkinSortProxyModel::resetFilter()
{
    if (m_outfitNameFilter.isEmpty())
        return;
    beginFilterChange();
    m_outfitNameFilter.clear();
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
    Q_EMIT filterChanged();
}

bool TBossSkinSortProxyModel::filterAcceptsRow(const int sourceRow, const QModelIndex& sourceParent) const
{
    if (m_outfitNameFilter.isEmpty())
        return true;
    const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    return index.data(TBossSkinModel::OutfitNameRole).toString().contains(m_outfitNameFilter, Qt::CaseInsensitive);
}

TConfigureCreaturePodiumDialogController::TConfigureCreaturePodiumDialogController(
    std::shared_ptr<appearances::TAppearanceInstanceFactory> AppearanceInstanceFactory, config::TClientOptions* ClientOptions,
    QQmlEngine* Engine, QQuickItem* DialogParentItem, QObject* parent)
    : TDialogController(QStringLiteral("outfitdialog/ConfigureCreaturePodiumDialog.qml"), Engine, DialogParentItem, parent)
    , m_pAppearanceInstanceFactory(std::move(AppearanceInstanceFactory))
    , m_pClientOptions(ClientOptions)
    , m_proxy(&m_skins)
    , m_preset(new qmlcomponents::TOutfitPresetQmlType(this))
{
    m_bossSkinsModel = &m_proxy;
    m_outfitPreset = m_preset;
    m_showFloor = true;
    m_showOutfit = true;
    m_showSocket = true;
    m_direction = LookDir::SOUTH;
    m_selectedIndex = -1;
}

void TConfigureCreaturePodiumDialogController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    for (const QMetaObject::Connection& connection : m_sessionConnections)
        disconnect(connection);
    m_sessionConnections.clear();
    m_pGameSession = GameSession;
    m_pOutfitDialogStorage.reset();
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_pGameSession.get());
    if (!session)
        return;
    if (const auto& player = session->playerProtocolMessageHandler()) {
        m_sessionConnections.push_back(connect(player.get(), &game::TPlayerProtocolMessageHandler::showConfigureCreaturePodiumDialog,
                                               this, &TConfigureCreaturePodiumDialogController::open));
    }
    m_pOutfitDialogStorage = session->outfitDialogStorage();
}

void TConfigureCreaturePodiumDialogController::open()
{
    m_pData = m_pOutfitDialogStorage ? m_pOutfitDialogStorage->configureCreaturePodiumData() : nullptr;
    if (!m_pData)
        return;
    const game::TConfigureCreaturePodiumData& data = *m_pData;
    const Outfit& selected = data.selectedOutfit;
    const uint16_t selectedRace = data.selectedRace;
    m_selected = selected;
    m_position = data.position;
    m_stackpos = data.stackPosition;
    // A podium without a creature shows the creature view.
    m_showOutfit = lookOf(selected) == 0 ? true : data.monsterVisible;
    m_showSocket = data.podiumVisible;
    m_direction = lookDirectionFromProtocol(data.direction);
    m_socketTypeID = data.objectId;

    QList<TBossSkinModel::Entry> entries;
    entries.reserve(static_cast<qsizetype>(data.races.size()));
    for (const auto& [raceId, name, outfit] : data.races)
        entries.append(entryFromRace(raceId, name, outfit));
    setPreset(selected);

    // The selected skin is the one with the podium's outfit and race.
    m_selectedIndex = -1;
    for (qsizetype i = 0; i < entries.size(); ++i) {
        if (sameOutfit(entries[i].outfit, selected) && entries[i].raceId == selectedRace) {
            m_selectedIndex = int(i);
            break;
        }
    }
    Q_EMIT selectedIndexChanged();
    if (m_chosenRace != selectedRace) {
        m_chosenRace = selectedRace;
        Q_EMIT chosenRaceChanged();
        Q_EMIT chosenOutfitChanged();
    }
    m_skins.setEntries(std::move(entries));
    m_proxy.resetFilter();
    m_proxy.invalidate();
    Q_EMIT showChanged();
    dialog()->show();
}

int TConfigureCreaturePodiumDialogController::chosenOutfit() const
{
    for (const TBossSkinModel::Entry& entry : m_skins.entries()) {
        if (entry.raceId == m_chosenRace)
            return lookOf(entry.outfit);
    }
    return 0;
}

void TConfigureCreaturePodiumDialogController::setPreset(const Outfit& outfit)
{
    // A fresh preset: the outfit's look, colours and addons, no mount or summon.
    const bool objectOutfit = outfit.getCategory() == ThingCategoryItem || (outfit.getId() == 0 && outfit.getAuxId() != 0);
    m_preset->setName({});
    m_preset->setOutfitID(lookOf(outfit));
    m_preset->outfitColor()->copyFrom(createJavaScriptOwnedOutfitColor(outfit.getHead(), outfit.getBody(), outfit.getLegs(), outfit.getFeet()));
    m_preset->setOutfitFirstAddOn((outfit.getAddons() & 1) != 0);
    m_preset->setOutfitSecondAddOn((outfit.getAddons() & 2) != 0);
    m_preset->setIsObjectOutfit(objectOutfit);
    m_preset->setMountID(0);
    m_preset->setSummonID(0);
    Q_EMIT selectedBossOutfitChanged();
}

void TConfigureCreaturePodiumDialogController::onIndexChanged(const int SelectedIndex, const int ChosenRace)
{
    m_selectedIndex = SelectedIndex;
    Q_EMIT selectedIndexChanged();
    if (m_chosenRace != ChosenRace) {
        m_chosenRace = ChosenRace;
        Q_EMIT chosenRaceChanged();
        Q_EMIT chosenOutfitChanged();
    }
    // The preview shows the skin of the chosen race; without one it is empty
    // and no race is chosen.
    for (const TBossSkinModel::Entry& entry : m_skins.entries()) {
        if (entry.raceId == m_chosenRace) {
            setPreset(entry.outfit);
            return;
        }
    }
    setPreset(Outfit{});
    if (m_chosenRace != 0) {
        m_chosenRace = 0;
        Q_EMIT chosenRaceChanged();
        Q_EMIT chosenOutfitChanged();
    }
}

void TConfigureCreaturePodiumDialogController::onCancelClicked()
{
    dialog()->close();
}

void TConfigureCreaturePodiumDialogController::onOkClicked()
{
    // Without the podium shown, OK needs a chosen race and the creature shown;
    // otherwise the dialog stays open.
    bool showCreature = m_showOutfit;
    if (!m_showSocket) {
        if (m_chosenRace == 0 || !m_showOutfit)
            return;
        showCreature = true;
    }
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionSetCreaturePodiumConfiguration>(
        static_cast<uint32_t>(m_chosenRace),
        input::TPodiumLocation{ m_position, static_cast<uint16_t>(m_socketTypeID), static_cast<uint8_t>(std::max(int(m_stackpos), 0)) },
        static_cast<uint8_t>(protocolFromLookDirection(m_direction)), m_showSocket, showCreature));
    dialog()->close();
}

void TConfigureCreaturePodiumDialogController::onControllerFinished() { dialog()->destroy(); }

/// The base properties and whether textures are smoothed (antialiasing).
QVariantMap TConfigureCreaturePodiumDialogController::initialDialogProperties() const
{
    QVariantMap properties = TDialogController::initialDialogProperties();
    properties.insert(QStringLiteral("clientSettingSmoothFiltering"),
                      m_pClientOptions
                          && m_pClientOptions->antialiasingMode()
                                 == static_cast<int>(cip::tibia::qmlcomponents::enums::EAntialiasingMode::AntialiasingModeAntialiasing));
    return properties;
}

}
