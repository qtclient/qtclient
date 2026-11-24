#pragma once

#include "gamewindow/dialogmanager.h"

#include <cip_gen.h>

class QQmlEngine;
class QQuickItem;

namespace tibia {
namespace gamewindow {

/// CIP TTwoFactorCodeEnterTextDialogController — TwoFactorTextEnterDialog.qml:
/// the code prompt of the two-factor login (e-mail or authenticator).
class TTwoFactorCodeEnterTextDialogController final
    : public TDialogController<cip::tibia::gamewindow::TTwoFactorCodeEnterTextDialogController>
{
    Q_OBJECT
public:
    /// CIP: the dialog without a maximum input length.
    TTwoFactorCodeEnterTextDialogController(QQmlEngine* engine, QQuickItem* dialogParentItem, QObject* parent = nullptr);
    /// CIP: the dialog with the maximum length of the code (announced with
    /// maxInputLengthChanged).
    TTwoFactorCodeEnterTextDialogController(uint MaxInputLength, QQmlEngine* engine, QQuickItem* dialogParentItem,
                                            QObject* parent = nullptr);

    /// CIP v26: the dialog's item goes.
    void destroyDialog() { dialog()->destroy(); }

    /// CIP: the code the player entered last (empty after an empty OK).
    QString enteredText() const override;
    /// CIP: whether the dialog offers to send the e-mail code again.
    void setEmailResendPossible(bool Possible) override;

public Q_SLOTS:
    void onCancelClicked() override;
    void onOkClicked(const QString& EnteredText) override;
    void onButtonClicked() override;
    void onControllerFinished() override;
    bool getIsTrustedDevice() override;
    /// Sets the flag without a change signal.
    void setIsTrustedDevice(bool TrustedDevice) override;
    ETwoFactorMethod getTwoFactorMethod() override;
    /// Sets the method without a change signal.
    void setTwoFactorMethod(ETwoFactorMethod Method) override;
    QString getAdditionalInfoText() override;
    uint getMaxInputLength() override;
};

}
}
