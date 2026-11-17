#include "systemsurvey/fpshistorycontroller.h"

#include "systemsurvey/fpshistory.h"
#include "systemsurvey/fpshistoryuploader.h"

namespace tibia::systemsurvey {

TFPSHistoryController::TFPSHistoryController(std::shared_ptr<TFPSHistoryUploader> uploader,
                                             std::shared_ptr<TFPSHistory> history, const int renderer, QObject* parent)
    : QObject(parent)
    , m_uploader(std::move(uploader))
    , m_history(std::move(history))
    , m_renderer(renderer)
{
}

TFPSHistoryController::~TFPSHistoryController() = default;

void TFPSHistoryController::uploadHistoryAndClearIt() const
{
    if (m_history->size() == 0)
        return;
    m_uploader->uploadHistory(*m_history, m_renderer);
    m_history->clear();
}

}
