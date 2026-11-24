#include "outfitdialogcontroller.h"
#include "game/gameciptypes.h"
#include "game/gamesessionbase.h"
#include "game/selectoutfitdialogdata.h"

#include "app/clientoptions.h"

#include "gamewindow/genericentertextdialogcontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "qmlcomponents/outfitpalette.h"
#include "qmlcomponents/outfittypes.h"

#include <tibia/appearances/appearancetype.h>
#include <client/outfit.h>
#include <tibia/game/thingsession_forward.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJSEngine>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>
#include <tibia/game/gamesession.h>

namespace tibia::gamewindow {

bool outfitCanBeColorized(const int lookType)
{
    if (!tibia::game::isValidDatId(lookType, ThingCategoryCreature))
        return false;
    const TAppearanceTypePtr& type = tibia::game::thingType(lookType, ThingCategoryCreature);
    return type && type->getLayers() > 1;
}

namespace {

using cip::tibia::qmlcomponents::TOutfitAppearanceInstanceQmlType;
using qmlcomponents::TClipboardHelperQmlService;
using qmlcomponents::TOutfitColorQmlType;
using qmlcomponents::TOutfitPresetQmlType;

constexpr int OwnedOutfitType = 0;

using Gender = cip::tibia::gamewindow::TSelectOutfitDialogController::EOutfitGender;

}

/// CIP: the count follows the inserted and removed rows.
TCustomiseCharacterModel::TCustomiseCharacterModel(QObject* parent)
    : cip::TCustomiseCharacterModel(parent)
{
    connect(this, &QAbstractItemModel::rowsInserted, this, &TCustomiseCharacterModel::rowCountChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &TCustomiseCharacterModel::rowCountChanged);
}

void TCustomiseCharacterModel::setEntries(QList<Entry> entries)
{
    beginResetModel();
    m_entries = std::move(entries);
    m_count = m_entries.size();
    endResetModel();
    Q_EMIT rowCountChanged();
}

const TCustomiseCharacterModel::Entry* TCustomiseCharacterModel::ownedEntry(const int id) const
{
    for (const Entry& entry : m_entries)
        if (entry.id == id && entry.type == OwnedOutfitType)
            return &entry;
    return nullptr;
}

int TCustomiseCharacterModel::count() const
{
    return m_count;
}

int TCustomiseCharacterModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

QVariant TCustomiseCharacterModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= m_entries.size())
        return {};
    const Entry& entry = m_entries[index.row()];
    switch (role) {
        case Qt::DisplayRole:
        case OutfitNameRole: return entry.name;
        case OutfitIDRole: return entry.id;
        case OutfitSourceRole: return 0;
        case FirstAddOnRole: return entry.firstAddOn;
        case SecondAddOnRole: return entry.secondAddOn;
        case OutfitTypeRole: return entry.type;
        case CanBeColorizedRole: return entry.canBeColorized;
        case OfferIDRole: return entry.offerId;
        case OutfitGenderRole: return entry.gender;
        case OtherGenderOutfitIDRole: return entry.otherGenderId;
        default: return {};
    }
}

QHash<int, QByteArray> TCustomiseCharacterModel::roleNames() const
{
    return {
        { OutfitIDRole, "outfitID" },
        { OutfitNameRole, "outfitName" },
        { OutfitSourceRole, "outfitSource" },
        { FirstAddOnRole, "firstAddOn" },
        { SecondAddOnRole, "secondAddOn" },
        { OutfitTypeRole, "outfitType" },
        { CanBeColorizedRole, "canBeColorized" },
        { OfferIDRole, "offerID" },
        { OutfitGenderRole, "outfitGender" },
        { OtherGenderOutfitIDRole, "otherGenderOutfitID" },
    };
}

TCustomizeCharacterSortProxyModel::TCustomizeCharacterSortProxyModel(TCustomiseCharacterModel* source, QObject* parent)
    : cip::TCustomizeCharacterSortProxyModel(parent)
{
    setSourceModel(source);
}

bool TCustomizeCharacterSortProxyModel::showOnlyOwnOutfits() const
{
    return m_showOnlyOwnOutfits;
}

