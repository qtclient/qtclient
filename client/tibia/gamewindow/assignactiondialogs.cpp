#include "assignactiondialogs.h"

#include "game/gameciptypes.h"
#include "gamewindow/gamewindowciptypes.h"

#include "gamewindow/actionbarcontroller.h"
#include "gamewindow/actionbarmanager.h"
#include "gamewindow/mousecursorcontroller.h"
#include "config/hotkeyoptions.h"
#include "input/gameactionhandlers.h"
#include "spells/spellinfostorage.h"

#include "container/containerciptypes.h"

#include <tibia/assets/thingtype.h>
#include <tibia/assets/things.h>
#include <QCoreApplication>
#include <QKeyEvent>
#include <tibia/game/gamesession.h>
#include <tibia/game/thingsession_forward.h>

#include <algorithm>
#include <set>

namespace tibia::gamewindow {

namespace {

using Setting = config::TActionSetting;
using ECastType = cip::tibia::qmlcomponents::enums::ESpellCastOptionalTargetType;
using Info = cip::tibia::qmlcomponents::TObjectAppearanceInstanceInfoQmlType;

enum ESpellRole
{
    EnumIdRole = Qt::UserRole + 1,
    NameRole,
    FormulaRawRole,
    IconUrlRole,
    IconUrlSmallRole,
    CastPossibleRole,
    ParameterHintsRole,
    IsCrossHairSpellRole,
    IsAimAtTargetConfigurableRole,
    GroupPrimaryIconUrlRole,
    MinimumCasterLevelRole,
};

QString buttonName(const int actionBarId, const int buttonId)
{
    return qtTrId("actionbar_button_identifier").arg(actionBarId).arg(buttonId);
}

ThingTypePtr itemType(const int objectId)
{
    return objectId > 0 ? tibia::game::thingType(uint16_t(objectId), ThingCategoryItem) : nullptr;
}

bool isEquipable(const ThingTypePtr& type)
{
    return type && (type->isCloth() || type->getClothSlot() > 0);
}

std::set<int> knownSpellIds()
{
    std::set<int> ids;
    for (const uint16_t id : tibia::game::localPlayerSpells())
        ids.insert(int(id));
    return ids;
}

}

TSpellListDataModel::TSpellListDataModel(QObject* parent)
    : cip::tibia::gamewindow::TSpellListDataModel(parent)
{
    rebuild();
}

void TSpellListDataModel::rebuild()
{
    beginResetModel();
    m_rows.clear();
    const QString filter = m_filterText.trimmed();
    const std::set<int> known = m_showOnlyKnown ? knownSpellIds() : std::set<int>{};
    for (const auto& [id, spell] : spells::TSpellInfoStorage::instance()->spells()) {
        if (spell.words.isEmpty())
            continue;
        if (m_showOnlyKnown && !known.count(id))
            continue;
        if (!filter.isEmpty() && !spell.name.contains(filter, Qt::CaseInsensitive) && !spell.words.contains(filter, Qt::CaseInsensitive))
            continue;
        m_rows.push_back(id);
    }
    std::sort(m_rows.begin(), m_rows.end(), [](const int a, const int b) {
        const spells::TSpellInfoStorage* storage = spells::TSpellInfoStorage::instance();
        return QString::localeAwareCompare(storage->spell(a)->name, storage->spell(b)->name) < 0;
    });
    endResetModel();
}

int TSpellListDataModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant TSpellListDataModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= int(m_rows.size()))
        return {};
    const spells::TSpellInfo* spell = spells::TSpellInfoStorage::instance()->spell(m_rows[size_t(index.row())]);
    if (!spell)
        return {};
    switch (role) {
    case EnumIdRole:
        return spell->id;
    case Qt::DisplayRole:
    case NameRole:
        return spell->name;
    case FormulaRawRole:
        return spell->parameterHints.isEmpty() ? spell->words : spell->words + QLatin1Char(' ') + spell->parameterHints;
    case IconUrlRole:
        return spells::TSpellInfoStorage::iconUrl(*spell);
    case IconUrlSmallRole:
        return QStringLiteral("image://spell-icons-20x20/%1").arg(spell->iconIndex);
    case CastPossibleRole: {
        const std::set<int> known = knownSpellIds();
        return known.empty() || known.count(spell->id) > 0;
    }
    case ParameterHintsRole:
        return spell->parameterHints;
    case IsCrossHairSpellRole:
        return spell->crossHairTarget;
    case IsAimAtTargetConfigurableRole:
        return spell->aimAtTarget;
    case GroupPrimaryIconUrlRole:
        return QStringLiteral("image://spellgroup-icons-20x20/%1").arg(std::max(0, spell->groupPrimary - 1));
    case MinimumCasterLevelRole:
        return spell->minimumLevel;
    default:
        return {};
    }
}

