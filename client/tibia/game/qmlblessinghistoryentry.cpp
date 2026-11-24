#include "game/qmlblessinghistoryentry.h"

#include "qmlcomponents/texthelper.h"

#include <utility>

namespace tibia::game {

TQmlBlessingHistoryEntry::TQmlBlessingHistoryEntry(TBlessingDialogData::THistoryEntry&& Entry)
    : cip::tibia::game::TQmlBlessingHistoryEntry(nullptr)
    , m_entry(std::move(Entry))
{
}

QString TQmlBlessingHistoryEntry::timestamp() const
{
    qmlcomponents::TTextHelperQmlService textHelper;
    return textHelper.formatDateTime(static_cast<int>(m_entry.Timestamp), true);
}

bool TQmlBlessingHistoryEntry::blessingWasGained() const { return m_entry.Color != 5 && m_entry.Color != 0; }

QString TQmlBlessingHistoryEntry::description() const { return m_entry.Text; }

}
