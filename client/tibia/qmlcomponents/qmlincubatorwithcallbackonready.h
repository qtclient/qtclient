#pragma once

#include <QQmlIncubator>

#include <functional>

namespace tibia {
namespace qmlcomponents {

/// A QML incubator that runs a callback once its object is ready
/// (CIP: TQmlIncubatorWithCallbackOnReady). Asynchronous; the callback is
/// empty until a user sets it.
class TQmlIncubatorWithCallbackOnReady final : public QQmlIncubator
{
public:
    TQmlIncubatorWithCallbackOnReady()
        : QQmlIncubator(QQmlIncubator::Asynchronous)
    {
    }

    std::function<void()> m_CallbackOnReady = [] {};

protected:
    void statusChanged(Status status) override
    {
        QQmlIncubator::statusChanged(status);
        if (status == QQmlIncubator::Ready && m_CallbackOnReady)
            m_CallbackOnReady();
    }
};

}
}
