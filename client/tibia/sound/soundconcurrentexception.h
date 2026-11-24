#pragma once

#include <QException>

namespace tibia {
namespace sound {

/// CIP sound::TSoundConcurrentException: what a sound conversion on the
/// thread pool reports when it threw. The future keeps it; reading the
/// future's result throws it again on the reading thread.
class TSoundConcurrentException : public QException
{
public:
    void raise() const override { throw *this; }
    TSoundConcurrentException* clone() const override { return new TSoundConcurrentException(*this); }
};

}
}
