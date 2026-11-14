#include "whiteblacklistdialogcontroller.h"

#include "chat/chatprotocolmessagehandler.h"
#include "gamewindow/gamewindowciptypes.h"

#include <QQuickItem>

namespace tibia::gamewindow {

namespace {

using QmlEntry = TWhiteBlacklistQmlEntry;

}

TWhiteBlacklistDialogController::TWhiteBlacklistDialogController(chat::TWhiteBlacklist& lists, QObject* parent)
    : TDialogController(QStringLiteral("WhiteBlacklistDialog.qml"), parent)
    , m_lists(lists)
{}

TWhiteBlacklistDialogController::~TWhiteBlacklistDialogController()
{
    clear(m_blacklistEntries);
    clear(m_whitelistEntries);
}

void TWhiteBlacklistDialogController::open()
{
    m_blacklistActive = m_lists.blacklistActive();
    m_whitelistActive = m_lists.whitelistActive();
    m_ignoreYelling = m_lists.ignoreYelling();
    m_ignorePrivateMessages = m_lists.ignorePrivateMessages();
    m_allowVips = m_lists.allowVips();
    Q_EMIT displayDataChanged();
    dialog()->create();
    fill(m_blacklistEntries, true);
    fill(m_whitelistEntries, false);
    dialog()->show();
}

void TWhiteBlacklistDialogController::fill(Entries& entries, const bool blacklist)
{
    clear(entries);
    for (const auto& entry : blacklist ? m_lists.blacklist() : m_lists.whitelist()) {
        auto* item = new QmlEntry;
        item->setName(entry.name);
        item->setPermanently(entry.permanently);
        entries.append(item);
    }
    publish(blacklist ? "blacklistSource" : "whitelistSource", entries);
}

void TWhiteBlacklistDialogController::addEntry(Entries& entries)
{
    for (QObject* object : entries)
        static_cast<QmlEntry*>(object)->setEditmodeEnabled(false);
    auto* item = new QmlEntry;
    item->setEditmodeEnabled(true);
    entries.append(item);
}

void TWhiteBlacklistDialogController::removeEntry(Entries& entries, const int row)
{
    if (row >= 0 && row < entries.size())
        entries.takeAt(row)->deleteLater();
}

void TWhiteBlacklistDialogController::clear(Entries& entries)
{
    for (QObject* object : entries)
        object->deleteLater();
    entries.clear();
}

void TWhiteBlacklistDialogController::publish(const char* property, const Entries& entries)
{
    if (QQuickItem* item = dialog()->item())
        item->setProperty(property, QVariant::fromValue(entries));
}

void TWhiteBlacklistDialogController::addNewBlacklistEntry()
{
    addEntry(m_blacklistEntries);
    publish("blacklistSource", m_blacklistEntries);
}

void TWhiteBlacklistDialogController::addNewWhitelistEntry()
{
    addEntry(m_whitelistEntries);
    publish("whitelistSource", m_whitelistEntries);
}

void TWhiteBlacklistDialogController::removeEntryAtIndexFromBlacklist(const int RowIndex)
{
    removeEntry(m_blacklistEntries, RowIndex);
    publish("blacklistSource", m_blacklistEntries);
}

void TWhiteBlacklistDialogController::removeEntryAtIndexFromWhitelist(const int RowIndex)
{
    removeEntry(m_whitelistEntries, RowIndex);
    publish("whitelistSource", m_whitelistEntries);
}

void TWhiteBlacklistDialogController::onOkClicked()
{
    m_lists.setFlags(m_blacklistActive, m_whitelistActive, m_ignoreYelling, m_ignorePrivateMessages, m_allowVips);
    const auto collect = [](const Entries& entries) {
        chat::TWhiteBlacklist::Entries result;
        for (QObject* object : entries) {
            const auto* item = static_cast<const QmlEntry*>(object);
            if (!item->name().isEmpty())
                result.push_back({ item->name(), item->permanently() });
        }
        return result;
    };
    m_lists.setLists(collect(m_whitelistEntries), collect(m_blacklistEntries));
    close();
}

void TWhiteBlacklistDialogController::onCancelClicked()
{
    close();
}

void TWhiteBlacklistDialogController::onControllerFinished()
{
    close();
}

void TWhiteBlacklistDialogController::close()
{
    if (QQuickItem* item = dialog()->item()) {
        item->setProperty("blacklistSource", QVariant());
        item->setProperty("whitelistSource", QVariant());
    }
    clear(m_blacklistEntries);
    clear(m_whitelistEntries);
    dialog()->close();
}

}