QHash<int, QByteArray> TSpellListDataModel::roleNames() const
{
    return {
        { EnumIdRole, "enumId" },
        { NameRole, "name" },
        { FormulaRawRole, "formulaRaw" },
        { IconUrlRole, "iconUrl" },
        { IconUrlSmallRole, "iconUrlSmall" },
        { CastPossibleRole, "castPossible" },
        { ParameterHintsRole, "parameterHints" },
        { IsCrossHairSpellRole, "isCrossHairSpell" },
        { IsAimAtTargetConfigurableRole, "isAimAtTargetConfigurable" },
        { GroupPrimaryIconUrlRole, "groupPrimaryIconUrl" },
        { MinimumCasterLevelRole, "minimumCasterLevel" },
    };
}

void TSpellListDataModel::setFilterText(const QString& text)
{
    if (text == m_filterText)
        return;
    m_filterText = text;
    rebuild();
    Q_EMIT filterTextChanged();
}

void TSpellListDataModel::setShowOnlyKnown(const bool showOnlyKnown)
{
    if (showOnlyKnown == m_showOnlyKnown)
        return;
    m_showOnlyKnown = showOnlyKnown;
    rebuild();
}

void TSpellListDataModel::onSpellDataModificationsApplied()
{
    rebuild();
}

int TSpellListDataModel::rowOf(const int spellId) const
{
    const auto it = std::find(m_rows.begin(), m_rows.end(), spellId);
    return it == m_rows.end() ? -1 : int(it - m_rows.begin());
}

void TAssignActionDialogState::bind(QObject* dialog, TActionBarController* bar, const int buttonId, const int multiActionIndex)
{
    this->bar = bar;
    this->multiActionIndex = multiActionIndex;
    auto* shared = new TAssignActionDialogControllerBase(dialog);
    shared->bindActionButton(bar, buttonId);
    actionBarId = shared->actionBarId();
    this->buttonId = shared->buttonId();
    buttonName = shared->buttonName();
    showActionBarTarget = shared->showActionBarTarget();
}

Setting TAssignActionDialogState::current() const
{
    return bar ? bar->settingAt(int(buttonId), multiActionIndex) : Setting{};
}

void TAssignActionDialogState::assign(const Setting& setting) const
{
    if (bar)
        bar->assign(int(buttonId), multiActionIndex, setting);
}

TAssignSpellActionDialogController::TAssignSpellActionDialogController(TActionBarController* bar, const int buttonId, const int multiActionIndex,
                                                                       const int preselectedSpellId, QObject* parent)
    : TDialogController(QStringLiteral("AssignSpellActionDialog.qml"), parent)
{
    m_assign.bind(this, bar, buttonId, multiActionIndex);
    m_actionBarId = m_assign.actionBarId;
    m_buttonId = m_assign.buttonId;
    m_buttonName = m_assign.buttonName;
    m_showActionBarTarget = m_assign.showActionBarTarget;
    connect(this, &cip::tibia::gamewindow::TAssignSpellActionDialogController::dialogClosed, this, &QObject::deleteLater);
    m_spellListModel = QVariant::fromValue<QObject*>(&m_model);
    m_spellListIndex = uint(-1);
    const Setting setting = m_assign.current();
    const spells::TSpellInfo* spell = preselectedSpellId ? spells::TSpellInfoStorage::instance()->spell(preselectedSpellId)
        : setting.kind == Setting::TEXT                  ? spells::TSpellInfoStorage::instance()->spellOfText(setting.text)
                                                         : nullptr;
    if (spell) {
        m_spellId = spell->id;
        m_spellListIndex = uint(m_model.rowOf(spell->id));
        if (!preselectedSpellId) {
            m_spellParameters = setting.text.trimmed().mid(spell->words.size()).trimmed();
            m_currentSpellCastType = ECastType(setting.spellCastType);
        }
    }
}

