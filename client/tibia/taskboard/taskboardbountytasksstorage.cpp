#include "taskboard/taskboardbountytasksstorage.h"

namespace tibia::taskboard {

TTaskboardBountyTasksStorage::TTaskboardBountyTasksStorage(QObject* parent)
    : cip::tibia::taskboard::TTaskboardBountyTasksStorage(parent)
{
}

void TTaskboardBountyTasksStorage::setBountyTaskData(TBountyTaskData data)
{
    m_data = std::move(data);
    Q_EMIT bountyTaskDataChanged(m_data);
}

}
