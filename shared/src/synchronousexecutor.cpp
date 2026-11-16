#include "synchronousexecutor.h"

namespace tibia::shared {

void TSynchronousExecutor::start(QRunnable* Task)
{
    Task->run();
    Q_EMIT executed(Task);
}

}
