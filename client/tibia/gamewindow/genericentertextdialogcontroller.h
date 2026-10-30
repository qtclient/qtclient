#pragma once

#include "gamewindow/dialogmanager.h"

#include <cip_gen.h>

namespace tibia::gamewindow {

/// A one-line text prompt, GenericEnterTextDialog.qml (CIP:
/// TGenericEnterTextDialogController). OK emits textEntered; both buttons close.
class TGenericEnterTextDialogController final : public TDialogController<cip::tibia::gamewindow::TGenericEnterTextDialogController>
{
    Q_OBJECT
public:
    explicit TGenericEnterTextDialogController(QObject* parent = nullptr);

    void setCaption(const QString& caption) { m_caption = caption; }
    void setDescription(const QString& description) { m_description = description; }
    void setText(const QString& text) { m_enteredText = text; }
    void setMaxInputLength(uint length) { m_maxInputLength = length; }
    void setConfirmLabel(const QString& label) { m_confirmLable = label; }
    void setIntOnly(bool intOnly) { m_intOnly = intOnly; }

public Q_SLOTS:
    void onCancelClicked() override;
    void onOkClicked(const QString& EnteredText) override;
    void onControllerFinished() override;
};

}
