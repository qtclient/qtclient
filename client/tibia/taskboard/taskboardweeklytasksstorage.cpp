#include "taskboard/taskboardweeklytasksstorage.h"

namespace tibia::taskboard {

TTaskboardWeeklyTasksStorage::TTaskboardWeeklyTasksStorage(QObject* parent)
    : cip::tibia::taskboard::TTaskboardWeeklyTasksStorage(parent)
{
}

void TTaskboardWeeklyTasksStorage::setWeeklyTaskData(TWeeklyTaskData data)
{
    m_data = std::move(data);
    Q_EMIT weeklyTaskDataChanged(m_data);
}

}
