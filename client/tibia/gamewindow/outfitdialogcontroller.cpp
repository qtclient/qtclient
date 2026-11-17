#include "outfitdialogcontroller.h"
#include "game/gameciptypes.h"

#include "app/clientoptions.h"

#include "gamewindow/genericentertextdialogcontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
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
constexpr int StoreOutfitType = 1;

using Entries = QList<TCustomiseCharacterModel::Entry>;
using Gender = cip::tibia::gamewindow::TSelectOutfitDialogController::EOutfitGender;

/// CIP : the pair partner, or the id itself.
int otherGenderOutfit(const QList<QPair<int, int>>& pairs, const int id)
{
    for (const auto& [male, female] : pairs) {
        if (male == id)
            return female;
        if (female == id)
            return male;
    }
    return id;
}

/// CIP / : male when first of a pair, female when second.
int outfitGender(const QList<QPair<int, int>>& pairs, const int id)
{
    for (const auto& pair : pairs)
        if (pair.first == id)
            return Gender::OutfitGenderMale;
    for (const auto& pair : pairs)
        if (pair.second == id)
            return Gender::OutfitGenderFemale;
    return Gender::OutfitGenderUnknown;
}

/// CIP : a store entry keeps its offer id.
void applyOffers(Entries& entries, const QList<int>& offers)
{
    for (qsizetype i = 0; i < entries.size(); ++i)
        entries[i].offerId = entries[i].type == StoreOutfitType ? offers.value(i) : 0;
}

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
    , m_preset(new TOutfitPresetQmlType(this))
{
    // View options live in the controller for the session, no file (CIP ctor
    // defaults; QML writes them back on every change).
    m_dialogMode = SelectOutfit;
    m_showMovement = false;
    m_showOutfit = true;
    m_showSummon = true;
    m_showFloor = true;
    m_outfitsModel = &m_outfitsProxy;
    m_mountsModel = &m_mountsProxy;
    m_summonsModel = &m_summonsProxy;
    m_presetsModel = &m_presets;
    m_selectedOutfitPreset = m_preset;
    for (int i = 0; i < qmlcomponents::outfitpalette::Count; ++i)
        m_hsvColors.append(qmlcomponents::outfitpalette::color(i));
}

TSelectOutfitDialogController::~TSelectOutfitDialogController() = default;

void TSelectOutfitDialogController::setWindowOptions(WindowOptions options)
{
    m_randomMount = options.randomMount;
    m_windowOptions = std::move(options);
    m_dialogMode = m_windowOptions.tryOutfitMode == 5 ? ConfigureShowOffSocket : SelectOutfit;
    Q_EMIT dialogModeChanged();
    Q_EMIT showChanged();
}

/// The socket configuration's outfit flag, else the dialog's own.
bool TSelectOutfitDialogController::showOutfit() const
{
    return m_dialogMode == ConfigureShowOffSocket ? m_windowOptions.podiumShowOutfit : m_showOutfit;
}

/// Only a show-off socket can be shown.
bool TSelectOutfitDialogController::showSocket() const
{
    return m_dialogMode == ConfigureShowOffSocket && m_windowOptions.podiumVisible;
}

/// The window's look direction (south unless a socket says otherwise).
TOutfitAppearanceInstanceQmlType::ECreatureLookDirection TSelectOutfitDialogController::direction() const
{
    return static_cast<TOutfitAppearanceInstanceQmlType::ECreatureLookDirection>(m_windowOptions.podiumDirection);
}

bool TSelectOutfitDialogController::isMounted() const { return m_windowOptions.mounted; }

/// The base properties and whether textures are smoothed (antialiasing).
QVariantMap TSelectOutfitDialogController::initialDialogProperties() const
{
    QVariantMap properties = TDialogController::initialDialogProperties();
    properties.insert(QStringLiteral("clientSettingSmoothFiltering"),
                      app::ClientOptions::load().options().value(QLatin1String("antialiasingMode")).toInt()
                          == static_cast<int>(cip::tibia::qmlcomponents::enums::EAntialiasingMode::AntialiasingModeAntialiasing));
    return properties;
}

