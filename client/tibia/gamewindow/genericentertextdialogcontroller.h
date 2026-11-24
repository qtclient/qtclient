#pragma once

#include "gamewindow/dialogmanager.h"

#include <cip_gen.h>

namespace tibia {
namespace gamewindow {

/// A one-line text prompt, GenericEnterTextDialog.qml (CIP:
/// TGenericEnterTextDialogController). OK with a text (or with an empty text
/// when that is allowed) emits textEntered; Cancel, and OK with an empty text
/// otherwise, emit abort. Both buttons close the dialog.
class TGenericEnterTextDialogController final : public TDialogController<cip::tibia::gamewindow::TGenericEnterTextDialogController>
{
    Q_OBJECT
public:
    explicit TGenericEnterTextDialogController(QObject* parent = nullptr);
    TGenericEnterTextDialogController(const QString& caption, const QString& description, uint maxInputLength,
                                      QObject* parent = nullptr);
    TGenericEnterTextDialogController(const QString& caption, const QString& description, uint maxInputLength, QQmlEngine* engine,
                                      QQuickItem* dialogParentItem, QObject* parent = nullptr);

    /// CIP: sets the text the input starts with, then creates and shows the dialog.
    void showWithText(const QString& text);
    /// CIP: deletes the dialog item.
    void destroyDialog();

    void setCaption(const QString& caption) { m_caption = caption; }
    void setDescription(const QString& description) { m_description = description; }
    void setText(const QString& text) { m_enteredText = text; }
    void setMaxInputLength(uint length) { m_maxInputLength = length; }
    void setReadOnly(bool readOnly) { m_readOnly = readOnly; }
    void setDialogWidth(int width) { m_dialogWidth = width; }
    void setConfirmLabel(const QString& label) { m_confirmLable = label; }
    void setIntOnly(bool intOnly);
    /// CIP: OK with an empty text emits textEntered instead of abort.
    void setAllowEmptyText(bool allow) { m_allowEmptyText = allow; }

public Q_SLOTS:
    void onCancelClicked() override;
    void onOkClicked(const QString& EnteredText) override;
    void onControllerFinished() override;

private:
    bool m_allowEmptyText = false;
};

}
}