void TCustomizeCharacterSortProxyModel::setShowOnlyOwnOutfits(const bool v)
{
    if (v == m_showOnlyOwnOutfits)
        return;
    beginFilterChange();
    m_showOnlyOwnOutfits = v;
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
    Q_EMIT filterChanged();
}

QString TCustomizeCharacterSortProxyModel::outfitNameFilter() const
{
    return m_outfitNameFilter;
}

void TCustomizeCharacterSortProxyModel::setOutfitNameFilter(const QString& v)
{
    if (v == m_outfitNameFilter)
        return;
    beginFilterChange();
    m_outfitNameFilter = v;
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
    Q_EMIT filterChanged();
}

int TCustomizeCharacterSortProxyModel::numberOfOwnedOutfits() const
{
    const auto* source = static_cast<const TCustomiseCharacterModel*>(sourceModel());
    return static_cast<int>(std::ranges::count_if(source->entries(), [](const auto& entry) { return entry.type == OwnedOutfitType; }));
}

/// CIP .
void TCustomizeCharacterSortProxyModel::resetFilter()
{
    beginFilterChange();
    m_showOnlyOwnOutfits = false;
    m_outfitNameFilter.clear();
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
    Q_EMIT filterChanged();
}

/// CIP .
bool TCustomizeCharacterSortProxyModel::filterAcceptsRow(const int sourceRow, const QModelIndex& sourceParent) const
{
    const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    if (m_showOnlyOwnOutfits && index.data(TCustomiseCharacterModel::OutfitTypeRole).toInt() != OwnedOutfitType)
        return false;
    if (!m_outfitNameFilter.isEmpty()
        && !index.data(TCustomiseCharacterModel::OutfitNameRole).toString().contains(m_outfitNameFilter, Qt::CaseInsensitive))
        return false;
    return true;
}

/// CIP: the count follows the inserted and removed rows.
TOutfitPresetsModel::TOutfitPresetsModel(QObject* parent)
    : cip::TOutfitPresetsModel(parent)
{
    connect(this, &QAbstractItemModel::rowsInserted, this, &TOutfitPresetsModel::rowCountChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &TOutfitPresetsModel::rowCountChanged);
}

void TOutfitPresetsModel::setPresets(QList<QJsonObject> presets, QList<bool> owned)
{
    beginResetModel();
    m_presets = std::move(presets);
    m_owned = std::move(owned);
    m_count = m_presets.size();
    endResetModel();
    Q_EMIT rowCountChanged();
}

int TOutfitPresetsModel::count() const
{
    return m_count;
}

int TOutfitPresetsModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_presets.size();
}

QVariant TOutfitPresetsModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= m_presets.size())
        return {};
    switch (role) {
        case JsonDataRole: return m_presets[index.row()];
        case EverythingOwnedRole: return m_owned.value(index.row());
        default: return {};
    }
}

QHash<int, QByteArray> TOutfitPresetsModel::roleNames() const
{
    return { { JsonDataRole, "jsondata" }, { EverythingOwnedRole, "everythingowned" } };
}

TSelectOutfitDialogController::TSelectOutfitDialogController(QObject* parent)
    : TDialogController(QStringLiteral("outfitdialog/OutfitDialog.qml"), parent)
    , m_outfitsProxy(&m_outfits)
    , m_mountsProxy(&m_mounts)
    , m_summonsProxy(&m_summons)
    , m_renameDialog(new TGenericEnterTextDialogController(this))
{
    // View options live in the controller for the session, no file (QML
    // writes them back on every change).
    m_showMovement = false;
    m_showOutfit = true;
    m_showSummon = true;
    m_showFloor = true;
    m_outfitsModel = &m_outfitsProxy;
    m_mountsModel = &m_mountsProxy;
    m_summonsModel = &m_summonsProxy;
    m_presetsModel = &m_presets;
    connect(m_renameDialog, &TGenericEnterTextDialogController::textEntered, this, &TSelectOutfitDialogController::onPresetNameEntered);
    m_renameDialog->setCaption(qtTrId("outfit_preset_rename_dialog_caption"));
    m_renameDialog->setAllowEmptyText(true);
    m_renameDialog->setMaxInputLength(60);
}

