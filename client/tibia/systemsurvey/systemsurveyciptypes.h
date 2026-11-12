#pragma once

// CIP-named systemsurvey surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia {
namespace systemsurvey {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TFPSCounter final : public cip::systemsurvey::TFPSCounter
{
    Q_OBJECT
public:
    using cip::systemsurvey::TFPSCounter::TFPSCounter;
};

class TFPSHistory final : public cip::systemsurvey::TFPSHistory
{
    Q_OBJECT
public:
    using cip::systemsurvey::TFPSHistory::TFPSHistory;
};

class TFPSHistoryController final : public cip::systemsurvey::TFPSHistoryController
{
    Q_OBJECT
public:
    using cip::systemsurvey::TFPSHistoryController::TFPSHistoryController;
};

class TFPSHistoryToJsonConverter final : public ::tibia::CipTyped
{
public:
    TFPSHistoryToJsonConverter() = default;
};

class TFPSHistoryUploader final : public ::tibia::CipTyped
{
public:
    TFPSHistoryUploader() = default;
};

class TSystemSurveyValueRetriever final : public ::tibia::CipTyped
{
public:
    TSystemSurveyValueRetriever() = default;
};

}
}

