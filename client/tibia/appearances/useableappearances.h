#pragma once

namespace tibia::appearances {

/// The appearances manager's set of useable appearances. The manager makes it
/// empty when it is created and hands it out unchanged
/// (TAppearancesManager::useableAppearances); the client never fills or reads
/// it, so it carries no entries.
class TUseableAppearances
{
};

} // namespace tibia::appearances