TSelectOutfitDialogController::~TSelectOutfitDialogController() = default;

void TSelectOutfitDialogController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    m_gameSessionConnections.disconnectAll();
    m_gameSession = GameSession;
    m_renameDialog->dialog()->close();
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    if (!session)
        return;
    m_gameSessionConnections.add(connect(session->playerProtocolMessageHandler().get(),
                                         &game::TPlayerProtocolMessageHandler::showSelectOutfitDialog, this,
                                         &TSelectOutfitDialogController::onShowSelectOutfitDialog));
    m_outfitDialogStorage = session->outfitDialogStorage();
    if (m_outfitDialogStorage) {
        m_gameSessionConnections.add(connect(m_outfitDialogStorage.get(), &game::TOutfitDialogStorage::customizeCharacterPresetsChanged, this,
                                             &TSelectOutfitDialogController::onCharacterPresetsChanged));
        m_gameSessionConnections.add(connect(m_outfitDialogStorage.get(),
                                             &game::TOutfitDialogStorage::configureShowOffSocketPresetsChanged, this,
                                             &TSelectOutfitDialogController::onCharacterPresetsChanged));
    }
    onCharacterPresetsChanged();
}

void TSelectOutfitDialogController::onShowSelectOutfitDialog()
{
    m_dialogData = m_outfitDialogStorage ? m_outfitDialogStorage->selectOutfitDialogData() : nullptr;
    m_ownedIds.clear();
    if (!m_dialogData)
        return;
    const game::TSelectOutfitDialogData& data = *m_dialogData;

    QList<TCustomiseCharacterModel::Entry> outfits;
    for (const auto& outfit : data.Outfits) {
        TCustomiseCharacterModel::Entry entry{ .id = outfit.Id,
                                               .name = outfit.Name,
                                               .firstAddOn = (outfit.AddOns & 1) != 0,
                                               .secondAddOn = (outfit.AddOns & 2) != 0,
                                               .type = outfit.Type,
                                               .offerId = static_cast<int>(outfit.OfferId) };
        entry.otherGenderId = data.otherGenderOutfit(outfit.Id);
        if (entry.otherGenderId != 0)
            entry.gender = data.isMaleOutfit(outfit.Id) ? Gender::OutfitGenderMale
                           : data.isFemaleOutfit(outfit.Id) ? Gender::OutfitGenderFemale
                                                            : Gender::OutfitGenderUnknown;
        entry.canBeColorized = outfitCanBeColorized(outfit.Id);
        outfits.append(entry);
        if (entry.type == OwnedOutfitType) {
            m_ownedIds.insert(entry.id);
            if (entry.otherGenderId != 0)
                m_ownedIds.insert(static_cast<uint16_t>(entry.otherGenderId));
        }
    }
    m_outfits.setEntries(std::move(outfits));
    m_outfitsProxy.resetFilter();

    // Mounts and summons have no gender twin.
    const auto entries = [this](const std::vector<game::TSelectOutfitDialogData::TMountOrSummon>& List) {
        QList<TCustomiseCharacterModel::Entry> result;
        for (const auto& item : List) {
            result.append({ .id = item.Id,
                            .name = item.Name,
                            .type = item.Type,
                            .canBeColorized = outfitCanBeColorized(item.Id),
                            .offerId = static_cast<int>(item.OfferId) });
            if (item.Type == OwnedOutfitType)
                m_ownedIds.insert(item.Id);
        }
        return result;
    };
    m_mounts.setEntries(entries(data.Mounts));
    m_mountsProxy.resetFilter();
    m_summons.setEntries(entries(data.Summons));
    m_summonsProxy.resetFilter();

    m_randomMount = data.RandomMount;
    removeUnsupportedAddOns();
    dialog()->create();
    dialog()->show();
    onCharacterPresetsChanged();
}

