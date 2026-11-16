#pragma once

#include <QObject>
#include <QRunnable>

namespace tibia {
namespace shared {

/// CIP `shared::TExecutor`: runs QRunnable tasks and reports each finished one
/// through `executed`.
class TExecutor : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

Q_SIGNALS:
    void executed(QRunnable* Task);

public Q_SLOTS:
    virtual void start(QRunnable* Task) = 0;
    void emitExecutedSignal(QRunnable* Task);
};

}
}
