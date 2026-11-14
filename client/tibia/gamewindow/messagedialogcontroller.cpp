#include "messagedialogcontroller.h"

#include <QQuickItem>

namespace tibia::gamewindow {

TMessageDialogButton::TMessageDialogButton(const QString& label, const EMessageDialogButtonIdentifier identifier, const QKeySequence& key, QObject* parent)
    : cip::tibia::gamewindow::TMessageDialogButton(parent)
    , m_key(key)
{
    m_label = label;
    m_identifier = identifier;
}

TMessageDialogController::TMessageDialogController(QObject* parent)
    : TDialogController(QStringLiteral("TibiaMessageDialog.qml"), parent)
{
}

void TMessageDialogController::setYesNoButtons()
{
    qDeleteAll(m_buttons);
    m_buttons = {
        new TMessageDialogButton(qtTrId("yes"), Button::YES, QKeySequence(QStringLiteral("Return")), this),
        new TMessageDialogButton(qtTrId("no"), Button::NO, QKeySequence(QStringLiteral("Esc")), this),
    };
}

void TMessageDialogController::setButtons(const QList<TMessageDialogButton*>& buttons)
{
    qDeleteAll(m_buttons);
    m_buttons = buttons;
    for (TMessageDialogButton* button : buttons)
        button->setParent(this);
}

QVariantMap TMessageDialogController::initialDialogProperties() const
{
    QVariantMap properties = TDialogController::initialDialogProperties();
    properties.insert(QStringLiteral("textCentered"), m_textCentered);
    properties.insert(QStringLiteral("caption"), m_caption);
    properties.insert(QStringLiteral("message"), m_message);
    properties.insert(QStringLiteral("checkBoxText"), m_checkBoxText);
    properties.insert(QStringLiteral("checkBoxIsChecked"), m_checkBoxIsChecked);
    QList<QObject*> buttons(m_buttons.begin(), m_buttons.end());
    properties.insert(QStringLiteral("buttons"), QVariant::fromValue(buttons));
    return properties;
}

void TMessageDialogController::applyProperties()
{
    QQuickItem* item = dialog()->item();
    if (!item)
        return;
    const QVariantMap properties = initialDialogProperties();
    for (auto it = properties.begin(); it != properties.end(); ++it)
        item->setProperty(it.key().toUtf8().constData(), it.value());
}

void TMessageDialogController::showMessage(const QString& caption, const QString& message)
{
    m_caption = caption;
    m_message = message;
    applyProperties();
    dialog()->show();
}

void TMessageDialogController::showMessageWithCheckBox(const QString& caption, const QString& message, const QString& checkBoxText, const bool checked)
{
    m_checkBoxText = checkBoxText;
    m_checkBoxIsChecked = checked;
    showMessage(caption, message);
}

void TMessageDialogController::onDialogButtonClicked(const QVariant& Identifier, const bool CheckBoxIsChecked)
{
    const auto identifier = static_cast<Button>(Identifier.toInt());
    Q_EMIT buttonClicked(identifier, CheckBoxIsChecked);
    switch (identifier) {
    case Button::OK:
    case Button::YES:
        Q_EMIT acceptClicked(CheckBoxIsChecked);
        break;
    case Button::CANCEL:
    case Button::NO:
        Q_EMIT discardClicked(CheckBoxIsChecked);
        break;
    default:
        break;
    }
    if (m_closeOnButtonClicked)
        dialog()->close();
}

void TMessageDialogController::pressButtonWithKey(const QKeySequence& key)
{
    for (TMessageDialogButton* button : std::as_const(m_buttons)) {
        if (button->key() == key) {
            const QQuickItem* item = dialog()->item();
            const bool checked = item && item->property("checkBoxIsChecked").toBool();
            onDialogButtonClicked(static_cast<int>(button->identifier()), checked);
            return;
        }
    }
}

void TMessageDialogController::onDialogReturnKeyPressed()
{
    pressButtonWithKey(QKeySequence(QStringLiteral("Return")));
}

void TMessageDialogController::onDialogEscapeKeyPressed()
{
    pressButtonWithKey(QKeySequence(QStringLiteral("Esc")));
}

void TMessageDialogController::onDialogKeyPressed(const int Key, const int Modifiers)
{
    pressButtonWithKey(QKeySequence(QKeyCombination(Qt::KeyboardModifiers(Modifiers), Qt::Key(Key))));
}

}
