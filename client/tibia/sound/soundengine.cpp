#include "sound/soundengine.h"

#include "sound/audiobackend.h"
#include "sound/soundconfiguration.h"

#include <QDebug>

namespace tibia::sound {

TSoundEngine& TSoundEngine::instance()
{
    static TSoundEngine engine;
    return engine;
}

void TSoundEngine::initialize(const std::shared_ptr<TSoundConfiguration>& configuration)
{
    m_configuration = configuration;
    updateAvailableDevices();
    m_initialized = true;
    if (m_configuration)
        connect(m_configuration.get(), &TSoundConfiguration::soundConfigurationChanged, this, &TSoundEngine::switchDevice,
                Qt::UniqueConnection);
    // CipSoft also follows QMediaDevices::audioOutputsChanged; qtclient
    // re-reads the list when the sound options page asks for it.
    if (!playbackdevice::initialize())
        m_initialized = false;
    const std::optional<QString> defaultDevice = playbackdevice::defaultDevice();
    if (!defaultDevice) {
        m_initialized = false;
        qWarning() << "Sound engine could not not be initialized. No default sound device found";
        return;
    }
    m_defaultDevice = defaultDevice->toStdString();
    qDebug() << "Sound engine initialized with default sound device" << m_defaultDevice.c_str();
    switchDevice();
}

void TSoundEngine::updateAvailableDevices()
{
    const QStringList devices = playbackdevice::availableDevices();
    if (devices != m_availableDevices) {
        m_availableDevices = devices;
        Q_EMIT availableSoundDevicesChanged();
    }
    switchDevice();
}

void TSoundEngine::switchDevice()
{
    const std::optional<QString> defaultDevice = playbackdevice::defaultDevice();
    const std::optional<QString> currentDevice = playbackdevice::device();
    std::string wanted;
    if (m_configuration) {
        const std::string configured = m_configuration->usedSoundDevice().toStdString();
        const QString name = QString::fromStdString(configured);
        const bool available = m_availableDevices.contains(name);
        if (available && !configured.empty() && name != qtTrId("optionsmenu_sound_used_sound_device_auto")
            && name != QLatin1String("auto"))
            wanted = configured;
        else if (defaultDevice)
            wanted = defaultDevice->toStdString();
    }
    if (wanted.empty())
        return;
    if (currentDevice && currentDevice->toStdString() == wanted)
        return;
    if (!playbackdevice::setDevice(QString::fromStdString(wanted))) {
        qDebug() << "Error while switching audio device to" << wanted.c_str();
        return;
    }
    qDebug() << "Switch used audio device to" << wanted.c_str();
    m_usedDevice = wanted;
}

}
