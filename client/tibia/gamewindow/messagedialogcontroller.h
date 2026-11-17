#pragma once

#include "gamewindow/dialogmanager.h"

#include <cip_gen.h>

#include <QKeySequence>
#include <QList>

#include <vector>

namespace tibia {
namespace gamewindow {

/// One button of TibiaMessageDialog.qml (CIP: TMessageDialogButton): an
/// identifier, a label and the key sequences that press it.
class TMessageDialogButton final : public cip::tibia::gamewindow::TMessageDialogButton
{
    Q_OBJECT
public:
    /// Default keys of a button (CIP flags): Return and Enter, Esc.
    enum EDefaultKeys : uint
    {
        NO_KEYS = 0,
        RETURN_KEYS = 1,
        ESCAPE_KEY = 2,
    };

    TMessageDialogButton(EMessageDialogButtonIdentifier identifier, const QString& label, uint defaultKeys, QObject* parent = nullptr);
    TMessageDialogButton(EMessageDialogButtonIdentifier identifier, const QString& label, const QKeySequence& key, QObject* parent = nullptr);
    TMessageDialogButton(const QString& label, EMessageDialogButtonIdentifier identifier, const QKeySequence& key, QObject* parent = nullptr)
        : TMessageDialogButton(identifier, label, key, parent)
    {}
    /// CIP: a copy of `other` with the same parent.
    explicit TMessageDialogButton(const TMessageDialogButton& other);

    const std::vector<QKeySequence>& keySequences() const { return m_keySequences; }
    bool hasKey(const QKeySequence& key) const;

private:
    std::vector<QKeySequence> m_keySequences;
};

/// "OK" pressed by Return or Escape.
QList<TMessageDialogButton*> okDialogButtons();
/// "Cancel" pressed by Return or Escape.
QList<TMessageDialogButton*> cancelDialogButtons();
/// "OK" (Return) and "Cancel" (Escape).
QList<TMessageDialogButton*> okCancelDialogButtons();

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
