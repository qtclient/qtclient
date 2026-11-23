#pragma once

#include "appearances/appearanceinstance.h"
#include "config/hotkeyoptions.h"

#include <cip_gen.h>

#include <QColor>
#include <QKeySequence>
#include <QString>

namespace tibia {
namespace config {

/// A key sequence's text as the hotkeys UI shows it: "Backtab" reads "Tab".
QString hotkeyTextForDisplay(QString text);

/// A setting's text in the hotkeys page. A chat text shows itself; an empty
/// setting "hotkey_empty"; a static action its translated identifier (its
/// name when untranslated), an action button its bar's side, bar and button;
/// an object its use type. qtclient: a pack action its description.
QString hotkeyActionSettingDescription(const TActionSetting& setting);

/// One row of OptionsPageHotkeys.qml: a hotkey action setting, the picture of
/// its object and its two keys.
class TTibiaHotkeyActionSettingQmlWrapper final : public cip::tibia::config::TTibiaHotkeyActionSettingQmlWrapper
{
    Q_OBJECT
public:
    /// A row with an empty setting, `firstKeySequence` and no second key; it
    /// cannot have smart mode and is visible.
    explicit TTibiaHotkeyActionSettingQmlWrapper(const QKeySequence& firstKeySequence, QObject* parent = nullptr);

    QString firstHotkeyText() const override;
    QString secondHotkeyText() const override;
    QString chatText() const override;
    /// Changed when the setting changes.
    void setChatText(const QString& text) override;
    bool isEmpty() const override;
    bool isChatText() const override;
    bool isUseObject() const override;
    /// Any use type but plain use.
    bool isUseWithObject() const override;
    bool isStaticAction() const override;
    /// The use type; 0 for one out of range.
    int useObjectType() const override;
    /// A use type out of range is plain use; changed when it differs.
    void setUseObjectType(int type) override;
    /// Off: nothing turns it on.
    bool canHaveSmartMode() const override;
    bool smartModeEnabled() const override;
    void setSmartModeEnabled(bool enabled) override;
    QString descriptionText() const override;
    /// Red when empty; a chat text white when sent automatically, else grey;
    /// an object by its use type; a static action light grey; invalid otherwise.
    QColor descriptionColor() const override;
    bool sendAutomatically() const override;
    /// Changed when the setting changes.
    void setSendAutomatically(bool enabled) override;
    uint objectID() const override;
    uint objectCount() const override;
    cip::tibia::qmlcomponents::TObjectAppearanceInstanceInfoQmlType::ELiquidType liquidType() const override;
    cip::tibia::qmlcomponents::TObjectAppearanceInstanceInfoQmlType::EHookDirection hookDirection() const override;

    const TActionSetting& hotkeyActionSetting() const { return m_hotkeyActionSetting; }
    TActionSetting& hotkeyActionSetting() { return m_hotkeyActionSetting; }
    const appearances::TAppearanceInstancePtr& objectAppearanceInstance() const { return m_objectAppearanceInstance; }
    void setObjectAppearanceInstance(appearances::TAppearanceInstancePtr instance) { m_objectAppearanceInstance = std::move(instance); }
    const QKeySequence& firstKeySequence() const { return m_firstKeySequence; }
    void setFirstKeySequence(const QKeySequence& keys) { m_firstKeySequence = keys; }
    const QKeySequence& secondKeySequence() const { return m_secondKeySequence; }
    void setSecondKeySequence(const QKeySequence& keys) { m_secondKeySequence = keys; }
    /// Shown in the page's list: of the page's hotkey mode and matching its name filter.
    bool isVisible() const { return m_visible; }
    void setVisible(bool visible) { m_visible = visible; }

    /// Announces a changed setting or key to QML.
    void notifyChanged() { Q_EMIT changed(); }

private:
    TActionSetting m_hotkeyActionSetting;
    appearances::TAppearanceInstancePtr m_objectAppearanceInstance;
    QKeySequence m_firstKeySequence;
    QKeySequence m_secondKeySequence;
    bool m_canHaveSmartModeFlag = false;
    bool m_visible = true;
};

}
}