void TSelectOutfitDialogController::open(const Outfit& current, QList<TCustomiseCharacterModel::Entry> outfits,
                                         QList<TCustomiseCharacterModel::Entry> mounts, QList<TCustomiseCharacterModel::Entry> summons)
{
    if (dialog()->isShown())
        dialog()->close();

    const auto& pairs = m_windowOptions.genderPairs;
    for (auto& entry : outfits) {
        entry.otherGenderId = otherGenderOutfit(pairs, entry.id);
        entry.gender = entry.otherGenderId != 0 ? outfitGender(pairs, entry.id) : int(Gender::OutfitGenderUnknown);
    }
    // Mounts and summons have no gender twin.
    for (Entries* list : { &mounts, &summons })
        for (auto& entry : *list) {
            entry.otherGenderId = 0;
            entry.gender = Gender::OutfitGenderUnknown;
        }
    applyOffers(outfits, m_windowOptions.outfitOffers);
    applyOffers(mounts, m_windowOptions.mountOffers);
    applyOffers(summons, m_windowOptions.summonOffers);

    // CIP : a current outfit (or its other-gender twin), mount or
    // summon missing from its list falls back to the list's first entry
    // (outfit add-ons cleared), or none.
    const auto contains = [](const Entries& list, const int id, const int twin) {
        return std::ranges::any_of(list, [&](const auto& entry) { return entry.id == id || entry.id == twin; });
    };
    int outfitId = current.getId();
    int addons = current.getAddons();
    if (!contains(outfits, outfitId, otherGenderOutfit(pairs, outfitId))) {
        outfitId = outfits.isEmpty() ? 0 : outfits.first().id;
        addons = 0;
    }
    int mountId = current.getMount();
    if (!contains(mounts, mountId, mountId))
        mountId = mounts.isEmpty() ? 0 : mounts.first().id;
    int summonId = current.getFamiliar();
    if (!contains(summons, summonId, summonId))
        summonId = summons.isEmpty() ? 0 : summons.first().id;

    m_outfits.setEntries(std::move(outfits));
    m_mounts.setEntries(std::move(mounts));
    m_summons.setEntries(std::move(summons));
    m_outfitsProxy.resetFilter();
    m_mountsProxy.resetFilter();
    m_summonsProxy.resetFilter();

    m_preset->setName({});
    m_preset->setIsObjectOutfit(false);
    m_preset->setOutfitID(outfitId);
    m_preset->setOutfitFirstAddOn((addons & 1) != 0);
    m_preset->setOutfitSecondAddOn((addons & 2) != 0);
    TOutfitColorQmlType outfitColor(current.getHead(), current.getBody(), current.getLegs(), current.getFeet());
    m_preset->outfitColor()->copyFrom(&outfitColor);
    m_preset->setMountID(mountId);
    TOutfitColorQmlType mountColor(current.getMountHead(), current.getMountBody(), current.getMountLegs(), current.getMountFeet());
    m_preset->mountColor()->copyFrom(&mountColor);
    m_preset->setSummonID(summonId);

    onCharacterPresetsChanged();
    dialog()->show();
}

void TSelectOutfitDialogController::openCharacterInfoSelection(const Outfit& current, Entries outfits, Entries mounts, Entries summons,
                                                               const bool isOutfit, const bool isMount, const bool isSummonSkin)
{
    for (Entries* list : { &outfits, &mounts })
        for (auto& entry : *list)
            entry.canBeColorized = outfitCanBeColorized(entry.id);
    m_dialogMode = CharacterInfoShowSelection;
    m_showOutfit = isOutfit || (!isMount && !isSummonSkin);
    m_showSummon = isSummonSkin;
    Q_EMIT dialogModeChanged();
    Q_EMIT showChanged();
    open(current, std::move(outfits), std::move(mounts), std::move(summons));
}

/// CIP .
void TSelectOutfitDialogController::onCancelClicked()
{
    dialog()->close();
}

/// CIP : closes, then opens the store on the offer (open action 4).
void TSelectOutfitDialogController::onClickedOpenStoreOffer(const int offerID)
{
    dialog()->close();
    auto action = std::make_shared<input::TGameActionOpenStore>(4, offerID);
    input::publish(action);
}

void TSelectOutfitDialogController::onControllerFinished()
{
    dialog()->close();
}

/// CIP : a preset counts as owned when its outfit and mount are.
void TSelectOutfitDialogController::onCharacterPresetsChanged()
{
    QList<QJsonObject> presets;
    QList<bool> owned;
    TOutfitPresetQmlType preset;
    const QJsonArray* list = presetList();
    for (const QJsonValue& value : list ? *list : QJsonArray()) {
        const QJsonObject json = value.toObject();
        if (!preset.canBeFilledFromJsonObject(json))
            continue;
        preset.fromJsonObject(json);
        bool everythingOwned = true;
        if (preset.outfitID() != 0) {
            const auto* outfit = m_outfits.ownedEntry(preset.outfitID());
            everythingOwned = outfit && (!preset.outfitFirstAddOn() || outfit->firstAddOn) && (!preset.outfitSecondAddOn() || outfit->secondAddOn);
        }
        if (preset.mountID() != 0)
            everythingOwned = everythingOwned && m_mounts.ownedEntry(preset.mountID());
        presets.append(json);
        owned.append(everythingOwned);
    }
    m_presets.setPresets(std::move(presets), std::move(owned));
}