uint TAssignSpellActionDialogController::actionBarId() const { return m_assign.actionBarId; }
uint TAssignSpellActionDialogController::buttonId() const { return m_assign.buttonId; }
QString TAssignSpellActionDialogController::buttonName() const { return m_assign.buttonName; }
bool TAssignSpellActionDialogController::showActionBarTarget() const { return m_assign.showActionBarTarget; }
uint TAssignSpellActionDialogController::spellListIndex() const { return m_spellListIndex; }
QString TAssignSpellActionDialogController::spellParameters() const { return m_spellParameters; }
bool TAssignSpellActionDialogController::showOnlyKnownSpells() const { return m_showOnlyKnownSpells; }
ECastType TAssignSpellActionDialogController::currentSpellCastType() const { return m_currentSpellCastType; }
QVariant TAssignSpellActionDialogController::spellListModel() const { return m_spellListModel; }

void TAssignSpellActionDialogController::setShowOnlyKnownSpells(const bool v)
{
    if (::cip::unchanged(m_showOnlyKnownSpells, v))
        return;
    m_showOnlyKnownSpells = v;
    m_model.setShowOnlyKnown(v);
    m_spellListIndex = m_spellId ? uint(m_model.rowOf(m_spellId)) : uint(-1);
    Q_EMIT spellFilterChanged();
    Q_EMIT selectedSpellChanged();
}

void TAssignSpellActionDialogController::onOkClicked(const QString& Parameters, const ECastType CastType)
{
    onApplyClicked(Parameters, CastType);
    dialog()->close();
}

void TAssignSpellActionDialogController::onApplyClicked(const QString& Parameters, const ECastType CastType)
{
    const spells::TSpellInfo* spell = spells::TSpellInfoStorage::instance()->spell(m_spellId);
    if (!spell)
        return;
    Setting setting;
    setting.kind = Setting::TEXT;
    setting.text = Parameters.trimmed().isEmpty() ? spell->words : spell->words + QLatin1Char(' ') + Parameters.trimmed();
    setting.sendAutomatically = true;
    setting.spellCastType = int(CastType);
    m_assign.assign(setting);
}

void TAssignSpellActionDialogController::spellSelected(const uint SpellEnumId)
{
    if (int(SpellEnumId) == m_spellId)
        return;
    m_spellId = int(SpellEnumId);
    m_spellListIndex = uint(m_model.rowOf(m_spellId));
    Q_EMIT selectedSpellChanged();
}

void TAssignSpellActionDialogController::onCloseClicked()
{
    dialog()->close();
}

TAssignObjectActionDialogController::TAssignObjectActionDialogController(TActionBarController* bar, const int buttonId, const int multiActionIndex, QObject* parent)
    : TDialogController(QStringLiteral("AssignObjectActionDialog.qml"), parent)
{
    m_assign.bind(this, bar, buttonId, multiActionIndex);
    m_actionBarId = m_assign.actionBarId;
    m_buttonId = m_assign.buttonId;
    m_buttonName = m_assign.buttonName;
    m_showActionBarTarget = m_assign.showActionBarTarget;
    connect(this, &cip::tibia::gamewindow::TAssignObjectActionDialogController::dialogClosed, this, &QObject::deleteLater);
    m_clientSettingSmoothFiltering = true;
    const Setting setting = m_assign.current();
    if (setting.kind == Setting::OBJECT) {
        setObject(setting.objectId, setting.upgradeTier.value_or(0));
        if (setting.liquidType)
            m_liquidType = Info::ELiquidType(*setting.liquidType);
        m_useObjectType = uint(setting.useType);
        m_smartModeEnabled = setting.useEquipSmartMode;
    }
}

uint TAssignObjectActionDialogController::actionBarId() const { return m_assign.actionBarId; }
uint TAssignObjectActionDialogController::buttonId() const { return m_assign.buttonId; }
QString TAssignObjectActionDialogController::buttonName() const { return m_assign.buttonName; }
bool TAssignObjectActionDialogController::showActionBarTarget() const { return m_assign.showActionBarTarget; }
uint TAssignObjectActionDialogController::objectID() const { return m_objectID; }
uint TAssignObjectActionDialogController::objectCount() const { return m_objectCount; }
uint TAssignObjectActionDialogController::upgradeTier() const { return m_upgradeTier; }
Info::ELiquidType TAssignObjectActionDialogController::liquidType() const { return m_liquidType; }
Info::EHookDirection TAssignObjectActionDialogController::hookDirection() const { return m_hookDirection; }
uint TAssignObjectActionDialogController::useObjectType() const { return m_useObjectType; }
bool TAssignObjectActionDialogController::canBeEquiped() const { return m_canBeEquiped; }
bool TAssignObjectActionDialogController::canBeMultiUsed() const { return m_canBeMultiUsed; }
bool TAssignObjectActionDialogController::canHaveSmartMode() const { return m_canHaveSmartMode; }
bool TAssignObjectActionDialogController::smartModeEnabled() const { return m_smartModeEnabled; }
bool TAssignObjectActionDialogController::clientSettingSmoothFiltering() const { return m_clientSettingSmoothFiltering; }

