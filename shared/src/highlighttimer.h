#pragma once

#include <QObject>
#include <QTimer>

namespace tibia {
namespace shared {

/// CIP `shared::THighlightTimer`: keeps something highlighted for one
/// interval. A protected highlight cannot be stopped early without force.
class THighlightTimer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool highlighted READ isHighlighted NOTIFY changed FINAL)

public:
    explicit THighlightTimer(int Interval);
    ~THighlightTimer() override = default;

    /// True while the interval runs.
    bool isHighlighted() const;
    qint64 interval() const;
    /// (Re)starts the interval; once protected, a highlight stays protected
    /// until it is stopped.
    void start(bool Protected);
    /// Stops an unprotected highlight, or any highlight when `Force`.
    void stop(bool Force);

Q_SIGNALS:
    void changed();
    void timeout(bool Protected);

private:
    QTimer m_timer;
    bool m_protected = false;
};

}
}
