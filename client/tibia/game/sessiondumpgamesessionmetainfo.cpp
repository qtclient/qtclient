#include "game/sessiondumpgamesessionmetainfo.h"

namespace tibia::game {

TSessiondumpGameSessionMetaInfo::TSessiondumpGameSessionMetaInfo() = default;

TGameSessionMetaInfo::EType TSessiondumpGameSessionMetaInfo::getType() const
{
    return EType::Sessiondump;
}

}
