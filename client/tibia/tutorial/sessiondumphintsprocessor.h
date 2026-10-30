#pragma once

#include <cip_gen.h>

#include <QJsonObject>
#include <QString>
#include <QTimer>
#include <QVector>

namespace tibia::tutorial {

/// Drives CIP `tutorial-sessiondump.hints` sidecar actions during a dmpd replay
/// (CIP: TSessiondumpHintsProcessor). Timestamps are milliseconds from dump start.
class TSessiondumpHintsProcessor : public cip::tibia::tutorial::TSessiondumpHintsProcessor
{
    Q_OBJECT
public:
    explicit TSessiondumpHintsProcessor(QObject* parent = nullptr);
    ~TSessiondumpHintsProcessor() override;

    /// Load `path` (`.hints` JSON). No-op when missing / invalid.
    bool load(const QString& hintsPath);
    /// Arm timers relative to now (call when PacketPlayer starts).
    void start();
    void stop();

private:
    void fireBucket(qint64 timestampMs);
    void dispatchAction(const QJsonObject& action);

    QJsonObject m_actionsByTs;
    QVector<QTimer*> m_timers;
    bool m_running = false;
};

/// Sibling path of a `.dmp` → `.hints` when present.
QString hintsPathForDump(const QString& dumpPath);

}
