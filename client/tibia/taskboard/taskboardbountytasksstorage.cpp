#include "taskboard/taskboardbountytasksstorage.h"

#include <QCoreApplication>

namespace tibia::taskboard {

TTaskboardBountyTasksStorage::TTaskboardBountyTasksStorage(QObject* parent)
    : cip::tibia::taskboard::TTaskboardBountyTasksStorage(parent)
{
}

const std::shared_ptr<TTaskboardBountyTasksStorage>& TTaskboardBountyTasksStorage::shared()
{
    static const std::shared_ptr<TTaskboardBountyTasksStorage> s_instance = std::make_shared<TTaskboardBountyTasksStorage>();
    return s_instance;
}

TTaskboardBountyTasksStorage* TTaskboardBountyTasksStorage::instance() { return shared().get(); }

void TTaskboardBountyTasksStorage::setBountyTaskData(TBountyTaskData data)
{
    m_data = std::move(data);
    Q_EMIT bountyTaskDataChanged(m_data);
}

}