void TAssignObjectActionDialogController::setObject(const int objectId, const int upgradeTier)
{
    const ThingTypePtr type = itemType(objectId);
    m_objectID = uint(objectId);
    m_upgradeTier = uint(upgradeTier);
    m_objectCount = 0;
    m_liquidType = Info::ELiquidType::EMPTY;
    m_hookDirection = Info::EHookDirection::NONE;
    m_canBeMultiUsed = type && type->isMultiUse();
    m_canBeEquiped = isEquipable(type);
    m_canHaveSmartMode = m_canBeEquiped;
    m_useObjectType = uint(TActionBarManager::defaultUseType(objectId));
    m_clientSettingSmoothFiltering = true;
    Q_EMIT objectAppearanceChanged();
    Q_EMIT changed();
}

void TAssignObjectActionDialogController::onOkClicked(const uint UseObjectTypeEnumId, const bool UseEquipSmartModeEnabled)
{
    onApplyClicked(UseObjectTypeEnumId, UseEquipSmartModeEnabled);
    dialog()->close();
}

void TAssignObjectActionDialogController::onApplyClicked(const uint UseObjectTypeEnumId, const bool UseEquipSmartModeEnabled)
{
    if (!m_objectID)
        return;
    Setting setting;
    setting.kind = Setting::OBJECT;
    setting.objectId = int(m_objectID);
    setting.useType = config::EUseType(std::min<uint>(UseObjectTypeEnumId, uint(config::EUseType::UseAtMouseCursor)));
    if (m_upgradeTier)
        setting.upgradeTier = int(m_upgradeTier);
    if (m_liquidType != Info::ELiquidType::EMPTY)
        setting.liquidType = int(m_liquidType);
    setting.useEquipSmartMode = setting.useType == config::EUseType::Equip && UseEquipSmartModeEnabled;
    m_useObjectType = UseObjectTypeEnumId;
    m_smartModeEnabled = setting.useEquipSmartMode;
    m_assign.assign(setting);
    Q_EMIT changed();
}

void TAssignObjectActionDialogController::handleGameAction()
{
    // CIP moc shared_ptr dispatch; qtclient routes via startTargetSelection.
}

void TAssignObjectActionDialogController::startTargetSelection()
{
    if (TMouseCursorController* cursor = TMouseCursorController::instance())
        cursor->setTargetSelection(true);
    dialog()->hide();
    QPointer<TAssignObjectActionDialogController> self(this);
    input::startTargetSelection([self](const input::TGameActionPtr& target) {
        if (TMouseCursorController* cursor = TMouseCursorController::instance())
            cursor->setTargetSelection(false);
        if (!self)
            return;
        if (target && target->objectClientId) {
            uint8_t tier = 0;
            if (target->position.x == UINT16_MAX) {
                if (target->position.y & 0x40) {
                    if (container::TContainerStorage* storage = container::TContainerStorage::instance()) {
                        if (const container::TStoredContainer* row = storage->containerAt(target->position.y & 0x3F))
                            tier = storage->slotAt(row->id, static_cast<int>(row->firstIndex) + target->stackPosition).tier;
                    }
                } else if (container::TInventoryContainer* inventory = container::TInventoryContainer::instance()) {
                    tier = inventory->slotAt(target->position.y).tier;
                }
            }
            self->setObject(target->objectClientId, tier);
        }
        self->dialog()->show();
    });
}

void TAssignObjectActionDialogController::onCloseClicked()
{
    dialog()->close();
}

