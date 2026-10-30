#pragma once

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia::config {

class TActionBarOptions final : public cip::tibia::config::TActionBarOptions
{
    Q_OBJECT
public:
    using cip::tibia::config::TActionBarOptions::TActionBarOptions;
};

class TActionBarsQMLOptionsPage : public cip::tibia::config::TActionBarsQMLOptionsPage
{
    Q_OBJECT
public:
    using cip::tibia::config::TActionBarsQMLOptionsPage::TActionBarsQMLOptionsPage;
};

class TBattleSoundsQMLOptionsPage : public cip::tibia::config::TBattleSoundsQMLOptionsPage
{
    Q_OBJECT
public:
    using cip::tibia::config::TBattleSoundsQMLOptionsPage::TBattleSoundsQMLOptionsPage;
};

class TButtonBarQMLOptionsPage : public cip::tibia::config::TButtonBarQMLOptionsPage
{
    Q_OBJECT
public:
    using cip::tibia::config::TButtonBarQMLOptionsPage::TButtonBarQMLOptionsPage;
};

class TCharacterConfigurationManager final : public cip::tibia::config::TCharacterConfigurationManager
{
    Q_OBJECT
public:
    using cip::tibia::config::TCharacterConfigurationManager::TCharacterConfigurationManager;
};

class TClientOptions final : public cip::tibia::config::TClientOptions
{
    Q_OBJECT
public:
    using cip::tibia::config::TClientOptions::TClientOptions;
};

class TClientOptionsModeSwitcher final : public cip::tibia::config::TClientOptionsModeSwitcher
{
    Q_OBJECT
public:
    using cip::tibia::config::TClientOptionsModeSwitcher::TClientOptionsModeSwitcher;
};

class TConsoleQMLOptionsPage : public cip::tibia::config::TConsoleQMLOptionsPage
{
    Q_OBJECT
public:
    using cip::tibia::config::TConsoleQMLOptionsPage::TConsoleQMLOptionsPage;
};

class TControlsQMLOptionsPage : public cip::tibia::config::TControlsQMLOptionsPage
{
    Q_OBJECT
public:
    using cip::tibia::config::TControlsQMLOptionsPage::TControlsQMLOptionsPage;
};

class TEffectsQMLOptionsPage : public cip::tibia::config::TEffectsQMLOptionsPage
{
    Q_OBJECT
public:
    using cip::tibia::config::TEffectsQMLOptionsPage::TEffectsQMLOptionsPage;
};

class TGameWindowQMLOptionsPage : public cip::tibia::config::TGameWindowQMLOptionsPage
{
    Q_OBJECT
public:
    using cip::tibia::config::TGameWindowQMLOptionsPage::TGameWindowQMLOptionsPage;
};

class TGameplayQMLOptionsPage : public cip::tibia::config::TGameplayQMLOptionsPage
{
    Q_OBJECT
public:
    using cip::tibia::config::TGameplayQMLOptionsPage::TGameplayQMLOptionsPage;
};

class TGraphicsQMLOptionsPage : public cip::tibia::config::TGraphicsQMLOptionsPage
{
    Q_OBJECT
public:
    using cip::tibia::config::TGraphicsQMLOptionsPage::TGraphicsQMLOptionsPage;
};

class THudQMLOptionsPage : public cip::tibia::config::THudQMLOptionsPage
{
    Q_OBJECT
public:
    using cip::tibia::config::THudQMLOptionsPage::THudQMLOptionsPage;
};

class TInterfaceQMLOptionsPage : public cip::tibia::config::TInterfaceQMLOptionsPage
{
    Q_OBJECT
public:
    using cip::tibia::config::TInterfaceQMLOptionsPage::TInterfaceQMLOptionsPage;
};

class TMiscQMLOptionsPage : public cip::tibia::config::TMiscQMLOptionsPage
{
    Q_OBJECT
public:
    using cip::tibia::config::TMiscQMLOptionsPage::TMiscQMLOptionsPage;
};

class TOptionPageQMLWrapperBase final : public cip::tibia::config::TOptionPageQMLWrapperBase
{
    Q_OBJECT
public:
    using cip::tibia::config::TOptionPageQMLWrapperBase::TOptionPageQMLWrapperBase;
};

class TScreenshotsQMLOptionsPage : public cip::tibia::config::TScreenshotsQMLOptionsPage
{
    Q_OBJECT
public:
    using cip::tibia::config::TScreenshotsQMLOptionsPage::TScreenshotsQMLOptionsPage;
};

class TSoundQMLOptionsPage : public cip::tibia::config::TSoundQMLOptionsPage
{
    Q_OBJECT
public:
    using cip::tibia::config::TSoundQMLOptionsPage::TSoundQMLOptionsPage;
};

class TUISoundsQMLOptionsPage : public cip::tibia::config::TUISoundsQMLOptionsPage
{
    Q_OBJECT
public:
    using cip::tibia::config::TUISoundsQMLOptionsPage::TUISoundsQMLOptionsPage;
};

} // namespace tibia::config

namespace tibia::config::TConfigBackupReader {

class TConfigBackupException final : public ::tibia::CipTyped
{
public:
    TConfigBackupException() = default;
};

} // namespace tibia::config::TConfigBackupReader
