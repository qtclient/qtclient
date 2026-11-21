#include "taskboard/taskboardshopstorage.h"

namespace tibia::taskboard {

TTaskboardShopStorage::TTaskboardShopStorage(QObject* parent)
    : cip::tibia::taskboard::TTaskboardShopStorage(parent)
{
}

/// Replaces the offers; the change signal fires also when the same list is set again.
void TTaskboardShopStorage::setShopRewardData(const std::vector<TShopRewardData>& data)
{
    if (&m_data != &data)
        m_data = data;
    Q_EMIT shopRewardDataChanged();
}

}
