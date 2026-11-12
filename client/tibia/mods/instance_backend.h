#pragma once

#include <QObject>

namespace tibia {
namespace mods {

/// Optional instance/place backend (cyclopedia map default selection, etc.).
class InstanceBackend : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;

    virtual int subareaId() const = 0;
};

}
}
