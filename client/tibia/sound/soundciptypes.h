#pragma once

// CIP-named sound surfaces owned by TSoundStorage / game session.

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia::sound {

class TActiveSoundPlayer final : public cip::tibia::sound::TActiveSoundPlayer
{
    Q_OBJECT
public:
    using cip::tibia::sound::TActiveSoundPlayer::TActiveSoundPlayer;
};

class TGameSessionSoundProvider final : public cip::tibia::sound::TGameSessionSoundProvider
{
    Q_OBJECT
public:
    using cip::tibia::sound::TGameSessionSoundProvider::TGameSessionSoundProvider;
};

class TObjectSoundController final : public cip::tibia::sound::TObjectSoundController
{
    Q_OBJECT
public:
    using cip::tibia::sound::TObjectSoundController::TObjectSoundController;
};

class TSoundConcurrentException final : public ::tibia::CipTyped {};

class TSoundConfiguration final : public cip::tibia::sound::TSoundConfiguration
{
    Q_OBJECT
public:
    using cip::tibia::sound::TSoundConfiguration::TSoundConfiguration;
};

class TSoundController final : public cip::tibia::sound::TSoundController
{
    Q_OBJECT
public:
    using cip::tibia::sound::TSoundController::TSoundController;
};

class TSoundControllerAmbienceObjectSound final : public ::tibia::CipTyped {};

class TSoundControllerAmbienceStream final : public ::tibia::CipTyped {};

class TSoundControllerBase final : public cip::tibia::sound::TSoundControllerBase
{
    Q_OBJECT
public:
    using cip::tibia::sound::TSoundControllerBase::TSoundControllerBase;
};

class TSoundControllerCombatSound final : public ::tibia::CipTyped {};

class TSoundControllerMusic final : public ::tibia::CipTyped {};

class TSoundDecompressorThreadpool final : public cip::tibia::sound::TSoundDecompressorThreadpool
{
    Q_OBJECT
public:
    using cip::tibia::sound::TSoundDecompressorThreadpool::TSoundDecompressorThreadpool;
};

class TSoundEngine final : public cip::tibia::sound::TSoundEngine
{
    Q_OBJECT
public:
    using cip::tibia::sound::TSoundEngine::TSoundEngine;
    void notifyAvailableSoundDevicesChanged() { Q_EMIT availableSoundDevicesChanged(); }
};

class TSoundManager final : public cip::tibia::sound::TSoundManager
{
    Q_OBJECT
public:
    using cip::tibia::sound::TSoundManager::TSoundManager;
};

class TSoundMetadataManager final : public cip::tibia::sound::TSoundMetadataManager
{
    Q_OBJECT
public:
    using cip::tibia::sound::TSoundMetadataManager::TSoundMetadataManager;
};

class TSoundPlayer final : public cip::tibia::sound::TSoundPlayer
{
    Q_OBJECT
public:
    using cip::tibia::sound::TSoundPlayer::TSoundPlayer;
};

class TSoundPlayerManager final : public cip::tibia::sound::TSoundPlayerManager
{
    Q_OBJECT
public:
    using cip::tibia::sound::TSoundPlayerManager::TSoundPlayerManager;
};

} // namespace tibia::sound

