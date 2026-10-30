#include "assignactiondialogs.h"

#include "game/gameciptypes.h"
#include "gamewindow/gamewindowciptypes.h"

#include "gamewindow/actionbarcontroller.h"
#include "gamewindow/actionbarmanager.h"
#include "gamewindow/mousecursorcontroller.h"
#include "config/hotkeyoptions.h"
#include "input/gameactionhandlers.h"
#include "spells/spellinfostorage.h"

#include <client/item.h>
#include <tibia/assets/thingtype.h>
#include <tibia/assets/things.h>

#include <QCoreApplication>
#include <QKeyEvent>

namespace tibia::gamewindow {

namespace {

using Setting = config::TActionSetting;
using ECastType = cip::tibia::qmlcomponents::enums::ESpellCastOptionalTargetType;

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
    GroupPrimaryIconUrlRole,
    MinimumCasterLevelRole,
};

QString buttonName(const int actionBarId, const int buttonId)
{
    return qtTrId("actionbar_button_identifier").arg(actionBarId).arg(buttonId);
}

ThingTypePtr itemType(const int objectId)
{
    return objectId > 0 ? g_things.getThingType(uint16_t(objectId), ThingCategoryItem) : nullptr;
}

bool isEquipable(const ThingTypePtr& type)
{
    return type && (type->isCloth() || type->getClothSlot() > 0);
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
    for (const auto& [id, spell] : spells::TSpellInfoStorage::instance()->spells()) {
        if (spell.words.isEmpty())
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
    case CastPossibleRole:
        return true;
    case ParameterHintsRole:
        return spell->parameterHints;
    case IsCrossHairSpellRole:
        return spell->crossHairTarget;
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

int TSpellListDataModel::rowOf(const int spellId) const
{
    const auto it = std::find(m_rows.begin(), m_rows.end(), spellId);
    return it == m_rows.end() ? -1 : int(it - m_rows.begin());
}

template <class Base>
TAssignActionDialog<Base>::TAssignActionDialog(const QString& qmlFile, TActionBarController* bar, const int buttonId, const int multiActionIndex, QObject* parent)
    : TDialogController<Base>(qmlFile, parent)
    , m_bar(bar)
    , m_multiActionIndex(multiActionIndex)
{
    auto* shared = new TAssignActionDialogControllerBase(this);
    shared->bindActionButton(bar, buttonId);
    this->m_actionBarId = shared->actionBarId();
    this->m_buttonId = shared->buttonId();
    this->m_buttonName = shared->buttonName();
    this->m_showActionBarTarget = shared->showActionBarTarget();
    QObject::connect(this, &Base::dialogClosed, this, &QObject::deleteLater);
}

template <class Base>
Setting TAssignActionDialog<Base>::current() const
{
    return m_bar ? m_bar->settingAt(int(this->m_buttonId), m_multiActionIndex) : Setting{};
}

template <class Base>
void TAssignActionDialog<Base>::assign(const Setting& setting)
{
    if (m_bar)
        m_bar->assign(int(this->m_buttonId), m_multiActionIndex, setting);
}

TAssignSpellActionDialogController::TAssignSpellActionDialogController(TActionBarController* bar, const int buttonId, const int multiActionIndex,
                                                                       const int preselectedSpellId, QObject* parent)
    : TAssignActionDialog(QStringLiteral("AssignSpellActionDialog.qml"), bar, buttonId, multiActionIndex, parent)
{
    m_spellListModel = QVariant::fromValue<QObject*>(&m_model);
    m_spellListIndex = uint(-1);
    const Setting setting = current();
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
    assign(setting);
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
    : TAssignActionDialog(QStringLiteral("AssignObjectActionDialog.qml"), bar, buttonId, multiActionIndex, parent)
{
    const Setting setting = current();
    if (setting.kind == Setting::OBJECT) {
        setObject(setting.objectId, setting.upgradeTier.value_or(0));
        m_useObjectType = uint(setting.useType);
        m_smartModeEnabled = setting.useEquipSmartMode;
    }
}

void TAssignObjectActionDialogController::setObject(const int objectId, const int upgradeTier)
{
    const ThingTypePtr type = itemType(objectId);
    m_objectID = uint(objectId);
    m_upgradeTier = uint(upgradeTier);
    m_objectCount = 0;
    m_canBeMultiUsed = type && type->isMultiUse();
    m_canBeEquiped = isEquipable(type);
    m_canHaveSmartMode = m_canBeEquiped;
    m_useObjectType = uint(TActionBarManager::defaultUseType(objectId));
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
    setting.useEquipSmartMode = setting.useType == config::EUseType::Equip && UseEquipSmartModeEnabled;
    assign(setting);
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
        if (target && target->object && target->object->isItem()) {
            const ItemPtr item = target->object->static_self_cast<Item>();
            self->setObject(item->getId(), item->getTier());
        }
        self->dialog()->show();
    });
}

void TAssignObjectActionDialogController::onCloseClicked()
{
    dialog()->close();
}

TAssignTextActionDialogController::TAssignTextActionDialogController(TActionBarController* bar, const int buttonId, const int multiActionIndex, QObject* parent)
    : TAssignActionDialog(QStringLiteral("AssignTextActionDialog.qml"), bar, buttonId, multiActionIndex, parent)
{
    const Setting setting = current();
    m_sendAutomatically = true;
    if (setting.kind == Setting::TEXT) {
        m_text = setting.text;
        m_sendAutomatically = setting.sendAutomatically;
    }
}

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
    assign(setting);
    Q_EMIT changed();
}

void TAssignTextActionDialogController::onCloseClicked()
{
    dialog()->close();
}

TAssignPassiveAbilityActionDialogController::TAssignPassiveAbilityActionDialogController(TActionBarController* bar, const int buttonId, QObject* parent)
    : TAssignActionDialog(QStringLiteral("AssignPassiveAbilityActionDialog.qml"), bar, buttonId, -1, parent)
{
    m_passiveAbilityListIndex = uint(-1);
    auto* model = new TPassiveAbilityListDataModel(this);
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
            model->setEntries(std::move(copy));
        }
    }
    auto* proxy = new TPassiveAbilityListDataModelSortFilterProxy(this);
    proxy->setSourceModel(model);
    m_passiveAbilityListModel = QVariant::fromValue(static_cast<QObject*>(proxy));
    const Setting setting = current();
    if (setting.kind == Setting::PASSIVE_ABILITY)
        m_passiveAbility = setting.passiveAbility;
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
    assign(setting);
}

