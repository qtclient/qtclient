#include "datetimehelper.h"

namespace tibia::shared {

QString formatTimestamp(const QDateTime& dateTime)
{
    return dateTime.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss,zzz"));
}

}
