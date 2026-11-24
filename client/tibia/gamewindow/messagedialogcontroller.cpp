#include "messagedialogcontroller.h"

#include <QQuickItem>

#include <algorithm>

namespace tibia::gamewindow {

TMessageDialogButton::TMessageDialogButton(const EMessageDialogButtonIdentifier identifier, const QString& label,
                                           const uint defaultKeys, QObject* parent)
    : cip::tibia::gamewindow::TMessageDialogButton(parent)
{
    m_identifier = identifier;
    m_label = label;
    if (defaultKeys & RETURN_KEYS) {
        m_keySequences.emplace_back(QStringLiteral("Return"));
        m_keySequences.emplace_back(QStringLiteral("Enter"));
    }
    if (defaultKeys & ESCAPE_KEY)
        m_keySequences.emplace_back(QStringLiteral("Esc"));
}

TMessageDialogButton::TMessageDialogButton(const EMessageDialogButtonIdentifier identifier, const QString& label,
                                           const QKeySequence& key, QObject* parent)
    : cip::tibia::gamewindow::TMessageDialogButton(parent)
    , m_keySequences{ key }
{
    m_identifier = identifier;
    m_label = label;
}

TMessageDialogButton::TMessageDialogButton(const TMessageDialogButton& other)
    : cip::tibia::gamewindow::TMessageDialogButton(other.parent())
    , m_keySequences(other.m_keySequences)
{
    m_identifier = other.m_identifier;
    m_label = other.m_label;
}

bool TMessageDialogButton::hasKey(const QKeySequence& key) const
{
    return std::find(m_keySequences.begin(), m_keySequences.end(), key) != m_keySequences.end();
}

QList<TMessageDialogButton*> okDialogButtons()
{
    return { new TMessageDialogButton(TMessageDialogButton::EMessageDialogButtonIdentifier::OK, qtTrId("ok"),
                                      TMessageDialogButton::RETURN_KEYS | TMessageDialogButton::ESCAPE_KEY) };
}

QList<TMessageDialogButton*> cancelDialogButtons()
{
    return { new TMessageDialogButton(TMessageDialogButton::EMessageDialogButtonIdentifier::CANCEL, qtTrId("cancel"),
                                      TMessageDialogButton::RETURN_KEYS | TMessageDialogButton::ESCAPE_KEY) };
}

QList<TMessageDialogButton*> okCancelDialogButtons()
{
    return { new TMessageDialogButton(TMessageDialogButton::EMessageDialogButtonIdentifier::OK, qtTrId("ok"),
                                      TMessageDialogButton::RETURN_KEYS),
             new TMessageDialogButton(TMessageDialogButton::EMessageDialogButtonIdentifier::CANCEL, qtTrId("cancel"),
                                      TMessageDialogButton::ESCAPE_KEY) };
}

TMessageDialogController::TMessageDialogController(QObject* parent)
    : TDialogController(QStringLiteral("TibiaMessageDialog.qml"), parent)
{
}

TMessageDialogController::TMessageDialogController(QQmlEngine* engine, QQuickItem* dialogParentItem, QObject* parent)
    : TDialogController(QStringLiteral("TibiaMessageDialog.qml"), engine, dialogParentItem, parent)
{
}

void TMessageDialogController::setMessageDialogTextFormat(EMessageDialogTextFormat format)
{
    m_messageDialogTextFormat = format;
    Q_EMIT messageDialogTextFormatChanged();
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

void TMessageDialogController::showButtons(const QList<TMessageDialogButton*>& buttons)
{
    qDeleteAll(m_shownButtons);
    m_shownButtons.clear();
    QQuickItem* item = dialog()->item();
    if (!item)
        return;
    QList<QObject*> shown;
    for (const TMessageDialogButton* button : buttons) {
        auto* copy = new TMessageDialogButton(*button);
        copy->setParent(this);
        shown.append(copy);
        m_shownButtons.push_back(copy);
    }
    item->setProperty("buttons", QVariant::fromValue(QList<QObject*>()));
    item->setProperty("buttons", QVariant::fromValue(shown));
}

void TMessageDialogController::showMessage(const QString& caption, const QString& message,
                                           const QList<TMessageDialogButton*>& buttons)
{
    m_caption = caption;
    m_message = message;
    dialog()->create();
    if (QQuickItem* item = dialog()->item()) {
        item->setProperty("textCentered", m_textCentered);
        item->setProperty("caption", caption);
        item->setProperty("message", message);
    }
    showButtons(buttons);
    dialog()->show();
}

void TMessageDialogController::showMessage(const QString& caption, const QString& message)
{
    showMessage(caption, message, m_buttons);
}

void TMessageDialogController::showMessageWithCheckBox(const QString& caption, const QString& message, const QString& checkBoxText, const bool checked)
{
    m_checkBoxText = checkBoxText;
    m_checkBoxIsChecked = checked;
    dialog()->create();
    if (QQuickItem* item = dialog()->item()) {
        item->setProperty("checkBoxText", checkBoxText);
        item->setProperty("checkBoxIsChecked", checked);
    }
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
        if (button->hasKey(key)) {
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
