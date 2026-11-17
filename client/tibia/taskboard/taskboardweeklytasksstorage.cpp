#include "taskboard/taskboardweeklytasksstorage.h"

#include <QCoreApplication>

namespace tibia::taskboard {

TTaskboardWeeklyTasksStorage::TTaskboardWeeklyTasksStorage(QObject* parent)
    : cip::tibia::taskboard::TTaskboardWeeklyTasksStorage(parent)
{
}

TTaskboardWeeklyTasksStorage* TTaskboardWeeklyTasksStorage::instance()
{
    static TTaskboardWeeklyTasksStorage* s_instance = new TTaskboardWeeklyTasksStorage(QCoreApplication::instance());
    return s_instance;
}

void TTaskboardWeeklyTasksStorage::setWeeklyTaskData(TWeeklyTaskData data)
{
    m_data = std::move(data);
    Q_EMIT weeklyTaskDataChanged(m_data);
}

}
