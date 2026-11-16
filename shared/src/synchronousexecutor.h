#pragma once

#include "executor.h"

namespace tibia {
namespace shared {

/// CIP `shared::TSynchronousExecutor`: runs each task on the calling thread.
class TSynchronousExecutor : public TExecutor
{
    Q_OBJECT

public:
    using TExecutor::TExecutor;

public Q_SLOTS:
    void start(QRunnable* Task) override;
};

}
}
