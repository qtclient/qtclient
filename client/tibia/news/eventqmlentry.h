#pragma once

#include <cip_gen.h>

#include <QColor>
#include <QString>

namespace tibia {
namespace news {

struct TEventInformation;

/// Calendar-day row for gamewindow.qml (CIP: TEventQMLEntry).
class TEventQMLEntry final : public cip::tibia::news::TEventQMLEntry
{
    Q_OBJECT
public:
    TEventQMLEntry(QString title, QColor primary, QColor secondary, bool startsOrEndsThatDay,
                   QObject* parent = nullptr);
    /// CIP: the entry of `event`.
    TEventQMLEntry(const TEventInformation& event, bool startsOrEndsThatDay, QObject* parent = nullptr);

    const QString& description() const { return m_description; }
    int displayPriority() const { return m_displayPriority; }

    /// CIP: by display priority, then by title.
    static bool lessThan(const TEventQMLEntry* left, const TEventQMLEntry* right);

private:
    QString m_description;
    int m_displayPriority = 0;
};

}
}
