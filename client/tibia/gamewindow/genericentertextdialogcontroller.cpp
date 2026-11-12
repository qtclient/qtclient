#include "genericentertextdialogcontroller.h"

namespace tibia::gamewindow {

TGenericEnterTextDialogController::TGenericEnterTextDialogController(QObject* parent)
    : TDialogController(QStringLiteral("GenericEnterTextDialog.qml"), parent)
{
    m_maxInputLength = 255;
    m_dialogWidth = -1;
    m_confirmLable = qtTrId("ok");
}

void TGenericEnterTextDialogController::onCancelClicked()
{
    dialog()->close();
}

void TGenericEnterTextDialogController::onOkClicked(const QString& EnteredText)
{
    m_enteredText = EnteredText;
    Q_EMIT textEntered(EnteredText);
    dialog()->close();
}

void TGenericEnterTextDialogController::onControllerFinished()
{
    dialog()->close();
}

}
