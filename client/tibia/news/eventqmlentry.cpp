#include "eventqmlentry.h"

namespace tibia::news {

TEventQMLEntry::TEventQMLEntry(QString title, QColor primary, QColor secondary, const bool startsOrEndsThatDay,
                               QObject* parent)
    : cip::tibia::news::TEventQMLEntry(parent)
{
    m_title = std::move(title);
    m_primaryColor = std::move(primary);
    m_secondaryColor = std::move(secondary);
    m_startsOrEndsThatDay = startsOrEndsThatDay;
}

}