TAssignTextActionDialogController::TAssignTextActionDialogController(TActionBarController* bar, const int buttonId, const int multiActionIndex, QObject* parent)
    : TDialogController(QStringLiteral("AssignTextActionDialog.qml"), parent)
{
    m_assign.bind(this, bar, buttonId, multiActionIndex);
    m_actionBarId = m_assign.actionBarId;
    m_buttonId = m_assign.buttonId;
    m_buttonName = m_assign.buttonName;
    m_showActionBarTarget = m_assign.showActionBarTarget;
    connect(this, &cip::tibia::gamewindow::TAssignTextActionDialogController::dialogClosed, this, &QObject::deleteLater);
    const Setting setting = m_assign.current();
    m_sendAutomatically = true;
    if (setting.kind == Setting::TEXT) {
        m_text = setting.text;
        m_sendAutomatically = setting.sendAutomatically;
    }
}

uint TAssignTextActionDialogController::actionBarId() const { return m_assign.actionBarId; }
uint TAssignTextActionDialogController::buttonId() const { return m_assign.buttonId; }
QString TAssignTextActionDialogController::buttonName() const { return m_assign.buttonName; }
bool TAssignTextActionDialogController::showActionBarTarget() const { return m_assign.showActionBarTarget; }
QString TAssignTextActionDialogController::text() const { return m_text; }
bool TAssignTextActionDialogController::sendAutomatically() const { return m_sendAutomatically; }

void TAssignTextActionDialogController::onOkClicked(const QString& Text, const bool SendAutomatically)
{
    onApplyClicked(Text, SendAutomatically);
    dialog()->close();
}

void TAssignTextActionDialogController::onApplyClicked(const QString& Text, const bool SendAutomatically)
{
    if (Text.isEmpty())
        return;
    Setting setting;
    setting.kind = Setting::TEXT;
    setting.text = Text;
    setting.sendAutomatically = SendAutomatically;
    m_text = Text;
    m_sendAutomatically = SendAutomatically;
    m_assign.assign(setting);
    Q_EMIT changed();
}

void TAssignTextActionDialogController::onCloseClicked()
{
    dialog()->close();
}

TAssignPassiveAbilityActionDialogController::TAssignPassiveAbilityActionDialogController(TActionBarController* bar, const int buttonId, QObject* parent)
    : TDialogController(QStringLiteral("AssignPassiveAbilityActionDialog.qml"), parent)
{
    m_assign.bind(this, bar, buttonId, -1);
    m_actionBarId = m_assign.actionBarId;
    m_buttonId = m_assign.buttonId;
    m_buttonName = m_assign.buttonName;
    m_showActionBarTarget = m_assign.showActionBarTarget;
    connect(this, &cip::tibia::gamewindow::TAssignPassiveAbilityActionDialogController::dialogClosed, this, &QObject::deleteLater);
    m_passiveAbilityListIndex = uint(-1);
    m_model = new TPassiveAbilityListDataModel(this);
    if (game::TPassiveAbilityInfoStorage* storage = game::TPassiveAbilityInfoStorage::instance()) {
        // Seed from storage's live list when the CipSoft assign dialog opens.
        if (gamewindow::TPassiveAbilityListDataModel* shared = storage->listModel()) {
            std::vector<TPassiveAbilityListDataModel::Entry> copy;
            for (int i = 0; i < shared->rowCount(); ++i) {
                TPassiveAbilityListDataModel::Entry entry;
                entry.id = shared->data(shared->index(i, 0), TPassiveAbilityListDataModel::IdRole).toInt();
                entry.name = shared->data(shared->index(i, 0), TPassiveAbilityListDataModel::NameRole).toString();
                entry.iconUrl = shared->data(shared->index(i, 0), TPassiveAbilityListDataModel::IconUrlRole).toString();
                entry.usableByCurrentVocation =
                    shared->data(shared->index(i, 0), TPassiveAbilityListDataModel::UsableRole).toBool();
                copy.push_back(std::move(entry));
            }
            m_model->setEntries(std::move(copy));
        }
    }
    m_proxy = new TPassiveAbilityListDataModelSortFilterProxy(this);
    m_proxy->setSourceModel(m_model);
    m_passiveAbilityListModel = QVariant::fromValue(static_cast<QObject*>(m_proxy));
    const Setting setting = m_assign.current();
    if (setting.kind == Setting::PASSIVE_ABILITY)
        m_passiveAbility = setting.passiveAbility;
    refreshSelectedPassive();
}

