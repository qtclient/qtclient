#pragma once

#include <cip_gen.h>

namespace tibia::appearances {

/// CIP moc surfaces promoted out of cip_ports (QObject trackers / generic storage).
class TAppearanceInstanceGenericStorage final : public cip::tibia::appearances::TAppearanceInstanceGenericStorage
{
public:
    using cip::tibia::appearances::TAppearanceInstanceGenericStorage::TAppearanceInstanceGenericStorage;
};

class TAppearanceInstanceGenericStoragesManager final
    : public cip::tibia::appearances::TAppearanceInstanceGenericStoragesManager
{
public:
    using cip::tibia::appearances::TAppearanceInstanceGenericStoragesManager::TAppearanceInstanceGenericStoragesManager;
};

class TAppearanceInstanceTracker final : public cip::tibia::appearances::TAppearanceInstanceTracker
{
public:
    using cip::tibia::appearances::TAppearanceInstanceTracker::TAppearanceInstanceTracker;
};

}