/// CIP (SelectOutfit): publishes SET_PLAYER_OUTFIT, then closes.
/// Mode 5 (ConfigureShowOffSocket): publishes SET_SHOW_OFF_SOCKET_CONFIGURATION.
void TSelectOutfitDialogController::setCharacterConfiguration(cip::tibia::qmlcomponents::TOutfitPresetQmlType* pOutfitPreset,
                                                              const int LookDirectionRaw, const bool ShowSocket,
                                                              const bool MountCharacter)
{
    if (pOutfitPreset && tibia::game::isOnline()) {
        const auto* outfitColor = pOutfitPreset->outfitColor();
        const auto* mountColor = pOutfitPreset->mountColor();
        Outfit outfit;
        outfit.setCategory(ThingCategoryCreature);
        outfit.setId(pOutfitPreset->outfitID());
        outfit.setHead(outfitColor->headColorIndex());
        outfit.setBody(outfitColor->torsoColorIndex());
        outfit.setLegs(outfitColor->legsColorIndex());
        outfit.setFeet(outfitColor->detailColorIndex());
        outfit.setAddons((pOutfitPreset->outfitFirstAddOn() ? 1 : 0) + (pOutfitPreset->outfitSecondAddOn() ? 2 : 0));
        outfit.setMount(pOutfitPreset->mountID());
        outfit.setMountColors(mountColor->headColorIndex(), mountColor->torsoColorIndex(), mountColor->legsColorIndex(),
                              mountColor->detailColorIndex());
        outfit.setFamiliar(pOutfitPreset->summonID());
        if (m_dialogMode == CharacterInfoShowSelection) {
            // Preview-only: Character Info never applies an outfit from this dialog.
        } else if (m_dialogMode == ConfigureShowOffSocket) {
            // The socket shows a mount only when the character is mounted.
            if (!MountCharacter)
                outfit.setMount(0);
            input::publish(std::make_shared<input::TGameActionSetShowOffSocketConfiguration>(
                input::TPodiumLocation{ m_windowOptions.podiumPosition, m_windowOptions.podiumItemId, m_windowOptions.podiumStackpos },
                outfit, static_cast<uint8_t>(LookDirectionRaw), ShowSocket));
        } else {
            input::publish(std::make_shared<input::TGameActionSetPlayerOutfit>(outfit, MountCharacter, m_randomMount));
        }
    }
    dialog()->close();
}

/// The presets the dialog mode edits: the show-off socket presets when
/// configuring a socket, none in the try/hireling/character info modes, else
/// the outfit presets.
QJsonArray* TSelectOutfitDialogController::presetList()
{
    game::TOutfitDialogStorage* storage = game::TOutfitDialogStorage::instance();
    if (!storage)
        return nullptr;
    if (m_dialogMode == ConfigureShowOffSocket)
        return &storage->showOffPresets();
    if (m_dialogMode >= TryOutfit && m_dialogMode <= CharacterInfoShowSelection)
        return nullptr;
    return &storage->presets();
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

/// Asks for the preset's new name; onPresetNameEntered applies it.
void TSelectOutfitDialogController::outfitPresetsRenameIndex(const int Index)
{
    const QJsonArray* list = presetList();
    if (!list || Index < 0 || Index >= list->size())
        return;
    m_renameIndex = Index;
    if (!m_renameDialog) {
        m_renameDialog = new TGenericEnterTextDialogController(this);
        m_renameDialog->setCaption(qtTrId("outfit_preset_rename_dialog_caption"));
        connect(m_renameDialog, &TGenericEnterTextDialogController::textEntered, this, &TSelectOutfitDialogController::onPresetNameEntered);
    }
    m_renameDialog->setText(list->at(Index).toObject().value(QStringLiteral("name")).toString());
    m_renameDialog->dialog()->show();
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

/// CIP : a preset or a colour set.
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

/// CIP : a JS-owned OutfitColor or OutfitPreset, or null.
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
    if (game::TOutfitDialogStorage* storage = game::TOutfitDialogStorage::instance())
        storage->announcePresetsChanged();
}

}
