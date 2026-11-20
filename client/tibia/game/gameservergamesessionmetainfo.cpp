#include "game/gameservergamesessionmetainfo.h"

namespace tibia::game {

TGameSessionMetaInfo::EType TGameserverGameSessionMetaInfo::getType() const
{
    return EType::Gameserver;
}

}
