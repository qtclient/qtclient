#include "taskboard/taskboardbountytasksstorage.h"

#include <QCoreApplication>

namespace tibia::taskboard {

TTaskboardBountyTasksStorage::TTaskboardBountyTasksStorage(QObject* parent)
    : cip::tibia::taskboard::TTaskboardBountyTasksStorage(parent)
{
}

TTaskboardBountyTasksStorage* TTaskboardBountyTasksStorage::instance()
{
    static TTaskboardBountyTasksStorage* s_instance = new TTaskboardBountyTasksStorage(QCoreApplication::instance());
    return s_instance;
}

void TTaskboardBountyTasksStorage::setBountyTaskData(TBountyTaskData data)
{
    m_data = std::move(data);
    Q_EMIT bountyTaskDataChanged(m_data);
}

}
