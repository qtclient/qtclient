#include "eventqmlentry.h"

#include "news/eventsuploader.h"

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

TEventQMLEntry::TEventQMLEntry(const TEventInformation& event, const bool startsOrEndsThatDay, QObject* parent)
    : cip::tibia::news::TEventQMLEntry(parent)
    , m_description(event.description)
    , m_displayPriority(event.displayPriority)
{
    m_title = event.name;
    m_primaryColor = event.colorLight;
    m_secondaryColor = event.colorDark;
    m_startsOrEndsThatDay = startsOrEndsThatDay;
}

bool TEventQMLEntry::lessThan(const TEventQMLEntry* left, const TEventQMLEntry* right)
{
    if (left->m_displayPriority != right->m_displayPriority)
        return static_cast<uint>(left->m_displayPriority) < static_cast<uint>(right->m_displayPriority);
    return QString::compare(left->m_title, right->m_title, Qt::CaseSensitive) < 0;
}

}
