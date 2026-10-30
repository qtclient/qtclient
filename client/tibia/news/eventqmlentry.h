#pragma once

#include <cip_gen.h>

#include <QColor>
#include <QString>

namespace tibia::news {

/// Calendar-day row for gamewindow.qml (CIP: TEventQMLEntry).
class TEventQMLEntry final : public cip::tibia::news::TEventQMLEntry
{
    Q_OBJECT
public:
    TEventQMLEntry(QString title, QColor primary, QColor secondary, bool startsOrEndsThatDay,
                   QObject* parent = nullptr);
};

}
