#include "gamewindow/twofactorcodeentertextdialogcontroller.h"

namespace tibia::gamewindow {

TTwoFactorCodeEnterTextDialogController::TTwoFactorCodeEnterTextDialogController(QQmlEngine* engine,
                                                                                 QQuickItem* dialogParentItem,
                                                                                 QObject* parent)
    : TDialogController(QStringLiteral("TwoFactorTextEnterDialog.qml"), engine, dialogParentItem, parent)
{}

TTwoFactorCodeEnterTextDialogController::TTwoFactorCodeEnterTextDialogController(const uint MaxInputLength,
                                                                                 QQmlEngine* engine,
                                                                                 QQuickItem* dialogParentItem,
                                                                                 QObject* parent)
    : TDialogController(QStringLiteral("TwoFactorTextEnterDialog.qml"), engine, dialogParentItem, parent)
{
    m_maxInputLength = MaxInputLength;
    Q_EMIT maxInputLengthChanged();
}

QString TTwoFactorCodeEnterTextDialogController::enteredText() const
{
    return m_enteredText;
}

void TTwoFactorCodeEnterTextDialogController::setEmailResendPossible(const bool Possible)
{
    if (m_emailResendPossible == Possible)
        return;
    m_emailResendPossible = Possible;
    Q_EMIT emailResendPossibleChanged();
}

void TTwoFactorCodeEnterTextDialogController::onCancelClicked()
{
    dialog()->close();
    Q_EMIT abort();
}

/// A code is passed on; an empty one aborts.
void TTwoFactorCodeEnterTextDialogController::onOkClicked(const QString& EnteredText)
{
    dialog()->close();
    if (!EnteredText.isEmpty()) {
        m_enteredText = EnteredText;
        Q_EMIT textEntered(EnteredText);
        return;
    }
    m_enteredText.clear();
    Q_EMIT abort();
}

void TTwoFactorCodeEnterTextDialogController::onButtonClicked()
{
    Q_EMIT buttonClicked();
}

void TTwoFactorCodeEnterTextDialogController::onControllerFinished()
{
    dialog()->close();
    Q_EMIT dialogClosed();
}

bool TTwoFactorCodeEnterTextDialogController::getIsTrustedDevice()
{
    return m_trustedDevice;
}

void TTwoFactorCodeEnterTextDialogController::setIsTrustedDevice(const bool TrustedDevice)
{
    m_trustedDevice = TrustedDevice;
}

void TTwoFactorCodeEnterTextDialogController::setTwoFactorMethod(const ETwoFactorMethod Method)
{
    m_twoFactorMethod = Method;
}

TTwoFactorCodeEnterTextDialogController::ETwoFactorMethod TTwoFactorCodeEnterTextDialogController::getTwoFactorMethod()
{
    return m_twoFactorMethod;
}

QString TTwoFactorCodeEnterTextDialogController::getAdditionalInfoText()
{
    return m_additionalInfoText;
}

uint TTwoFactorCodeEnterTextDialogController::getMaxInputLength()
{
    return m_maxInputLength;
}

} // namespace tibia::gamewindow