/// An outfit offers only the add-ons its idle appearance has patterns for.
void TSelectOutfitDialogController::removeUnsupportedAddOns()
{
    for (auto& outfit : m_dialogData->Outfits) {
        if (!tibia::game::isValidDatId(outfit.Id, ThingCategoryCreature))
            continue;
        const TAppearanceTypePtr& type = tibia::game::thingType(outfit.Id, ThingCategoryCreature);
        if (!type || !type->hasFrameGroup(tibia::appearances::EFixedFrameGroup::OutfitIdle))
            continue;
        const uint32_t patterns = type->frameGroup(tibia::appearances::EFixedFrameGroup::OutfitIdle).patternHeight();
        outfit.AddOns &= static_cast<uint8_t>(~(~0u << ((patterns - 1u) & 0x1f)));
    }
}

/// The data's mode; no data or an unknown mode select the own outfit.
TSelectOutfitDialogController::EDialogMode TSelectOutfitDialogController::dialogMode() const
{
    if (!m_dialogData)
        return SelectOutfit;
    const int mode = m_dialogData->DialogMode;
    return mode >= TryOutfit && mode <= ConfigureShowOffSocket ? static_cast<EDialogMode>(mode) : SelectOutfit;
}

/// The socket configuration's outfit flag, else the dialog's own.
bool TSelectOutfitDialogController::showOutfit() const
{
    if (!m_dialogData)
        return false;
    return m_dialogData->DialogMode == ConfigureShowOffSocket ? m_dialogData->ShowOutfit : m_showOutfit;
}

/// Only a show-off socket can be shown.
bool TSelectOutfitDialogController::showSocket() const
{
    return m_dialogData && m_dialogData->DialogMode == ConfigureShowOffSocket && m_dialogData->ShowSocket;
}

/// The data's look direction, south without data.
TOutfitAppearanceInstanceQmlType::ECreatureLookDirection TSelectOutfitDialogController::direction() const
{
    return static_cast<TOutfitAppearanceInstanceQmlType::ECreatureLookDirection>(m_dialogData ? m_dialogData->LookDirection : 2);
}

bool TSelectOutfitDialogController::isMounted() const { return m_dialogData && m_dialogData->Mounted; }

/// A new JS-owned preset of the data's outfit (colours and add-ons), mount
/// (colours) and summon.
cip::tibia::qmlcomponents::TOutfitPresetQmlType* TSelectOutfitDialogController::selectedOutfitPreset() const
{
    if (!m_dialogData)
        return nullptr;
    const game::TSelectOutfitDialogData& data = *m_dialogData;
    auto* preset = new TOutfitPresetQmlType();
    QJSEngine::setObjectOwnership(preset, QJSEngine::JavaScriptOwnership);
    preset->setOutfitID(data.OutfitId);
    TOutfitColorQmlType outfitColor(data.HeadColor, data.TorsoColor, data.LegsColor, data.DetailColor);
    preset->outfitColor()->copyFrom(&outfitColor);
    preset->setOutfitFirstAddOn((data.AddOns & 1) != 0);
    preset->setOutfitSecondAddOn((data.AddOns & 2) != 0);
    preset->setMountID(data.MountId);
    TOutfitColorQmlType mountColor(data.MountHeadColor, data.MountTorsoColor, data.MountLegsColor, data.MountDetailColor);
    preset->mountColor()->copyFrom(&mountColor);
    preset->setSummonID(data.SummonId);
    return preset;
}

/// The outfit palette, built once.
QVariantList TSelectOutfitDialogController::hsvColors() const
{
    static const QVariantList colors = [] {
        QVariantList list;
        list.reserve(qmlcomponents::outfitpalette::Count);
        for (int i = 0; i < qmlcomponents::outfitpalette::Count; ++i)
            list.append(qmlcomponents::outfitpalette::color(i));
        return list;
    }();
    return colors;
}

/// The base properties and whether textures are smoothed (antialiasing).
QVariantMap TSelectOutfitDialogController::initialDialogProperties() const
{
    QVariantMap properties = TDialogController::initialDialogProperties();
    properties.insert(QStringLiteral("clientSettingSmoothFiltering"),
                      app::ClientOptions::load().options().value(QLatin1String("antialiasingMode")).toInt()
                          == static_cast<int>(cip::tibia::qmlcomponents::enums::EAntialiasingMode::AntialiasingModeAntialiasing));
    return properties;
}

void TSelectOutfitDialogController::onCancelClicked()
{
    dialog()->close();
}

