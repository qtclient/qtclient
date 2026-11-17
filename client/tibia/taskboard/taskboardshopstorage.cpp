#include "taskboard/taskboardshopstorage.h"

#include <QCoreApplication>

namespace tibia::taskboard {

TTaskboardShopStorage::TTaskboardShopStorage(QObject* parent)
    : cip::tibia::taskboard::TTaskboardShopStorage(parent)
{
}

const std::shared_ptr<TTaskboardShopStorage>& TTaskboardShopStorage::shared()
{
    static const std::shared_ptr<TTaskboardShopStorage> s_instance = std::make_shared<TTaskboardShopStorage>();
    return s_instance;
}

TTaskboardShopStorage* TTaskboardShopStorage::instance() { return shared().get(); }

/// Replaces the offers; the change signal fires also when the same list is set again.
void TTaskboardShopStorage::setShopRewardData(const std::vector<TShopRewardData>& data)
{
    if (&m_data != &data)
        m_data = data;
    Q_EMIT shopRewardDataChanged();
}

}
