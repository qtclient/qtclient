#pragma once

#include "input/gameaction.h"

#include <cip_gen.h>

#include <QString>

namespace tibia {
namespace gamewindow {

class TMessageDialogController;

/// CIP gamewindow::TLinkCopyWarningDialogController: copying a text that holds
/// a link asks first. The text waits while a Yes / No message dialog shows;
/// Yes copies it as trusted text, No drops it.
class TLinkCopyWarningDialogController final : public cip::tibia::gamewindow::TLinkCopyWarningDialogController
{
    Q_OBJECT
public:
    explicit TLinkCopyWarningDialogController(QObject* parent = nullptr);

    /// REQUEST_CONFORMATION_FOR_COPYING_TEXT_TO_CLIPBOARD: keeps the text and
    /// shows the warning.
    void handleGameAction(const input::IGameAction& action);

public Q_SLOTS:
    /// Yes copies, No drops the text.
    void onButtonClicked(cip::tibia::gamewindow::TMessageDialogButton::EMessageDialogButtonIdentifier ButtonIdentifier) override;
    /// Publishes the text as trusted text to copy, then drops it.
    void onYesClicked() override;
    void onNoClicked() override;

private:
    TMessageDialogController* m_messageDialog;
    QString m_text;
};

}
}
