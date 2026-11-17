#include "genericentertextdialogcontroller.h"

namespace tibia::gamewindow {

TGenericEnterTextDialogController::TGenericEnterTextDialogController(QObject* parent)
    : TDialogController(QStringLiteral("GenericEnterTextDialog.qml"), parent)
{
    m_maxInputLength = 255;
    m_dialogWidth = -1;
    m_confirmLable = qtTrId("ok");
}

TGenericEnterTextDialogController::TGenericEnterTextDialogController(const QString& caption, const QString& description,
                                                                     const uint maxInputLength, QObject* parent)
    : TGenericEnterTextDialogController(parent)
{
    m_caption = caption;
    m_description = description;
    m_maxInputLength = maxInputLength;
}

void TGenericEnterTextDialogController::showWithText(const QString& text)
{
    m_enteredText = text;
    dialog()->create();
    dialog()->show();
}

void TGenericEnterTextDialogController::destroyDialog()
{
    dialog()->destroy();
}

void TGenericEnterTextDialogController::setIntOnly(const bool intOnly)
{
    if (m_intOnly == intOnly)
        return;
    m_intOnly = intOnly;
    Q_EMIT changed();
}

void TGenericEnterTextDialogController::onCancelClicked()
{
    dialog()->close();
    Q_EMIT abort();
}

void TGenericEnterTextDialogController::onOkClicked(const QString& EnteredText)
{
    dialog()->close();
    if (EnteredText.isEmpty() && !m_allowEmptyText) {
        m_enteredText = QString();
        Q_EMIT abort();
        return;
    }
    m_enteredText = EnteredText;
    Q_EMIT textEntered(EnteredText);
}

void TGenericEnterTextDialogController::onControllerFinished()
{
    dialog()->close();
}

}
