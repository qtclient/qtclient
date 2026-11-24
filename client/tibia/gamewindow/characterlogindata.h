#pragma once

#include <QString>

namespace tibia {
namespace gamewindow {

/// What CipSoft's character selection hands on for a login: the character and
/// how to reach its world (CIP TCharacterLoginData).
struct TCharacterLoginData
{
    QString characterName;
    QString worldAddressProtected;
    QString worldAddressUnprotected;
    int worldPortUnprotected = 0;
    int worldPortProtected = 0;
    QString worldName;
    bool antiCheatProtection = false;
    bool tutorial = false;
    bool isMale = false;
};

}
}
