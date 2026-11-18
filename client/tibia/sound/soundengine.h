#pragma once

#include <cip_gen.h>

#include <QString>
#include <QStringList>

#include <memory>
#include <string>

namespace tibia {
namespace sound {

class TSoundConfiguration;

/// CIP sound::TSoundEngine: the output device. A process-wide singleton.
class TSoundEngine final : public cip::tibia::sound::TSoundEngine
{
    Q_OBJECT
public:
    static TSoundEngine& instance();

    /// Opens the default device and follows the configured device from now on.
    void initialize(const std::shared_ptr<TSoundConfiguration>& configuration);
    bool isInitialized() const { return m_initialized; }
    void setInitialized(bool initialized) { m_initialized = initialized; }
    const QStringList& availableSoundDevices() const { return m_availableDevices; }

public Q_SLOTS:
    /// Re-reads the device list; switches device when needed.
    void updateAvailableDevices();
    /// Moves playback to the configured device, or the default one.
    void switchDevice();

private:
    TSoundEngine() = default;

    bool m_initialized = false;
    std::string m_usedDevice;
    std::string m_defaultDevice;
    std::shared_ptr<TSoundConfiguration> m_configuration;
    QStringList m_availableDevices;
};

}
}