/// Closes, then opens the store on the offer (open action 4).
void TSelectOutfitDialogController::onClickedOpenStoreOffer(const int offerID)
{
    dialog()->close();
    auto action = std::make_shared<input::TGameActionOpenStore>(4, offerID);
    Q_EMIT publishGameAction(action);
}

void TSelectOutfitDialogController::onControllerFinished()
{
    dialog()->close();
}

/// Each preset the dialog mode edits gets "everythingOwned": its outfit, mount
/// and summon (those set) are all owned.
void TSelectOutfitDialogController::onCharacterPresetsChanged()
{
    QList<QJsonObject> presets;
    QList<bool> owned;
    TOutfitPresetQmlType preset;
    const QJsonArray* list = presetList();
    const auto isOwned = [this](const int id) { return id == 0 || m_ownedIds.contains(static_cast<uint16_t>(id)); };
    for (const QJsonValue& value : list ? *list : QJsonArray()) {
        QJsonObject json = value.toObject();
        bool everythingOwned = false;
        if (preset.canBeFilledFromJsonObject(json)) {
            preset.fromJsonObject(json);
            everythingOwned = isOwned(preset.outfitID()) && isOwned(preset.mountID()) && isOwned(preset.summonID());
            json.insert(QStringLiteral("everythingOwned"), everythingOwned);
        }
        presets.append(json);
        owned.append(everythingOwned);
    }
    m_presets.setPresets(std::move(presets), std::move(owned));
}

/// Sends what the data's mode configures, then closes: a show-off socket
/// (its mount only when the character is mounted), a hireling's outfit, or
/// the own outfit, mount and summon. The other modes send nothing.
void TSelectOutfitDialogController::setCharacterConfiguration(cip::tibia::qmlcomponents::TOutfitPresetQmlType* pOutfitPreset,
                                                              const int LookDirectionRaw, const bool ShowSocket,
                                                              const bool MountCharacter)
{
    if (pOutfitPreset && m_dialogData) {
        const auto* outfitColor = pOutfitPreset->outfitColor();
        const auto* mountColor = pOutfitPreset->mountColor();
        const uint8_t addOns = static_cast<uint8_t>((pOutfitPreset->outfitFirstAddOn() ? 1 : 0) + (pOutfitPreset->outfitSecondAddOn() ? 2 : 0));
        Outfit outfit;
        outfit.setCategory(ThingCategoryCreature);
        outfit.setId(static_cast<uint16_t>(pOutfitPreset->outfitID()));
        outfit.setHead(static_cast<uint8_t>(outfitColor->headColorIndex()));
        outfit.setBody(static_cast<uint8_t>(outfitColor->torsoColorIndex()));
        outfit.setLegs(static_cast<uint8_t>(outfitColor->legsColorIndex()));
        outfit.setFeet(static_cast<uint8_t>(outfitColor->detailColorIndex()));
        outfit.setAddons(addOns);
        const auto setMount = [&](const int MountId) {
            outfit.setMount(static_cast<uint16_t>(MountId));
            outfit.setMountColors(static_cast<uint8_t>(mountColor->headColorIndex()), static_cast<uint8_t>(mountColor->torsoColorIndex()),
                                  static_cast<uint8_t>(mountColor->legsColorIndex()), static_cast<uint8_t>(mountColor->detailColorIndex()));
        };
        switch (m_dialogData->DialogMode) {
            case ConfigureShowOffSocket:
                setMount(MountCharacter ? pOutfitPreset->mountID() : 0);
                Q_EMIT publishGameAction(std::make_shared<input::TGameActionSetShowOffSocketConfiguration>(
                    m_dialogData->PodiumLocation, outfit, static_cast<uint8_t>(LookDirectionRaw), ShowSocket));
                break;
            case HirelingSelectOutfit:
                Q_EMIT publishGameAction(std::make_shared<input::TGameActionSetHirelingOutfit>(outfit, m_dialogData->HirelingCreatureId));
                break;
            case SelectOutfit:
                setMount(pOutfitPreset->mountID());
                outfit.setFamiliar(static_cast<uint16_t>(pOutfitPreset->summonID()));
                Q_EMIT publishGameAction(std::make_shared<input::TGameActionSetPlayerOutfit>(outfit, MountCharacter, m_randomMount));
                break;
            default: break;
        }
    }
    dialog()->close();
}

