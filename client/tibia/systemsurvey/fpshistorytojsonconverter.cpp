#include "systemsurvey/fpshistorytojsonconverter.h"

#include "client/clientciptypes.h"
#include "datetimehelper.h"
#include "systemsurvey/fpshistory.h"

#include <QJsonArray>
#include <QJsonObject>

namespace tibia::systemsurvey {

namespace {

const QString DataTypeKey = QStringLiteral("dataType");
const QString ClientIdKey = QStringLiteral("clientID");
const QString FPSHistoryKey = QStringLiteral("FPSHistory");
const QString MaximumFPSKey = QStringLiteral("maxFPS");
const QString MinimumFPSKey = QStringLiteral("minFPS");
const QString AverageFPSKey = QStringLiteral("averageFPS");
const QString TimeStampKey = QStringLiteral("timeStamp");
const QString RendererKey = QStringLiteral("renderer");

int roundedFPS(const double fps) { return int(fps + 0.5); }

QJsonObject sampleToJson(const TFPSSample& sample)
{
    QJsonObject object;
    object.insert(MaximumFPSKey, roundedFPS(sample.maximumFPS));
    object.insert(MinimumFPSKey, roundedFPS(sample.minimumFPS));
    object.insert(AverageFPSKey, roundedFPS(sample.averageFPS));
    object.insert(TimeStampKey, shared::formatTimestamp(sample.time));
    return object;
}

QJsonArray samplesToJson(const std::vector<TFPSSample>& samples)
{
    QJsonArray array;
    for (const TFPSSample& sample : samples)
        array.append(sampleToJson(sample));
    return array;
}

}

QJsonDocument TFPSHistoryToJsonConverter::convert(const TFPSHistory& history, const int renderer) const
{
    QJsonDocument document;
    QJsonObject object;
    object.insert(DataTypeKey, 1);
    object.insert(ClientIdKey, client::TGameApplication::hardwareId());
    object.insert(FPSHistoryKey, samplesToJson(history.samples()));
    object.insert(RendererKey, renderer);
    document.setObject(object);
    return document;
}

}
