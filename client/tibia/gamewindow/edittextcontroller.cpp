#include "edittextcontroller.h"

#include "input/gameaction.h"
#include "input/gameactionhandlers.h"

#include <tibia/world/game.h>
#include <tibia/assets/thingtype.h>
#include <tibia/assets/things.h>

namespace tibia::gamewindow {

namespace {

/// CIP 0x100c30570.
QString itemTextDescription(const QString& author, const QString& date, const bool hasText, const bool editable)
{
    QString description;
    if (!hasText)
        description = qtTrId(editable ? "edit_text_writable" : "edit_text_unwritable");
    else if (author.isEmpty())
        description = qtTrId("edit_text_no_author");
    else if (!date.isEmpty())
        description = qtTrId("edit_text_name_plus_date").arg(author, date);
    else
        description = qtTrId("edit_text_name_only").arg(author);
    if (editable)
        description.append(QLatin1Char('\n') + qtTrId("edit_text_enter_new_text"));
    return description;
}

}

void TEditTextUIData::set(const QString& caption, const QString& description, const QString& text, const bool editable, const int maxLength,
                          const uint objectId, const bool phishingWarning, const bool recentlyTradedWarning, const bool reportButton)
{
    m_dialogCaption = caption;
    m_editDescription = description;
    m_editText = text;
    m_textIsEditable = editable;
    m_maximumEditTextLength = maxLength;
    m_hasDisplaySprite = objectId != 0;
    m_objectID = objectId;
    m_objectCount = objectId != 0 ? 1 : 0;
    m_showPhishingWarning = phishingWarning;
    m_showRecentlyTradedWarning = recentlyTradedWarning;
    m_showReportButton = reportButton;
}

TEditTextController::TEditTextController(QObject* parent)
    : TDialogController(QStringLiteral("EditTextDialog.qml"), parent)
{
    m_modelData = &m_data;
}

void TEditTextController::showItemText(const uint32_t id, const uint itemId, const int maxLength, const QString& text, const QString& author,
                                       const QString& date, const bool recentlyTraded)
{
    bool editable = false;
    bool reportable = false;
    if (g_things.isValidDatId(itemId, ThingCategoryItem)) {
        const ThingTypePtr& type = g_things.getThingType(itemId, ThingCategoryItem);
        editable = type->isWritable() || (type->isWritableOnce() && text.isEmpty());
        reportable = type->isReportable();
    }
    const bool stampedLetter = itemId != 0 && itemId == g_things.getStampedLetterId();
    m_mode = EMode::ItemText;
    m_id = id;
    m_author = author;
    m_data.set(qtTrId(editable ? "edit_text_caption" : "edit_text_show_text"), itemTextDescription(author, date, !text.isEmpty(), editable),
               text, editable, maxLength, itemId, stampedLetter && !author.isEmpty(), recentlyTraded,
               reportable && !text.isEmpty() && !author.isEmpty());
    reopen();
}

void TEditTextController::showEditList(const uint32_t id, const uint8_t doorId, const QString& text)
{
    m_mode = EMode::EditList;
    m_id = id;
    m_doorId = doorId;
    m_data.set(qtTrId("edit_text_edit_list"), qtTrId("edit_text_one_per_line"), text, true, 1999);
    reopen();
}

/// CIP 0x100c302d0 — guild Motd edit; OK calls submitNewText → editGuildMessage.
void TEditTextController::showEditGuildMessageDialog(const QString& Text)
{
    m_mode = EMode::GuildMessage;
    m_id = 0;
    m_data.set(qtTrId("context_edit_guild_message"), qtTrId("edit_text_enter_new_text"), Text, true, 255);
    reopen();
}

/// CIP 0x100c30920.
void TEditTextController::showReadOnlyText(const QString& DialogCaption, const QString& Text)
{
    m_mode = EMode::ItemText;
    m_id = 0;
    m_data.set(DialogCaption, {}, Text, false, Text.size());
    reopen();
}

/// CIP 0x100c30a90.
void TEditTextController::submitNewText(const QString& NewText)
{
    dialog()->close();
    if (!g_game.isOnline())
        return;
    switch (m_mode) {
        case EMode::ItemText:
            if (m_data.textIsEditable())
                g_game.editText(m_id, NewText.toStdString());
            break;
        case EMode::EditList:
            g_game.editList(m_id, m_doorId, NewText.toStdString());
            break;
        case EMode::GuildMessage:
            g_game.editGuildMessage(NewText.toStdString());
            break;
    }
}

/// CIP 0x100c30c20.
void TEditTextController::abortEditing()
{
    dialog()->close();
}

/// CIP 0x100c30c80.
void TEditTextController::reportText()
{
    auto action = input::mutableClone(*input::makeAction(input::EGameAction::REPORT_TEXT));
    action->text = m_author;
    action->parameter = static_cast<int>(m_id);
    action->statement = m_data.editText();
    input::publish(action);
}

void TEditTextController::reopen()
{
    dialog()->close();
    dialog()->show();
}

}