uint TAssignPassiveAbilityActionDialogController::actionBarId() const { return m_assign.actionBarId; }
uint TAssignPassiveAbilityActionDialogController::buttonId() const { return m_assign.buttonId; }
QString TAssignPassiveAbilityActionDialogController::buttonName() const { return m_assign.buttonName; }
bool TAssignPassiveAbilityActionDialogController::showActionBarTarget() const { return m_assign.showActionBarTarget; }
uint TAssignPassiveAbilityActionDialogController::passiveAbilityListIndex() const { return m_passiveAbilityListIndex; }
QString TAssignPassiveAbilityActionDialogController::passiveAbilityName() const { return m_passiveAbilityName; }
QString TAssignPassiveAbilityActionDialogController::passiveAbilityIconUrl() const { return m_passiveAbilityIconUrl; }
QVariant TAssignPassiveAbilityActionDialogController::passiveAbilityListModel() const { return m_passiveAbilityListModel; }

void TAssignPassiveAbilityActionDialogController::refreshSelectedPassive()
{
    m_passiveAbilityListIndex = uint(-1);
    m_passiveAbilityName.clear();
    m_passiveAbilityIconUrl.clear();
    if (!m_model || !m_passiveAbility)
        return;
    for (int i = 0; i < m_model->rowCount(); ++i) {
        const QModelIndex idx = m_model->index(i, 0);
        if (m_model->data(idx, TPassiveAbilityListDataModel::IdRole).toInt() != m_passiveAbility)
            continue;
        m_passiveAbilityListIndex = uint(i);
        m_passiveAbilityName = m_model->data(idx, TPassiveAbilityListDataModel::NameRole).toString();
        m_passiveAbilityIconUrl = m_model->data(idx, TPassiveAbilityListDataModel::IconUrlRole).toString();
        if (m_passiveAbilityIconUrl.isEmpty())
            m_passiveAbilityIconUrl = QStringLiteral("image://passiveability-icons-32x32/%1").arg(m_passiveAbility);
        break;
    }
    if (m_passiveAbilityIconUrl.isEmpty() && m_passiveAbility)
        m_passiveAbilityIconUrl = QStringLiteral("image://passiveability-icons-32x32/%1").arg(m_passiveAbility);
}

void TAssignPassiveAbilityActionDialogController::onOkClicked()
{
    onApplyClicked();
    dialog()->close();
}

void TAssignPassiveAbilityActionDialogController::onApplyClicked()
{
    if (!m_passiveAbility)
        return;
    Setting setting;
    setting.kind = Setting::PASSIVE_ABILITY;
    setting.passiveAbility = m_passiveAbility;
    m_assign.assign(setting);
}

void TAssignPassiveAbilityActionDialogController::passiveAbilitySelected(const uint PassiveAbilityEnumId)
{
    m_passiveAbility = int(PassiveAbilityEnumId);
    refreshSelectedPassive();
    Q_EMIT selectedPassiveAbilityChanged();
}

void TAssignPassiveAbilityActionDialogController::onCloseClicked()
{
    dialog()->close();
}

TRecordKeyboardShortcutDialogController::TRecordKeyboardShortcutDialogController(const int actionBarId, const int buttonId, const bool chatModeOn, QObject* parent)
    : TDialogController(QStringLiteral("RecordKeyboardShortcutDialog.qml"), parent)
    , m_actionBarId(actionBarId)
    , m_buttonId(buttonId)
    , m_bindOnOk(true)
    , m_actionName(config::THotkeyOptions::triggerActionButtonName(actionBarId, buttonId))
{
    m_chatModeOn = chatModeOn;
    m_actionDescription = buttonName(actionBarId, buttonId);
    m_keys = config::THotkeyOptions::instance()->buttonKeys(actionBarId, buttonId, chatModeOn);
    m_keySequence = m_keys.toString(QKeySequence::NativeText);
    connect(this, &cip::tibia::gamewindow::TRecordKeyboardShortcutDialogController::dialogClosed, this, &QObject::deleteLater);
    QCoreApplication::instance()->installEventFilter(this);
}

TRecordKeyboardShortcutDialogController::TRecordKeyboardShortcutDialogController(const QString& description, const QString& actionName,
                                                                                 const bool chatModeOn, const QKeySequence& current,
                                                                                 QObject* parent)
    : TDialogController(QStringLiteral("RecordKeyboardShortcutDialog.qml"), parent)
    , m_bindOnOk(false)
    , m_actionName(actionName)
{
    m_chatModeOn = chatModeOn;
    m_actionDescription = description;
    m_keys = current;
    m_keySequence = m_keys.toString(QKeySequence::NativeText);
    connect(this, &cip::tibia::gamewindow::TRecordKeyboardShortcutDialogController::dialogClosed, this, &QObject::deleteLater);
    QCoreApplication::instance()->installEventFilter(this);
    // Seed occupied/hardcoded flags for the current binding (CIP showDialog path).
    onKeySequenceEntered(current);
}

