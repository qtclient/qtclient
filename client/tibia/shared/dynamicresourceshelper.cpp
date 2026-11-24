#include "shared/dynamicresourceshelper.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QResource>

#include <stdexcept>

namespace tibia::shared {

QString TDynamicResourcesHelper::getResourceFilePath(const QString& resourceName)
{
    const QDir applicationDirectory(QCoreApplication::applicationDirPath());
    return applicationDirectory.filePath(QStringLiteral("%1_resources.rcc").arg(resourceName));
}

void TDynamicResourcesHelper::registerResources(const std::vector<QString>& resourceNames)
{
    for (const QString& resourceName : resourceNames) {
        const QString filePath = getResourceFilePath(resourceName);
        if (!QFile::exists(filePath)) {
            const QString message =
                QStringLiteral("Could not register resource '%1'. File '%2' not found").arg(resourceName).arg(filePath);
            qCritical() << message;
            throw std::runtime_error(message.toStdString());
        }
        if (!QResource::registerResource(filePath, QString())) {
            const QString message =
                QStringLiteral("Could not register resource '%1' in File '%2'. Error while registering")
                    .arg(resourceName).arg(filePath);
            qCritical() << message;
            throw std::runtime_error(message.toStdString());
        }
    }
}

}
