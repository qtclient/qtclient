#pragma once

#include "gamewindow/dialogmanager.h"

#include <cip_gen.h>

#include <QKeySequence>
#include <QList>

namespace tibia::gamewindow {

/// One button of TibiaMessageDialog.qml (CIP: TMessageDialogButton, built by
/// 0x100b8ecb0, vtable 0x1026348d0).
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
/// TibiaMessageDialog.qml (CIP: TMessageDialogController, ctor 0x100b8fec0,
/// vtable 0x102634958).
class TMessageDialogController final : public TDialogController<cip::tibia::gamewindow::TMessageDialogController>
{
    Q_OBJECT
public:
    using Button = cip::tibia::gamewindow::TMessageDialogButton::EMessageDialogButtonIdentifier;

    explicit TMessageDialogController(QObject* parent = nullptr);

    /// CIP 0x100b8fbb0: "Yes" (Return) and "No" (Escape).
    void setYesNoButtons();
    /// The buttons CIP passes to 0x100b905c0 (e.g. FUN_100b8f8a0's OK and Cancel).
    void setButtons(const QList<TMessageDialogButton*>& buttons);
    /// CIP 0x100b90580: a button click closes the dialog.
    void setCloseOnButtonClicked(bool close) { m_closeOnButtonClicked = close; }
    void setTextCentered(bool centered) { m_textCentered = centered; }
    /// CIP 0x100b905c0.
    void showMessage(const QString& caption, const QString& message);
    /// CIP 0x100b906e0.
    void showMessageWithCheckBox(const QString& caption, const QString& message, const QString& checkBoxText, bool checked);

    QVariantMap initialDialogProperties() const override;

    /// CIP 0x100b907f0: buttonClicked, then acceptClicked (OK, YES) or
    /// discardClicked (CANCEL, NO).
    Q_INVOKABLE void onDialogButtonClicked(const QVariant& Identifier, bool CheckBoxIsChecked) override;
    /// CIP 0x100b90890 / 0x100b90c00 / 0x100b90cc0: the button whose key
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
