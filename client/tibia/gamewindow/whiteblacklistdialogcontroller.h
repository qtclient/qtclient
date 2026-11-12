#pragma once

#include "gamewindow/dialogmanager.h"

#include <cip_gen.h>

#include <QList>

namespace tibia {
namespace chat {
class TWhiteBlacklist;
}
}

namespace tibia {
namespace gamewindow {

/// The ignore list dialog, WhiteBlacklistDialog.qml (CIP:
/// TWhiteBlacklistDialogController, OPEN_WHITEBLACKLIST_DIALOG). The QML
/// tables edit TWhiteBlacklistQmlEntry objects; OK writes the flags and the
/// named entries back to the TWhiteBlacklist .
class TWhiteBlacklistDialogController final : public TDialogController<cip::tibia::gamewindow::TWhiteBlacklistDialogController>
{
    Q_OBJECT
public:
    explicit TWhiteBlacklistDialogController(chat::TWhiteBlacklist& lists, QObject* parent = nullptr);
    ~TWhiteBlacklistDialogController() override;

    /// CIP : the flags and both lists from the TWhiteBlacklist, then shown.
    void open();

    Q_INVOKABLE void addNewBlacklistEntry() override;
    Q_INVOKABLE void addNewWhitelistEntry() override;
    Q_INVOKABLE void removeEntryAtIndexFromBlacklist(int RowIndex) override;
    Q_INVOKABLE void removeEntryAtIndexFromWhitelist(int RowIndex) override;

public Q_SLOTS:
    void onCancelClicked() override;
    void onOkClicked() override;
    void onControllerFinished() override;

private:
    using Entries = QList<QObject*>;

    /// CIP.
    void fill(Entries& entries, bool blacklist);
    /// CIP : the other entries leave edit mode, a new unnamed one enters it.
    void addEntry(Entries& entries);
    void removeEntry(Entries& entries, int row);
    /// CIP.
    static void clear(Entries& entries);
    void publish(const char* property, const Entries& entries);
    /// CIP .
    void close();

    chat::TWhiteBlacklist& m_lists;
    Entries m_blacklistEntries;
    Entries m_whitelistEntries;
};

}
}
