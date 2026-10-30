#pragma once

#include "gamewindow/dialogmanager.h"

#include <cip_gen.h>

#include <QList>

namespace tibia::chat {
class TWhiteBlacklist;
}

namespace tibia::gamewindow {

/// The ignore list dialog, WhiteBlacklistDialog.qml (CIP:
/// TWhiteBlacklistDialogController, OPEN_WHITEBLACKLIST_DIALOG). The QML
/// tables edit TWhiteBlacklistQmlEntry objects; OK writes the flags and the
/// named entries back to the TWhiteBlacklist (0x100c293e0).
class TWhiteBlacklistDialogController final : public TDialogController<cip::tibia::gamewindow::TWhiteBlacklistDialogController>
{
    Q_OBJECT
public:
    explicit TWhiteBlacklistDialogController(chat::TWhiteBlacklist& lists, QObject* parent = nullptr);
    ~TWhiteBlacklistDialogController() override;

    /// CIP 0x100c28cf0: the flags and both lists from the TWhiteBlacklist, then shown.
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

    /// CIP 0x100c28d80 / 0x100c28f90.
    void fill(Entries& entries, bool blacklist);
    /// CIP 0x100c29bd0: the other entries leave edit mode, a new unnamed one enters it.
    void addEntry(Entries& entries);
    void removeEntry(Entries& entries, int row);
    /// CIP 0x100c292b0 / 0x100c29340.
    static void clear(Entries& entries);
    void publish(const char* property, const Entries& entries);
    /// CIP 0x100c291a0.
    void close();

    chat::TWhiteBlacklist& m_lists;
    Entries m_blacklistEntries;
    Entries m_whitelistEntries;
};

}
