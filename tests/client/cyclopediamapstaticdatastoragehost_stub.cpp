// The part of cyclopediamapstaticdatastoragehost.cpp the storage's vtable
// reaches (its IStaticHouseDataObserver override calls setHousesStorage),
// without the houses storage and the sprite system that file needs.

#include "cyclopedia/cyclopediamapstaticdatastorage.h"

namespace tibia::cyclopedia {

void TCyclopediaMapStaticDataStorage::setHousesStorage(const THousesStorage*) {}

} // namespace tibia::cyclopedia
