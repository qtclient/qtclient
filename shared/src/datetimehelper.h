#pragma once

#include <QDateTime>
#include <QString>

namespace tibia {
namespace shared {

/// "yyyy-MM-dd hh:mm:ss,zzz", CipSoft's log and report timestamp.
QString formatTimestamp(const QDateTime& dateTime);

}
}
