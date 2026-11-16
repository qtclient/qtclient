#include "executor.h"

namespace tibia::shared {

void TExecutor::emitExecutedSignal(QRunnable* Task)
{
    Q_EMIT executed(Task);
}

}
