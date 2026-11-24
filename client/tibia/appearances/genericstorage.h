#pragma once

#include <cip_gen.h>

#include <QtGlobal>

#include <cstdint>
#include <list>
#include <memory>
#include <unordered_map>

class QQmlContext;

namespace tibia {
namespace appearances {

class TAppearanceInstance;
class TAppearanceInstanceFactory;

/// CIP appearances::TAppearanceInstanceGenericStorage: the appearance
/// instances one QML item shows, by their serial numbers. While it is enabled
/// the storages manager advances them before each frame; when any of them
/// changed, `appearancesUpdated` follows.
class TAppearanceInstanceGenericStorage final : public cip::tibia::appearances::TAppearanceInstanceGenericStorage
{
    Q_OBJECT
public:
    using TAppearanceInstances = std::unordered_map<uint32_t, std::shared_ptr<TAppearanceInstance>>;

    /// A storage of instances made by `AppearanceInstanceFactory`; enabled from the start.
    explicit TAppearanceInstanceGenericStorage(std::shared_ptr<TAppearanceInstanceFactory> AppearanceInstanceFactory);
    ~TAppearanceInstanceGenericStorage() override;

    /// Keeps `Instance` under its serial number (replacing the one there).
    void addAppearanceInstance(const std::shared_ptr<TAppearanceInstance>& Instance);
    /// Forgets the instance with `Instance`'s serial number.
    void removeAppearanceInstance(const std::shared_ptr<TAppearanceInstance>& Instance);
    /// The instance kept under `SerialNumber`; null without one.
    std::shared_ptr<TAppearanceInstance> appearanceInstance(uint32_t SerialNumber) const;
    /// Forgets every instance; with `KeepEffects` the effect instances stay.
    void clear(bool KeepEffects);

    /// The factory the storage's instances are made with.
    const std::shared_ptr<TAppearanceInstanceFactory>& appearanceInstanceFactory() const { return m_pAppearanceInstanceFactory; }
    const TAppearanceInstances& appearanceInstances() const { return m_appearanceInstances; }
    bool isEnabled() const { return m_enabled; }
    void setEnabled(const bool Enabled) { m_enabled = Enabled; }

private:
    std::shared_ptr<TAppearanceInstanceFactory> m_pAppearanceInstanceFactory;
    TAppearanceInstances m_appearanceInstances;
    bool m_enabled = true;
};

/// CIP appearances::TAppearanceInstanceGenericStoragesManager: the client's
/// one source of generic storages. QML items find it in their context
/// (`AppearanceInstanceGenericStorageManager`) and take a storage of their
/// own; the manager only watches them (a storage goes with its item) and,
/// before each frame, advances the instances of every storage still there.
class TAppearanceInstanceGenericStoragesManager final
    : public cip::tibia::appearances::TAppearanceInstanceGenericStoragesManager
{
    Q_OBJECT
public:
    explicit TAppearanceInstanceGenericStoragesManager(
        std::shared_ptr<TAppearanceInstanceFactory> AppearanceInstanceFactory, QObject* parent = nullptr);
    ~TAppearanceInstanceGenericStoragesManager() override;

    /// A new storage of the manager's factory, watched from now on.
    std::shared_ptr<TAppearanceInstanceGenericStorage> createStorage();
    /// Every enabled storage advances its instances to `Timestamp`
    /// (milliseconds); a storage with a changed instance emits
    /// `appearancesUpdated`. Storages that went are forgotten.
    void updateWithTimestamp(qint64 Timestamp);

    /// The manager as `Context`'s `AppearanceInstanceGenericStorageManager`.
    void registerInQmlContext(QQmlContext* Context);
    /// The manager `Context` offers; null without one.
    static TAppearanceInstanceGenericStoragesManager* fromQmlContext(QQmlContext* Context);

private:
    std::shared_ptr<TAppearanceInstanceFactory> m_pAppearanceInstanceFactory;
    std::list<std::weak_ptr<TAppearanceInstanceGenericStorage>> m_storages;
};

}
}
