#pragma once

#include <QObject>

#include <memory>

namespace tibia {
namespace systemsurvey {

class TFPSHistory;
class TFPSHistoryUploader;

/// CIP `systemsurvey::TFPSHistoryController`: uploads the FPS history (with
/// the renderer it was measured on) when it has samples, then empties it.
class TFPSHistoryController : public QObject
{
    Q_OBJECT
public:
    TFPSHistoryController(std::shared_ptr<TFPSHistoryUploader> uploader, std::shared_ptr<TFPSHistory> history,
                          int renderer, QObject* parent = nullptr);
    ~TFPSHistoryController() override;

public Q_SLOTS:
    void uploadHistoryAndClearIt() const;

private:
    std::shared_ptr<TFPSHistoryUploader> m_uploader;
    std::shared_ptr<TFPSHistory> m_history;
    int m_renderer = 0;
};

}
}