void TAssignPassiveAbilityActionDialogController::passiveAbilitySelected(const uint PassiveAbilityEnumId)
{
    m_passiveAbility = int(PassiveAbilityEnumId);
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

TRecordKeyboardShortcutDialogController::TRecordKeyboardShortcutDialogController(const QString& description, const bool chatModeOn,
                                                                                 const QKeySequence& current, QObject* parent)
    : TDialogController(QStringLiteral("RecordKeyboardShortcutDialog.qml"), parent)
    , m_bindOnOk(false)
{
    m_chatModeOn = chatModeOn;
    m_actionDescription = description;
    m_keys = current;
    m_keySequence = m_keys.toString(QKeySequence::NativeText);
    connect(this, &cip::tibia::gamewindow::TRecordKeyboardShortcutDialogController::dialogClosed, this, &QObject::deleteLater);
    QCoreApplication::instance()->installEventFilter(this);
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
        // CIP record path: same as 0x100e85b80 — modifiers() | key().
        onKeySequenceEntered(QKeySequence(key->modifiers() | Qt::Key(key->key())));
    return true;
}

void TRecordKeyboardShortcutDialogController::onKeySequenceEntered(const QKeySequence& NewKeySequence)
{
    m_keys = NewKeySequence;
    m_keySequence = NewKeySequence.toString(QKeySequence::NativeText);
    m_keySequenceHardcoded = NewKeySequence == QKeySequence(Qt::Key_Escape);
    const Setting* bound = config::THotkeyOptions::instance()->binding(NewKeySequence, m_chatModeOn);
    m_keySequenceOccupied = bound && !(bound->kind == Setting::ACTION && bound->action == m_actionName);
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