/// The presets the dialog mode edits: the show-off socket presets when
/// configuring a socket, none in the try/hireling/character info modes, else
/// the outfit presets.
QJsonArray* TSelectOutfitDialogController::presetList()
{
    if (!m_outfitDialogStorage)
        return nullptr;
    const int mode = m_dialogData ? m_dialogData->DialogMode : SelectOutfit;
    if (mode == ConfigureShowOffSocket)
        return &m_outfitDialogStorage->showOffPresets();
    if (mode >= TryOutfit && mode <= CharacterInfoShowSelection)
        return nullptr;
    return &m_outfitDialogStorage->presets();
}

const QJsonArray* TSelectOutfitDialogController::presetList() const
{
    return const_cast<TSelectOutfitDialogController*>(this)->presetList();
}

/// Replaces the preset at `Index`, or appends it for -1.
void TSelectOutfitDialogController::outfitPresetsSaveIndex(const QJsonObject& JsonObject, const int Index)
{
    QJsonArray* list = presetList();
    if (!list)
        return;
    if (Index == -1)
        list->append(JsonObject);
    else if (Index < list->size())
        list->replace(Index, JsonObject);
    savePresets();
    onCharacterPresetsChanged();
}

void TSelectOutfitDialogController::outfitPresetsDeleteIndex(const int Index)
{
    QJsonArray* list = presetList();
    if (!list || Index < 0 || Index >= list->size())
        return;
    list->removeAt(Index);
    savePresets();
    onCharacterPresetsChanged();
}

/// Asks for the preset's new name (the rename dialog starts with the current
/// one); onPresetNameEntered applies it.
void TSelectOutfitDialogController::outfitPresetsRenameIndex(const int Index)
{
    const QJsonArray* list = presetList();
    if (!list || Index < 0 || Index >= list->size())
        return;
    m_renameIndex = Index;
    TOutfitPresetQmlType preset;
    preset.fromJsonObject(list->at(Index).toObject());
    m_renameDialog->showWithText(preset.name());
}

/// Renames the preset the rename dialog was opened for.
void TSelectOutfitDialogController::onPresetNameEntered(const QString& PlayerName)
{
    QJsonArray* list = presetList();
    if (!list || m_renameIndex < 0 || m_renameIndex >= list->size())
        return;
    QJsonObject json = list->at(m_renameIndex).toObject();
    json[QStringLiteral("name")] = PlayerName;
    list->replace(m_renameIndex, json);
    savePresets();
    onCharacterPresetsChanged();
}

/// A preset or a colour set.
void TSelectOutfitDialogController::copyToClipboard(QObject* pData)
{
    QJsonObject json;
    if (auto* preset = qobject_cast<cip::tibia::qmlcomponents::TOutfitPresetQmlType*>(pData))
        json = preset->toJsonObject();
    else if (auto* color = qobject_cast<cip::tibia::qmlcomponents::TOutfitColorQmlType*>(pData))
        json = color->toJsonObject();
    else
        return;
    TClipboardHelperQmlService clipboard;
    clipboard.setJsonObjectAsBase64String(json);
}

/// A JS-owned OutfitColor or OutfitPreset, or null.
QObject* TSelectOutfitDialogController::createFromClipboard()
{
    TClipboardHelperQmlService clipboard;
    const QJsonObject json = clipboard.getBase64StringAsJsonObject();
    QObject* result = nullptr;
    TOutfitColorQmlType color;
    TOutfitPresetQmlType preset;
    if (color.canBeFilledFromJsonObject(json)) {
        auto* created = new TOutfitColorQmlType();
        created->fromJsonObject(json);
        result = created;
    } else if (preset.canBeFilledFromJsonObject(json)) {
        auto* created = new TOutfitPresetQmlType();
        created->fromJsonObject(json);
        result = created;
    }
    if (result)
        QJSEngine::setObjectOwnership(result, QJSEngine::JavaScriptOwnership);
    return result;
}

void TSelectOutfitDialogController::savePresets()
{
    if (m_outfitDialogStorage)
        m_outfitDialogStorage->announcePresetsChanged();
}

}
