#include "config/tibiahotkeyactionsettingqmlwrapper.h"

#include "gamewindow/actionbarcontroller.h"
#include "input/gameaction.h"
#include "input/keyboardshortcuthandler.h"
#include "qmlcomponents/appearanceinstancerenderer.h"

namespace tibia::config {

using ObjectInfo = qmlcomponents::TObjectAppearanceInstanceInfoQmlType;

QString hotkeyTextForDisplay(QString text)
{
    text.replace(QStringLiteral("Backtab"), QStringLiteral("Tab"), Qt::CaseSensitive);
    return text;
}

QString hotkeyActionSettingDescription(const TActionSetting& setting)
{
    if (!setting.text.isEmpty())
        return setting.text;
    if (setting.action.isEmpty() && setting.objectId == 0 && setting.kind != TActionSetting::PASSIVE_ABILITY) {
        if (!setting.multiActions.empty())
            return {};
        return qtTrId("hotkey_empty");
    }
    if (setting.objectId == 0) {
        // A passive ability has no text.
        if (setting.action.isEmpty())
            return {};
        for (const auto& product : input::TKeyboardShortcutHandler::productActions()) {
            if (product.name == setting.action)
                return product.description.isEmpty() ? setting.action : product.description;
        }
        const QString id = QStringLiteral("label_gameactionidentifier_") + setting.action;
        // CIP map::at throws only for an unknown key. Movement names (GoNorth, …)
        // stay in the static table with a null action — the movement interpreter
        // owns them — so a missing IGameAction still gets a translated label.
        const input::IGameActionPtr action = input::TKeyboardShortcutHandler::staticAction(setting.action);
        QString text;
        if (const auto* button = input::gameActionAs<input::TGameActionActionButtonTriggered>(action)) {
            QString side;
            switch (gamewindow::TActionBarGroup::positionOfBar(button->actionBarId())) {
            case gamewindow::EActionBarPosition::ActionBarPositionBottom: side = qtTrId("bottom"); break;
            case gamewindow::EActionBarPosition::ActionBarPositionLeft: side = qtTrId("left"); break;
            case gamewindow::EActionBarPosition::ActionBarPositionRight: side = qtTrId("right"); break;
            }
            text = side + QLatin1Char(' ') + qtTrId("actionbar") + QStringLiteral(": ")
                 + qtTrId("actionbar_button_identifier")
                       .arg(button->actionBarId())
                       .arg(button->actionButtonId(), 2, 10, QLatin1Char('0'));
        } else {
            text = qtTrId(id.toLocal8Bit().constData());
        }
        return text == id ? setting.action : text;
    }
    switch (setting.useType) {
    case EUseType::Use: return qtTrId("hotkey_use_object");
    case EUseType::UseOnYourself: return qtTrId("hotkey_use_object_on_yourself");
    case EUseType::UseOnTarget: return qtTrId("hotkey_use_object_on_target");
    case EUseType::SelectUseTarget: return qtTrId("hotkey_use_object_with_crosshair");
    case EUseType::Equip: return qtTrId("hotkey_equip_object");
    case EUseType::UseAtMouseCursor: return qtTrId("hotkey_use_object_at_mousecursor");
    }
    return QStringLiteral("invalid use object type");
}

TTibiaHotkeyActionSettingQmlWrapper::TTibiaHotkeyActionSettingQmlWrapper(const QKeySequence& firstKeySequence, QObject* parent)
    : cip::tibia::config::TTibiaHotkeyActionSettingQmlWrapper(parent)
    , m_firstKeySequence(firstKeySequence)
{
}

QString TTibiaHotkeyActionSettingQmlWrapper::firstHotkeyText() const
{
    return hotkeyTextForDisplay(m_firstKeySequence.toString(QKeySequence::NativeText));
}

QString TTibiaHotkeyActionSettingQmlWrapper::secondHotkeyText() const
{
    return hotkeyTextForDisplay(m_secondKeySequence.toString(QKeySequence::NativeText));
}

QString TTibiaHotkeyActionSettingQmlWrapper::chatText() const
{
    return m_hotkeyActionSetting.text;
}

void TTibiaHotkeyActionSettingQmlWrapper::setChatText(const QString& text)
{
    const TActionSetting previous = m_hotkeyActionSetting;
    m_hotkeyActionSetting.setChatText(text);
    const TActionSetting& current = m_hotkeyActionSetting;
    if (previous.text != current.text || previous.objectId != current.objectId || previous.liquidType != current.liquidType
        || previous.multiActions != current.multiActions)
        Q_EMIT changed();
}

bool TTibiaHotkeyActionSettingQmlWrapper::isEmpty() const
{
    return m_hotkeyActionSetting.isEmpty();
}

bool TTibiaHotkeyActionSettingQmlWrapper::isChatText() const
{
    return !m_hotkeyActionSetting.text.isEmpty();
}

bool TTibiaHotkeyActionSettingQmlWrapper::isUseObject() const
{
    return m_hotkeyActionSetting.objectId != 0;
}

bool TTibiaHotkeyActionSettingQmlWrapper::isUseWithObject() const
{
    return m_hotkeyActionSetting.useType != EUseType::Use;
}

bool TTibiaHotkeyActionSettingQmlWrapper::isStaticAction() const
{
    return !m_hotkeyActionSetting.action.isEmpty();
}

int TTibiaHotkeyActionSettingQmlWrapper::useObjectType() const
{
    const int type = int(m_hotkeyActionSetting.useType);
    return unsigned(type - 1) > 4 ? 0 : type;
}

void TTibiaHotkeyActionSettingQmlWrapper::setUseObjectType(const int type)
{
    const EUseType useType = unsigned(type - 1) < 5 ? EUseType(type) : EUseType::Use;
    if (m_hotkeyActionSetting.useType == useType)
        return;
    m_hotkeyActionSetting.setUseType(useType);
    Q_EMIT changed();
}

bool TTibiaHotkeyActionSettingQmlWrapper::canHaveSmartMode() const
{
    return m_canHaveSmartModeFlag;
}

bool TTibiaHotkeyActionSettingQmlWrapper::smartModeEnabled() const
{
    return m_hotkeyActionSetting.useEquipSmartMode;
}

void TTibiaHotkeyActionSettingQmlWrapper::setSmartModeEnabled(const bool enabled)
{
    if (m_hotkeyActionSetting.useEquipSmartMode == enabled)
        return;
    m_hotkeyActionSetting.setUseEquipSmartMode(enabled);
    Q_EMIT changed();
}

QString TTibiaHotkeyActionSettingQmlWrapper::descriptionText() const
{
    return hotkeyActionSettingDescription(m_hotkeyActionSetting);
}

QColor TTibiaHotkeyActionSettingQmlWrapper::descriptionColor() const
{
    const TActionSetting& setting = m_hotkeyActionSetting;
    if (setting.isEmpty())
        return QColor(0xff, 0x00, 0x00);
    if (!setting.text.isEmpty())
        return setting.sendAutomatically ? QColor(0xff, 0xff, 0xff) : QColor(0xaf, 0xaf, 0xaf);
    if (setting.objectId != 0) {
        switch (setting.useType) {
        case EUseType::Use: return QColor(0xb0, 0xb0, 0xff);
        case EUseType::UseOnYourself: return QColor(0xb0, 0xff, 0xb0);
        case EUseType::UseOnTarget: return QColor(0xff, 0xb0, 0xb0);
        case EUseType::SelectUseTarget:
        case EUseType::UseAtMouseCursor: return QColor(0xc8, 0x7d, 0x7d);
        case EUseType::Equip: return QColor(0xbf, 0xbf, 0x00);
        }
        return QColor(0xff, 0x00, 0x00);
    }
    if (!setting.action.isEmpty())
        return QColor(0xc0, 0xc0, 0xc0);
    return {};
}

bool TTibiaHotkeyActionSettingQmlWrapper::sendAutomatically() const
{
    return m_hotkeyActionSetting.sendAutomatically;
}

void TTibiaHotkeyActionSettingQmlWrapper::setSendAutomatically(const bool enabled)
{
    const bool previous = m_hotkeyActionSetting.sendAutomatically;
    m_hotkeyActionSetting.setSendAutomatically(enabled);
    if (previous != enabled)
        Q_EMIT changed();
}

uint TTibiaHotkeyActionSettingQmlWrapper::objectID() const
{
    return ObjectInfo::getQmlTypeId(m_objectAppearanceInstance);
}

uint TTibiaHotkeyActionSettingQmlWrapper::objectCount() const
{
    return ObjectInfo::getQmlCumulativeCount(m_objectAppearanceInstance);
}

cip::tibia::qmlcomponents::TObjectAppearanceInstanceInfoQmlType::ELiquidType TTibiaHotkeyActionSettingQmlWrapper::liquidType() const
{
    return ObjectInfo::getQmlLiquidType(m_objectAppearanceInstance);
}

cip::tibia::qmlcomponents::TObjectAppearanceInstanceInfoQmlType::EHookDirection TTibiaHotkeyActionSettingQmlWrapper::hookDirection() const
{
    return ObjectInfo::getQmlHookDirection(m_objectAppearanceInstance);
}

}
