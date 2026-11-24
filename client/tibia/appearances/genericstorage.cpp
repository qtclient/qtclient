#include "appearances/genericstorage.h"

#include "appearances/appearanceinstance.h"

#include <QQmlContext>
#include <QString>
#include <QVariant>

namespace tibia::appearances {

namespace {

QString storagesManagerContextPropertyName()
{
    return QStringLiteral("AppearanceInstanceGenericStorageManager");
}

}

TAppearanceInstanceGenericStorage::TAppearanceInstanceGenericStorage(
    std::shared_ptr<TAppearanceInstanceFactory> AppearanceInstanceFactory)
    : cip::tibia::appearances::TAppearanceInstanceGenericStorage(nullptr)
    , m_pAppearanceInstanceFactory(std::move(AppearanceInstanceFactory))
{
}

TAppearanceInstanceGenericStorage::~TAppearanceInstanceGenericStorage() = default;

void TAppearanceInstanceGenericStorage::addAppearanceInstance(const std::shared_ptr<TAppearanceInstance>& Instance)
{
    m_appearanceInstances[static_cast<uint32_t>(Instance->serialNumber())] = Instance;
}

void TAppearanceInstanceGenericStorage::removeAppearanceInstance(const std::shared_ptr<TAppearanceInstance>& Instance)
{
    m_appearanceInstances.erase(static_cast<uint32_t>(Instance->serialNumber()));
}

void TAppearanceInstanceGenericStorage::clear(const bool KeepEffects)
{
    if (!KeepEffects) {
        m_appearanceInstances.clear();
        return;
    }
    std::erase_if(m_appearanceInstances, [](const auto& entry) {
        return !std::dynamic_pointer_cast<TEffectInstance>(entry.second);
    });
}

TAppearanceInstanceGenericStoragesManager::TAppearanceInstanceGenericStoragesManager(
    std::shared_ptr<TAppearanceInstanceFactory> AppearanceInstanceFactory, QObject* parent)
    : cip::tibia::appearances::TAppearanceInstanceGenericStoragesManager(parent)
    , m_pAppearanceInstanceFactory(std::move(AppearanceInstanceFactory))
{
}

TAppearanceInstanceGenericStoragesManager::~TAppearanceInstanceGenericStoragesManager() = default;

std::shared_ptr<TAppearanceInstanceGenericStorage> TAppearanceInstanceGenericStoragesManager::createStorage()
{
    auto storage = std::make_shared<TAppearanceInstanceGenericStorage>(m_pAppearanceInstanceFactory);
    m_storages.push_back(storage);
    return storage;
}

void TAppearanceInstanceGenericStoragesManager::updateWithTimestamp(const qint64 Timestamp)
{
    for (auto it = m_storages.begin(); it != m_storages.end();) {
        const std::shared_ptr<TAppearanceInstanceGenericStorage> storage = it->lock();
        if (!storage) {
            it = m_storages.erase(it);
            continue;
        }
        if (storage->isEnabled() && !storage->appearanceInstances().empty()) {
            bool changed = false;
            for (const auto& [serialNumber, instance] : storage->appearanceInstances())
                changed |= instance->updateAnimation(Timestamp);
            if (changed)
                Q_EMIT storage->appearancesUpdated();
        }
        ++it;
    }
}

void TAppearanceInstanceGenericStoragesManager::registerInQmlContext(QQmlContext* Context)
{
    Context->setContextProperty(storagesManagerContextPropertyName(), this);
}

TAppearanceInstanceGenericStoragesManager* TAppearanceInstanceGenericStoragesManager::fromQmlContext(QQmlContext* Context)
{
    return Context->contextProperty(storagesManagerContextPropertyName())
        .value<TAppearanceInstanceGenericStoragesManager*>();
}

}
