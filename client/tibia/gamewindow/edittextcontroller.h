#pragma once

#include "gamewindow/dialogmanager.h"

#include <cip_gen.h>

namespace tibia::gamewindow {

/// What EditTextDialog.qml shows (CIP: TEditTextUIData).
class TEditTextUIData final : public cip::tibia::gamewindow::TEditTextUIData
{
    Q_OBJECT
public:
    using cip::tibia::gamewindow::TEditTextUIData::TEditTextUIData;

    /// CIP 0x100c30480.
    void set(const QString& caption, const QString& description, const QString& text, bool editable, int maxLength, uint objectId = 0,
             bool phishingWarning = false, bool recentlyTradedWarning = false, bool reportButton = false);
};

/// Books, letters, house lists, the guild message and read-only texts
/// (EditTextDialog.qml; CIP: TEditTextController).
class TEditTextController final : public TDialogController<cip::tibia::gamewindow::TEditTextController>
{
    Q_OBJECT
public:
    explicit TEditTextController(QObject* parent = nullptr);

    /// CIP 0x100c2fee0: a writable or readable item's text.
    void showItemText(uint32_t id, uint itemId, int maxLength, const QString& text, const QString& author, const QString& date,
                      bool recentlyTraded);
    /// CIP 0x100c30130: a house access list.
    void showEditList(uint32_t id, uint8_t doorId, const QString& text);

    void showEditGuildMessageDialog(const QString& Text) override;
    void showReadOnlyText(const QString& DialogCaption, const QString& Text) override;
    void submitNewText(const QString& NewText) override;
    void abortEditing() override;
    void reportText() override;

private:
    enum class EMode { ItemText, EditList, GuildMessage };

    void reopen();

    TEditTextUIData m_data;
    EMode m_mode = EMode::ItemText;
    uint32_t m_id = 0;
    uint8_t m_doorId = 0;
    QString m_author;
};

}