QString TRecordKeyboardShortcutDialogController::keySequence() const { return m_keySequence; }
bool TRecordKeyboardShortcutDialogController::keySequenceOccupied() const { return m_keySequenceOccupied; }
bool TRecordKeyboardShortcutDialogController::keySequenceHardcoded() const { return m_keySequenceHardcoded; }
QString TRecordKeyboardShortcutDialogController::actionDescription() const { return m_actionDescription; }
bool TRecordKeyboardShortcutDialogController::chatModeOn() const { return m_chatModeOn; }
bool TRecordKeyboardShortcutDialogController::onlySingleKey() const { return m_onlySingleKey; }

void TRecordKeyboardShortcutDialogController::showDialog(const bool ChatModeOn, const bool OnlySingleKey, const QKeySequence& CurrentKeySequence,
                                                         const QString& ActionDescription, const std::set<QKeySequence>& OccupiedKeys)
{
    m_chatModeOn = ChatModeOn;
    m_onlySingleKey = OnlySingleKey;
    m_actionDescription = ActionDescription;
    m_occupiedKeys = QSet<QKeySequence>(OccupiedKeys.begin(), OccupiedKeys.end());
    onKeySequenceEntered(CurrentKeySequence);
    dialog()->show();
}

TRecordKeyboardShortcutDialogController::~TRecordKeyboardShortcutDialogController()
{
    if (QCoreApplication* app = QCoreApplication::instance())
        app->removeEventFilter(this);
}

bool TRecordKeyboardShortcutDialogController::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() != QEvent::KeyPress || !dialog()->isShown())
        return QObject::eventFilter(watched, event);
    const auto* key = static_cast<QKeyEvent*>(event);
    switch (key->key()) {
    case Qt::Key_Shift:
    case Qt::Key_Control:
    case Qt::Key_Meta:
    case Qt::Key_Alt:
    case Qt::Key_AltGr:
    case Qt::Key_CapsLock:
    case Qt::Key_unknown:
        return true;
    default:
        break;
    }
    if (!key->isAutoRepeat())
        // CIP record path: same as — modifiers | key.
        onKeySequenceEntered(QKeySequence(key->modifiers() | Qt::Key(key->key())));
    return true;
}

void TRecordKeyboardShortcutDialogController::onKeySequenceEntered(const QKeySequence& NewKeySequence)
{
    m_keys = NewKeySequence;
    m_keySequence = NewKeySequence.toString(QKeySequence::NativeText);
    m_keySequenceHardcoded = NewKeySequence == QKeySequence(Qt::Key_Escape);
    const Setting* bound = config::THotkeyOptions::instance()->binding(NewKeySequence, m_chatModeOn);
    const bool occupiedByOtherBinding = bound && !(bound->kind == Setting::ACTION && bound->action == m_actionName);
    m_keySequenceOccupied = occupiedByOtherBinding || m_occupiedKeys.contains(NewKeySequence);
    Q_EMIT keySequenceChanged(NewKeySequence);
}

void TRecordKeyboardShortcutDialogController::onOkButtonClicked()
{
    if (m_keySequenceHardcoded)
        return;
    if (m_bindOnOk)
        config::THotkeyOptions::instance()->setButtonKeys(m_actionBarId, m_buttonId, m_chatModeOn, m_keys);
    Q_EMIT keySequenceSelected(m_keys);
    dialog()->close();
}

void TRecordKeyboardShortcutDialogController::onClearButtonClicked()
{
    m_keys = {};
    m_keySequence.clear();
    m_keySequenceOccupied = false;
    m_keySequenceHardcoded = false;
    Q_EMIT keySequenceChanged(m_keys);
    // Commit the empty binding (CIP keeps Clear + OK separate; qtclient applies so
    // Clear alone removes the hotkey without a second confirm).
    onOkButtonClicked();
}

void TRecordKeyboardShortcutDialogController::onCancelButtonClicked()
{
    dialog()->close();
}

void TRecordKeyboardShortcutDialogController::requestClose()
{
    dialog()->close();
}

}
