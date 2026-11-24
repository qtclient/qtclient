#include "minimapmanager.h"

#include "cyclopedia/cyclopediamapstaticdatastorage.h"

#include <QQmlContext>
#include <QStandardPaths>

namespace tibia::minimap {

TMinimapManager::TMinimapManager(std::shared_ptr<cyclopedia::TCyclopediaMapStaticDataStorage> CyclopediaMapStaticDataStorage)
    : cip::tibia::minimap::TMinimapManager(nullptr)
    , m_pCyclopediaMapStaticDataStorage(std::move(CyclopediaMapStaticDataStorage))
{
}

void TMinimapManager::registerInQmlContext(QQmlContext* Context)
{
    Context->setContextProperty(QStringLiteral("MinimapManager"), this);
}

QString TMinimapManager::minimapDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/minimap");
}

}
