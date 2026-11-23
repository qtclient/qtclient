#include "linkcopywarningdialogcontroller.h"

#include "gamewindow/messagedialogcontroller.h"

#include <memory>

namespace tibia::gamewindow {

TLinkCopyWarningDialogController::TLinkCopyWarningDialogController(QObject* parent)
    : cip::tibia::gamewindow::TLinkCopyWarningDialogController(parent)
    , m_messageDialog(new TMessageDialogController(this))
{
    // A click on Yes or No closes the warning.
    m_messageDialog->setCloseOnButtonClicked(true);
    connect(m_messageDialog, &TMessageDialogController::buttonClicked, this,
            [this](const cip::tibia::gamewindow::TMessageDialogButton::EMessageDialogButtonIdentifier ButtonIdentifier, bool) {
                onButtonClicked(ButtonIdentifier);
            });
}

void TLinkCopyWarningDialogController::handleGameAction(const input::IGameAction& action)
{
    if (action.gameActionType() != input::EGameAction::REQUEST_CONFORMATION_FOR_COPYING_TEXT_TO_CLIPBOARD)
        return;
    const auto* copy = dynamic_cast<const input::TGameActionCopyTextToClipboard*>(&action);
    m_text = copy ? copy->text() : QString();
    m_messageDialog->setYesNoButtons();
    m_messageDialog->showMessage(qtTrId("link_copy_warning_caption"), qtTrId("link_copy_warning_body"));
}

void TLinkCopyWarningDialogController::onButtonClicked(
    const cip::tibia::gamewindow::TMessageDialogButton::EMessageDialogButtonIdentifier ButtonIdentifier)
{
    using B = cip::tibia::gamewindow::TMessageDialogButton::EMessageDialogButtonIdentifier;
    if (ButtonIdentifier == B::NO)
        onNoClicked();
    else if (ButtonIdentifier == B::YES)
        onYesClicked();
}

void TLinkCopyWarningDialogController::onYesClicked()
{
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionCopyTrustedTextToClipboard>(m_text));
    m_text.clear();
}

void TLinkCopyWarningDialogController::onNoClicked()
{
    m_text.clear();
}

}
