#pragma once

#include "gamewindow/dialogmanager.h"

#include <cip_gen.h>

#include <QKeySequence>
#include <QList>

namespace tibia {
namespace gamewindow {

/// One button of TibiaMessageDialog.qml (CIP: TMessageDialogButton, built by
/// , vtable).
class TMessageDialogButton final : public cip::tibia::gamewindow::TMessageDialogButton
{
    Q_OBJECT
public:
    TMessageDialogButton(const QString& label, EMessageDialogButtonIdentifier identifier, const QKeySequence& key, QObject* parent = nullptr);

    const QKeySequence& key() const { return m_key; }

private:
    QKeySequence m_key;
};

/// A message box with buttons and an optional check box,
/// TibiaMessageDialog.qml (CIP: TMessageDialogController, ctor ,
/// vtable).
class TMessageDialogController final : public TDialogController<cip::tibia::gamewindow::TMessageDialogController>
{
    Q_OBJECT
public:
    using Button = cip::tibia::gamewindow::TMessageDialogButton::EMessageDialogButtonIdentifier;

    explicit TMessageDialogController(QObject* parent = nullptr);

    /// CIP : "Yes" (Return) and "No" (Escape).
    void setYesNoButtons();
    /// The buttons CIP passes to (e.g. 's OK and Cancel).
    void setButtons(const QList<TMessageDialogButton*>& buttons);
    /// CIP : a button click closes the dialog.
    void setCloseOnButtonClicked(bool close) { m_closeOnButtonClicked = close; }
    void setTextCentered(bool centered) { m_textCentered = centered; }
    /// CIP .
    void showMessage(const QString& caption, const QString& message);
    /// CIP .
    void showMessageWithCheckBox(const QString& caption, const QString& message, const QString& checkBoxText, bool checked);

    QVariantMap initialDialogProperties() const override;

    /// CIP : buttonClicked, then acceptClicked (OK, YES) or
    /// discardClicked (CANCEL, NO).
    Q_INVOKABLE void onDialogButtonClicked(const QVariant& Identifier, bool CheckBoxIsChecked) override;
    /// CIP / / : the button whose key
    /// sequence is Return / Escape / the key.
    Q_INVOKABLE void onDialogReturnKeyPressed() override;
    Q_INVOKABLE void onDialogEscapeKeyPressed() override;
    Q_INVOKABLE void onDialogKeyPressed(int Key, int Modifiers) override;

private:
    void pressButtonWithKey(const QKeySequence& key);
    void applyProperties();

    QList<TMessageDialogButton*> m_buttons;
    QString m_caption;
    QString m_message;
    QString m_checkBoxText;
    bool m_checkBoxIsChecked = false;
    bool m_closeOnButtonClicked = false;
    bool m_textCentered = false;
};

}
}
