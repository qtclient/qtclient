#pragma once

namespace tibia {

/// Legacy marker on handwritten CIP-named types. New ports do not derive from it;
/// a class that derives from it and has no members is an unported shell.
class CipTyped
{
protected:
    CipTyped() = default;
    ~CipTyped() = default;
    CipTyped(const CipTyped&) = default;
    CipTyped& operator=(const CipTyped&) = default;
};

}
