#include "taskboard/taskboardweeklytasksstorage.h"

#include <QCoreApplication>

namespace tibia::taskboard {

TTaskboardWeeklyTasksStorage::TTaskboardWeeklyTasksStorage(QObject* parent)
    : cip::tibia::taskboard::TTaskboardWeeklyTasksStorage(parent)
{
}

const std::shared_ptr<TTaskboardWeeklyTasksStorage>& TTaskboardWeeklyTasksStorage::shared()
{
    static const std::shared_ptr<TTaskboardWeeklyTasksStorage> s_instance = std::make_shared<TTaskboardWeeklyTasksStorage>();
    return s_instance;
}

TTaskboardWeeklyTasksStorage* TTaskboardWeeklyTasksStorage::instance() { return shared().get(); }

void TTaskboardWeeklyTasksStorage::setWeeklyTaskData(TWeeklyTaskData data)
{
    m_data = std::move(data);
    Q_EMIT weeklyTaskDataChanged(m_data);
}

}
