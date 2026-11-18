#pragma once

#include "authentication/authenticationjsondocumenthelper.h"

#include <cip_gen.h>

namespace tibia {
namespace gamewindow {

/// One row of CharacterSelection.qml's table: the character's columns, its
/// outfit and whether it is the main, a pinned or a hidden character.
class TCharacterInformationModelData final : public cip::tibia::gamewindow::TCharacterInformationModelData
{
    Q_OBJECT
public:
    TCharacterInformationModelData(const authentication::TCharacter& character, const QString& level, const QString& vocation,
                                   const QString& world, const QString& worldRules, bool isPinned);
};

}
}
